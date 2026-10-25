"""The dashboard's server: what the agent is doing (from its files) and what
waits for the owner (from GitHub), on one page, with the owner's actions.

It listens on 127.0.0.1 only; on the workstation `tailscale serve` puts it on
the tailnet over HTTPS, and a password guards it on top. It reads the
agent's home but never its secrets, and acts on GitHub with the owner's own
token, kept in a file the agent's account cannot read.
"""
from __future__ import annotations

import base64
import hashlib
import hmac
import json
import logging
import mimetypes
import re
import secrets
import threading
import time
from http import cookies
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

from ..config import Config
from .github import GitHubAPI, GitHubAPIError

log = logging.getLogger("xppagent.web")
STATIC = Path(__file__).resolve().parent / "static"
MARK = "<!-- xpps-agent -->"
SESSION_DAYS = 14
RUN_NAME = re.compile(r"^(\d{8}-\d{6})-(.+)-([0-9a-f]{6})$")
SAFE_FILE = re.compile(r"^[\w.-]+\.jsonl?$")


# -- passwords -------------------------------------------------------------------

def hash_password(password: str, iterations: int = 310_000) -> str:
    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, iterations)
    return "pbkdf2_sha256${}${}${}".format(iterations, base64.b64encode(salt).decode(),
                                           base64.b64encode(digest).decode())


def check_password(password: str, stored: str) -> bool:
    try:
        scheme, iterations, salt, digest = stored.split("$")
        if scheme != "pbkdf2_sha256":
            return False
        mine = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"),
                                   base64.b64decode(salt), int(iterations))
        return hmac.compare_digest(mine, base64.b64decode(digest))
    except (ValueError, TypeError):
        return False


def read_secrets(path: Path | None) -> dict:
    out: dict = {}
    if path and path.exists():
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, value = line.split("=", 1)
                out[key.strip()] = value.strip().strip('"')
    return out


# -- what a Claude run did, in a few words a line ---------------------------------

def _short(text, n=240) -> str:
    text = " ".join(str(text or "").split())
    return text if len(text) <= n else text[: n - 1] + "…"


def _relative(path: str, cwd: str) -> str:
    p = str(path).replace("\\", "/")
    base = str(cwd or "").replace("\\", "/").rstrip("/")
    if base and p.lower().startswith(base.lower() + "/"):
        return p[len(base) + 1:]
    return p


def describe_tool(name: str, args: dict, cwd: str = "") -> str:
    args = args if isinstance(args, dict) else {}
    if name in ("Read", "Write", "Edit", "MultiEdit", "NotebookEdit"):
        return _relative(args.get("file_path") or args.get("notebook_path") or "?", cwd)
    if name == "Bash":
        return _short(args.get("description") or args.get("command"), 200)
    if name == "Grep":
        where = _relative(args.get("path") or "", cwd)
        return _short(f"{args.get('pattern', '')}" + (f"  in {where}" if where else ""), 200)
    if name == "Glob":
        return _short(args.get("pattern"), 200)
    if name in ("TodoWrite", "update_plan"):
        return "updated its to-do list"
    if name in ("Task", "Agent"):
        return _short(args.get("description") or args.get("prompt"), 200)
    if name in ("WebFetch", "WebSearch"):
        return _short(args.get("url") or args.get("query"), 200)
    return _short(", ".join(f"{k}={v}" for k, v in list(args.items())[:3]), 200)


def summarize(event: dict, cwd: str = "") -> list[dict]:
    kind = event.get("type")
    out: list[dict] = []
    if kind == "system" and event.get("subtype") == "init":
        out.append({"kind": "start", "text": "Claude Code started"})
    elif kind == "assistant":
        for item in (event.get("message") or {}).get("content") or []:
            if item.get("type") == "text" and str(item.get("text", "")).strip():
                out.append({"kind": "say", "text": _short(item["text"], 700)})
            elif item.get("type") == "tool_use":
                name = item.get("name", "?")
                out.append({"kind": "tool", "tool": name,
                            "text": describe_tool(name, item.get("input") or {}, cwd)})
    elif kind == "user":
        for item in (event.get("message") or {}).get("content") or []:
            if isinstance(item, dict) and item.get("type") == "tool_result" and item.get("is_error"):
                content = item.get("content")
                if isinstance(content, list):
                    content = " ".join(c.get("text", "") for c in content if isinstance(c, dict))
                out.append({"kind": "error", "text": _short(content, 300)})
    elif kind == "result":
        out.append({"kind": "end", "ok": not event.get("is_error"),
                    "turns": event.get("num_turns"),
                    "seconds": round((event.get("duration_ms") or 0) / 1000),
                    "text": _short(event.get("result"), 700)})
    return out


