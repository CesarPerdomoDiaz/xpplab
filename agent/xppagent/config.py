"""The agent's settings, read from a TOML file (see agent/config.example.toml).

Secrets never live here: the GitHub token and the Claude token come from the
environment (GH_TOKEN, CLAUDE_CODE_OAUTH_TOKEN), which the service loads from
a secrets file outside the repository.
"""
from __future__ import annotations

import os
import tomllib
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Job:
    """A recurring maintenance task: an issue opened every `every_hours`."""
    name: str
    every_hours: float
    title: str
    body: str


@dataclass
class Config:
    # the repository
    slug: str = "CesarPerdomoDiaz/xppstudio"
    owner: str = "CesarPerdomoDiaz"   # the only person the agent takes orders from
    base: str = "main"
    # the agent's home: its own clone, worktrees, state, logs
    home: Path = Path("C:/xpps-agent")
    # Claude Code
    claude: str = "claude"
    model: str = ""                   # empty: the CLI's default
    review_model: str = ""            # a different model for the second opinion
    run_minutes: int = 60             # a run that takes longer is stopped
    max_turns: int = 250
    # pacing (a Max subscription is shared with its owner)
    window_hours: float = 5.0
    runs_per_window: int = 8
    quiet_hours: list[str] = field(default_factory=list)  # ["09:00-18:00"]
    poll_seconds: int = 60
    # how many rounds of fixes before asking for help
    gate_rounds: int = 3
    review_rounds: int = 2
    ci_rounds: int = 3
    ci_timeout_minutes: int = 90
    # checks that must pass in the worktree before anything is pushed
    shell: list[str] = field(default_factory=lambda: ["bash", "-c"])
    path_prefix: list[str] = field(default_factory=list)
    gates: list[str] = field(default_factory=list)
    # labels
    label: str = "agent"
    label_blocked: str = "agent:blocked"
    label_working: str = "agent:working"
    label_priority: str = "agent:priority"
    label_paused: str = "agent:paused"   # on the log issue: pause the agent
    max_open: int = 3                 # tasks in progress at once (PRs included)
    # the agent's commits
    git_name: str = "XPP Studio agent"
    git_email: str = "agent@xppstudio.invalid"
    # the daily log
    log_issue_title: str = "Maintainer agent log"
    log_hour: int = 7
    post_merge_watch_hours: float = 12.0
    jobs: list[Job] = field(default_factory=list)
    dry_run: bool = False

    @property
    def clone(self) -> Path:
        return self.home / "repo"

    @property
    def work(self) -> Path:
        return self.home / "work"

    @property
    def state_file(self) -> Path:
        return self.home / "state.json"

    @property
    def runs_dir(self) -> Path:
        return self.home / "runs"

    @property
    def stop_file(self) -> Path:
        return self.home / "STOP"

    @property
    def secrets_file(self) -> Path:
        return self.home / "secrets.env"

    @property
    def log_file(self) -> Path:
        return self.home / "agent.log"


def load(path: str | os.PathLike | None) -> Config:
    """Read the TOML file; anything left out keeps its default."""
    cfg = Config()
    if path is None:
        return cfg
    data = tomllib.loads(Path(path).read_text(encoding="utf-8"))
    flat: dict = {}
    for section, values in data.items():
        if section == "jobs":
            continue
        if isinstance(values, dict):
            flat.update(values)
        else:
            flat[section] = values
    for key, value in flat.items():
        if not hasattr(cfg, key):
            raise ValueError(f"unknown setting {key!r} in {path}")
        if key == "home":
            value = Path(value)
        setattr(cfg, key, value)
    cfg.jobs = [Job(**j) for j in data.get("jobs", [])]
    return cfg
