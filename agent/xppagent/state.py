"""What the agent is doing, kept in one JSON file so that it survives
restarts: the tasks, the Claude runs used in the current window, the
schedule, and the day's events for the log."""
from __future__ import annotations

import json
import os
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path

# A task moves through these stages; see pipeline.py.
STAGES = ("queued", "planned", "implemented", "pr", "reviewed", "ci",
          "owner", "merged", "blocked", "failed", "closed")


@dataclass
class Task:
    issue: int
    title: str
    stage: str = "queued"
    branch: str = ""
    session: str = ""            # the implementer's Claude session, resumed for fixes
    pr: int = 0
    rounds: dict = field(default_factory=dict)   # gate/review/ci round counts
    pending: list = field(default_factory=list)  # feedback waiting for the agent
    seen_comments: list = field(default_factory=list)  # comment ids already handled
    question: str = ""
    plan: str = ""
    before_block: str = ""       # the stage to return to when unblocked
    failed_at: str = ""          # the stage it failed in, to carry on from
    waiting_since: float = 0.0   # when the wait for CI began
    merged_at: float = 0.0
    merge_sha: str = ""
    created: float = field(default_factory=time.time)
    updated: float = field(default_factory=time.time)
    note: str = ""
    attempt: int = 1             # labelled again after a closed PR: a new branch

    def bump(self, key: str) -> int:
        self.rounds[key] = self.rounds.get(key, 0) + 1
        return self.rounds[key]


@dataclass
class State:
    tasks: dict = field(default_factory=dict)      # str(issue) -> Task
    runs: list = field(default_factory=list)       # start times of Claude runs
    paused_until: float = 0.0                      # usage limit reached
    jobs_last: dict = field(default_factory=dict)  # job name -> time last opened
    log_day: str = ""
    log_time: float = 0.0                          # when the last log was posted
    alerted: float = 0.0                           # last alert to the owner
    ignored: dict = field(default_factory=dict)    # issue -> updatedAt, not the owner's
    events: list = field(default_factory=list)     # (time, text) for the log

    def task(self, issue: int) -> Task | None:
        return self.tasks.get(str(issue))

    def add(self, task: Task) -> Task:
        self.tasks[str(task.issue)] = task
        return task

    def event(self, text: str) -> None:
        self.events.append([time.time(), text])
        del self.events[:-500]

    # -- persistence ---------------------------------------------------------

    @classmethod
    def load(cls, path: Path) -> "State":
        if not path.exists():
            return cls()
        raw = json.loads(path.read_text(encoding="utf-8"))
        st = cls(**{k: v for k, v in raw.items() if k != "tasks"})
        st.tasks = {k: Task(**v) for k, v in raw.get("tasks", {}).items()}
        return st

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_suffix(".tmp")
        data = asdict(self)
        tmp.write_text(json.dumps(data, indent=1), encoding="utf-8")
        os.replace(tmp, path)  # never a half-written state file
