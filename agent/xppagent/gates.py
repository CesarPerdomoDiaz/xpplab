"""The checks a change must pass in its worktree before it is pushed: the
same build, tests and golden comparison a person would run (CLAUDE.md)."""
from __future__ import annotations

import os
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path


@dataclass
class GateResult:
    ok: bool
    command: str = ""
    output: str = ""          # the end of the failing command's output
    summary: str = ""         # one line per command, for the pull request


def token_lines(cwd: Path, base: str, pushed: list[str]) -> tuple[str, str]:
    """Added lines that hold a token, shown redacted: those in the change as
    it stands, and those only in its history (committed, then removed),
    counting only commits not on `pushed`. Nothing of the kind may be
    pushed, whatever a run was asked to do: a run with a shell can read the
    agent's own secrets."""
    from .gh import redact

    def git(*args):
        return subprocess.run(["git", *args], cwd=str(cwd), capture_output=True, text=True,
                              encoding="utf-8", errors="replace").stdout

    def added(diff):
        found, name = [], ""
        for line in diff.splitlines():
            if line.startswith("+++ "):
                name = line[6:] if line.startswith("+++ b/") else line[4:]
            elif line.startswith("+") and redact(line) != line:
                found.append(f"{name}: {redact(line[1:]).strip()[:200]}")
        return found
    fork = git("merge-base", f"origin/{base}", "HEAD").strip() or f"origin/{base}"
    tree = added(git("diff", "--unified=0", "--no-color", fork))   # with uncommitted work
    for name in git("ls-files", "--others", "--exclude-standard").splitlines():
        try:
            text = (Path(cwd) / name).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        tree += [f"{name}: {redact(line).strip()[:200]}" for line in text.splitlines()
                 if redact(line) != line]
    history = added(git("log", "-p", "--unified=0", "--no-color", "--format=", "HEAD",
                        "--not", *pushed))
    return "\n".join(dict.fromkeys(tree)), "\n".join(dict.fromkeys(history))


def run_gates(commands: list[str], cwd: Path, shell: list[str], path_prefix: list[str],
              timeout_minutes: int = 60, tail: int = 150) -> GateResult:
    env = {k: v for k, v in os.environ.items()
           if k not in ("GH_TOKEN", "GITHUB_TOKEN", "CLAUDE_CODE_OAUTH_TOKEN")}
    if path_prefix:
        env["PATH"] = os.pathsep.join(path_prefix + [env.get("PATH", "")])
    lines = []
    for cmd in commands:
        start = time.time()
        try:
            r = subprocess.run([*shell, cmd], cwd=str(cwd), env=env, capture_output=True,
                               text=True, encoding="utf-8", errors="replace",
                               timeout=timeout_minutes * 60)
            code, out = r.returncode, (r.stdout or "") + (r.stderr or "")
        except subprocess.TimeoutExpired as e:
            code = -1
            out = f"timed out after {timeout_minutes} minutes\n{e.stdout or ''}"
        took = time.time() - start
        mark = "passed" if code == 0 else f"FAILED (exit {code})"
        lines.append(f"- `{cmd}`: {mark} in {took:.0f} s")
        if code != 0:
            last = "\n".join(out.splitlines()[-tail:])
            return GateResult(False, cmd, last, "\n".join(lines))
    return GateResult(True, "", "", "\n".join(lines))
