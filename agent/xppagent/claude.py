"""Running Claude Code unattended (claude -p) and reading its answer."""
from __future__ import annotations

import collections
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path

# Variables of a parent Claude Code session (when the agent is tested from
# inside one); a child must not inherit them.
_PARENT_VARS = ("CLAUDECODE", "CLAUDE_CODE_ENTRYPOINT", "CLAUDE_CODE_SSE_PORT",
                "CLAUDE_CODE_SESSION_ID", "CLAUDE_CODE_HOST_SESSION_ID",
                "CLAUDE_CODE_CHILD_SESSION", "CLAUDE_CODE_MESSAGING_SOCKET",
                "CLAUDE_CODE_MESSAGING_TOKEN", "CLAUDE_CODE_EXECPATH",
                "CLAUDE_CODE_SESSION_ATTENDED", "CLAUDE_PID",
                "CLAUDE_CODE_SDK_HAS_HOST_AUTH_REFRESH")

LIMIT_WORDS = re.compile(r"usage limit|limit reached|rate.?limit|out of (usage|credits)"
                         r"|quota|overloaded", re.I)
# The CLI is signed out or its token expired: nothing works until a person
# signs it in again.
AUTH_WORDS = re.compile(r"not logged in|/login|invalid api key|oauth token"
                        r"|authentication_error|unauthori[sz]ed", re.I)


@dataclass
class Result:
    ok: bool
    text: str = ""                 # the final message
    session: str = ""
    limited: bool = False          # the subscription's usage limit was hit
    signed_out: bool = False       # the CLI needs signing in again
    retry_at: float = 0.0          # when to try again, if known
    timed_out: bool = False
    raw: dict = field(default_factory=dict)

    def json_block(self) -> dict | None:
        """The last ```json ... ``` block of the final message, parsed."""
        blocks = re.findall(r"```json\s*(\{.*?\})\s*```", self.text, re.S)
        for b in reversed(blocks):
            try:
                return json.loads(b)
            except json.JSONDecodeError:
                continue
        return None


# The supervisor, not the model, talks to GitHub: Claude's runs (and the
# gates, which run code the model wrote) never see the GitHub token.
SECRET_VARS = ("GH_TOKEN", "GITHUB_TOKEN", "GH_ENTERPRISE_TOKEN")


def _env() -> dict:
    return {k: v for k, v in os.environ.items()
            if k not in _PARENT_VARS and k not in SECRET_VARS}


def _feed(proc: subprocess.Popen, prompt: str) -> None:
    """Write the prompt to the run's stdin, from a thread of its own, so that
    a long prompt cannot block reading its output."""
    try:
        proc.stdin.write(prompt)
        proc.stdin.close()
    except (BrokenPipeError, OSError, ValueError):
        pass


def _kill_tree(proc: subprocess.Popen) -> None:
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)],
                       capture_output=True)
    else:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            proc.kill()


def _retry_time(text: str, now: float | None = None) -> float:
    """When to try again after a limit message: '...|1761300000',
    'resets 3pm', 'resets at 15:30', 'resets in 2h'; else in 30 minutes
    (10 for an overloaded service)."""
    now = time.time() if now is None else now
    m = re.search(r"\|(\d{10})\b", text)
    if m:
        return float(m.group(1))
    m = re.search(r"resets?\s+(?:at\s+)?(\d{1,2})(?::(\d{2}))?\s*(am|pm)?\b", text, re.I)
    if m:
        hour, minute = int(m.group(1)), int(m.group(2) or 0)
        if m.group(3):
            hour = hour % 12 + (12 if m.group(3).lower() == "pm" else 0)
        t = time.localtime(now)
        at = time.mktime((t.tm_year, t.tm_mon, t.tm_mday, hour, minute, 0, 0, 0, -1))
        if at <= now:  # tomorrow (by the calendar: a day is not always 24 hours)
            at = time.mktime((t.tm_year, t.tm_mon, t.tm_mday + 1, hour, minute, 0, 0, 0, -1))
        return at + 120
    m = re.search(r"(\d+)\s*(?:hours?|hrs?|h)\b", text, re.I)
    if m:
        return now + 3600 * int(m.group(1))
    if re.search(r"overloaded", text, re.I):
        return now + 600
    return now + 1800


