"""The loop that runs all day: take in labelled issues, move tasks on, open
scheduled maintenance issues, post a daily log, and pace the Claude runs so
that a Max subscription shared with its owner is never used up."""
from __future__ import annotations

import logging
import os
import time
from pathlib import Path

from .claude import Claude
from .config import Config
from .gh import GitHub, GitHubError
from .git import GitError, Repo, git
from .pipeline import PROMPTS, Pipeline, Wait
from .state import State, Task

log = logging.getLogger("xppagent")

# Stages in which a task needs attention, most advanced first: finishing work
# beats starting it.
ORDER = ("owner", "ci", "pr", "planned", "blocked", "queued", "merged")
OPEN = ("planned", "pr", "ci", "owner", "blocked")
LABEL_COLOURS = {"agent": "5319e7", "agent:working": "fbca04", "agent:blocked": "d93f0b",
                 "agent:priority": "b60205", "agent:paused": "cccccc"}


def load_secrets(path: Path) -> None:
    """KEY=VALUE lines into the environment (GH_TOKEN, CLAUDE_CODE_OAUTH_TOKEN)."""
    if not path.exists():
        return
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            os.environ[key.strip()] = value.strip().strip('"')


def _in_ranges(ranges: list[str], when: time.struct_time) -> bool:
    """Is the local time inside one of the "HH:MM-HH:MM" ranges?"""
    now = when.tm_hour * 60 + when.tm_min
    for r in ranges:
        a, b = (int(h) * 60 + int(m) for h, m in (x.split(":") for x in r.split("-")))
        if (a <= now < b) if a <= b else (now >= a or now < b):
            return True
    return False


class Budget:
    """Claude runs allowed: a sliding window, quiet hours, a usage-limit pause."""

    def __init__(self, cfg: Config, state: State, clock=time.time):
        self.cfg = cfg
        self.state = state
        self.clock = clock

    def check(self) -> None:
        now = self.clock()
        if now < self.state.paused_until:
            raise Wait(self.state.paused_until, "paused after a usage limit")
        if self.cfg.quiet_hours and _in_ranges(self.cfg.quiet_hours, time.localtime(now)):
            raise Wait(now + 600, "quiet hours")
        window = self.cfg.window_hours * 3600
        self.state.runs = [t for t in self.state.runs if now - t < window]
        if len(self.state.runs) >= self.cfg.runs_per_window:
            raise Wait(self.state.runs[0] + window, "the run budget for this window is used")

    def take(self) -> None:
        self.check()
        self.state.runs.append(self.clock())

    def used(self) -> int:
        now = self.clock()
        return sum(1 for t in self.state.runs if now - t < self.cfg.window_hours * 3600)


