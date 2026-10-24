"""The dashboard: its server, end to end over HTTP, against a fake GitHub and
an agent home written by hand."""
from __future__ import annotations

import http.client
import json
import threading
import time
from pathlib import Path

import pytest

from xppagent.config import Config
from xppagent.web.server import (Dashboard, Sessions, check_password, hash_password,
                                 parse_roadmap, read_events, serve, summarize)

ROOT = Path(__file__).resolve().parents[2]
RUN = "20261024-205221-6-review-d0c3be"


class FakeAPI:
    def __init__(self):
        self.calls = []

    def overview(self):
        return {
            "viewer": {"login": "owner"},
            "repository": {
                "issues": {"nodes": [
                    {"number": 6, "title": "Exit status", "url": "https://github.com/o/r/issues/6",
                     "createdAt": "2026-10-24T18:00:00Z", "updatedAt": "2026-10-24T19:00:00Z",
                     "body": "Make it fail.", "author": {"login": "owner"},
                     "labels": {"nodes": [{"name": "agent"}, {"name": "agent:working"}]},
                     "milestone": None},
                    {"number": 5, "title": "Maintainer agent log", "url": "https://github.com/o/r/issues/5",
                     "createdAt": "2026-10-24T17:00:00Z", "updatedAt": "2026-10-24T19:00:00Z",
                     "body": "", "author": {"login": "limit-cyclist"}, "labels": {"nodes": []},
                     "milestone": None},
                    {"number": 9, "title": "Add a pole-placement tool", "url": "https://github.com/o/r/issues/9",
                     "createdAt": "2026-10-24T17:00:00Z", "updatedAt": "2026-10-24T19:00:00Z",
                     "body": "Why: control design.\n<!-- xpps-agent -->", "author": {"login": "limit-cyclist"},
                     "labels": {"nodes": [{"name": "agent:proposal"}]}, "milestone": {"title": "M7"}}]},
                "pullRequests": {"nodes": [
                    {"number": 7, "title": "App: exit non-zero", "url": "https://github.com/o/r/pull/7",
                     "isDraft": False, "createdAt": "2026-10-24T19:00:00Z",
                     "updatedAt": "2026-10-24T19:30:00Z", "mergeable": "MERGEABLE",
                     "headRefName": "agent/6-x", "additions": 65, "deletions": 4, "changedFiles": 5,
                     "body": "Closes #6.\n\nfail() now flags an error.\n\n**Checks run before pushing**\n\n- x\n\n<!-- xpps-agent -->",
                     "author": {"login": "limit-cyclist"},
                     "commits": {"nodes": [{"commit": {"oid": "abc", "statusCheckRollup": {
                         "state": "SUCCESS", "contexts": {"nodes": [None]}}}}]}}]},
                "merged": {"nodes": []},
                "milestones": {"nodes": []},
                "roadmap": {"text": "## Milestones\n### M1 Engine\n- [x] load\n- [ ] the `xpp` tool\n"},
            }}

    def workflow_jobs(self, sha):
        self.calls.append(("jobs", sha))
        return [{"name": "Windows", "status": "completed", "conclusion": "success", "url": "https://x"},
                {"name": "macos", "status": "completed", "conclusion": "skipped", "url": "https://y"}]

    def comments(self, number):
        return [{"body": "### 🔍 Agent review\n\n- **low** an old quirk\n\n<!-- xpps-agent -->"}]

    def merge(self, pr):
        self.calls.append(("merge", pr))
        return {"sha": "def"}

    def comment(self, number, body):
        self.calls.append(("comment", number, body))

    def add_labels(self, number, labels):
        self.calls.append(("label", number, tuple(labels)))

    def remove_label(self, number, label):
        self.calls.append(("unlabel", number, label))

    def create_issue(self, title, body, labels):
        self.calls.append(("issue", title, tuple(labels)))
        return 10

    def close(self, number):
        self.calls.append(("close", number))


