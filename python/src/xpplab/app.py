"""Drive the running desktop app from Python: a notebook, a script or a
coding agent.

    import xpplab as xs
    app = xs.app.connect()          # the XPP Lab window on this computer
    app.set(iapp=0.2)
    sol = app.run()                 # the app runs; its plots follow
    app.command("auto steady iapp") # anything the command window takes

The app listens on this computer only and writes its port and a token to
``~/.xpplab/channel.json`` (or the file ``$XPPLAB_CHANNEL`` names), which
only its user can read. Each action goes through the app's command window,
so the app's plots, Workspace and session script show what Python did.
"""
from __future__ import annotations

import json
import os
import socket
from pathlib import Path

import numpy as np

from .model import Trajectory

__all__ = ["connect", "App", "AppError"]


class AppError(RuntimeError):
    """The app could not be reached, or refused or failed a request. For a
    failed command, ``log`` holds what the app's command window showed."""

    def __init__(self, message: str, log: str = ""):
        super().__init__(message)
        self.log = log


def channel_file() -> Path:
    env = os.environ.get("XPPLAB_CHANNEL")
    return Path(env) if env else Path.home() / ".xpplab" / "channel.json"


def connect(timeout: float = 30.0) -> "App":
    """Attach to the running XPP Lab app (start it first)."""
    path = channel_file()
    try:
        info = json.loads(path.read_text())
    except (OSError, ValueError):
        raise AppError("XPP Lab is not running, or does not let Python connect "
                       "(no %s): start the app first" % path) from None
    return App(int(info["port"]), str(info["token"]), timeout)


class App:
    """A connection to the running app. Close it with :meth:`close`, or use
    it in a ``with`` block."""

    def __init__(self, port: int, token: str, timeout: float = 30.0):
        self._token = token
        try:
            self._sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        except OSError as e:
            raise AppError("cannot reach XPP Lab on port %d (%s): is it still "
                           "running?" % (port, e)) from None
        self._buf = b""
        hello = self._request("hello")
        self.version = hello.get("version", "")
        self.model_path = hello.get("model", "")

    # -- the wire -------------------------------------------------------------

    def _request(self, op: str, text: str = "") -> dict:
        msg = json.dumps({"token": self._token, "op": op, "text": text}) + "\n"
        try:
            self._sock.sendall(msg.encode("utf-8"))
            while b"\n" not in self._buf:
                chunk = self._sock.recv(1 << 16)
                if not chunk:
                    raise AppError("XPP Lab closed the connection")
                self._buf += chunk
        except OSError as e:
            raise AppError("lost XPP Lab: %s" % e) from None
        line, self._buf = self._buf.split(b"\n", 1)
        reply = json.loads(line)
        if not reply.get("ok", False) and "error" in reply:
            raise AppError(reply["error"])
        return reply

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass

    def __enter__(self) -> "App":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def __repr__(self) -> str:
        return "<XPP Lab %s: %s>" % (self.version, Path(self.model_path).name or "no model")

    # -- what the app does ------------------------------------------------------

    def command(self, text: str) -> str:
        """Run a command-window command (``run``, ``set iapp=0.2``, ``auto
        steady iapp``, ``range iapp 0 0.3 20 V``, ...) and return what the
        command window showed. A command that fails raises AppError."""
        reply = self._request("command", text)
        if not reply.get("ok", False):
            log = reply.get("log", "")
            errors = [l for l in log.splitlines() if l.startswith("error")]
            raise AppError(errors[0] if errors else "the command failed", log)
        self.last_python = reply.get("python", "")
        return reply.get("log", "")

    def set(self, **values) -> "App":
        """Set parameters, initial values or options, as ``set`` does."""
        if values:
            self.command("set " + " ".join("%s=%s" % (k, _num(v)) for k, v in values.items()))
        return self

    def get(self, name: str) -> float:
        """A parameter, initial value or option of the app's model."""
        return float(self._request("get", name)["value"])

    def run(self, **values) -> Trajectory:
        """Set values, run the model in the app and return the run."""
        self.set(**values)
        self.command("run")
        return self.last_run()

    def last_run(self) -> Trajectory:
        """The run the app shows, every number exact."""
        reply = self._request("run")
        return Trajectory({name: np.asarray(col, dtype=float)
                           for name, col in zip(reply["columns"], reply["data"])})

    def open(self, path) -> "App":
        """Open a model file in the app."""
        self.command("open " + str(Path(path).resolve()))
        self.model_path = self._request("hello").get("model", "")
        return self

    @property
    def parameters(self) -> dict:
        """The app's parameters and their values (a snapshot)."""
        r = self._request("values")
        return dict(zip(r["parameters"], r["parameter_values"]))

    @property
    def initial_conditions(self) -> dict:
        """The app's initial values (a snapshot)."""
        r = self._request("values")
        return dict(zip(r["variables"], r["initial_values"]))

    ics = initial_conditions

    @property
    def variables(self) -> list:
        return list(self._request("names")["variables"])

    @property
    def columns(self) -> list:
        return list(self._request("names")["columns"])


def _num(v) -> str:
    if isinstance(v, str):
        return v
    return repr(float(v))