class Claude:
    """claude -p in a working directory, with a session that can be resumed."""

    KEEP_RUNS = 400               # run logs kept in log_dir (two files each)

    def __init__(self, command: str | list, *, model: str = "", minutes: float = 60,
                 max_turns: int = 250, system: Path | None = None,
                 log_dir: Path | None = None):
        # A full path: on Windows, Popen does not search PATH for claude.exe
        # the way a shell does, and an npm claude.cmd would mangle arguments.
        # (A list is a command with arguments of its own, for tests.)
        self.command = ([shutil.which(command) or command] if isinstance(command, str)
                        else list(command))
        self.model = model
        self.minutes = minutes
        self.max_turns = max_turns
        self.system = system          # a file appended to Claude Code's system prompt
        self.log_dir = log_dir

    def run(self, prompt: str, cwd: Path, *, session: str = "", resume: bool = False,
            read_only: bool = False, model: str = "", tag: str = "run") -> Result:
        # The prompt goes in on stdin: a diff can be longer than Windows
        # allows on a command line. Events stream out one JSON object a line
        # (stream-json), into runs/<run>.jsonl as they come, which is what
        # the dashboard shows live; the last one is the result.
        args = [*self.command, "-p", "--output-format", "stream-json", "--verbose",
                "--max-turns", str(self.max_turns), "--strict-mcp-config"]
        if read_only:
            # Planning and reviewing: read the code and the history, change
            # nothing. (Plan mode would wait for an approval nobody gives.)
            args += ["--allowedTools", "Read", "Glob", "Grep", "Bash(git log:*)",
                     "Bash(git diff:*)", "Bash(git show:*)", "Bash(git blame:*)",
                     "--disallowedTools", "Edit", "Write", "NotebookEdit"]
        else:
            # The agent runs as its own Windows user in its own worktree; the
            # supervisor, not the model, pushes and talks to GitHub.
            args += ["--dangerously-skip-permissions"]
        # The agent commits under its own GitHub account; Claude Code's own
        # co-author line would add a third name to its work. (Both setting
        # names: includeCoAuthoredBy, and attribution in newer versions.)
        args += ["--settings", json.dumps({"includeCoAuthoredBy": False,
                                           "attribution": {"commit": "", "pr": ""}})]
        if model or self.model:
            args += ["--model", model or self.model]
        if self.system:
            args += ["--append-system-prompt-file", str(self.system)]
        if resume and session:
            args += ["--resume", session]
        elif session:
            args += ["--session-id", session]
        kwargs: dict = dict(cwd=str(cwd), env=_env(), stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, encoding="utf-8",
                            errors="replace")
        if sys.platform != "win32":
            kwargs["start_new_session"] = True
        start = time.time()
        name = time.strftime("%Y%m%d-%H%M%S") + f"-{tag}-{uuid.uuid4().hex[:6]}"
        live = None
        if self.log_dir:
            self.log_dir.mkdir(parents=True, exist_ok=True)
            live = open(self.log_dir / f"{name}.jsonl", "w", encoding="utf-8")
            self._current({"tag": tag, "started": start, "file": f"{name}.jsonl",
                           "cwd": str(cwd), "read_only": read_only})
        proc = subprocess.Popen(args, **kwargs)
        timed_out: list = []
        errors: list = []
        threading.Thread(target=_feed, args=(proc, prompt), daemon=True).start()
        reader = threading.Thread(target=lambda: errors.append(proc.stderr.read()),
                                  daemon=True)
        reader.start()
        timer = threading.Timer(self.minutes * 60,
                                lambda: (timed_out.append(True), _kill_tree(proc)))
        timer.daemon = True
        timer.start()
        last = collections.deque(maxlen=20)   # the result is the last line
        try:
            for line in proc.stdout:
                last.append(line)
                if live:
                    live.write(line)
                    live.flush()
        finally:
            timer.cancel()
            proc.wait()
            reader.join(10)
            if live:
                live.close()
                self._current(None)
        out, err = "".join(last), "".join(errors)
        self._log(name, args, out, err, start)
        if timed_out:
            return Result(False, f"stopped after {self.minutes:g} minutes", session,
                          timed_out=True)
        return self._parse(out, err, session)

    def _current(self, info: dict | None) -> None:
        """runs/current.json names the run in progress, for the dashboard."""
        path = self.log_dir / "current.json"
        if info is None:
            path.unlink(missing_ok=True)
            return
        tmp = path.with_suffix(".tmp")
        tmp.write_text(json.dumps(info), encoding="utf-8")
        os.replace(tmp, path)

    def _parse(self, out: str, err: str, session: str) -> Result:
        try:
            data = json.loads(out.strip().splitlines()[-1]) if out.strip() else {}
        except (json.JSONDecodeError, IndexError):
            data = {}
        text = str(data.get("result") or err or out).strip()
        sid = data.get("session_id") or session
        ok = bool(data) and not data.get("is_error") and data.get("subtype") == "success"
        signed_out = not ok and bool(AUTH_WORDS.search(text))
        limited = not ok and not signed_out and bool(LIMIT_WORDS.search(text))
        return Result(ok, text, sid, limited, signed_out,
                      _retry_time(text) if limited else 0.0, raw=data)

    def _log(self, name, args, out, err, start) -> None:
        """runs/<run>.json: how the run was made and how it ended (the events
        themselves are in runs/<run>.jsonl). Old runs are pruned."""
        if not self.log_dir:
            return
        self.log_dir.mkdir(parents=True, exist_ok=True)
        shown = [a if len(a) < 400 else a[:400] + "..." for a in args]
        (self.log_dir / f"{name}.json").write_text(json.dumps({
            "args": shown, "started": start, "seconds": round(time.time() - start, 1),
            "events": f"{name}.jsonl", "stdout": out[-200000:], "stderr": err[-20000:]},
            indent=1), encoding="utf-8")
        logs = sorted(self.log_dir.glob("2*.json"))
        for old in logs[:-self.KEEP_RUNS]:
            old.unlink(missing_ok=True)
            old.with_suffix(".jsonl").unlink(missing_ok=True)
