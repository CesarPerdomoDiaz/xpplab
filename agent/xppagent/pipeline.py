"""One issue's way from label to merged pull request.

    queued -> planned -> pr -> ci -> owner -> merged -> closed
                    \\-> blocked (asks the owner; resumes on their reply)

Each call to Pipeline.step() moves a task on by at most one stage and
returns whether anything happened. Every Claude run goes through the
budget first, which may raise Wait; the task then stays where it is.
"""
from __future__ import annotations

import copy
import logging
import string
import time
import uuid
from pathlib import Path

from .gates import run_gates
from .gh import MARK, Comment, is_question
from .git import Repo, slug
from .state import State, Task

log = logging.getLogger("xppagent")
PROMPTS = Path(__file__).resolve().parent.parent / "prompts"


class Wait(Exception):
    """No Claude run is allowed now (budget, quiet hours, usage limit).
    `alert` asks the supervisor to tell the owner: only a person can fix it."""

    def __init__(self, until: float, why: str, alert: bool = False):
        super().__init__(why)
        self.until = until
        self.why = why
        self.alert = alert


def prompt(name: str, **values) -> str:
    text = (PROMPTS / f"{name}.md").read_text(encoding="utf-8")
    return string.Template(text).safe_substitute(**values)


def system_prompt() -> str:
    return (PROMPTS / "system.md").read_text(encoding="utf-8")


def _quote(c: Comment) -> str:
    return f"**{c.author}** wrote:\n\n{c.body.strip()}\n"


