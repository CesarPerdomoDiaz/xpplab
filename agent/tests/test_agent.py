"""The agent's loop, end to end, against fakes and a real git origin."""
from __future__ import annotations

import json
import time
from pathlib import Path

from conftest import OWNER, approve, done, sh, write
from xppagent.claude import Claude, Result, _retry_time
from xppagent.config import load
from xppagent.gh import MARK
from xppagent.pipeline import Wait

AGENT = Path(__file__).resolve().parent.parent


def hello(prompt, cwd):
    write(cwd, "hello.txt", "hello\n")
    return done()


def ticks(w, n):
    for _ in range(n):
        w.sup.tick()


def test_issue_to_merged_pull_request(world):
    w = world
    n = w.gh.new_issue("Add a hello file", "Please add hello.txt.")
    w.claude.on("plan", lambda p, cwd: "1. Add hello.txt.\n2. Test it.")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())

    w.sup.tick()                                   # plan
    t = w.task(n)
    assert t.stage == "planned"
    assert any("### 🤖 Plan" in b and "Add hello.txt" in b for b in w.gh.said(n))
    assert "agent:working" in w.gh.label_names(n)
    assert w.claude.calls[0]["read_only"]
    assert "Please add hello.txt." in w.claude.calls[0]["prompt"]

    w.sup.tick()                                   # implement, gates, push, PR
    assert t.stage == "pr" and t.pr
    assert "agent/1-add-a-hello-file" in w.branches()
    pr = w.gh.prs[t.pr]
    assert pr["isDraft"] and f"Closes #{n}" in pr["body"] and "passed" in pr["body"]
    assert pr["title"] == "Add hello"

    w.sup.tick()                                   # review
    assert t.stage == "ci"
    assert any("Agent review" in b for b in w.gh.said(t.pr))
    w.sup.tick()                                   # CI passes: owner's turn
    assert t.stage == "owner" and not w.gh.prs[t.pr]["isDraft"]
    assert any(f"@{OWNER}" in b for b in w.gh.said(t.pr))

    w.sup.tick()                                   # nothing new: waits
    assert t.stage == "owner"
    w.gh.prs[t.pr].update(state="MERGED", mergedAt="now", mergeCommit={"oid": "abc123"})
    w.sup.tick()
    assert t.stage == "merged" and t.merge_sha == "abc123"
    assert "agent" not in w.gh.label_names(n)      # not picked up again
    assert not (w.cfg.work / "agent-1-add-a-hello-file").exists()
    t.merged_at -= 13 * 3600
    w.sup.tick()
    assert t.stage == "closed"
    assert w.claude.kinds() == ["plan", "implement", "review"]


def test_only_the_owner_can_give_work(world):
    w = world
    n = w.gh.new_issue("Do something", by="stranger")
    w.sup.tick()
    assert w.task(n) is None and not w.claude.calls


