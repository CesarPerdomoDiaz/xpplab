"""The agent's own clone of the repository and one worktree per task."""
from __future__ import annotations

import re
import subprocess
from pathlib import Path


class GitError(RuntimeError):
    pass


def git(*args: str, cwd: Path, check: bool = True) -> str:
    r = subprocess.run(["git", *args], cwd=str(cwd), capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if check and r.returncode != 0:
        raise GitError(f"git {' '.join(args[:3])}: {r.stderr.strip()[:500]}")
    return r.stdout


def slug(text: str, n: int = 40) -> str:
    s = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return s[:n].rstrip("-") or "task"


class Repo:
    def __init__(self, url: str, clone: Path, work: Path, base: str,
                 dry_run: bool = False):
        self.url = url
        self.clone = clone
        self.work = work
        self.base = base
        self.dry_run = dry_run  # everything local happens; nothing is pushed

    def ensure(self) -> None:
        if not (self.clone / ".git").exists():
            self.clone.parent.mkdir(parents=True, exist_ok=True)
            git("clone", self.url, str(self.clone), cwd=self.clone.parent)
        git("fetch", "--prune", "origin", cwd=self.clone)

    def worktree(self, branch: str) -> Path:
        """A worktree for the branch: new from origin/base, or the existing one."""
        path = self.work / branch.replace("/", "-")
        if path.exists():
            return path
        self.work.mkdir(parents=True, exist_ok=True)
        self.ensure()
        exists = git("ls-remote", "--heads", "origin", branch, cwd=self.clone).strip()
        if exists:
            git("worktree", "add", "-B", branch, str(path), f"origin/{branch}",
                cwd=self.clone)
        else:
            git("worktree", "add", "-B", branch, str(path), f"origin/{self.base}",
                cwd=self.clone)
        return path

    def fresh(self, name: str) -> Path:
        """A worktree of the newest origin/base, made anew each time."""
        self.remove(name)
        self.ensure()
        path = self.work / name.replace("/", "-")
        git("worktree", "add", "--detach", str(path), f"origin/{self.base}", cwd=self.clone)
        return path

    def remove(self, branch: str) -> None:
        path = self.work / branch.replace("/", "-")
        if path.exists():
            git("worktree", "remove", "--force", str(path), cwd=self.clone, check=False)
        git("worktree", "prune", cwd=self.clone, check=False)

    @staticmethod
    def dirty(path: Path) -> bool:
        return bool(git("status", "--porcelain", cwd=path).strip())

    @staticmethod
    def commit_all(path: Path, message: str) -> None:
        """Commit whatever the agent left uncommitted (its worktree is its own)."""
        git("add", "-A", cwd=path)
        git("commit", "-m", message, cwd=path)

    def ahead(self, path: Path) -> int:
        """Commits on the branch that are not on origin/base."""
        git("fetch", "origin", self.base, cwd=path, check=False)
        out = git("rev-list", "--count", f"origin/{self.base}..HEAD", cwd=path)
        return int(out.strip() or 0)

    def diff(self, path: Path, limit: int = 60000) -> str:
        out = git("diff", f"origin/{self.base}...HEAD", cwd=path)
        return out if len(out) <= limit else out[:limit] + "\n... (diff truncated)"

    def stat(self, path: Path) -> str:
        return git("diff", "--stat", f"origin/{self.base}...HEAD", cwd=path)

    @staticmethod
    def head(path: Path) -> str:
        return git("rev-parse", "HEAD", cwd=path).strip()

    def push(self, path: Path, branch: str) -> None:
        if self.dry_run:
            return
        git("push", "-u", "origin", f"HEAD:refs/heads/{branch}", "--force-with-lease",
            cwd=path)

    def merge_base(self, path: Path) -> str:
        """Bring the branch up to date with base; returns '' or the conflict list."""
        git("fetch", "origin", self.base, cwd=path, check=False)
        r = subprocess.run(["git", "merge", "--no-edit", f"origin/{self.base}"],
                           cwd=str(path), capture_output=True, text=True)
        if r.returncode == 0:
            return ""
        files = git("diff", "--name-only", "--diff-filter=U", cwd=path).strip()
        return files or r.stdout[-2000:] + r.stderr[-2000:]
