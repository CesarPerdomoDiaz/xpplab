"""A fake GitHub, a scripted Claude, and a real git repository with a local
bare 'origin', so the agent's whole loop runs offline in a few seconds."""
from __future__ import annotations

import itertools
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from xppagent.claude import Result  # noqa: E402
from xppagent.config import Config  # noqa: E402
from xppagent.gh import MARK, Comment  # noqa: E402
from xppagent.supervisor import Supervisor  # noqa: E402

OWNER = "owner"


def sh(*args, cwd):
    return subprocess.run(args, cwd=str(cwd), check=True, capture_output=True,
                          text=True).stdout


class FakeGitHub:
    def __init__(self, me=OWNER):
        self.me = me
        self.numbers = itertools.count(1)
        self.ids = itertools.count(100)
        self.issues_ = {}
        self.prs = {}
        self.comments_ = {}
        self.runs = []
        self.created_issues = []

    # test helpers
    def new_issue(self, title, body="", labels=("agent",), by=OWNER):
        n = next(self.numbers)
        self.issues_[n] = {"number": n, "title": title, "body": body, "state": "OPEN",
                           "labels": [{"name": l} for l in labels], "labeled_by": by,
                           "updatedAt": "t0", "author": {"login": by}}
        return n

    def say(self, n, body, who=OWNER):
        self.comments_.setdefault(n, []).append(
            Comment(str(next(self.ids)), who, body, "now"))

    def said(self, n):
        return [c.body for c in self.comments_.get(n, [])]

    def label_names(self, n):
        return {l["name"] for l in self.issues_[n]["labels"]}

    # the GitHub interface
    def login(self):
        return self.me

    def ensure_labels(self, names):
        pass

    def issues(self, label):
        return [dict(i) for i in self.issues_.values()
                if i["state"] == "OPEN" and label in self.label_names(i["number"])]

    def issue(self, n):
        return dict(self.issues_[n]) if n in self.issues_ else dict(self.prs[n])

    def labeled_by(self, n, label):
        return self.issues_[n]["labeled_by"]

    def comments(self, n):
        return list(self.comments_.get(n, []))

    def review_comments(self, n):
        return []

    def comment(self, n, body):
        self.comments_.setdefault(n, []).append(
            Comment(str(next(self.ids)), self.me, f"{body}\n\n{MARK}", "now"))

    def labels(self, n, add=(), remove=()):
        if n not in self.issues_:
            return
        names = (self.label_names(n) | set(add)) - set(remove)
        self.issues_[n]["labels"] = [{"name": l} for l in sorted(names)]

    def close_issue(self, n):
        self.issues_[n]["state"] = "CLOSED"

    def find_open_issue(self, title):
        for i in self.issues_.values():
            if i["title"] == title and i["state"] == "OPEN":
                return i["number"]
        return 0

    def create_issue(self, title, body, labels=()):
        n = self.new_issue(title, f"{body}\n\n{MARK}", labels, by=self.me)
        self.created_issues.append(n)
        return n

    def pr_for_branch(self, branch):
        for p in self.prs.values():
            if p["headRefName"] == branch:
                return p["number"]
        return 0

    def create_pr(self, branch, base, title, body):
        n = next(self.numbers)
        self.prs[n] = {"number": n, "title": title, "body": body, "headRefName": branch,
                       "state": "OPEN", "isDraft": True, "mergedAt": None,
                       "mergeCommit": None, "mergeable": "MERGEABLE",
                       "checks": [{"name": "build", "bucket": "pass"}]}
        return n

    def pr(self, n):
        return dict(self.prs[n])

    def pr_ready(self, n):
        self.prs[n]["isDraft"] = False

    def checks(self, n):
        return list(self.prs[n]["checks"])

    def failed_log(self, branch, sha):
        return "test_thing failed: expected 1, got 2"

    def runs_for_commit(self, sha, branch):
        return list(self.runs)


class FakeClaude:
    """Answers each run by its tag's last part (plan, implement, review,
    gates, ci-fix, owner, answer, ...) with a scripted function."""

    def __init__(self):
        self.calls = []
        self.script = {}

    def on(self, kind, fn):
        self.script[kind] = fn

    def run(self, prompt, cwd, *, session="", resume=False, read_only=False, model="",
            tag="run"):
        kind = tag.rsplit("-", 1)[-1]
        self.calls.append({"kind": kind, "prompt": prompt, "cwd": Path(cwd),
                           "session": session, "resume": resume, "read_only": read_only})
        fn = self.script.get(kind)
        if fn is None:
            return Result(True, "Nothing to do.\n```json\n{\"status\": \"done\"}\n```",
                          session or "s-new")
        res = fn(prompt, Path(cwd))
        if isinstance(res, str):
            res = Result(True, res, session or "s-new")
        return res

    def kinds(self):
        return [c["kind"] for c in self.calls]


def done(title="Add hello", summary="Added hello.txt."):
    return (f"Done.\n```json\n{{\"status\": \"done\", \"title\": \"{title}\", "
            f"\"summary\": \"{summary}\"}}\n```")


def approve():
    return "Looks fine.\n```json\n{\"verdict\": \"approve\", \"findings\": []}\n```"


def write(cwd, name, text):
    (Path(cwd) / name).write_text(text, encoding="utf-8")


@pytest.fixture
def world(tmp_path):
    """An origin with one commit on main, a config, fakes, and a supervisor."""
    origin = tmp_path / "origin.git"
    seed = tmp_path / "seed"
    sh("git", "init", "--bare", "-b", "main", str(origin), cwd=tmp_path)
    sh("git", "init", "-b", "main", str(seed), cwd=tmp_path)
    write(seed, "README.md", "hello\n")
    sh("git", "add", "README.md", cwd=seed)
    sh("git", "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-m", "start", cwd=seed)
    sh("git", "push", str(origin), "main", cwd=seed)

    cfg = Config(owner=OWNER, home=tmp_path / "home", runs_per_window=100, log_hour=24,
                 shell=[sys.executable, "-c"],
                 # the change must contain hello.txt saying "hello"
                 gates=["import pathlib, sys; p = pathlib.Path('hello.txt'); "
                        "sys.exit(0 if p.exists() and p.read_text().strip() == 'hello' "
                        "else print('hello.txt is wrong') or 1)"])
    gh = FakeGitHub()
    claude = FakeClaude()
    from xppagent.git import Repo
    repo = Repo(str(origin), cfg.clone, cfg.work, "main")
    sup = Supervisor(cfg, gh=gh, claude=claude, repo=repo)
    sup.setup()

    class World:
        pass
    w = World()
    w.cfg, w.gh, w.claude, w.repo, w.sup, w.origin, w.seed = cfg, gh, claude, repo, sup, origin, seed
    w.task = lambda n: sup.state.task(n)
    w.branches = lambda: sh("git", "branch", "--list", cwd=origin)
    return w