class Supervisor:
    def __init__(self, cfg: Config, *, gh=None, claude=None, repo=None, clock=time.time):
        self.cfg = cfg
        self.clock = clock
        self.state = State.load(cfg.state_file)
        self.gh = gh or GitHub(cfg.slug, dry_run=cfg.dry_run)
        self.claude = claude or Claude(cfg.claude, model=cfg.model, minutes=cfg.run_minutes,
                                       max_turns=cfg.max_turns,
                                       system=PROMPTS / "system.md", log_dir=cfg.runs_dir)
        self.repo = repo or Repo(f"https://github.com/{cfg.slug}.git", cfg.clone, cfg.work,
                                 cfg.base, dry_run=cfg.dry_run)
        self.budget = Budget(cfg, self.state, clock)
        self.pipeline = Pipeline(cfg, self.gh, self.repo, self.claude, self.state,
                                 self.budget)
        self.me = ""              # the token's login
        self.log_issue = 0
        self.waiting = ""         # why no Claude run is allowed now, if so
        self.labelled: list[dict] = []   # this pass's open issues with the label

    # -- setup -----------------------------------------------------------------

    def setup(self) -> None:
        """The clone, its git settings, the labels."""
        self.cfg.home.mkdir(parents=True, exist_ok=True)
        clone = self.cfg.clone
        if not (clone / ".git").exists():
            clone.parent.mkdir(parents=True, exist_ok=True)
            # gh supplies the credentials (GH_TOKEN), not a credential manager.
            git("-c", "credential.helper=", "-c",
                "credential.helper=!gh auth git-credential", "clone", self.repo.url,
                str(clone), cwd=clone.parent)
        for key, value in (("user.name", self.cfg.git_name),
                           ("user.email", self.cfg.git_email),
                           ("core.autocrlf", "false")):
            git("config", key, value, cwd=clone)
        git("config", "--replace-all", "credential.helper", "", cwd=clone)
        git("config", "--add", "credential.helper", "!gh auth git-credential", cwd=clone)
        self.repo.ensure()
        self.me = self.gh.login()
        self.gh.ensure_labels({self.cfg.label: LABEL_COLOURS["agent"],
                               self.cfg.label_working: LABEL_COLOURS["agent:working"],
                               self.cfg.label_blocked: LABEL_COLOURS["agent:blocked"],
                               self.cfg.label_priority: LABEL_COLOURS["agent:priority"],
                               self.cfg.label_paused: LABEL_COLOURS["agent:paused"]})

    # -- one pass of the loop --------------------------------------------------

    def tick(self) -> bool:
        """Do what can be done now; returns whether anything happened."""
        if self.cfg.stop_file.exists():
            log.info("%s exists: doing nothing", self.cfg.stop_file)
            return False
        if self._paused():
            return False
        self.repo.ensure()
        self.intake()
        self.schedule()
        did = self.work()
        self.daily_log()
        self.save()
        return did

    def _paused(self) -> bool:
        n = self._log_issue()
        if not n:
            return False
        labels = {l["name"] for l in self.gh.issue(n).get("labels", [])}
        if self.cfg.label_paused in labels:
            log.info("paused by the %s label on #%s", self.cfg.label_paused, n)
            return True
        return False

    def _log_issue(self) -> int:
        if not self.log_issue:
            self.log_issue = self.gh.find_open_issue(self.cfg.log_issue_title)
        return self.log_issue

    def intake(self) -> None:
        """New labelled issues become tasks; tasks whose issue went away end."""
        trusted = {self.cfg.owner, self.me} - {""}
        labelled = {}
        self.labelled = self.gh.issues(self.cfg.label)
        for issue in self.labelled:
            n = issue["number"]
            labelled[n] = issue
            task = self.state.task(n)
            if task and task.stage not in ("closed", "failed"):
                continue
            if self.state.ignored.get(str(n)) == issue.get("updatedAt"):
                continue
            who = self.gh.labeled_by(n, self.cfg.label)
            if who not in trusted:
                log.info("#%s was labelled by %s, not the owner: ignored", n, who)
                self.state.ignored[str(n)] = issue.get("updatedAt")
                continue
            if task and task.stage == "failed" and task.failed_at:
                # It failed part-way: carry on in the same stage, with the same
                # branch and pull request (a closed one ends it there).
                task.stage, task.failed_at, task.note = task.failed_at, "", ""
                task.rounds.pop("errors", None)
                self.gh.labels(n, add=[self.cfg.label_working])
                self.state.event(f"#{n} labelled again: carrying on at {task.stage}")
                continue
            new = self.state.add(Task(n, issue["title"]))
            if task:  # worked on before: start over on a branch of its own
                new.attempt = task.attempt + 1
            self.state.event(f"#{n} taken in: {issue['title']}")
            log.info("took in #%s %s", n, issue["title"])
        for task in self.state.tasks.values():
            if task.stage in ("closed", "failed", "merged") or task.issue in labelled:
                continue
            info = self.gh.issue(task.issue)
            if info.get("state") == "CLOSED" or not any(
                    l["name"] == self.cfg.label for l in info.get("labels", [])):
                # Merging a pull request that says "Closes #n" closes the issue
                # too: that is a merge (watched afterwards), not a stop.
                if task.pr and self.pipeline._finished(task):
                    continue
                task.stage = "closed"
                self.gh.labels(task.issue, remove=[self.cfg.label_working,
                                                   self.cfg.label_blocked])
                if task.branch:
                    self.repo.remove(task.branch)
                self.state.event(f"#{task.issue} stopped: the issue was closed or unlabelled")

    def _priority(self, task: Task, labels: dict) -> tuple:
        urgent = self.cfg.label_priority in labels.get(task.issue, ())
        return (not urgent, ORDER.index(task.stage), task.issue)

    def work(self) -> bool:
        """Step each task once, most advanced first; start new ones while
        fewer than max_open are in progress."""
        labels = {i["number"]: {l["name"] for l in i.get("labels", [])}
                  for i in self.labelled}
        tasks = sorted((t for t in self.state.tasks.values() if t.stage in ORDER),
                       key=lambda t: self._priority(t, labels))
        did = False
        self.waiting = ""
        for task in tasks:
            if task.stage == "queued" and sum(t.stage in OPEN for t in
                                              self.state.tasks.values()) >= self.cfg.max_open:
                continue
            try:
                stepped = self.pipeline.step(task)
            except Wait as w:
                self._wait(w)
                break
            except (GitHubError, GitError, OSError) as e:
                self._error(task, e)
                continue
            if stepped:
                task.rounds.pop("errors", None)
                did = True
                self.save()
        return did

    def _wait(self, w: Wait) -> None:
        self.waiting = f"{w.why} (until {time.strftime('%H:%M', time.localtime(w.until))})"
        log.info("waiting: %s", self.waiting)
        if "usage limit" in w.why or w.alert:
            self.state.paused_until = max(self.state.paused_until, w.until)
        if w.alert and self.clock() - self.state.alerted > 12 * 3600:
            self.state.alerted = self.clock()
            self.state.event(f"needs a person: {w.why}")
            n = self._log_issue()
            if n:
                self.gh.comment(n, f"@{self.cfg.owner} ⚠️ I have stopped: {w.why}.")

    def _error(self, task: Task, e: Exception) -> None:
        count = task.bump("errors")
        log.exception("#%s (%s): %s", task.issue, task.stage, e)
        task.note = f"error: {str(e)[:300]}"
        if count >= 5:
            self.state.event(f"#{task.issue} failed: {str(e)[:200]}")
            self.gh.comment(task.issue, f"🤖 I keep failing on this ({task.stage}) and have "
                                        f"stopped:\n\n```\n{str(e)[:1500]}\n```\n\n"
                                        f"Add the `{self.cfg.label}` label again to have me "
                                        "carry on from there.")
            self.gh.labels(task.issue, remove=[self.cfg.label, self.cfg.label_working])
            task.failed_at = task.stage
            task.stage = "failed"

    # -- scheduled work and the log --------------------------------------------

    def schedule(self) -> None:
        """Open each recurring job's issue when it is due (one at a time)."""
        now = self.clock()
        for job in self.cfg.jobs:
            last = self.state.jobs_last.get(job.name)
            if last is None:
                self.state.jobs_last[job.name] = now  # the first comes one period later
                continue
            if now - last < job.every_hours * 3600:
                continue
            self.state.jobs_last[job.name] = now
            if self.gh.find_open_issue(job.title):
                continue
            n = self.gh.create_issue(job.title, job.body.strip() + "\n\n_A scheduled task "
                                     f"(`{job.name}`) of the maintainer agent._",
                                     [self.cfg.label])
            self.state.event(f"opened #{n} for the scheduled task {job.name}")

    def daily_log(self) -> None:
        now = self.clock()
        t = time.localtime(now)
        today = time.strftime("%Y-%m-%d", t)
        if self.state.log_day == today or t.tm_hour < self.cfg.log_hour:
            return
        n = self._log_issue()
        if not n:
            n = self.log_issue = self.gh.create_issue(
                self.cfg.log_issue_title,
                "The maintainer agent posts a summary here every day. Add the "
                f"`{self.cfg.label_paused}` label to this issue to pause it; remove "
                "the label to let it go on.")
        self.gh.comment(n, self.report(since=self.state.log_time or now - 86400))
        self.state.log_day = today
        self.state.log_time = now

    def report(self, since: float) -> str:
        events = [(t, e) for t, e in self.state.events if t >= since]
        lines = [f"### 🤖 {time.strftime('%A %d %B %Y', time.localtime(self.clock()))}", ""]
        lines += ([f"- {time.strftime('%H:%M', time.localtime(t))} {e}" for t, e in events]
                  or ["Nothing happened since the last report."])
        active = [t for t in self.state.tasks.values() if t.stage in OPEN + ("queued",)]
        if active:
            lines += ["", "**In progress**", ""]
            for t in sorted(active, key=lambda t: t.issue):
                pr = f", PR #{t.pr}" if t.pr else ""
                lines.append(f"- #{t.issue} {t.title}: {t.stage}{pr}")
        lines += ["", f"Claude runs in the last {self.cfg.window_hours:g} hours: "
                      f"{self.budget.used()} of {self.cfg.runs_per_window}."]
        if self.state.paused_until > self.clock():
            lines.append("Paused until " + time.strftime(
                "%H:%M", time.localtime(self.state.paused_until)) + ".")
        return "\n".join(lines)

    def save(self) -> None:
        if not self.cfg.dry_run:
            self.state.save(self.cfg.state_file)

    # -- the loop --------------------------------------------------------------

    def run(self, sleep=time.sleep, passes: int | None = None) -> None:
        """Pass after pass, for ever (or `passes` times, for tests). Setting up
        is retried too: after a reboot the network may not be up yet."""
        log.info("XPP Studio maintainer agent started for %s", self.cfg.slug)
        ready = False
        failures = 0
        while passes is None or passes > 0:
            if passes is not None:
                passes -= 1
            try:
                if not ready:
                    self.setup()
                    ready = True
                did = self.tick()
                failures = 0
            except Exception:  # a bad hour on GitHub must not end the agent
                failures += 1
                log.exception("the loop failed (%d in a row)", failures)
                did = False
            pause = 5 if did else self.cfg.poll_seconds * min(failures + 1, 10)
            sleep(pause)

    def status(self) -> str:
        lines = [f"repository {self.cfg.slug}, home {self.cfg.home}",
                 f"Claude runs in the window: {self.budget.used()}/{self.cfg.runs_per_window}"]
        if self.state.paused_until > self.clock():
            lines.append("paused until " + time.ctime(self.state.paused_until))
        if self.cfg.stop_file.exists():
            lines.append(f"STOP file present: {self.cfg.stop_file}")
        for t in sorted(self.state.tasks.values(), key=lambda t: t.issue):
            pr = f" PR #{t.pr}" if t.pr else ""
            note = f"  [{t.note}]" if t.note else ""
            lines.append(f"#{t.issue:<5} {t.stage:<8}{pr}  {t.title}{note}")
        return "\n".join(lines)