def read_events(path: Path, after: int = 0, cwd: str = "") -> tuple[list[dict], int]:
    """The summarized events of a run's .jsonl from line `after` on, and the
    number of lines read so far (the next `after`)."""
    events, n = [], 0
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            for n, line in enumerate(f, 1):
                if n <= after or not line.strip():
                    continue
                try:
                    events.extend(summarize(json.loads(line), cwd))
                except json.JSONDecodeError:
                    n -= 1          # a line still being written: read it next time
                    break
    except FileNotFoundError:
        pass
    return events, max(n, after)


def run_info(name: str) -> dict:
    """'20261024-204137-6-plan-393892' -> its time, task and kind."""
    m = RUN_NAME.match(name)
    if not m:
        return {"name": name}
    stamp, tag, _ = m.groups()
    issue, _, kind = tag.partition("-")
    try:
        when = time.mktime(time.strptime(stamp, "%Y%m%d-%H%M%S"))
    except ValueError:
        when = 0
    return {"name": name, "started": when, "issue": int(issue) if issue.isdigit() else None,
            "kind": kind or tag}


# -- the roadmap, from ROADMAP.md -------------------------------------------------------

def parse_roadmap(text: str) -> list[dict]:
    milestones, current, last = [], None, None
    for line in (text or "").splitlines():
        head = re.match(r"^###\s+(M\d+)\s+(.*)$", line)
        if head:
            current = {"id": head.group(1), "title": head.group(2).strip(), "items": []}
            milestones.append(current)
            last = None
            continue
        if line.startswith(("## ", "### ")):   # a heading without an M number ends one
            current = None
            continue
        if current is None:
            continue
        boxes = re.findall(r"\[( |x|X)\]\s*([^\[]+)", line)
        if boxes and line.lstrip().startswith(("- [", "[")):
            for done, item in boxes:
                last = {"done": done.lower() == "x", "text": item.strip().rstrip(";").strip()}
                current["items"].append(last)
        elif last is not None and line.startswith("  ") and line.strip():
            last["text"] += " " + line.strip()
        elif not line.strip():
            last = None
    for m in milestones:
        m["done"] = sum(i["done"] for i in m["items"])
        m["total"] = len(m["items"])
    return milestones


def parse_path(text: str) -> list[str]:
    """The main path's milestones, in order, from the roadmap's "Path:" line."""
    for line in (text or "").splitlines():
        if line.startswith("Path:"):
            return re.findall(r"M\d+", line)
    return []


def parse_focus(text: str) -> str | None:
    """The milestone in progress: the one in brackets on the "Path:" line."""
    for line in (text or "").splitlines():
        if line.startswith("Path:"):
            m = re.search(r"\[\s*(M\d+)\s*\]", line)
            return m.group(1) if m else None
    return None