def write_home(home: Path, now: float) -> None:
    runs = home / "runs"
    runs.mkdir(parents=True)
    (home / "state.json").write_text(json.dumps({
        "tasks": {"6": {"issue": 6, "title": "Exit status", "stage": "owner", "pr": 7,
                        "branch": "agent/6-x", "question": "", "plan": "1. Fix it.",
                        "updated": now - 60, "created": now - 3600, "rounds": {}}},
        "events": [[now - 300, "#6 draft PR #7"]], "paused_until": 0}), encoding="utf-8")
    (home / "status.json").write_text(json.dumps({
        "time": now - 20, "doing": "idle", "me": "limit-cyclist", "log_issue": 5,
        "poll_seconds": 60, "runs_used": 4, "runs_per_window": 8, "window_hours": 5}),
        encoding="utf-8")
    events = [
        {"type": "system", "subtype": "init", "session_id": "s"},
        {"type": "assistant", "message": {"content": [
            {"type": "text", "text": "Reading the change."},
            {"type": "tool_use", "name": "Read", "input": {"file_path": "C:/w/app/src/main.c"}},
            {"type": "tool_use", "name": "Bash", "input": {"command": "ctest", "description": "Run the tests"}}]}},
        {"type": "user", "message": {"content": [
            {"type": "tool_result", "is_error": True, "content": "No such file"}]}},
        {"type": "result", "subtype": "success", "is_error": False, "num_turns": 3,
         "duration_ms": 21000, "result": "Looks fine."}]
    (runs / f"{RUN}.jsonl").write_text("\n".join(json.dumps(e) for e in events) + "\n", encoding="utf-8")
    (runs / f"{RUN}.json").write_text(json.dumps({
        "seconds": 21.8, "events": f"{RUN}.jsonl", "stdout": json.dumps(events[-1])}), encoding="utf-8")
    (runs / "current.json").write_text(json.dumps({
        "tag": "6-review", "started": now - 30, "file": f"{RUN}.jsonl", "cwd": "C:/w"}), encoding="utf-8")


@pytest.fixture
def site(tmp_path):
    now = time.time()
    home = tmp_path / "home"
    write_home(home, now)
    cfg = Config(home=home, owner="owner", slug="o/r")
    api = FakeAPI()
    dash = Dashboard(cfg, api, refresh_seconds=3600)
    dash.refresh_github()
    api.calls.clear()
    sessions = Sessions(hash_password("correct horse battery"))
    httpd = serve(dash, sessions, "127.0.0.1", 0, background=False)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    port = httpd.server_address[1]

    class Site:
        pass
    s = Site()
    s.api, s.dash, s.sessions, s.port, s.home = api, dash, sessions, port, home

    def request(method, path, body=None, cookie=None, headers=None, form=False):
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        h = dict(headers or {})
        if cookie:
            h["Cookie"] = f"xpps={cookie}"
        data = None
        if body is not None:
            if form:
                from urllib.parse import urlencode
                data = urlencode(body)
                h["Content-Type"] = "application/x-www-form-urlencoded"
            else:
                data = json.dumps(body)
                h["Content-Type"] = "application/json"
        c.request(method, path, body=data, headers=h)
        r = c.getresponse()
        raw = r.read()
        c.close()
        try:
            payload = json.loads(raw) if raw else None
        except ValueError:
            payload = raw.decode("utf-8", "replace")
        return r.status, dict(r.getheaders()), payload

    def login():
        status, headers, _ = request("POST", "/login", {"password": "correct horse battery"}, form=True)
        assert status == 303 and headers["Location"] == "/"
        return headers["Set-Cookie"].split(";")[0].split("=", 1)[1]
    s.request, s.login = request, login
    yield s
    httpd.shutdown()


def test_a_password_is_needed(site):
    status, headers, _ = site.request("GET", "/")
    assert status == 303 and headers["Location"] == "/login"
    assert site.request("GET", "/api/overview")[0] == 401
    status, headers, _ = site.request("POST", "/login", {"password": "wrong"}, form=True)
    assert status == 303 and "wrong=1" in headers["Location"] and "Set-Cookie" not in headers
    status, _, page = site.request("GET", "/login")
    assert status == 200 and "Mission control" in page


def test_too_many_wrong_passwords_are_refused_for_a_while(site):
    for _ in range(5):
        site.request("POST", "/login", {"password": "nope"}, form=True)
    status, headers, _ = site.request("POST", "/login", {"password": "correct horse battery"}, form=True)
    assert "wait=1" in headers["Location"]


def test_the_page_shows_what_the_bot_does_and_what_waits(site):
    cookie = site.login()
    status, headers, page = site.request("GET", "/api/overview", cookie=cookie)
    assert status == 200 and "default-src 'self'" in headers["Content-Security-Policy"]
    assert page["bot"] == "limit-cyclist" and page["agent"]["alive"]
    assert page["agent"]["current"]["kind"] == "review" and page["agent"]["current"]["issue"] == 6
    kinds = {i["kind"]: i for i in page["inbox"]}
    review = kinds["review"]
    assert review["pr"] == 7 and review["summary"] == "fail() now flags an error."
    assert review["checks"]["items"][0]["result"] == "success"
    assert review["checks"]["state"] == "success"      # a skipped job does not hold it back
    assert "an old quirk" in review["review"]
    assert kinds["proposal"]["issue"] == 9 and "control design" in kinds["proposal"]["body"]
    assert page["runs"][0]["kind"] == "review" and page["runs"][0]["ok"]
    assert page["roadmap"][0]["done"] == 1 and page["roadmap"][0]["total"] == 2
    assert page["csrf"]


