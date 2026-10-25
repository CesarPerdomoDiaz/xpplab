"""Stands in for `claude -p --output-format stream-json` in the tests: reads
the prompt, prints a few events one JSON object a line, then the result.
FAKE_SLEEP makes it hang first; FAKE_RUNS names the folder whose
current.json it reports on."""
import json
import os
import sys
import time
from pathlib import Path

prompt = sys.stdin.read()
runs = os.environ.get("FAKE_RUNS")
current = "yes" if runs and (Path(runs) / "current.json").exists() else "no"


def say(event):
    print(json.dumps(event), flush=True)


say({"type": "system", "subtype": "init", "session_id": "s-fake", "cwd": os.getcwd()})
# As Claude Code reports the subscription's use (the real event has more fields).
say({"type": "rate_limit_event", "rate_limit_info": {
    "status": "allowed", "rateLimitType": "five_hour", "unifiedWindows": {
        "five_hour": {"utilization": 0.35, "resetsAt": 1792951200, "status": "allowed"},
        "seven_day": {"utilization": 0.55, "resetsAt": 1793034000, "status": "allowed"}}}})
time.sleep(float(os.environ.get("FAKE_SLEEP", "0")))
say({"type": "assistant", "message": {"content": [
    {"type": "text", "text": f"Reading the code ({len(prompt)} characters of prompt)."},
    {"type": "tool_use", "id": "t1", "name": "Read", "input": {"file_path": "app/src/main.c"}}]}})
say({"type": "user", "message": {"content": [
    {"type": "tool_result", "tool_use_id": "t1", "content": "int main(void) { return 0; }"}]}})
say({"type": "result", "subtype": "success", "is_error": False, "session_id": "s-fake",
     "num_turns": 2, "result": f"Done. current: {current}"})