def _key(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", " ", (text or "").lower()).strip()


def diff_roadmap(before: list[dict], after: list[dict]) -> list[dict]:
    """What a change to the roadmap did to its items: "done" for an item
    ticked (or added already ticked, as a pull request that ships something
    new does), "planned" for an open item added."""
    was = {_key(i["text"]): i["done"] for m in before for i in m["items"]}
    out = []
    for m in after:
        for n, i in enumerate(m["items"]):
            k = _key(i["text"])
            if i["done"] and not was.get(k, False):
                out.append({"milestone": m["id"], "item": n, "kind": "done"})
            elif not i["done"] and k not in was:
                out.append({"milestone": m["id"], "item": n, "kind": "planned"})
    return out


def evolution(roadmap_text: str, history: list[dict], merged: list[dict],
              roadmap_at: dict[str, list[dict]]) -> dict:
    """The app's path for the dashboard's graph: the milestones in path order
    (the rest as side tracks), the one in progress and what is left behind
    it, and what each change to the roadmap advanced. A merged pull request
    that ticked an item on the path moved it forward; one that ticked only a
    side track's items, or none, is upkeep or a detour."""
    milestones = parse_roadmap(roadmap_text)
    ids = [m["id"] for m in milestones]
    path = [i for i in parse_path(roadmap_text) if i in ids]
    by_id = {m["id"]: m for m in milestones}
    unfinished = [i for i in path if by_id[i]["done"] < by_id[i]["total"]]
    current = parse_focus(roadmap_text)
    if current not in path:
        current = unfinished[0] if unfinished else None
    behind = [i for i in unfinished if path.index(i) < path.index(current)] if current else []
    nxt = None
    if current and current in unfinished:
        n = next(k for k, it in enumerate(by_id[current]["items"]) if not it["done"])
        nxt = {"milestone": current, "item": n}
    # Each step: one commit that changed the roadmap, newest first. Items are
    # matched to today's roadmap by their words, so a step whose item has
    # since been reworded keeps only its milestone.
    today = {(m["id"], _key(i["text"])): k for m in milestones for k, i in enumerate(m["items"])}
    steps = []
    for c in history:
        parent = ((c.get("parents") or {}).get("nodes") or [{}])[0].get("oid")
        now, before = roadmap_at.get(c.get("oid")), roadmap_at.get(parent) if parent else []
        if now is None or before is None:
            continue
        pr = ((c.get("associatedPullRequests") or {}).get("nodes") or [None])[0] or {}
        step = {"oid": c["oid"], "date": c.get("committedDate"), "pr": pr.get("number"),
                "title": pr.get("title") or c.get("messageHeadline", ""),
                "url": pr.get("url") or c.get("url"), "created": not before, "done": [], "planned": []}
        if before:
            then = {m["id"]: m for m in now}
            for d in diff_roadmap(before, now):
                text = then[d["milestone"]]["items"][d["item"]]["text"]
                k = today.get((d["milestone"], _key(text)))
                step[d["kind"]].append({"milestone": d["milestone"], "item": k})
        steps.append(step)
    on_path = {s["oid"] for s in steps if any(d["milestone"] in path for d in s["done"])}
    side = {s["oid"] for s in steps if s["done"]} - on_path
    planned = {s["oid"] for s in steps if s["planned"] and not s["done"]}
    drift = []
    for p in merged:
        oid = (p.get("mergeCommit") or {}).get("oid")
        drift.append({"number": p.get("number"), "title": p.get("title"), "url": p.get("url"),
                      "merged": p.get("mergedAt"),
                      "kind": "path" if oid in on_path else "side" if oid in side
                      else "planned" if oid in planned else "other"})
    drift.sort(key=lambda d: d["merged"] or "")
    return {"milestones": milestones, "path": path,
            "side": [i for i in ids if i not in path], "current": current, "behind": behind,
            "next": nxt, "steps": steps, "drift": drift}


def pr_summary(body: str) -> str:
    """The agent's pull request description without its boilerplate."""
    text = (body or "").replace(MARK, "")
    text = text.split("**Checks run before pushing**")[0]
    text = re.sub(r"^\s*Closes #\d+\.?\s*", "", text)
    return text.strip()


# -- the dashboard's data and actions ------------------------------------------

class Dashboard:
    def __init__(self, cfg: Config, gh: GitHubAPI | None, *, read_only: bool = False,
                 refresh_seconds: float = 30, clock=time.time):
        self.cfg = cfg
        self.gh = gh
        self.read_only = read_only or gh is None
        self.refresh_seconds = refresh_seconds
        self.clock = clock
        self._lock = threading.Lock()
        self._github: dict = {}
        self._github_time = 0.0
        self._github_error = ""
        self._comments: dict = {}     # pr -> (updatedAt, comments)
        self._jobs: dict = {}         # commit sha -> its CI jobs
        self._asked: dict = {}        # number -> a question sent, until the agent has it
        self._roadmap_at: dict = {}   # commit -> the roadmap then (commits do not change)
        self._wake = threading.Event()

    # files of the agent's home
    def _json(self, path: Path, default=None):
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return default

    def agent_files(self) -> dict:
        home = self.cfg.home
        state = self._json(home / "state.json", {}) or {}
        status = self._json(home / "status.json", {}) or {}
        current = self._json(self.cfg.runs_dir / "current.json")
        explain = self._json(self.cfg.explain_file, {}) or {}
        explaining = self._json(self.cfg.runs_dir / "explaining.json")
        return {"state": state, "status": status, "current": current,
                "explain": explain.get("items") or [], "explaining": explaining,
                "stop": (home / "STOP").exists()}

    def explanations(self, items: list[dict]) -> dict:
        """The newest question about each issue or pull request: the agent's
        record of it, or the one just sent from here that it has not read yet."""
        out = {}
        for it in items:                       # oldest first: the newest wins
            out[it.get("issue")] = {k: it.get(k) for k in (
                "question", "asked", "status", "answer", "note", "answered", "url")}
        with self._lock:
            known = {it.get("comment") for it in items}
            for n, sent in list(self._asked.items()):
                if sent["comment"] in known or self.clock() - sent["time"] > 3600:
                    del self._asked[n]
                else:
                    out[n] = {"question": sent["question"], "asked": sent["time"],
                              "status": "sent"}
        return out

    def recent_runs(self, n: int = 25) -> list[dict]:
        out = []
        for path in sorted(self.cfg.runs_dir.glob("2*.json"))[-n:][::-1]:
            info = run_info(path.stem)
            data = self._json(path, {}) or {}
            info["seconds"] = data.get("seconds")
            last = (data.get("stdout") or "").strip().splitlines()[-1:] or [""]
            try:
                result = json.loads(last[0])
            except ValueError:
                result = {}
            info["ok"] = bool(result) and not result.get("is_error")
            info["turns"] = result.get("num_turns")
            info["text"] = _short(result.get("result"), 300)
            info["events"] = data.get("events")
            out.append(info)
        return out

    # GitHub, refreshed in the background
    def refresh_github(self) -> None:
        if not self.gh:
            return
        try:
            data = self.gh.overview()
            self._fetch_roadmaps(data)
            files = self.agent_files()
            tasks = (files["state"].get("tasks") or {}).values()
            waiting = {t.get("pr") for t in tasks if t.get("stage") in ("owner", "pr", "ci") and t.get("pr")}
            open_prs = data["repository"]["pullRequests"]["nodes"]
            shas = set()
            for pr in open_prs:
                sha = self._head(pr)
                if not sha:
                    continue
                shas.add(sha)
                known = self._jobs.get(sha)
                # Finished CI does not change: fetch only what may still move.
                if known is None or not known or any(j["status"] != "completed" for j in known):
                    self._jobs[sha] = self.gh.workflow_jobs(sha)
            self._jobs = {k: v for k, v in self._jobs.items() if k in shas}
            for pr in open_prs:
                if pr["number"] in waiting:
                    seen = self._comments.get(pr["number"])
                    if not seen or seen[0] != pr["updatedAt"]:
                        self._comments[pr["number"]] = (pr["updatedAt"], self.gh.comments(pr["number"]))
            with self._lock:
                self._github, self._github_time, self._github_error = data, self.clock(), ""
        except (GitHubAPIError, KeyError, TypeError) as e:
            with self._lock:
                self._github_error = str(e)
            log.warning("GitHub refresh failed: %s", e)

    @staticmethod
    def _history(data: dict) -> list[dict]:
        repo = (data or {}).get("repository") or {}
        target = (repo.get("roadmapHistory") or {}).get("target") or {}
        return (target.get("history") or {}).get("nodes") or []

    def _fetch_roadmaps(self, data: dict) -> None:
        """The roadmap at each commit that changed it, and just before: fetched
        once, since a commit's files never change."""
        want = []
        for c in self._history(data):
            parent = ((c.get("parents") or {}).get("nodes") or [{}])[0].get("oid")
            want += [o for o in (c.get("oid"), parent) if o and o not in self._roadmap_at]
        want = list(dict.fromkeys(want))
        try:
            for i in range(0, len(want), 30):
                for oid, text in self.gh.roadmap_texts(want[i:i + 30]).items():
                    self._roadmap_at[oid] = parse_roadmap(text)
        except (GitHubAPIError, KeyError, TypeError) as e:
            log.warning("could not read the roadmap's history: %s", e)

    def refresher(self) -> None:
        while True:
            self.refresh_github()
            self._wake.wait(self.refresh_seconds)
            self._wake.clear()

    def wake(self) -> None:
        self._wake.set()

    # the page
    def overview(self) -> dict:
        files = self.agent_files()
        state, status, current = files["state"], files["status"], files["current"]
        now = self.clock()
        with self._lock:
            gh, gh_time, gh_error = self._github, self._github_time, self._github_error
        repo = (gh or {}).get("repository") or {}
        issues = repo.get("issues", {}).get("nodes", [])
        prs = repo.get("pullRequests", {}).get("nodes", [])
        pr_by_number = {p["number"]: p for p in prs}
        log_issue = status.get("log_issue") or next(
            (i["number"] for i in issues if i["title"] == self.cfg.log_issue_title), 0)
        paused_label = any(i["number"] == log_issue and any(
            l["name"] == self.cfg.label_paused for l in i["labels"]["nodes"]) for i in issues)
        seen = status.get("time") or 0
        poll = status.get("poll_seconds") or self.cfg.poll_seconds
        alive = bool(current) or (now - seen) < max(3 * poll, 300)

        tasks = []
        for t in (state.get("tasks") or {}).values():
            tasks.append({k: t.get(k) for k in ("issue", "title", "stage", "pr", "branch",
                                                 "question", "updated", "created", "rounds",
                                                 "waiting_since", "merged_at")}
                         | {"plan": _short(t.get("plan"), 1500)})
        tasks.sort(key=lambda t: -(t.get("updated") or 0))
        explained = self.explanations(files["explain"])
        for t in tasks:
            t["explain"] = explained.get(t["issue"])
        bot = status.get("me") or ""

        inbox = []
        for t in tasks:
            if t["stage"] == "owner" and t.get("pr") in pr_by_number:
                p = pr_by_number[t["pr"]]
                inbox.append({"kind": "review", "issue": t["issue"], "pr": p["number"],
                              "title": p["title"], "url": p["url"], "mergeable": p["mergeable"],
                              "summary": pr_summary(p.get("body")),
                              "additions": p["additions"], "deletions": p["deletions"],
                              "files": p["changedFiles"], "checks": self._checks(p),
                              "review": self._agent_review(p["number"]),
                              "updated": p["updatedAt"], "explain": explained.get(p["number"])})
            elif t["stage"] == "blocked":
                inbox.append({"kind": "question", "issue": t["issue"], "title": t["title"],
                              "question": t.get("question") or "",
                              "url": f"https://github.com/{self.cfg.slug}/issues/{t['issue']}",
                              "explain": explained.get(t["issue"])})
        for i in issues:
            names = {l["name"] for l in i["labels"]["nodes"]}
            if "agent:proposal" in names and self.cfg.label not in names:
                inbox.append({"kind": "proposal", "issue": i["number"], "title": i["title"],
                              "url": i["url"], "body": _short(i.get("body", "").replace(MARK, ""), 2500),
                              "milestone": (i.get("milestone") or {}).get("title"),
                              "explain": explained.get(i["number"])})
            elif (not names & {self.cfg.label, self.cfg.label_proposal}
                  and (i.get("author") or {}).get("login") not in (self.cfg.owner, bot)
                  and i["number"] != log_issue):
                # someone else's issue: the bot takes it only once approved
                inbox.append({"kind": "issue", "issue": i["number"], "title": i["title"],
                              "url": i["url"], "author": (i.get("author") or {}).get("login"),
                              "body": _short(i.get("body", ""), 2500),
                              "created": i.get("createdAt"),
                              "explain": explained.get(i["number"])})

        return {
            "generated": now, "repo": self.cfg.slug, "owner": self.cfg.owner,
            "bot": status.get("me") or "", "viewer": (gh or {}).get("viewer", {}).get("login"),
            "read_only": self.read_only,
            "agent": {"alive": alive, "seen": seen, "doing": status.get("doing", ""),
                      "paused": paused_label or files["stop"], "stop_file": files["stop"],
                      "paused_until": status.get("paused_until") or state.get("paused_until") or 0,
                      "runs_used": status.get("runs_used"),
                      "runs_per_window": status.get("runs_per_window") or self.cfg.runs_per_window,
                      "window_hours": status.get("window_hours") or self.cfg.window_hours,
                      "usage": status.get("usage") or state.get("usage") or {},
                      "max_five_hour_percent": status.get("max_five_hour_percent")
                      or self.cfg.max_five_hour_percent,
                      "max_week_percent": status.get("max_week_percent")
                      or self.cfg.max_week_percent,
                      "current": current | run_info(Path(current["file"]).stem)
                      if current and current.get("file") else None,
                      "explaining": files["explaining"]
                      | run_info(Path(files["explaining"]["file"]).stem)
                      if (files["explaining"] or {}).get("file") else None},
            "tasks": tasks,
            "inbox": inbox,
            "issues": [{"number": i["number"], "title": i["title"], "url": i["url"],
                        "labels": [l["name"] for l in i["labels"]["nodes"]],
                        "author": (i.get("author") or {}).get("login"), "updated": i["updatedAt"]}
                       for i in issues],
            "prs": [{"number": p["number"], "title": p["title"], "url": p["url"],
                     "draft": p["isDraft"], "author": (p.get("author") or {}).get("login"),
                     "checks": self._checks(p), "mergeable": p["mergeable"],
                     "updated": p["updatedAt"]} for p in prs],
            "merged": repo.get("merged", {}).get("nodes", []),
            "events": (state.get("events") or [])[-40:][::-1],
            "runs": self.recent_runs(),
            "roadmap": parse_roadmap((repo.get("roadmap") or {}).get("text", "")),
            "evolution": evolution((repo.get("roadmap") or {}).get("text", ""),
                                   self._history(gh), repo.get("merged", {}).get("nodes", []),
                                   self._roadmap_at),
            "milestones": repo.get("milestones", {}).get("nodes", []),
            "log_issue": log_issue,
            "github_age": round(now - gh_time) if gh_time else None,
            "github_error": gh_error,
            "github_warnings": (gh or {}).get("warnings") or [],
        }

    @staticmethod
    def _head(pr: dict) -> str:
        nodes = (pr.get("commits") or {}).get("nodes") or [{}]
        return (nodes[0].get("commit") or {}).get("oid") or ""

    def _checks(self, pr: dict) -> dict:
        """CI of the pull request's head, from its Actions jobs. The overall
        state: failure, pending, success, or none."""
        items = [{"name": j["name"], "url": j.get("url"),
                  "result": (j.get("conclusion") or j.get("status") or "").lower()}
                 for j in self._jobs.get(self._head(pr), [])]
        results = {i["result"] for i in items}
        if results & {"failure", "error", "cancelled", "timed_out", "action_required"}:
            state = "failure"
        elif results - {"success", "skipped", "neutral"}:
            state = "pending"
        else:
            state = "success" if items else ""
        return {"state": state, "items": items}

    def _agent_review(self, pr: int) -> str:
        comments = (self._comments.get(pr) or (None, []))[1]
        for c in reversed(comments):
            body = c.get("body") or ""
            if body.startswith("### 🔍 Agent review"):
                return body.replace(MARK, "").replace("### 🔍 Agent review", "").strip()
        return ""

    def live(self, name: str, after: int) -> dict:
        if not SAFE_FILE.match(name or ""):
            raise ValueError("no such run")
        current = self.agent_files()["current"] or {}
        path = self.cfg.runs_dir / (name if name.endswith(".jsonl") else name + ".jsonl")
        events, upto = read_events(path, after, current.get("cwd", ""))
        return {"events": events, "after": upto,
                "running": bool(current) and current.get("file") == path.name}

    # the owner's actions
    def act(self, action: str, data: dict) -> dict:
        if self.read_only:
            raise PermissionError("this dashboard is read-only")
        gh = self.gh

        def number(key):
            value = int(data.get(key))
            if value <= 0:
                raise ValueError(f"bad {key}")
            return value

        def text(key, limit):
            value = str(data.get(key) or "").strip()
            if not value:
                raise ValueError(f"{key} is empty")
            if len(value) > limit:
                raise ValueError(f"{key} is too long")
            return value

        if action == "merge":
            pr = number("pr")
            out = gh.merge(pr)
            result = {"message": f"Merged #{pr}", "sha": (out or {}).get("sha")}
        elif action == "comment":
            n = number("number")
            gh.comment(n, text("body", 20000))
            result = {"message": f"Commented on #{n}"}
        elif action == "ask":
            labels = [self.cfg.label] + ([self.cfg.label_priority] if data.get("priority") else [])
            n = gh.create_issue(text("title", 200), str(data.get("body") or "").strip()[:20000],
                                labels)
            result = {"message": f"Opened #{n} for the bot", "number": n}
        elif action == "close":
            n = number("number")
            gh.close(n)
            result = {"message": f"Closed #{n}"}
        elif action == "approve":
            n = number("issue")
            gh.add_labels(n, [self.cfg.label])
            result = {"message": f"Approved #{n}: the bot will plan it"}
        elif action == "explain":
            n = number("number")
            question = str(data.get("question") or "").strip()
            if len(question) > 4000:
                raise ValueError("question is too long")
            out = gh.comment(n, "/explain" + (f" {question}" if question else "")) or {}
            with self._lock:
                self._asked[n] = {"question": question, "time": self.clock(),
                                  "comment": str(out.get("id", ""))}
            result = {"message": f"Asked the bot about #{n}: its answer appears here "
                                 "in a few minutes"}
        elif action == "decline":
            n = number("issue")
            if data.get("reason"):
                gh.comment(n, str(data["reason"])[:5000])
            gh.close(n)
            result = {"message": f"Declined #{n}"}
        elif action in ("pause", "resume"):
            n = self.overview()["log_issue"]
            if not n:
                raise ValueError("the agent's log issue was not found")
            if action == "pause":
                gh.add_labels(n, [self.cfg.label_paused])
            else:
                gh.remove_label(n, self.cfg.label_paused)
            result = {"message": "Paused: the bot finishes its step, then waits"
                      if action == "pause" else "Resumed"}
        else:
            raise ValueError(f"unknown action {action!r}")
        log.info("action %s %s: %s", action, {k: v for k, v in data.items() if k != "body"},
                 result["message"])
        self.wake()
        return result


# -- sessions -------------------------------------------------------------------

class Sessions:
    def __init__(self, password_hash: str | None, clock=time.time):
        self.password_hash = password_hash
        self.clock = clock
        self.sessions: dict = {}      # token -> (expiry, csrf)
        self.failures: list = []
        self._lock = threading.Lock()

    @property
    def open(self) -> bool:
        return self.password_hash is None     # no login (local development only)

    def login(self, password: str):
        now = self.clock()
        with self._lock:
            self.failures = [t for t in self.failures if now - t < 300]
            if len(self.failures) >= 5:
                return "throttled"
            if not self.password_hash or not check_password(password, self.password_hash):
                self.failures.append(now)
                return None
            token = secrets.token_urlsafe(32)
            self.sessions[token] = (now + SESSION_DAYS * 86400, secrets.token_urlsafe(24))
            return token

    def check(self, token: str | None):
        """The session's CSRF token, or None if the session is not valid."""
        if self.open:
            return "open"
        with self._lock:
            entry = self.sessions.get(token or "")
            if not entry or entry[0] < self.clock():
                self.sessions.pop(token or "", None)
                return None
            return entry[1]

    def logout(self, token: str | None) -> None:
        with self._lock:
            self.sessions.pop(token or "", None)


# -- HTTP -------------------------------------------------------------------------

SECURITY_HEADERS = {
    "Content-Security-Policy": "default-src 'self'; img-src 'self' data: https://github.com "
                               "https://avatars.githubusercontent.com; style-src 'self'; "
                               "script-src 'self'; connect-src 'self'; frame-ancestors 'none'; "
                               "base-uri 'none'; form-action 'self'",
    "X-Content-Type-Options": "nosniff",
    # Not no-referrer: with it, browsers send "Origin: null" on the page's own
    # form posts, which the same-site check below must refuse.
    "Referrer-Policy": "same-origin",
    "X-Frame-Options": "DENY",
    "Cache-Control": "no-store",
}


class Handler(BaseHTTPRequestHandler):
    server_version = "xpps-dashboard"
    dashboard: Dashboard
    sessions: Sessions

    def log_message(self, fmt, *args):            # quieter than the default
        log.debug("%s %s", self.address_string(), fmt % args)

    # helpers
    def _token(self):
        jar = cookies.SimpleCookie(self.headers.get("Cookie") or "")
        return jar["xpps"].value if "xpps" in jar else None

    def _send(self, code: int, body: bytes, ctype: str, extra: dict | None = None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (SECURITY_HEADERS | (extra or {})).items():
            self.send_header(k, v)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _json(self, code: int, data, extra=None):
        self._send(code, json.dumps(data).encode("utf-8"), "application/json; charset=utf-8", extra)

    def _static(self, name: str):
        path = (STATIC / name).resolve()
        if STATIC not in path.parents or not path.is_file():
            return self._json(404, {"error": "not found"})
        ctype = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        if ctype.startswith("text/") or ctype.endswith("javascript"):
            ctype += "; charset=utf-8"
        self._send(200, path.read_bytes(), ctype)

    def _secure(self) -> bool:
        return (self.headers.get("X-Forwarded-Proto") == "https"
                or (self.headers.get("Host") or "").split(":")[0].endswith(".ts.net"))

    def _body(self) -> dict:
        length = int(self.headers.get("Content-Length") or 0)
        if length > 100_000:
            raise ValueError("request too large")
        raw = self.rfile.read(length) if length else b""
        ctype = self.headers.get("Content-Type") or ""
        if ctype.startswith("application/json"):
            return json.loads(raw or b"{}")
        return {k: v[0] for k, v in parse_qs(raw.decode("utf-8")).items()}

    def _same_origin(self) -> bool:
        origin = self.headers.get("Origin")
        if not origin:
            return True
        return urlparse(origin).netloc == (self.headers.get("Host") or "")

    # routes
    def do_GET(self):
        url = urlparse(self.path)
        csrf = self.sessions.check(self._token())
        if url.path in ("/login", "/login.html"):
            return self._static("login.html")
        if url.path.startswith("/static/"):
            return self._static(url.path[len("/static/"):])
        if csrf is None:
            if url.path.startswith("/api/"):
                return self._json(401, {"error": "log in first"})
            return self._send(303, b"", "text/plain", {"Location": "/login"})
        if url.path in ("/", "/index.html"):
            return self._static("index.html")
        try:
            if url.path == "/api/overview":
                return self._json(200, self.dashboard.overview() | {"csrf": csrf})
            if url.path == "/api/live":
                q = parse_qs(url.query)
                return self._json(200, self.dashboard.live(q.get("file", [""])[0],
                                                           int(q.get("after", ["0"])[0])))
        except ValueError as e:
            return self._json(400, {"error": str(e)})
        return self._json(404, {"error": "not found"})

    do_HEAD = do_GET

    def do_POST(self):
        url = urlparse(self.path)
        if not self._same_origin():
            return self._json(403, {"error": "cross-site request refused"})
        if url.path == "/login":
            data = self._body()
            token = self.sessions.login(str(data.get("password") or ""))
            if token == "throttled":
                return self._send(303, b"", "text/plain", {"Location": "/login?wait=1"})
            if not token:
                return self._send(303, b"", "text/plain", {"Location": "/login?wrong=1"})
            cookie = (f"xpps={token}; HttpOnly; SameSite=Strict; Path=/; "
                      f"Max-Age={SESSION_DAYS * 86400}" + ("; Secure" if self._secure() else ""))
            return self._send(303, b"", "text/plain", {"Location": "/", "Set-Cookie": cookie})
        if url.path == "/logout":
            self.sessions.logout(self._token())
            return self._send(303, b"", "text/plain",
                              {"Location": "/login", "Set-Cookie": "xpps=; Path=/; Max-Age=0"})
        csrf = self.sessions.check(self._token())
        if csrf is None:
            return self._json(401, {"error": "log in first"})
        if url.path == "/api/action":
            if not self.sessions.open and not hmac.compare_digest(
                    self.headers.get("X-CSRF") or "", csrf):
                return self._json(403, {"error": "missing or wrong CSRF token"})
            try:
                data = self._body()
                return self._json(200, self.dashboard.act(str(data.get("action")), data))
            except PermissionError as e:
                return self._json(403, {"error": str(e)})
            except (ValueError, TypeError) as e:
                return self._json(400, {"error": str(e)})
            except GitHubAPIError as e:
                return self._json(502, {"error": str(e)})
        return self._json(404, {"error": "not found"})


def serve(dashboard: Dashboard, sessions: Sessions, host: str = "127.0.0.1",
          port: int = 8765, background: bool = True) -> ThreadingHTTPServer:
    handler = type("BoundHandler", (Handler,), {"dashboard": dashboard, "sessions": sessions})
    httpd = ThreadingHTTPServer((host, port), handler)
    httpd.daemon_threads = True
    if background:
        threading.Thread(target=dashboard.refresher, daemon=True).start()
    return httpd
