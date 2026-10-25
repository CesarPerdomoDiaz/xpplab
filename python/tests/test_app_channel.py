"""xpplab.app and the agent tools that use it, against a stand-in for the
desktop app's channel (tests/channel_check.py drives the real app)."""
import json
import socket
import threading

import numpy as np
import pytest

import xpplab as xs
from xpplab.mcp import Session, ToolError

TOKEN = "0123456789abcdef0123456789abcdef"


class FakeApp:
    """Answers the channel's operations as the app would, and records the
    commands it was given."""

    def __init__(self):
        self.commands = []
        self.values = {"iapp": 0.05, "phi": 0.333}
        self.sock = socket.socket()
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(4)
        self.port = self.sock.getsockname()[1]
        threading.Thread(target=self._serve, daemon=True).start()

    def _serve(self):
        while True:
            try:
                conn, _ = self.sock.accept()
            except OSError:
                return
            threading.Thread(target=self._client, args=(conn,), daemon=True).start()

    def _client(self, conn):
        f = conn.makefile("rwb")
        for line in f:
            req = json.loads(line)
            f.write((json.dumps(self._answer(req)) + "\n").encode())
            f.flush()
        conn.close()

    def _answer(self, req):
        if req["token"] != TOKEN:
            return {"ok": False, "error": "wrong token"}
        op, text = req["op"], req.get("text", "")
        if op == "hello":
            return {"ok": True, "app": "XPP Lab", "version": "test", "model": "/m/lecar.ode"}
        if op == "command":
            self.commands.append(text)
            if text.startswith("auto from 99"):
                return {"ok": False, "log": "error: no point labelled 99\n", "python": ""}
            if text.startswith("set "):
                for item in text[4:].split():
                    k, v = item.split("=")
                    self.values[k] = float(v)
            return {"ok": True, "log": "", "python": "sol = m.run()\n" if text == "run" else ""}
        if op == "get":
            return {"ok": True, "value": self.values[text]}
        if op in ("values", "names"):
            return {"ok": True, "variables": ["V", "W"], "parameters": list(self.values),
                    "columns": ["T", "V", "W"], "initial_values": [-0.144, 0.03],
                    "parameter_values": list(self.values.values())}
        if op == "run":
            t = [0.0, 0.5, 1.0]
            return {"ok": True, "columns": ["T", "V", "W"],
                    "data": [t, [-0.144, -0.2, -0.25], [0.03, 0.02, 0.0100000001]]}
        return {"ok": False, "error": "unknown operation"}


@pytest.fixture
def fake(tmp_path, monkeypatch):
    app = FakeApp()
    chan = tmp_path / "channel.json"
    chan.write_text(json.dumps({"port": app.port, "token": TOKEN, "pid": 1}))
    monkeypatch.setenv("XPPLAB_CHANNEL", str(chan))
    yield app
    app.sock.close()


def test_connect_set_run(fake):
    with xs.app.connect() as app:
        assert app.version == "test" and "lecar.ode" in repr(app)
        assert app.parameters == {"iapp": 0.05, "phi": 0.333}
        assert app.initial_conditions == {"V": -0.144, "W": 0.03}
        sol = app.run(iapp=0.2, total=50)
        assert fake.commands == ["set iapp=0.2 total=50.0", "run"]
        assert app.get("iapp") == 0.2 and app.last_python == "sol = m.run()\n"
        assert sol.columns == ["T", "V", "W"]
        assert np.array_equal(sol["W"], [0.03, 0.02, 0.0100000001])
        with pytest.raises(xs.app.AppError) as err:
            app.command("auto from 99 periodic")
        assert "no point labelled 99" in str(err.value) and "error" in err.value.log


def test_wrong_token_and_no_app(fake, tmp_path, monkeypatch):
    chan = tmp_path / "other.json"
    chan.write_text(json.dumps({"port": fake.port, "token": "nope"}))
    monkeypatch.setenv("XPPLAB_CHANNEL", str(chan))
    with pytest.raises(xs.app.AppError, match="wrong token"):
        xs.app.connect()
    monkeypatch.setenv("XPPLAB_CHANNEL", str(tmp_path / "missing.json"))
    with pytest.raises(xs.app.AppError, match="start the app"):
        xs.app.connect()


def test_agent_tools_act_in_the_app(fake):
    s = Session()
    status = s.app_status()
    assert status["model"] == "/m/lecar.ode" and status["parameters"]["iapp"] == 0.05
    out = s.app_command("nullclines")
    assert fake.commands[-1] == "nullclines" and "log" in out
    run = s.app_run({"iapp": 0.1})
    assert run["points"] == 3 and run["columns"]["V"]["final"] == -0.25
    assert fake.commands[-2:] == ["set iapp=0.1", "run"]
    with pytest.raises(ToolError, match="no point labelled 99"):
        s.app_command("auto from 99 periodic")


def test_agent_tools_say_when_no_app(tmp_path, monkeypatch):
    monkeypatch.setenv("XPPLAB_CHANNEL", str(tmp_path / "missing.json"))
    with pytest.raises(ToolError, match="XPP Lab window"):
        Session().app_status()
