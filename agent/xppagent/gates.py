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
