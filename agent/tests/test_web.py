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
                                 diff_roadmap, parse_focus, parse_path, parse_roadmap, read_events, serve,
                                 summarize)

ROOT = Path(__file__).resolve().parents[2]
RUN = "20261024-205221-6-review-d0c3be"
OLD, MID, NEW, OTHER = "0" * 40, "a" * 40, "b" * 40, "c" * 40
ROADMAP_NOW = ("## The main path\n\nPath: M1 \u2192 M2\n\n## Milestones\n"
               "### M1 Engine\n- [x] load\n- [ ] the `xpp` tool\n### M2 App\n- [ ] a window\n"
               "### M5 Docs\n- [ ] a manual\n")
ROADMAP_THEN = "## Milestones\n### M1 Engine\n- [ ] load\n- [ ] the `xpp` tool\n"


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
                     "labels": {"nodes": [{"name": "agent:proposal"}]}, "milestone": {"title": "M7"}},
                    {"number": 11, "title": "Plots freeze on resize", "url": "https://github.com/o/r/issues/11",
                     "createdAt": "2026-10-24T16:00:00Z", "updatedAt": "2026-10-24T19:00:00Z",
                     "body": "Resizing hangs it.", "author": {"login": "alice"},
                     "labels": {"nodes": []}, "milestone": None}]},
                "pullRequests": {"nodes": [
                    {"number": 7, "title": "App: exit non-zero", "url": "https://github.com/o/r/pull/7",
                     "isDraft": False, "createdAt": "2026-10-24T19:00:00Z",
                     "updatedAt": "2026-10-24T19:30:00Z", "mergeable": "MERGEABLE",
                     "headRefName": "agent/6-x", "additions": 65, "deletions": 4, "changedFiles": 5,
                     "body": "Closes #6.\n\nfail() now flags an error.\n\n**Checks run before pushing**\n\n- x\n\n<!-- xpps-agent -->",
                     "author": {"login": "limit-cyclist"},
                     "commits": {"nodes": [{"commit": {"oid": "abc"}}]}}]},
                "merged": {"nodes": [
                    {"number": 12, "title": "Load models", "url": "https://github.com/o/r/pull/12",
                     "mergedAt": "2026-10-25T10:00:00Z", "mergeCommit": {"oid": NEW}},
                    {"number": 11, "title": "Fix a typo", "url": "https://github.com/o/r/pull/11",
                     "mergedAt": "2026-10-25T09:00:00Z", "mergeCommit": {"oid": OTHER}}]},
                "milestones": {"nodes": []},
                "roadmap": {"text": ROADMAP_NOW},
                "roadmapHistory": {"target": {"history": {"nodes": [
                    {"oid": NEW, "committedDate": "2026-10-25T10:00:00Z", "messageHeadline": "x",
                     "url": "https://github.com/o/r/commit/b", "parents": {"nodes": [{"oid": MID}]},
                     "associatedPullRequests": {"nodes": [
                         {"number": 12, "title": "Load models", "url": "https://github.com/o/r/pull/12"}]}},
                    {"oid": MID, "committedDate": "2026-10-20T10:00:00Z", "messageHeadline": "Roadmap",
                     "url": "https://github.com/o/r/commit/a", "parents": {"nodes": [{"oid": OLD}]},
                     "associatedPullRequests": {"nodes": []}}]}}},
            }}

    def roadmap_texts(self, oids):
        self.calls.append(("roadmaps", tuple(oids)))
        return {o: {NEW: ROADMAP_NOW, MID: ROADMAP_THEN}.get(o, "") for o in oids}

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
        return {"id": 555}

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


def test_the_login_form_works_as_a_browser_sends_it(site):
    """A browser posts the form with the page's own Origin (and, under a
    no-referrer policy, with "Origin: null", which must stay refused)."""
    status, headers, _ = site.request("GET", "/login")
    assert headers["Referrer-Policy"] == "same-origin"
    here = f"http://127.0.0.1:{site.port}"
    status, headers, _ = site.request("POST", "/login", {"password": "correct horse battery"},
                                      form=True, headers={"Origin": here})
    assert status == 303 and headers["Location"] == "/" and "xpps=" in headers["Set-Cookie"]
    status, _, _ = site.request("POST", "/login", {"password": "correct horse battery"},
                                form=True, headers={"Origin": "null"})
    assert status == 403


