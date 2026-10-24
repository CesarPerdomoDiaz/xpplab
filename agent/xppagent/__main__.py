"""python -m xppagent run|once|status|doctor --config config.toml"""
from __future__ import annotations

import argparse
import json
import logging
import logging.handlers
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from . import __version__
from .config import load
from .supervisor import Supervisor, load_secrets


def _logging(cfg, verbose: bool) -> None:
    try:  # a Windows console may not take emoji; the log file always does
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass
    handlers: list[logging.Handler] = [logging.StreamHandler(sys.stdout)]
    try:
        cfg.home.mkdir(parents=True, exist_ok=True)
        handlers.append(logging.handlers.RotatingFileHandler(
            cfg.log_file, maxBytes=5_000_000, backupCount=5, encoding="utf-8"))
    except OSError:
        pass
    logging.basicConfig(level=logging.DEBUG if verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s", handlers=handlers)


def single_instance(path: Path):
    """Lock `path` for as long as the process lives. Returns the open file
    (keep it), or None when another agent already holds the lock: the
    scheduled task starts the agent every few minutes, and only the first
    one may run."""
    path.parent.mkdir(parents=True, exist_ok=True)
    f = open(path, "a+")
    try:
        if sys.platform == "win32":
            import msvcrt
            f.seek(0)
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        f.close()
        return None
    return f


def _run(args: list[str], **kw) -> tuple[int, str]:
    try:
        r = subprocess.run(args, capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=kw.pop("timeout", 120), **kw)
        return r.returncode, (r.stdout or "") + (r.stderr or "")
    except (OSError, subprocess.TimeoutExpired) as e:
        return -1, str(e)


def doctor(cfg, live: bool) -> int:
    """Check everything the agent needs, without changing anything."""
    bad = 0

    def report(ok: bool, what: str, detail: str = "") -> None:
        nonlocal bad
        bad += not ok
        print(f"  [{'ok' if ok else 'FAIL'}] {what}" + (f": {detail}" if detail else ""))

    print(f"XPP Studio maintainer agent {__version__}")
    report(sys.version_info >= (3, 11), "Python 3.11 or newer", sys.version.split()[0])
    try:
        cfg.home.mkdir(parents=True, exist_ok=True)
        tempfile.TemporaryFile(dir=cfg.home).close()
        report(True, "agent home is writable", str(cfg.home))
    except OSError as e:
        report(False, "agent home is writable", str(e))
    keys = [k for k in ("GH_TOKEN", "CLAUDE_CODE_OAUTH_TOKEN") if os.environ.get(k)]
    print(f"  [info] secrets file {cfg.secrets_file}: "
          + ("present" if cfg.secrets_file.exists() else "none")
          + f"; tokens in the environment: {', '.join(keys) or 'none'}")
    code, out = _run(["git", "--version"])
    report(code == 0, "git", out.strip())
    code, out = _run(["gh", "api", "user", "--jq", ".login"])
    report(code == 0, "GitHub token", out.strip()[:200])
    code, out = _run(["gh", "repo", "view", cfg.slug, "--json", "visibility,viewerPermission"])
    detail = out.strip()[:200]
    if code == 0:
        info = json.loads(out)
        detail = f"{info['visibility'].lower()}, permission {info['viewerPermission']}"
        report(info["viewerPermission"] in ("WRITE", "MAINTAIN", "ADMIN"),
               f"access to {cfg.slug}", detail)
    else:
        report(False, f"access to {cfg.slug}", detail)
    claude = shutil.which(cfg.claude)
    report(bool(claude), "Claude Code CLI", claude or f"{cfg.claude!r} not found")
    if claude:
        env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CODE_")
               or k == "CLAUDE_CODE_OAUTH_TOKEN"}
        env.pop("CLAUDECODE", None)
        code, out = _run([claude, "--version"], env=env)
        report(code == 0, "Claude Code version", out.strip()[:80])
        if os.environ.get("CLAUDE_CODE_OAUTH_TOKEN"):
            report(True, "Claude sign-in", "CLAUDE_CODE_OAUTH_TOKEN is set")
        else:
            code, out = _run([claude, "auth", "status"], env=env)
            try:
                signed = json.loads(out).get("loggedIn", False)
            except (json.JSONDecodeError, AttributeError):
                signed = False
            report(signed, "Claude sign-in", "signed in" if signed else
                   "not signed in: run `claude setup-token` (see agent/README.md)")
        if live:
            code, out = _run([claude, "-p", "--output-format", "json", "--max-turns", "1",
                              "--strict-mcp-config"], input="Reply with the word OK.",
                             env=env, timeout=300)
            try:
                data = json.loads(out.strip().splitlines()[-1])
                ok = not data.get("is_error") and "OK" in str(data.get("result"))
                report(ok, "a live Claude run", str(data.get("result"))[:120])
            except (json.JSONDecodeError, IndexError):
                report(False, "a live Claude run", out.strip()[-300:])
    shell = shutil.which(cfg.shell[0]) or (cfg.shell[0] if Path(cfg.shell[0]).exists() else "")
    report(bool(shell), "shell for the checks", shell or cfg.shell[0])
    if shell:
        env = dict(os.environ)
        if cfg.path_prefix:
            env["PATH"] = os.pathsep.join(cfg.path_prefix + [env.get("PATH", "")])
        for tool in ("cmake --version", "ninja --version", "gcc --version",
                     "python -c 'import sys, pytest, numpy; print(sys.version.split()[0])'"):
            code, out = _run([*cfg.shell, tool], env=env)
            report(code == 0, f"check tool: {tool.split()[0]}",
                   (out.strip().splitlines() or [""])[0][:80])
    report(bool(cfg.gates), "checks configured", f"{len(cfg.gates)} commands")
    print("all good" if not bad else f"{bad} problem(s)")
    return 1 if bad else 0


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="xppagent", description=__doc__)
    p.add_argument("command", choices=("run", "once", "status", "doctor"))
    p.add_argument("--config", help="the TOML settings file")
    p.add_argument("--dry-run", action="store_true",
                   help="read GitHub and work locally, but write nothing to GitHub "
                        "and keep no state")
    p.add_argument("--live", action="store_true", help="doctor: also make one Claude run")
    p.add_argument("-v", "--verbose", action="store_true")
    a = p.parse_args(argv)
    cfg = load(a.config)
    cfg.dry_run = cfg.dry_run or a.dry_run
    load_secrets(cfg.secrets_file)
    if a.command == "doctor":
        return doctor(cfg, a.live)
    _logging(cfg, a.verbose)
    sup = Supervisor(cfg)
    if a.command == "status":
        print(sup.status())
        return 0
    if a.command == "once":
        sup.setup()
        sup.tick()
        print(sup.status())
        if sup.waiting:
            print("waiting:", sup.waiting)
        return 0
    lock = single_instance(cfg.home / "agent.lock")
    if lock is None:
        print("another agent is already running; nothing to do")
        return 0
    sup.run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