class Pipeline:
    def __init__(self, cfg, gh, repo: Repo, claude, state: State, budget):
        self.cfg = cfg
        self.gh = gh
        self.repo = repo
        self.claude = claude
        self.state = state
        self.budget = budget

    # -- helpers ---------------------------------------------------------------

    def _ask(self, text: str, cwd: Path, *, task: Task | None = None,
             new_session: bool = False, read_only: bool = False, model: str = "",
             tag: str = "run"):
        self.budget.take()
        if task is not None and new_session:
            task.session = str(uuid.uuid4())
        res = self.claude.run(text, cwd,
                              session=task.session if task else "",
                              resume=bool(task and not new_session and task.session),
                              read_only=read_only, model=model, tag=tag)
        if res.signed_out:
            raise Wait(time.time() + 3600, "the Claude CLI is signed out; sign it in "
                       "again on the workstation (see agent/README.md)", alert=True)
        if res.limited:
            raise Wait(res.retry_at, "the Claude usage limit was reached")
        if task is not None and res.session and not read_only:
            task.session = res.session
        return res

    def _issue_text(self, task: Task, handled: bool = False) -> dict:
        """The issue for a prompt. `handled`: the implementer reads it, so its
        comments need not be passed on again later as new feedback."""
        issue = self.gh.issue(task.issue)
        # Only the owner's comments: anyone else's could steer the agent.
        # (His /explain questions are answered apart, and are not work.)
        comments = [c for c in self.gh.comments(task.issue)
                    if not c.mine and c.author == self.cfg.owner and not is_question(c.body)]
        if handled:
            task.seen_comments += [c.id for c in comments if c.id not in task.seen_comments]
        quoted = "".join("\n" + _quote(c) for c in comments)
        return {"number": task.issue, "title": issue.get("title", task.title),
                "body": issue.get("body") or "(no description)",
                "comments": ("\nComments on the issue:\n" + quoted) if quoted else ""}

    def _path(self, task: Task) -> Path:
        return self.repo.worktree(task.branch)

    def _block(self, task: Task, question: str) -> None:
        task.before_block = task.stage
        task.stage = "blocked"
        task.question = question
        self.gh.comment(task.issue, f"### 🤖 I need your help\n\n{question}\n\n"
                                    "_Reply here and I will continue._")
        self.gh.labels(task.issue, add=[self.cfg.label_blocked],
                       remove=[self.cfg.label_working])
        self.state.event(f"#{task.issue} blocked: {question[:120]}")

    def _commit_leftovers(self, task: Task, path: Path) -> None:
        if self.repo.dirty(path):
            self.repo.commit_all(path, f"Work on #{task.issue}: {task.title}")

    def _fix(self, task: Task, path: Path, what: str, details: str, tag: str):
        res = self._ask(prompt("fix", what=what, details=details), path, task=task,
                        tag=tag)
        self._commit_leftovers(task, path)
        return res

    def _gates(self, task: Task, path: Path) -> tuple[bool, str]:
        """Run the gates, and let the agent fix failures, a few rounds."""
        tries = 0
        while True:
            g = run_gates(self.cfg.gates, path, self.cfg.shell, self.cfg.path_prefix)
            if g.ok:
                return True, g.summary
            tries += 1
            if tries > self.cfg.gate_rounds:
                self._block(task, f"The checks still fail after "
                                  f"{self.cfg.gate_rounds} attempts: `{g.command}`."
                                  f"\n\n```\n{g.output[-3000:]}\n```")
                return False, g.summary
            self._fix(task, path, f"The supervisor's check `{g.command}` failed.",
                      f"```\n{g.output}\n```", tag=f"{task.issue}-gates")

    def _push(self, task: Task, path: Path) -> None:
        self.repo.push(path, task.branch)

    def _new_owner_comments(self, task: Task, numbers: list[int],
                            reviews: bool = False) -> list[Comment]:
        """New comments by the owner on these issues and pull requests, and
        with `reviews` the reviews of the task's pull request (an issue has
        none: GitHub answers 404)."""
        found = []
        for n in numbers:
            items = self.gh.comments(n)
            if reviews and n == task.pr:
                items += self.gh.review_comments(n)
            for c in items:
                if c.mine or c.id in task.seen_comments:
                    continue
                task.seen_comments.append(c.id)
                if c.author == self.cfg.owner and not is_question(c.body):
                    found.append(c)
        return found

    # -- the stages ------------------------------------------------------------

    def step(self, task: Task) -> bool:
        stage = task.stage
        handler = getattr(self, f"_{stage}", None)
        if handler is None:
            return False
        before = copy.deepcopy(task.__dict__)
        try:
            done = handler(task)
        except Wait:
            # Undo the bookkeeping (round counts, comments marked as seen) so
            # the step is repeated in full when runs are allowed again.
            task.__dict__.update(before)
            raise
        if done:
            task.updated = time.time()
        return done

    def _queued(self, task: Task) -> bool:
        """Plan on the issue, then start."""
        if not task.branch:
            again = f"-{task.attempt}" if task.attempt > 1 else ""
            task.branch = f"agent/{task.issue}-{slug(task.title)}{again}"
        path = self._path(task)
        info = self._issue_text(task)
        # Comments made before the agent started belong to the task itself.
        for c in self.gh.comments(task.issue):
            if c.id not in task.seen_comments:
                task.seen_comments.append(c.id)
        res = self._ask(prompt("plan", **info), path, read_only=True,
                        tag=f"{task.issue}-plan")
        if not res.ok:
            log.warning("plan for #%s failed: %s", task.issue, res.text[:300])
            if task.bump("plan") >= 3:
                self._block(task, "I could not make a plan (my runs failed). "
                                  "Reply here to have me try again.")
                return True
            return False
        task.plan = res.text.strip()
        self.gh.comment(task.issue, f"### 🤖 Plan\n\n{task.plan}\n\n"
                                    f"_I am starting on branch `{task.branch}`. "
                                    "Comment here to change course._")
        self.gh.labels(task.issue, add=[self.cfg.label_working])
        task.stage = "planned"
        self.state.event(f"#{task.issue} planned")
        return True

    def _planned(self, task: Task) -> bool:
        """Implement, pass the gates, open a draft pull request."""
        path = self._path(task)
        info = self._issue_text(task, handled=True)
        res = self._ask(prompt("implement", plan=task.plan, **info), path, task=task,
                        new_session=not task.session, tag=f"{task.issue}-implement")
        answer = res.json_block() or {}
        if answer.get("status") == "blocked":
            self._block(task, answer.get("question") or res.text[-1500:])
            return True
        if answer.get("status") == "no-change" and res.ok:
            return self._no_change(task, answer.get("summary") or res.text[-3000:])
        if not res.ok:
            if res.timed_out or task.bump("implement") > 2:
                self._block(task, "My run did not finish (it took too long or ran out "
                                  "of turns). Is the issue bigger than one change? "
                                  "Split it, or tell me where to start.")
                return True
            log.warning("implementing #%s failed: %s", task.issue, res.text[:300])
            return False
        self._commit_leftovers(task, path)
        if self.repo.ahead(path) == 0:
            self._block(task, "I finished without changing anything. "
                              + (answer.get("summary") or res.text[-800:]))
            return True
        ok, summary = self._gates(task, path)
        if not ok:
            return True
        self._push(task, path)
        title = answer.get("title") or task.title
        body = (f"Closes #{task.issue}.\n\n{answer.get('summary') or ''}\n\n"
                f"**Checks run before pushing**\n\n{summary}\n\n"
                f"```\n{self.repo.stat(path)}```\n\n"
                "_Draft opened by the XPP Studio maintainer agent. An agent review "
                "and CI come next; I will ask for your review when they pass._")
        task.pr = self.gh.pr_for_branch(task.branch) or \
            self.gh.create_pr(task.branch, self.cfg.base, title, body)
        self.gh.comment(task.issue, f"🤖 Draft pull request: #{task.pr}")
        task.stage = "pr"
        self.state.event(f"#{task.issue} draft PR #{task.pr}")
        return True

    def _no_change(self, task: Task, summary: str) -> bool:
        """Nothing to change: report on the issue and let it go. The agent's
        own (scheduled) issues are closed; the owner closes theirs."""
        self.gh.comment(task.issue, f"### 🤖 No change needed\n\n{summary}")
        self.gh.labels(task.issue, remove=[self.cfg.label, self.cfg.label_working])
        if MARK in (self.gh.issue(task.issue).get("body") or ""):
            self.gh.close_issue(task.issue)
        self.repo.remove(task.branch)
        task.stage = "closed"
        self.state.event(f"#{task.issue} needed no change")
        return True

    def _finished(self, task: Task, pr: dict | None = None) -> bool:
        """The pull request was merged or closed (by the owner, or by GitHub
        when its base was rewritten): move the task on and say so. Checked
        in every stage that has a pull request, not only the owner's."""
        pr = pr if pr is not None else self.gh.pr(task.pr)
        if pr.get("mergedAt"):
            task.stage = "merged"
            task.merged_at = time.time()
            task.merge_sha = (pr.get("mergeCommit") or {}).get("oid", "")
            self.gh.labels(task.issue, remove=[self.cfg.label, self.cfg.label_working])
            self.repo.remove(task.branch)
            self.state.event(f"#{task.issue} PR #{task.pr} merged")
            return True
        if pr.get("state") == "CLOSED":
            task.stage = "closed"
            self.gh.labels(task.issue, remove=[self.cfg.label, self.cfg.label_working])
            self.gh.comment(task.issue, f"🤖 #{task.pr} was closed without merging, so "
                                        f"I stopped. Add the `{self.cfg.label}` label "
                                        "again to have me start over.")
            self.repo.remove(task.branch)
            self.state.event(f"#{task.issue} PR #{task.pr} closed without merging")
            return True
        return False

    def _pr(self, task: Task) -> bool:
        """A second agent reviews; findings go back to the implementer."""
        if self._finished(task):
            return True
        path = self._path(task)
        info = self._issue_text(task)
        res = self._ask(prompt("review", base=self.cfg.base, diff=self.repo.diff(path),
                               **info), path, read_only=True,
                        model=self.cfg.review_model, tag=f"{task.issue}-review")
        verdict = res.json_block()
        if not res.ok or verdict is None:
            if task.bump("review-failed") <= 2:
                log.warning("review of #%s failed: %s", task.issue, res.text[:300])
                return False
            self.gh.comment(task.pr, "🔍 The agent review did not finish; "
                                     "going on to CI without it.")
            verdict = {}
        findings = [f for f in verdict.get("findings", [])
                    if str(f.get("severity", "")).lower() in ("high", "medium")]
        listed = "\n".join(f"- **{f.get('severity')}** `{f.get('file')}:{f.get('line')}`: "
                           f"{f.get('problem')} *Fix:* {f.get('fix')}"
                           for f in verdict.get("findings", [])) or "No problems found."
        self.gh.comment(task.pr, f"### 🔍 Agent review\n\n{listed}")
        if findings and task.bump("review") <= self.cfg.review_rounds:
            details = "\n".join(f"- [{f.get('severity')}] {f.get('file')}:{f.get('line')}: "
                                f"{f.get('problem')} Suggested fix: {f.get('fix')}"
                                for f in findings)
            self._fix(task, path, "A reviewer found these problems in your change:",
                      details, tag=f"{task.issue}-review-fix")
            ok, _ = self._gates(task, path)
            if not ok:
                return True
            self._push(task, path)
            return True  # stay in "pr": review again
        self._to_ci(task)
        return True

    def _to_ci(self, task: Task) -> None:
        task.stage = "ci"
        task.waiting_since = time.time()

    def _ci(self, task: Task) -> bool:
        """Wait for CI; feed the first failure back; ask for review when green."""
        if self._finished(task):
            return True
        checks = self.gh.checks(task.pr)
        buckets = {c.get("bucket") for c in checks}
        waited = (time.time() - task.waiting_since) / 60
        if not checks and waited < 15:
            return False  # CI has not registered yet (or skips docs-only changes)
        if "pending" in buckets:
            if waited < self.cfg.ci_timeout_minutes:
                return False
            self._block(task, f"CI has been running for over "
                              f"{self.cfg.ci_timeout_minutes} minutes. Is a runner stuck?")
            return True
        failed = [c for c in checks if c.get("bucket") in ("fail", "cancel")]
        path = self._path(task)
        if failed:
            if task.bump("ci") > self.cfg.ci_rounds:
                self._block(task, "CI still fails after "
                                  f"{self.cfg.ci_rounds} attempts: "
                                  + ", ".join(c["name"] for c in failed))
                return True
            sha = self.repo.head(path)
            logs = self.gh.failed_log(task.branch, sha) or "(no log found)"
            self._fix(task, path, "CI failed on GitHub for your pull request: "
                                  + ", ".join(c["name"] for c in failed),
                      f"```\n{logs}\n```", tag=f"{task.issue}-ci-fix")
            ok, _ = self._gates(task, path)
            if ok:
                self._push(task, path)
                task.waiting_since = time.time()
            return True
        self.gh.pr_ready(task.pr)
        self.gh.comment(task.pr, f"@{self.cfg.owner} this is ready for your review. "
                                 "The checks, an agent review and CI have passed. "
                                 "Comment here with any changes you want.")
        task.stage = "owner"
        self.state.event(f"#{task.issue} PR #{task.pr} ready for review")
        return True

    def _owner(self, task: Task) -> bool:
        """The owner's turn: merge, close, or comment (which the agent answers)."""
        pr = self.gh.pr(task.pr)
        if self._finished(task, pr):
            return True
        news = self._new_owner_comments(task, [task.pr, task.issue], reviews=True)
        conflicting = pr.get("mergeable") == "CONFLICTING"
        if not news and not conflicting:
            return False
        path = self._path(task)
        head = self.repo.head(path)
        # Bring in the base only when GitHub says the branch conflicts with
        # it: otherwise a merge commit would only clutter the pull request.
        conflict = self.repo.merge_base(path) if conflicting else ""
        if news or conflict:
            what = ("The maintainer commented on your pull request:" if news else
                    f"Your branch no longer merges cleanly into {self.cfg.base}.")
            details = "\n".join(_quote(c) for c in news)
            if conflict:
                details += (f"\n\nMerging origin/{self.cfg.base} into the branch left "
                            f"conflicts (the merge is in progress); resolve them and "
                            f"commit:\n{conflict}")
            res = self._fix(task, path, what, details, tag=f"{task.issue}-owner")
            answer = res.json_block() or {}
            if self.repo.head(path) == head and not self.repo.dirty(path):
                # Nothing changed (a question, or nothing to do): answer, and
                # go on waiting for the owner.
                reply = (answer.get("summary") or answer.get("question")
                         or res.text[-1500:]).strip()
                self.gh.comment(task.pr, f"🤖 {reply}")
                return True
        ok, _ = self._gates(task, path)
        if not ok:
            return True
        self._push(task, path)
        self.gh.comment(task.pr, "🤖 Updated after your comments; CI is running again."
                        if news else f"🤖 Merged `{self.cfg.base}` into the branch; "
                                     "CI is running again.")
        task.rounds["ci"] = 0
        self._to_ci(task)
        return True

    def _merged(self, task: Task) -> bool:
        """Watch the base branch after the merge; a failure becomes a priority task."""
        if time.time() - task.merged_at > self.cfg.post_merge_watch_hours * 3600:
            task.stage = "closed"
            return True
        if not task.merge_sha:
            return False
        runs = self.gh.runs_for_commit(task.merge_sha, self.cfg.base)
        if any(r.get("conclusion") == "failure" for r in runs):
            title = f"{self.cfg.base} fails after #{task.pr}"
            if not self.gh.find_open_issue(title):
                n = self.gh.create_issue(
                    title, f"CI failed on `{self.cfg.base}` after merging #{task.pr} "
                           f"(commit {task.merge_sha[:9]}). Fix it, or revert #{task.pr}.",
                    [self.cfg.label, self.cfg.label_priority])
                self.state.event(f"{self.cfg.base} is red after #{task.pr}: opened #{n}")
            task.stage = "closed"
            return True
        return False

    def _blocked(self, task: Task) -> bool:
        """Resume when the owner answers on the issue (or the pull request)."""
        numbers = [task.issue] + ([task.pr] if task.pr else [])
        news = self._new_owner_comments(task, numbers, reviews=bool(task.pr))
        if not news:
            return False
        self.gh.labels(task.issue, remove=[self.cfg.label_blocked],
                       add=[self.cfg.label_working])
        task.rounds = {}
        task.question = ""
        back = task.before_block or "planned"
        path = self._path(task)
        answer = "\n".join(_quote(c) for c in news)
        if back == "queued":
            task.stage = "queued"
            return True
        # Give the agent the answer, then carry on from where it stopped.
        self._fix(task, path, "The maintainer answered your question:", answer,
                  tag=f"{task.issue}-answer")
        if back == "planned" and not task.pr:
            ok, summary = self._gates(task, path)
            if not ok:
                return True
            if self.repo.ahead(path) == 0:
                self._block(task, "I still have no change to propose.")
                return True
            self._push(task, path)
            task.pr = self.gh.pr_for_branch(task.branch) or self.gh.create_pr(
                task.branch, self.cfg.base, task.title,
                f"Closes #{task.issue}.\n\n**Checks run before pushing**\n\n{summary}")
            task.stage = "pr"
            return True
        ok, _ = self._gates(task, path)
        if not ok:
            return True
        self._push(task, path)
        if task.pr:
            self._to_ci(task)
        else:
            task.stage = "planned"
        return True