def test_asking_the_bot_to_explain(site):
    cookie = site.login()
    page = site.request("GET", "/api/overview", cookie=cookie)[2]
    others = [i for i in page["inbox"] if i["kind"] == "issue"]
    assert [i["issue"] for i in others] == [11]        # not the owner's, the bot's or the log
    assert others[0]["author"] == "alice" and others[0]["explain"] is None
    status, _, out = site.request("POST", "/api/action",
                                  {"action": "explain", "number": 11, "question": "Is it real?"},
                                  cookie=cookie, headers={"X-CSRF": page["csrf"]})
    assert status == 200 and "few minutes" in out["message"]
    assert site.api.calls == [("comment", 11, "/explain Is it real?")]
    page = site.request("GET", "/api/overview", cookie=cookie)[2]
    sent = [i for i in page["inbox"] if i["issue"] == 11][0]["explain"]
    assert sent["status"] == "sent" and sent["question"] == "Is it real?"

    # the agent picks it up, then answers; a brief on the pull request too
    (site.home / "explain.json").write_text(json.dumps({"items": [
        {"comment": "555", "issue": 11, "question": "Is it real?", "asked": "2026-10-24T19:00:00Z",
         "status": "answered", "answer": "Yes: the handler redraws twice.", "answered": 1},
        {"comment": "556", "issue": 7, "question": "", "asked": "2026-10-24T19:01:00Z",
         "status": "working", "answer": ""}]}), encoding="utf-8")
    (site.home / "runs" / "explaining.json").write_text(json.dumps(
        {"tag": "7-explain", "file": "20261024-190100-7-explain-abcdef.jsonl"}), encoding="utf-8")
    page = site.request("GET", "/api/overview", cookie=cookie)[2]
    by = {(i["kind"], i["issue"]): i for i in page["inbox"]}
    assert by[("issue", 11)]["explain"]["answer"] == "Yes: the handler redraws twice."
    assert by[("review", 6)]["explain"]["status"] == "working"   # asked on the PR, #7
    assert page["agent"]["explaining"]["issue"] == 7
    status, _, _ = site.request("POST", "/api/action", {"action": "explain", "number": 11},
                                cookie=cookie, headers={"X-CSRF": page["csrf"]})
    assert status == 200 and site.api.calls[-1] == ("comment", 11, "/explain")


def test_the_path_and_what_moved_it(site):
    cookie = site.login()
    _, _, page = site.request("GET", "/api/overview", cookie=cookie)
    ev = page["evolution"]
    assert ev["path"] == ["M1", "M2"] and ev["side"] == ["M5"]
    assert ev["current"] == "M1" and ev["next"] == {"milestone": "M1", "item": 1}
    newest, first = ev["steps"]
    assert newest["pr"] == 12 and newest["done"] == [{"milestone": "M1", "item": 0}]
    assert newest["planned"] == [{"milestone": "M2", "item": 0}, {"milestone": "M5", "item": 0}]
    assert first["created"] and not first["done"]
    assert [(d["number"], d["kind"]) for d in ev["drift"]] == [(11, "other"), (12, "path")]
    # A commit's roadmap is fetched once: the next refresh asks for none.
    site.dash.refresh_github()
    assert not [c for c in site.api.calls if c[0] == "roadmaps"]


def test_a_ticked_item_is_done_and_a_new_open_one_planned():
    before = parse_roadmap("### M1 A\n- [ ] one\n- [ ] two\n")
    after = parse_roadmap("### M1 A\n- [x] one\n- [ ] two\n- [ ] three\n- [x] four\n")
    assert diff_roadmap(before, after) == [
        {"milestone": "M1", "item": 0, "kind": "done"},
        {"milestone": "M1", "item": 2, "kind": "planned"},
        {"milestone": "M1", "item": 3, "kind": "done"}]


def test_the_real_roadmap_has_a_path():
    text = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    milestones = parse_roadmap(text)
    path = parse_path(text)
    assert path[0] == "M0" and len(path) >= 5 and set(path) <= {m["id"] for m in milestones}
    assert parse_focus(text) in path              # the milestone in brackets
    # A heading without an M number does not leak its items into the one before.
    assert next(m for m in milestones if m["id"] == "M8")["total"] == 2