def test_the_live_feed(site):
    cookie = site.login()
    status, _, out = site.request("GET", f"/api/live?file={RUN}.jsonl&after=0", cookie=cookie)
    assert status == 200 and out["running"]
    kinds = [e["kind"] for e in out["events"]]
    assert kinds == ["start", "say", "tool", "tool", "error", "end"]
    assert out["events"][2]["text"] == "app/src/main.c"         # relative to the worktree
    assert out["events"][3]["text"] == "Run the tests"
    status, _, again = site.request("GET", f"/api/live?file={RUN}.jsonl&after={out['after']}",
                                    cookie=cookie)
    assert again["events"] == []
    assert site.request("GET", "/api/live?file=../secrets.env", cookie=cookie)[0] == 400


def test_actions_need_the_csrf_token_and_the_same_site(site):
    cookie = site.login()
    csrf = site.request("GET", "/api/overview", cookie=cookie)[2]["csrf"]
    status, _, _ = site.request("POST", "/api/action", {"action": "merge", "pr": 7}, cookie=cookie)
    assert status == 403 and not site.api.calls
    status, _, _ = site.request("POST", "/api/action", {"action": "merge", "pr": 7}, cookie=cookie,
                                headers={"X-CSRF": csrf, "Origin": "https://evil.example"})
    assert status == 403 and not site.api.calls
    status, _, out = site.request("POST", "/api/action", {"action": "merge", "pr": 7}, cookie=cookie,
                                  headers={"X-CSRF": csrf})
    assert status == 200 and site.api.calls == [("merge", 7)] and "Merged #7" in out["message"]


def test_every_action(site):
    cookie = site.login()
    csrf = site.request("GET", "/api/overview", cookie=cookie)[2]["csrf"]

    def act(**data):
        return site.request("POST", "/api/action", data, cookie=cookie, headers={"X-CSRF": csrf})
    assert act(action="comment", number=6, body="Shorter, please.")[0] == 200
    assert act(action="ask", title="Add templates", body="Neuron, oscillator.", priority=True)[0] == 200
    assert act(action="approve", issue=9)[0] == 200
    assert act(action="decline", issue=9, reason="Later.")[0] == 200
    assert act(action="pause")[0] == 200
    assert act(action="resume")[0] == 200
    assert act(action="close", number=7)[0] == 200
    assert act(action="comment", number=6, body="")[0] == 400
    assert act(action="launch-missiles")[0] == 400
    assert site.api.calls == [
        ("comment", 6, "Shorter, please."),
        ("issue", "Add templates", ("agent", "agent:priority")),
        ("label", 9, ("agent",)),
        ("comment", 9, "Later."), ("close", 9),
        ("label", 5, ("agent:paused",)),
        ("unlabel", 5, "agent:paused"),
        ("close", 7)]


def test_a_read_only_dashboard_acts_on_nothing(site):
    site.dash.read_only = True
    cookie = site.login()
    csrf = site.request("GET", "/api/overview", cookie=cookie)[2]["csrf"]
    status, _, _ = site.request("POST", "/api/action", {"action": "merge", "pr": 7}, cookie=cookie,
                                headers={"X-CSRF": csrf})
    assert status == 403 and not site.api.calls


def test_logging_out_ends_the_session(site):
    cookie = site.login()
    site.request("POST", "/logout", {}, cookie=cookie, form=True)
    assert site.request("GET", "/api/overview", cookie=cookie)[0] == 401


def test_passwords():
    stored = hash_password("a long passphrase")
    assert check_password("a long passphrase", stored)
    assert not check_password("a long passphras", stored)
    assert not check_password("x", "garbage")


def test_the_real_roadmap_parses():
    milestones = parse_roadmap((ROOT / "ROADMAP.md").read_text(encoding="utf-8"))
    ids = [m["id"] for m in milestones]
    assert ids[:3] == ["M0", "M1", "M2"] and all(m["total"] for m in milestones)
    assert any(not i["done"] for m in milestones for i in m["items"])


def test_a_line_still_being_written_is_read_next_time(tmp_path):
    path = tmp_path / "run.jsonl"
    path.write_text(json.dumps({"type": "system", "subtype": "init"}) + "\n{\"type\": \"assi",
                    encoding="utf-8")
    events, after = read_events(path)
    assert [e["kind"] for e in events] == ["start"] and after == 1
    assert summarize({"type": "result", "is_error": True, "result": "x"})[0]["ok"] is False


def test_finished_ci_is_not_fetched_again(site):
    site.dash.refresh_github()
    assert ("jobs", "abc") not in site.api.calls       # completed on the first refresh
