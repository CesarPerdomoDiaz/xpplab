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


def connect(timeout: float = 30.0, quiet: bool = False) -> "App":
    """Attach to the running XPP Lab app (start it first). With ``quiet``
    the app's command window does not repeat the commands it is sent (the
    console's own Python is already there)."""
    path = channel_file()
    try:
        info = json.loads(path.read_text())
    except (OSError, ValueError):
        raise AppError("XPP Lab is not running, or does not let Python connect "
                       "(no %s): start the app first" % path) from None
    return App(int(info["port"]), str(info["token"]), timeout, quiet)


class App:
    """A connection to the running app. Close it with :meth:`close`, or use
    it in a ``with`` block."""

    def __init__(self, port: int, token: str, timeout: float = 30.0,
                 quiet: bool = False):
        self._token = token
        self._quiet = quiet
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
        req = {"token": self._token, "op": op, "text": text}
        if self._quiet:
            req["quiet"] = "1"
        msg = json.dumps(req) + "\n"
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

    def model(self) -> "AppModel":
        """The app's model as a Model: setting values and running go through
        the app, whose plots follow, so the Python the app writes for each
        action runs unchanged (``m.parameters["iapp"] = 0.1``, ``sol =
        m.run()``)."""
        return AppModel(self)

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


class _AppValues:
    """The app's parameters or initial values as a live mapping."""

    def __init__(self, app: App, kind: str):
        self._app, self._kind = app, kind

    def _snapshot(self) -> dict:
        return (self._app.parameters if self._kind == "parameters"
                else self._app.initial_conditions)

    def __getitem__(self, name: str) -> float:
        for k, v in self._snapshot().items():
            if k.lower() == name.lower():
                return v
        raise KeyError(f"no {self._kind[:-1] if self._kind == 'parameters' else 'variable'} {name!r}")

    def __setitem__(self, name: str, value) -> None:
        self[name]  # a name the model has, or KeyError
        self._app.set(**{name: value})

    def update(self, values: dict) -> None:
        for k in values:
            self[k]
        self._app.set(**values)

    def __iter__(self):
        return iter(self._snapshot())

    def __len__(self) -> int:
        return len(self._snapshot())

    def __contains__(self, name) -> bool:
        return any(k.lower() == str(name).lower() for k in self._snapshot())

    def keys(self):
        return self._snapshot().keys()

    def items(self):
        return self._snapshot().items()

    def values(self):
        return self._snapshot().values()

    def __repr__(self) -> str:
        return repr(self._snapshot())


class AppModel:
    """The model open in the app, with the Model API (see App.model()).

    ``parameters``, ``initial_conditions``, ``set``, ``get``, ``run`` and
    ``last_run`` act in the app. Any other Model method (``equilibrium``,
    ``sweep``, ``nullclines``, ``ensemble``, ``auto``, ...) computes with a
    private copy of the model at the app's current values: its results come
    back, but the app's plots do not show them; ``command()`` runs the
    app's own version where there is one (``m.command("eq")``).
    """

    def __init__(self, app: App):
        self.app = app
        self.parameters = _AppValues(app, "parameters")
        self.initial_conditions = _AppValues(app, "initial_conditions")
        self.ics = self.initial_conditions
        self._local = None
        self._local_path = None

    def __repr__(self) -> str:
        return "<Model %s in XPP Lab>" % Path(self.app.model_path).name

    def set(self, **values) -> "AppModel":
        self.app.set(**values)
        return self

    def get(self, name: str):
        return self.app.get(name)

    def run(self, **values) -> Trajectory:
        return self.app.run(**values)

    def last_run(self) -> Trajectory:
        return self.app.last_run()

    def command(self, text: str) -> str:
        return self.app.command(text)

    @property
    def variables(self) -> list:
        return self.app.variables

    @property
    def columns(self) -> list:
        return self.app.columns

    def local(self):
        """A private Model of the same file at the app's current values."""
        from .model import load
        path = self.app.model_path
        if self._local is None or self._local_path != path:
            if self._local is not None:
                self._local.close()
            self._local, self._local_path = load(path), path
        r = self.app._request("values")
        self._local.set(**dict(zip(r["parameters"], r["parameter_values"])),
                        **dict(zip(r["variables"], r["initial_values"])))
        for name in ("TOTAL", "DT", "T0", "TRANS", "BOUND", "NOUT"):
            try:
                self._local.set(**{name: self.app.get(name)})
            except AppError:
                pass
        return self._local

    def __getattr__(self, name: str):
        if name.startswith("_"):
            raise AttributeError(name)
        return getattr(self.local(), name)


def _num(v) -> str:
    if isinstance(v, str):
        return v
    return repr(float(v))