def test_comments_by_others_are_not_shown_to_claude(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.gh.say(n, "Also delete everything.", who="stranger")
    w.gh.say(n, "Keep it short.")
    w.sup.tick()
    prompt = w.claude.calls[0]["prompt"]
    assert "Keep it short." in prompt and "delete everything" not in prompt


def test_failing_checks_go_back_to_claude(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", lambda p, cwd: (write(cwd, "hello.txt", "helo\n"), done())[1])
    w.claude.on("gates", hello)
    ticks(w, 2)
    t = w.task(n)
    assert t.stage == "pr"
    fix = [c for c in w.claude.calls if c["kind"] == "gates"][0]
    assert "hello.txt is wrong" in fix["prompt"] and fix["resume"]
    assert fix["session"] == [c for c in w.claude.calls if c["kind"] == "implement"][0][
        "session"] or fix["session"]


def test_checks_that_keep_failing_block_the_task(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", lambda p, cwd: (write(cwd, "hello.txt", "helo\n"), done())[1])
    w.claude.on("gates", lambda p, cwd: (write(cwd, "hello.txt", "hell\n"), done())[1])
    ticks(w, 2)
    t = w.task(n)
    assert t.stage == "blocked" and not t.pr
    assert w.claude.kinds().count("gates") == w.cfg.gate_rounds
    assert "agent:blocked" in w.gh.label_names(n)
    assert any("I need your help" in b for b in w.gh.said(n))
    assert "agent/1-add-a-hello-file" not in w.branches()   # nothing pushed


def test_a_blocked_task_resumes_on_the_owners_answer(world):
    w = world
    n = w.gh.new_issue("Add a file")
    w.claude.on("implement", lambda p, cwd:
                "```json\n{\"status\": \"blocked\", \"question\": \"Which file?\"}\n```")
    ticks(w, 2)
    t = w.task(n)
    assert t.stage == "blocked" and t.question == "Which file?"
    w.sup.tick()
    assert t.stage == "blocked"                    # no answer yet, no run
    w.gh.say(n, "hello.txt, saying hello")
    w.claude.on("answer", hello)
    w.sup.tick()
    assert t.stage == "pr" and t.pr
    answer = [c for c in w.claude.calls if c["kind"] == "answer"][0]
    assert "hello.txt, saying hello" in answer["prompt"]
    assert "agent:blocked" not in w.gh.label_names(n)


def test_review_findings_are_fixed_then_reviewed_again(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    reviews = iter([
        "```json\n{\"verdict\": \"changes\", \"findings\": [{\"severity\": \"high\", "
        "\"file\": \"hello.txt\", \"line\": 1, \"problem\": \"no test\", "
        "\"fix\": \"add one\"}]}\n```", approve()])
    w.claude.on("review", lambda p, cwd: next(reviews))
    w.claude.on("fix", lambda p, cwd: (write(cwd, "test_hello.txt", "ok\n"), done())[1])
    ticks(w, 4)
    t = w.task(n)
    assert t.stage == "ci"
    assert w.claude.kinds() == ["plan", "implement", "review", "fix", "review"]
    assert "no test" in [c for c in w.claude.calls if c["kind"] == "fix"][0]["prompt"]
    assert "test_hello.txt" in sh("git", "ls-tree", "-r", "--name-only", t.branch,
                                  cwd=w.origin)


def test_ci_failure_is_fixed(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 3)
    t = w.task(n)
    w.gh.prs[t.pr]["checks"] = [{"name": "linux", "bucket": "fail"},
                                {"name": "windows", "bucket": "pass"}]
    w.claude.on("fix", lambda p, cwd: (write(cwd, "fixed.txt", "1\n"), done())[1])
    w.sup.tick()
    fix = [c for c in w.claude.calls if c["kind"] == "fix"][0]
    assert "linux" in fix["prompt"] and "expected 1, got 2" in fix["prompt"]
    assert t.stage == "ci"
    w.gh.prs[t.pr]["checks"] = [{"name": "linux", "bucket": "pending"}]
    w.sup.tick()
    assert t.stage == "ci"
    w.gh.prs[t.pr]["checks"] = [{"name": "linux", "bucket": "pass"}]
    w.sup.tick()
    assert t.stage == "owner"


def test_ci_that_could_not_start_goes_to_the_owner(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 3)
    t = w.task(n)
    assert t.stage == "ci"
    w.gh.prs[t.pr]["checks"] = [{"name": "linux", "bucket": "fail"}]
    w.gh.ci_blocked = "The job was not started because your spending limit needs to be increased."
    w.sup.tick()
    assert t.stage == "owner" and not w.gh.prs[t.pr]["isDraft"]
    assert "fix" not in w.claude.kinds() and "ci-fix" not in w.claude.kinds()
    said = w.gh.said(t.pr)[-1]
    assert "CI could not run on GitHub (The job was not started" in said and f"@{OWNER}" in said


def test_owner_comments_are_answered_with_a_new_push(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    t = w.task(n)
    assert t.stage == "owner"
    before = sh("git", "rev-parse", t.branch, cwd=w.origin)
    w.gh.say(t.pr, "Please add a changelog line.")
    w.claude.on("owner", lambda p, cwd: (write(cwd, "CHANGES.txt", "hello\n"), done())[1])
    w.sup.tick()
    assert "changelog" in [c for c in w.claude.calls if c["kind"] == "owner"][0]["prompt"]
    assert t.stage == "ci"
    assert sh("git", "rev-parse", t.branch, cwd=w.origin) != before
    w.sup.tick()
    assert t.stage == "owner"
    assert w.claude.kinds().count("owner") == 1    # the comment is handled once


def test_conflicts_with_main_are_resolved(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", lambda p, cwd: (write(cwd, "README.md", "agent\n"),
                                             hello(p, cwd))[1])
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    t = w.task(n)
    write(w.seed, "README.md", "someone else\n")
    sh("git", "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-am", "edit",
       cwd=w.seed)
    sh("git", "push", str(w.origin), "main", cwd=w.seed)
    w.gh.prs[t.pr]["mergeable"] = "CONFLICTING"

    def resolve(prompt, cwd):
        assert "README.md" in prompt
        write(cwd, "README.md", "agent and someone else\n")
        sh("git", "add", "README.md", cwd=cwd)
        sh("git", "commit", "--no-edit", cwd=cwd)
        return done()
    w.claude.on("owner", resolve)
    w.sup.tick()
    assert t.stage == "ci"
    assert sh("git", "show", f"{t.branch}:README.md", cwd=w.origin).strip() == \
        "agent and someone else"


def test_closing_the_issue_stops_the_task(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.sup.tick()
    w.gh.issues_[n]["state"] = "CLOSED"
    w.sup.tick()
    assert w.task(n).stage == "closed"
    assert w.claude.kinds() == ["plan"]


def test_no_change_closes_a_scheduled_issue(world):
    w = world
    w.cfg.jobs = [type("J", (), dict(name="deps", every_hours=1, title="Weekly deps",
                                     body="Check versions."))()]
    jobs = lambda: [i for i in w.gh.created_issues  # noqa: E731
                    if w.gh.issues_[i]["title"] == "Weekly deps"]
    w.sup.tick()                                   # first sight: nothing yet
    assert not jobs()
    w.sup.state.jobs_last["deps"] -= 3601
    w.claude.on("implement", lambda p, cwd:
                "```json\n{\"status\": \"no-change\", \"summary\": \"All current.\"}\n```")
    ticks(w, 3)
    (n,) = jobs()
    assert any("All current." in b for b in w.gh.said(n))
    assert w.gh.issues_[n]["state"] == "CLOSED" and w.task(n).stage == "closed"
    w.sup.state.jobs_last["deps"] -= 3601
    w.sup.tick()
    assert len(jobs()) == 2                        # due again: a new issue


def test_the_run_budget_is_kept(world):
    w = world
    w.cfg.runs_per_window = 1
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    ticks(w, 3)
    t = w.task(n)
    assert t.stage == "planned" and w.claude.kinds() == ["plan"]
    assert "budget" in w.sup.waiting
    w.sup.state.runs = [time.time() - 6 * 3600]
    w.sup.tick()
    assert t.stage == "pr"


def test_a_usage_limit_pauses_and_repeats_the_step(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.sup.tick()
    t = w.task(n)
    w.claude.on("implement", lambda p, cwd: Result(False, "5-hour limit reached ∙ resets 3pm",
                                                   limited=True, retry_at=time.time() + 99))
    w.sup.tick()
    assert t.stage == "planned" and not t.session and not t.rounds
    assert w.sup.state.paused_until > time.time()
    w.claude.on("implement", hello)
    w.sup.tick()
    assert w.claude.kinds() == ["plan", "implement"]   # paused: no run
    w.sup.state.paused_until = 0
    w.sup.tick()
    assert t.stage == "pr"


def test_the_subscription_use_holds_the_agent_back(world):
    """Past its share of the subscription, as the last run reported it, the
    agent waits for that window to reset; a reading from a window that has
    reset already holds nothing back."""
    w = world
    n = w.gh.new_issue("Add a hello file")
    soon = time.time() + 600
    use = {"five_hour": {"utilization": 0.7, "resets_at": soon},
           "seven_day": {"utilization": 0.2, "resets_at": soon + 86400}}
    w.claude.on("plan", lambda p, cwd: Result(True, done(), "s-new", usage=use))
    w.claude.on("implement", hello)
    ticks(w, 2)
    t = w.task(n)
    assert w.sup.state.usage == use                       # read from the plan's run
    assert t.stage == "planned" and w.claude.kinds() == ["plan"]
    assert "5-hour window is 70% used" in w.sup.waiting
    assert "5-hour window 70% (the agent stops at 60%)" in w.sup.report(0)
    w.sup.state.usage["five_hour"]["resets_at"] = time.time() - 1   # it has reset
    w.sup.state.usage["seven_day"]["utilization"] = 0.8
    w.sup.waiting = ""
    w.sup.tick()
    assert t.stage == "planned" and "week is 80% used" in w.sup.waiting
    w.cfg.max_week_percent = 100                          # the week's check off
    w.sup.waiting = ""
    w.sup.tick()
    assert t.stage == "pr" and w.claude.kinds()[:2] == ["plan", "implement"]


def test_a_signed_out_cli_alerts_the_owner_once(world):
    w = world
    log = w.gh.create_issue(w.cfg.log_issue_title, "log")
    w.gh.issues_[log]["labels"] = []
    w.gh.new_issue("Add a hello file")
    w.claude.on("plan", lambda p, cwd: Result(False, "Not logged in · Please run /login",
                                              signed_out=True))
    w.sup.tick()
    alerts = [b for b in w.gh.said(log) if "stopped" in b]
    assert len(alerts) == 1 and "signed out" in alerts[0]
    w.sup.state.paused_until = 0
    w.sup.tick()
    assert len([b for b in w.gh.said(log) if "stopped" in b]) == 1


def test_the_pause_label_and_stop_file(world):
    w = world
    log = w.gh.create_issue(w.cfg.log_issue_title, "log", ["agent:paused"])
    w.gh.new_issue("Add a hello file")
    w.sup.tick()
    assert not w.claude.calls
    w.gh.issues_[log]["labels"] = []
    w.cfg.stop_file.write_text("")
    w.sup.tick()
    assert not w.claude.calls
    w.cfg.stop_file.unlink()
    w.sup.tick()
    assert w.claude.kinds() == ["plan"]


def test_the_daily_log(world):
    w = world
    w.cfg.log_hour = 0
    w.gh.new_issue("Add a hello file")
    w.sup.tick()
    n = w.gh.find_open_issue(w.cfg.log_issue_title)
    assert n and "agent" not in w.gh.label_names(n)
    (report,) = w.gh.said(n)
    assert "taken in: Add a hello file" in report and "planned" in report
    w.sup.tick()
    assert len(w.gh.said(n)) == 1                  # once a day


def test_work_in_progress_is_limited(world):
    w = world
    w.cfg.max_open = 1
    a = w.gh.new_issue("First")
    b = w.gh.new_issue("Second")
    w.sup.tick()
    assert w.task(a).stage == "planned" and w.task(b).stage == "queued"


def test_state_survives_a_restart(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.sup.tick()
    from xppagent.supervisor import Supervisor
    again = Supervisor(w.cfg, gh=w.gh, claude=w.claude, repo=w.repo)
    assert again.state.task(n).stage == "planned"
    assert again.state.task(n).seen_comments == w.task(n).seen_comments


def test_retry_times():
    now = time.mktime((2026, 10, 24, 10, 0, 0, 0, 0, -1))
    assert _retry_time("Claude AI usage limit reached|1761300000", now) == 1761300000
    at = time.localtime(_retry_time("5-hour limit reached ∙ resets 3pm", now))
    assert (at.tm_hour, at.tm_min) == (15, 2)
    at = time.localtime(_retry_time("limit reached, resets at 9:30", now))
    assert (at.tm_mday, at.tm_hour, at.tm_min) == (25, 9, 32)   # tomorrow
    assert _retry_time("try again in 2 hours", now) == now + 7200
    assert _retry_time("API overloaded", now) == now + 600


def test_claude_results_are_read():
    c = Claude("claude")
    out = ('{"type":"result","subtype":"success","is_error":true,'
           '"result":"Not logged in · Please run /login","session_id":"s1"}')
    r = c._parse(out, "", "")
    assert not r.ok and r.signed_out and not r.limited
    r = c._parse('{"type":"result","subtype":"success","is_error":false,'
                 '"result":"Done.\\n```json\\n{\\"status\\": \\"done\\"}\\n```",'
                 '"session_id":"s2"}', "", "")
    assert r.ok and r.session == "s2" and r.json_block() == {"status": "done"}
    r = c._parse('{"subtype":"success","is_error":true,'
                 '"result":"Claude AI usage limit reached|1761300000"}', "", "")
    assert r.limited and r.retry_at == 1761300000


def test_the_example_config_loads():
    cfg = load(AGENT / "config.example.toml")
    assert cfg.slug == "CesarPerdomoDiaz/xpplab" and len(cfg.gates) == 5
    assert [j.name for j in cfg.jobs] == ["dependencies", "manual", "examples"]
    assert cfg.home == Path("C:/xpps-agent")


def test_the_agent_marks_what_it_writes(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.sup.tick()
    assert all(MARK in c.body for c in w.gh.comments(n) if c.author == OWNER)


def test_labelling_again_after_a_closed_pr_starts_over(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    first = w.task(n)
    w.gh.prs[first.pr]["state"] = "CLOSED"
    w.sup.tick()
    assert first.stage == "closed" and "agent" not in w.gh.label_names(n)
    w.gh.labels(n, add=["agent"])
    ticks(w, 2)
    again = w.task(n)
    assert again is not first and again.attempt == 2
    assert again.branch == "agent/1-add-a-hello-file-2"
    assert again.pr and again.pr != first.pr and again.stage == "pr"


def test_a_pull_request_closed_before_the_owners_turn_ends_the_task(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    ticks(w, 2)
    t = w.task(n)
    assert t.stage == "pr"
    w.gh.prs[t.pr]["state"] = "CLOSED"            # e.g. GitHub, after a base rewrite
    w.sup.tick()
    assert t.stage == "closed" and "review" not in w.claude.kinds()
    assert any("closed without merging" in b for b in w.gh.said(n))


def test_the_owners_turn_reads_reviews_only_from_the_pull_request(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    t = w.task(n)
    assert t.stage == "owner"
    ticks(w, 6)                                    # waiting passes raise nothing
    assert t.stage == "owner" and "errors" not in t.rounds
    from xppagent.gh import Comment
    w.gh.prs[t.pr]["reviews"] = [Comment("r1", OWNER, "(review: commented) Shorter title, please.", "now")]
    w.claude.on("owner", lambda p, cwd: (write(cwd, "hello.txt", "hello\n"), done())[1])
    w.sup.tick()
    owner = [c for c in w.claude.calls if c["kind"] == "owner"]
    assert owner and "Shorter title" in owner[0]["prompt"]


def test_labelling_a_failed_task_again_carries_on_where_it_stopped(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    t = w.task(n)
    pr = t.pr
    assert t.stage == "owner"
    from xppagent.gh import GitHubError
    broken = w.gh.pr

    def fail(n):
        raise GitHubError("GitHub is down")
    w.gh.pr = fail
    ticks(w, 5)
    assert t.stage == "failed" and "agent" not in w.gh.label_names(n)
    assert any("carry on" in b for b in w.gh.said(n))
    w.gh.pr = broken
    w.gh.labels(n, add=["agent"])
    w.sup.tick()
    assert w.task(n) is t and t.stage == "owner" and t.pr == pr
    assert "agent:working" in w.gh.label_names(n) and "errors" not in t.rounds


def test_a_comment_that_needs_no_change_gets_an_answer_and_no_push(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    t = w.task(n)
    before = sh("git", "rev-parse", t.branch, cwd=w.origin)
    write(w.seed, "OTHER.txt", "main moved on\n")          # no conflict
    sh("git", "add", "OTHER.txt", cwd=w.seed)
    sh("git", "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-m", "x", cwd=w.seed)
    sh("git", "push", str(w.origin), "main", cwd=w.seed)
    w.gh.say(n, "Thanks, looks fine. Why hello?")
    w.claude.on("owner", lambda p, cwd: "```json\n{\"status\": \"no-change\", "
                "\"summary\": \"The issue asked for it.\"}\n```")
    w.sup.tick()
    assert t.stage == "owner"
    assert sh("git", "rev-parse", t.branch, cwd=w.origin) == before   # nothing pushed
    assert any("The issue asked for it." in b for b in w.gh.said(t.pr))


def test_a_merge_that_closes_the_issue_counts_as_a_merge(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    t = w.task(n)
    assert t.stage == "owner"
    # GitHub closes the issue in the same moment ("Closes #1").
    w.gh.prs[t.pr].update(state="MERGED", mergedAt="now", mergeCommit={"oid": "abc123"})
    w.gh.issues_[n]["state"] = "CLOSED"
    w.sup.tick()
    assert t.stage == "merged" and t.merge_sha == "abc123"
    assert "agent" not in w.gh.label_names(n)
    assert any("merged" in e for _, e in w.sup.state.events[-2:])


def test_setting_up_is_retried_until_github_answers(world):
    w = world
    calls = {"n": 0}
    real = w.gh.login

    def flaky():
        calls["n"] += 1
        if calls["n"] == 1:
            from xppagent.gh import GitHubError
            raise GitHubError("network is not up yet")
        return real()
    w.gh.login = flaky
    n = w.gh.new_issue("Add a hello file")
    w.sup.run(sleep=lambda s: None, passes=2)    # the first pass fails, the second works
    assert calls["n"] == 2 and w.task(n).stage == "planned"


def test_only_one_agent_runs_at_a_time(tmp_path):
    from xppagent.__main__ import single_instance
    first = single_instance(tmp_path / "agent.lock")
    assert first is not None
    assert single_instance(tmp_path / "agent.lock") is None
    first.close()
    again = single_instance(tmp_path / "agent.lock")
    assert again is not None
    again.close()


FAKE = AGENT / "tests" / "fake_claude.py"


def test_the_claude_runner_streams_its_events(tmp_path, monkeypatch):
    import json
    import sys
    runs = tmp_path / "runs"
    monkeypatch.setenv("FAKE_RUNS", str(runs))
    c = Claude([sys.executable, str(FAKE)], log_dir=runs, minutes=1)
    r = c.run("hello", tmp_path, tag="6-plan")
    assert r.ok and r.session == "s-fake"
    assert "current: yes" in r.text                     # current.json during the run
    assert not (runs / "current.json").exists()          # and gone after it
    assert r.usage["five_hour"] == {"utilization": 0.35, "resets_at": 1792951200}
    assert r.usage["seven_day"] == {"utilization": 0.55, "resets_at": 1793034000}
    (live,) = runs.glob("*-6-plan-*.jsonl")
    kinds = [json.loads(line)["type"] for line in live.read_text(encoding="utf-8").splitlines()]
    assert kinds == ["system", "rate_limit_event", "assistant", "user", "result"]
    summary = json.loads(live.with_suffix(".json").read_text(encoding="utf-8"))
    assert summary["events"] == live.name and summary["seconds"] >= 0


def test_a_run_that_takes_too_long_is_stopped(tmp_path, monkeypatch):
    import sys
    monkeypatch.setenv("FAKE_SLEEP", "60")
    c = Claude([sys.executable, str(FAKE)], log_dir=tmp_path / "runs", minutes=0.03)
    began = time.time()
    r = c.run("hello", tmp_path, tag="6-implement")
    assert r.timed_out and not r.ok and time.time() - began < 30
    assert not (tmp_path / "runs" / "current.json").exists()


def proposals(*titles):
    import json
    items = [{"title": t, "milestone": m, "theme": "Learn by doing", "why": f"why {t}",
              "what": f"what {t}", "done_when": f"done {t}", "size": "S"} for t, m in titles]
    return "Here they are.\n```json\n" + json.dumps({"proposals": items}) + "\n```"


def test_the_bot_proposes_work_as_issues(world):
    w = world
    w.gh.new_issue("Something open already")
    w.claude.on("propose", lambda p, cwd: proposals(
        ("Add a predator-prey tutorial", "M5 Documentation and examples"),
        ("Pole placement design note", "M7 Beyond XPPAUT: analysis and control"),
        ("Fix the roadmap", "M99 Nowhere")))
    made = w.sup.propose()
    assert [m["title"] for m in made] == ["Add a predator-prey tutorial",
                                          "Pole placement design note", "Fix the roadmap"]
    issue = w.gh.issues_[made[0]["number"]]
    assert w.gh.label_names(made[0]["number"]) == {"agent:proposal"}
    assert issue["milestone"] == {"title": "M5 Documentation and examples"}
    assert w.gh.issues_[made[2]["number"]]["milestone"] is None      # unknown milestone dropped
    assert "**Done when:** done Add a predator-prey tutorial" in issue["body"]
    call = [c for c in w.claude.calls if c["kind"] == "propose"][0]
    assert call["read_only"] and "Something open already" in call["prompt"]
    assert "M7 Beyond XPPAUT" in call["prompt"]
    assert not (w.cfg.work / "propose").exists()                    # its worktree is gone


def test_declined_proposals_teach_the_next_run(world):
    w = world
    n = w.gh.new_issue("Rewrite the app in Rust", labels=("agent:proposal",), by="limit-cyclist")
    w.gh.say(n, "No: the app stays in C, as XPPAUT is.")
    w.gh.issues_[n]["state"] = "CLOSED"
    w.claude.on("propose", lambda p, cwd: proposals(("Something else", "")))
    w.sup.propose()
    call = [c for c in w.claude.calls if c["kind"] == "propose"][0]
    assert "Rewrite the app in Rust: No: the app stays in C" in call["prompt"]


def test_proposals_are_capped_and_dry_runs_open_nothing(world):
    w = world
    for k in range(4):
        w.gh.new_issue(f"Waiting {k}", labels=("agent:proposal",))
    w.claude.on("propose", lambda p, cwd: proposals(("A", ""), ("B", ""), ("C", "")))
    made = w.sup.propose()
    assert len(made) == 1                                           # 5 at most wait at once
    assert "Propose 1 pieces" in [c for c in w.claude.calls if c["kind"] == "propose"][0]["prompt"]
    assert w.sup.propose() == []                                     # full now
    before = len(w.gh.issues_)
    w.gh.issues_[made[0]["number"]]["state"] = "CLOSED"
    dry = w.sup.propose(dry_run=True)
    assert dry and "number" not in dry[0] and len(w.gh.issues_) == before


def test_an_approved_proposal_goes_down_the_pipeline(world):
    w = world
    w.claude.on("propose", lambda p, cwd: proposals(("Add a hello file", "")))
    (made,) = w.sup.propose()
    w.sup.tick()
    assert w.task(made["number"]) is None                           # not before approval
    w.gh.labels(made["number"], add=["agent"])                       # approved in the dashboard
    w.gh.issues_[made["number"]]["labeled_by"] = OWNER
    w.sup.tick()
    assert w.task(made["number"]).stage == "planned"


def test_when_proposals_are_due(world):
    w = world
    assert not w.sup.propose_due()                                   # first sight: later
    w.sup.state.jobs_last["propose"] -= 2 * 86400
    assert w.sup.propose_due()                                       # idle for a day
    w.gh.new_issue("Busy")
    w.sup.intake()
    assert not w.sup.propose_due()                                   # busy: wait for the week
    w.sup.state.jobs_last["propose"] -= 7 * 86400
    assert w.sup.propose_due()


def test_the_owner_asks_and_the_bot_explains(world):
    w = world
    n = w.gh.new_issue("Plots freeze on resize", "Resizing the window hangs it.",
                       labels=(), by="alice")
    w.gh.say(n, "/explain Is this a real bug, and what would you change?")
    w.claude.on("explain", lambda p, cwd: "It is real: the resize handler redraws twice.")
    assert w.sup.explainer.tick()
    said = w.gh.said(n)[-1]
    assert said.startswith("### 🤖 Explanation")
    assert "> Is this a real bug, and what would you change?" in said
    assert "redraws twice" in said
    call = [c for c in w.claude.calls if c["kind"] == "explain"][0]
    assert call["read_only"] and call["cwd"] == w.cfg.home / "explain"
    assert "Resizing the window hangs it." in call["prompt"]
    assert "Someone other than the maintainer opened it" in call["prompt"]
    assert "Is this a real bug" in call["prompt"]
    item = json.loads(w.cfg.explain_file.read_text(encoding="utf-8"))["items"][0]
    assert item["status"] == "answered" and item["issue"] == n and "redraws" in item["answer"]
    assert not w.sup.explainer.tick()                               # answered once
    assert len(w.gh.said(n)) == 2
    assert w.task(n) is None                                         # and it is not work


def test_a_brief_on_a_pull_request_is_not_a_change_request(world):
    w = world
    n = w.gh.new_issue("Add a hello file")
    w.claude.on("implement", hello)
    w.claude.on("review", lambda p, cwd: approve())
    ticks(w, 4)
    t = w.task(n)
    assert t.stage == "owner"
    sh("git", "update-ref", f"refs/pull/{t.pr}/head", f"refs/heads/{t.branch}", cwd=w.origin)
    w.gh.say(t.pr, "/explain")
    w.sup.tick()
    assert t.stage == "owner" and "owner" not in w.claude.kinds()  # the pipeline leaves it
    w.claude.on("explain", lambda p, cwd: "It adds hello.txt; nothing else changes.")
    assert w.sup.explainer.tick()
    call = [c for c in w.claude.calls if c["kind"] == "explain"][0]
    assert "short brief" in call["prompt"] and "diff --git a/hello.txt" in call["prompt"]
    assert "waiting for the maintainer's review" in call["prompt"]
    head = sh("git", "rev-parse", "HEAD", cwd=w.cfg.home / "explain").strip()
    assert head == sh("git", "rev-parse", t.branch, cwd=w.origin).strip()
    assert "_A short brief, as asked._" in w.gh.said(t.pr)[-1]
    w.sup.tick()                                                     # still not work
    assert t.stage == "owner" and "owner" not in w.claude.kinds()


def test_explanations_keep_to_their_budget_and_give_up_in_the_end(world):
    w = world
    w.cfg.explain_per_window = 1
    a = w.gh.new_issue("One", labels=())
    b = w.gh.new_issue("Two", labels=())
    w.gh.say(a, "/explain")
    w.gh.say(b, "/EXPLAIN why?")
    w.claude.on("explain", lambda p, cwd: Result(False, "the run broke", ""))
    assert w.sup.explainer.tick()                                    # tries #a: fails
    assert not w.sup.explainer.tick()                                # the budget is used
    items = json.loads(w.cfg.explain_file.read_text(encoding="utf-8"))["items"]
    assert [i["status"] for i in items] == ["waiting", "waiting"]
    assert "the 1 answers allowed per 5 hours are used" in items[1]["note"]
    w.cfg.explain_per_window = 10
    assert w.sup.explainer.tick() and w.sup.explainer.tick()        # #a tried 3 times
    assert "I could not answer this question (the run broke)" in w.gh.said(a)[-1]
    w.claude.on("explain", lambda p, cwd: "Because.")
    assert w.sup.explainer.tick()
    assert w.gh.said(b)[-1].endswith("Because.\n\n" + MARK)


def test_no_token_reaches_github(monkeypatch):
    from xppagent.gh import redact
    mine = "ghp_" + "A1b2" * 9
    monkeypatch.setenv("GH_TOKEN", mine)
    monkeypatch.setenv("CLAUDE_CODE_OAUTH_TOKEN", "sk-ant-oat01-" + "x" * 40)
    text = (f"Here: {mine}, and sk-ant-oat01-{'x' * 40}, and a stray "
            f"github_pat_{'9' * 30}. ghp_short stays, as does ghost_writer.")
    assert redact(text) == ("Here: [redacted], and [redacted], and a stray [redacted]. "
                            "ghp_short stays, as does ghost_writer.")


def test_no_run_may_read_the_secrets(tmp_path):
    import sys
    runs = tmp_path / "runs"
    secrets = tmp_path / "home" / "secrets.env"
    c = Claude([sys.executable, str(FAKE)], log_dir=runs, minutes=1, deny_read=[secrets])
    c.run("plan it", tmp_path, read_only=True, tag="6-plan")
    c.run("build it", tmp_path, tag="6-implement")
    rule = "Read(//" + secrets.absolute().as_posix().replace(":", "", 1).lstrip("/")
    rule = rule[:7] + rule[7].lower() + rule[8:] if rule[8] == "/" else rule
    for kind in ("plan", "implement"):
        (log,) = runs.glob(f"*-6-{kind}-*.json")
        args = json.loads(log.read_text(encoding="utf-8"))["args"]
        assert any(a.startswith("Read(//") and a.endswith("/home/secrets.env)") for a in args)
        assert "--disallowedTools" in args
        assert not any("git diff" in a for a in args)   # it prints files outside the repo


def test_a_token_in_the_change_is_never_pushed(world):
    w = world
    token = "ghp_" + "Zx9" * 12

    def leaky(prompt, cwd):
        write(cwd, "hello.txt", "hello\n")
        write(cwd, "settings.ini", f"token = {token}\n")
        return done()
    w.claude.on("implement", leaky)
    w.claude.on("gates", lambda p, cwd: ((Path(cwd) / "settings.ini").unlink(), done())[1])
    n = w.gh.new_issue("Add a hello file")
    w.sup.tick()
    w.sup.tick()
    fix = [c for c in w.claude.calls if c["kind"] == "gates"][0]
    assert "the token check" in fix["prompt"] and "settings.ini: token = [redacted]" in fix["prompt"]
    assert token not in fix["prompt"]
    t = w.task(n)
    assert t.stage == "pr"
    pushed = sh("git", "log", "-p", t.branch, cwd=w.origin)
    assert token not in pushed and "hello" in pushed
