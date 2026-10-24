"""GitHub through the gh command-line tool.

The agent may post with its owner's token, so everything it writes carries
MARK: that is how its own comments are told apart from the owner's.
"""
from __future__ import annotations

import json
import logging
import subprocess
from dataclasses import dataclass

MARK = "<!-- xpps-agent -->"
log = logging.getLogger("xppagent.gh")


class GitHubError(RuntimeError):
    pass


@dataclass
class Comment:
    id: str
    author: str
    body: str
    created: str

    @property
    def mine(self) -> bool:
        return MARK in self.body


class GitHub:
    def __init__(self, slug: str, *, dry_run: bool = False):
        self.slug = slug
        self.dry_run = dry_run
        self._fake = 9000  # numbers handed out in dry runs

    # -- plumbing --------------------------------------------------------------

    def _gh(self, *args: str, write: bool = False, check: bool = True) -> str:
        if write and self.dry_run:
            log.info("dry run: gh %s", " ".join(a if len(a) < 80 else a[:77] + "..."
                                                for a in args))
            self._fake += 1
            return str(self._fake)
        r = subprocess.run(["gh", *args], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        if check and r.returncode != 0:
            raise GitHubError(f"gh {' '.join(args[:3])}: {r.stderr.strip()[:500]}")
        return r.stdout

    def _json(self, *args: str):
        out = self._gh(*args)
        return json.loads(out) if out.strip() else None

    def _api(self, path: str):
        return self._json("api", f"repos/{self.slug}/{path}", "--paginate")

    # -- issues ----------------------------------------------------------------

    def issues(self, label: str) -> list[dict]:
        return self._json("issue", "list", "--repo", self.slug, "--label", label,
                          "--state", "open", "--limit", "100", "--json",
                          "number,title,body,labels,author,updatedAt") or []

    def issue(self, number: int) -> dict:
        return self._json("issue", "view", str(number), "--repo", self.slug, "--json",
                          "number,title,body,labels,state,author")

    def labeled_by(self, number: int, label: str) -> str:
        """Who last added `label` to the issue (so strangers cannot task the agent)."""
        who = ""
        for ev in self._api(f"issues/{number}/events") or []:
            if ev.get("event") == "labeled" and ev.get("label", {}).get("name") == label:
                who = (ev.get("actor") or {}).get("login", "")
        return who

    def comments(self, number: int) -> list[Comment]:
        """Issue (or pull request conversation) comments, oldest first."""
        out = []
        for c in self._api(f"issues/{number}/comments") or []:
            out.append(Comment(str(c["id"]), c["user"]["login"], c.get("body") or "",
                               c["created_at"]))
        return out

    def review_comments(self, pr: int) -> list[Comment]:
        """Reviews and line comments on a pull request."""
        out = []
        for r in self._api(f"pulls/{pr}/reviews") or []:
            if r.get("body"):
                out.append(Comment("r" + str(r["id"]), r["user"]["login"],
                                   f"(review: {r.get('state', '').lower()}) {r['body']}",
                                   r.get("submitted_at") or ""))
        for c in self._api(f"pulls/{pr}/comments") or []:
            where = f"{c.get('path')}:{c.get('line') or c.get('original_line')}"
            out.append(Comment("c" + str(c["id"]), c["user"]["login"],
                               f"(on {where}) {c.get('body') or ''}", c["created_at"]))
        return out

    def comment(self, number: int, body: str) -> None:
        self._gh("issue", "comment", str(number), "--repo", self.slug, "--body",
                 f"{body}\n\n{MARK}", write=True)

    def labels(self, number: int, add: list[str] = (), remove: list[str] = ()) -> None:
        args = ["issue", "edit", str(number), "--repo", self.slug]
        for a in add:
            args += ["--add-label", a]
        for r in remove:
            args += ["--remove-label", r]
        if add or remove:
            self._gh(*args, write=True, check=False)

    def ensure_labels(self, names: dict[str, str]) -> None:
        existing = {l["name"] for l in self._json("label", "list", "--repo", self.slug,
                                                    "--json", "name", "--limit", "200") or []}
        for name, colour in names.items():
            if name not in existing:
                self._gh("label", "create", name, "--repo", self.slug, "--color", colour,
                         write=True, check=False)

    def close_issue(self, number: int) -> None:
        self._gh("issue", "close", str(number), "--repo", self.slug, write=True,
                 check=False)

    def login(self) -> str:
        """Whose token this is."""
        return (self._json("api", "user") or {}).get("login", "")

    def find_open_issue(self, title: str) -> int:
        for i in self._json("issue", "list", "--repo", self.slug, "--state", "open",
                            "--search", f'"{title}" in:title', "--json", "number,title",
                            "--limit", "50") or []:
            if i["title"] == title:
                return i["number"]
        return 0

    def create_issue(self, title: str, body: str, labels: list[str] = (),
                     milestone: str = "") -> int:
        args = ["issue", "create", "--repo", self.slug, "--title", title,
                "--body", f"{body}\n\n{MARK}"]
        for l in labels:
            args += ["--label", l]
        if milestone:
            args += ["--milestone", milestone]
        out = self._gh(*args, write=True)
        return int(out.strip().rstrip("/").split("/")[-1])

    # -- what the proposal job reads ---------------------------------------------

    def open_issues(self) -> list[dict]:
        return self._json("issue", "list", "--repo", self.slug, "--state", "open",
                          "--limit", "200", "--json", "number,title,labels,milestone") or []

    def closed_issues(self, label: str, limit: int = 30) -> list[dict]:
        return self._json("issue", "list", "--repo", self.slug, "--state", "closed",
                          "--label", label, "--limit", str(limit), "--json",
                          "number,title,labels,comments") or []

    def merged_prs(self, limit: int = 15) -> list[dict]:
        return self._json("pr", "list", "--repo", self.slug, "--state", "merged",
                          "--limit", str(limit), "--json", "number,title,mergedAt") or []

    def milestones(self) -> list[str]:
        return [m["title"] for m in self._api("milestones?state=open&per_page=100") or []]

    # -- pull requests ---------------------------------------------------------

    def pr_for_branch(self, branch: str) -> int:
        prs = self._json("pr", "list", "--repo", self.slug, "--head", branch,
                         "--state", "all", "--json", "number") or []
        return prs[0]["number"] if prs else 0

    def create_pr(self, branch: str, base: str, title: str, body: str) -> int:
        out = self._gh("pr", "create", "--repo", self.slug, "--head", branch,
                       "--base", base, "--draft", "--title", title,
                       "--body", f"{body}\n\n{MARK}", write=True)
        return int(out.strip().rstrip("/").split("/")[-1])

    def pr(self, number: int) -> dict:
        return self._json("pr", "view", str(number), "--repo", self.slug, "--json",
                          "number,state,isDraft,mergedAt,mergeCommit,mergeable,headRefOid,"
                          "headRefName")

    def pr_ready(self, number: int) -> None:
        self._gh("pr", "ready", str(number), "--repo", self.slug, write=True, check=False)

    def checks(self, number: int) -> list[dict]:
        """CI checks of a pull request: name, bucket (pass, fail, pending,
        skipping, cancel) and link."""
        out = self._gh("pr", "checks", str(number), "--repo", self.slug, "--json",
                       "name,bucket,link,workflow", check=False)
        try:
            return json.loads(out) if out.strip() else []
        except json.JSONDecodeError:
            return []

    def failed_log(self, branch: str, sha: str, lines: int = 200) -> str:
        """The failing jobs' log lines for the newest failed run of a commit."""
        runs = self._json("run", "list", "--repo", self.slug, "--branch", branch,
                          "--json", "databaseId,headSha,conclusion,name", "--limit", "20") or []
        failed = [r for r in runs if r["headSha"] == sha and r["conclusion"] == "failure"]
        if not failed:
            return ""
        out = self._gh("run", "view", str(failed[0]["databaseId"]), "--repo", self.slug,
                       "--log-failed", check=False)
        tail = out.splitlines()[-lines:]
        return f"Workflow: {failed[0]['name']}\n" + "\n".join(tail)

    def runs_for_commit(self, sha: str, branch: str) -> list[dict]:
        runs = self._json("run", "list", "--repo", self.slug, "--branch", branch,
                          "--json", "databaseId,headSha,conclusion,status,name",
                          "--limit", "30") or []
        return [r for r in runs if r["headSha"] == sha]
