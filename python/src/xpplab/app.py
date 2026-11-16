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
    """The app could not be reached, or refused or failed a request.

    Attributes:
        log (str): for a failed command, what the app's command window
            showed.

    Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

        >>> app = xs.app.connect()
        >>> try:
        ...     app.command("auto from 99 periodic")
        ... except xs.app.AppError as e:
        ...     print(e)
        ...     print(e.log)
        error: no point labelled 99
        >> auto from 99 periodic
        error: no point labelled 99
        <BLANKLINE>

    See also:
        [serve](../reference/serve.md) in the command window.
    """

    def __init__(self, message: str, log: str = ""):
        super().__init__(message)
        self.log = log


def channel_file() -> Path:
    env = os.environ.get("XPPLAB_CHANNEL")
    return Path(env) if env else Path.home() / ".xpplab" / "channel.json"


def connect(timeout: float = 30.0, quiet: bool = False) -> "App":
    """Connect to the XPP Lab app running on this computer.

    The app listens on this computer only, and writes its port and a token
    to ``~/.xpplab/channel.json`` (or the file ``$XPPLAB_CHANNEL`` names),
    which only its user can read. Start the app first.

    Args:
        timeout: how long to wait for each answer, in seconds.
        quiet: the app's command window does not repeat the commands it is
            sent; for its own Python console, whose lines are there already.

    Returns:
        An [`App`][xpplab.app.App].

    Raises:
        AppError: the app is not running, or does not answer.

    Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

        >>> app = xs.app.connect()
        >>> app
        <XPP Lab ...: lecar.ode>
        >>> app.close()

    See also:
        [serve](../reference/serve.md) in the command window; the guide
        [Driving the desktop app](app.md).
    """
    path = channel_file()
    try:
        info = json.loads(path.read_text())
    except (OSError, ValueError):
        raise AppError("XPP Lab is not running, or does not let Python connect "
                       "(no %s): start the app first" % path) from None
    return App(int(info["port"]), str(info["token"]), timeout, quiet)


class App:
    """A connection to the running desktop app.
    [`connect`][xpplab.app.connect] makes one; close it with
    [`close`][xpplab.app.App.close], or use it in a ``with`` block.

    Each action goes through the app's command window, so the app's plots,
    Workspace and session script show what Python did.

    Attributes:
        version (str): the app's version.
        model_path (str): the model the app has open.
        last_python (str): the Python the app wrote for the last command.

    Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

        >>> with xs.app.connect() as app:
        ...     sol = app.run(iapp=0.1, total=50)
        >>> sol
        <Trajectory: 1001 points, t=0..50, columns T, V, W, CUR, ICA, IK, RATE>

    See also:
        [serve](../reference/serve.md) in the command window.
    """

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
        """Close the connection. The app keeps running.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.close()

        See also:
            [`connect`][xpplab.app.connect].
        """
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
        """Run a command in the app's command window, as if typed there.

        Args:
            text: the command: ``run``, ``set iapp=0.2``,
                ``auto steady iapp``, ``range iapp 0 0.3 20 V``, ... (see the
                [command reference](../reference/reference.md)).

        Returns:
            What the command window showed.

        Raises:
            AppError: the command failed; its ``log`` holds what the window
                showed.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> print(app.command("get iapp"))
            >> get iapp
            iapp = 0.05
            <BLANKLINE>

        See also:
            the [command reference](../reference/reference.md).
        """
        reply = self._request("command", text)
        if not reply.get("ok", False):
            log = reply.get("log", "")
            errors = [l for l in log.splitlines() if l.startswith("error")]
            raise AppError(errors[0] if errors else "the command failed", log)
        self.last_python = reply.get("python", "")
        return reply.get("log", "")

    def set(self, **values) -> "App":
        """Set parameters, initial conditions or options in the app, as the
        command ``set`` does.

        Args:
            **values: names and values.

        Returns:
            The connection itself, so that calls chain.

        Raises:
            AppError: the app refused a name or a value.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.set(iapp=0.1, total=50).get("iapp")
            0.1

        See also:
            [set](../reference/set.md) in the command window.
        """
        if values:
            self.command("set " + " ".join("%s=%s" % (k, _num(v)) for k, v in values.items()))
        return self

    def get(self, name: str) -> float:
        """The value of a parameter, an initial condition or an option of
        the app's model.

        Args:
            name: the name.

        Returns:
            The value.

        Raises:
            AppError: nothing has that name.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.get("iapp"), app.get("total")
            (0.05, 30.0)

        See also:
            [get](../reference/get.md) in the command window.
        """
        return float(self._request("get", name)["value"])

    def run(self, **values) -> Trajectory:
        """Run the model in the app, whose plots show the run, and return
        it.

        Args:
            **values: set first, as by [`set`][xpplab.app.App.set], and
                kept.

        Returns:
            The run, as a [`Trajectory`][xpplab.Trajectory], every number
            as the app has it.

        Raises:
            AppError: the app refused a value, or the run failed.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.run(iapp=0.1, total=50)
            <Trajectory: 1001 points, t=0..50, columns T, V, W, CUR, ICA, IK, RATE>
            >>> app.last_python
            'sol = m.run()\\n'

        See also:
            [run](../reference/run.md) in the command window.
        """
        self.set(**values)
        self.command("run")
        return self.last_run()

    def last_run(self) -> Trajectory:
        """The run the app shows, every number exact.

        Returns:
            The run, as a [`Trajectory`][xpplab.Trajectory].

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> _ = app.command("run")
            >>> app.last_run()
            <Trajectory: 601 points, t=0..30, columns T, V, W, CUR, ICA, IK, RATE>

        See also:
            [`run`][xpplab.app.App.run].
        """
        reply = self._request("run")
        return Trajectory({name: np.asarray(col, dtype=float)
                           for name, col in zip(reply["columns"], reply["data"])})

    def open(self, path) -> "App":
        """Open a model file in the app.

        Args:
            path: the .ode file.

        Returns:
            The connection itself, so that calls chain.

        Raises:
            AppError: the app could not open the file.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.open(xs.example("fhn.ode"))
            <XPP Lab ...: fhn.ode>

        See also:
            [open](../reference/open.md) in the command window.
        """
        self.command("open " + str(Path(path).resolve()))
        self.model_path = self._request("hello").get("model", "")
        return self

    def model(self) -> "AppModel":
        """The app's model, with the methods of a [`Model`][xpplab.Model].

        Setting values and running go through the app, whose plots follow,
        so the Python the app writes for each action runs unchanged against
        it: ``m.parameters["iapp"] = 0.1``, ``sol = m.run()``.

        Returns:
            An [`AppModel`][xpplab.app.AppModel].

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.model()
            <Model lecar.ode in XPP Lab>

        See also:
            [`AppModel`][xpplab.app.AppModel].
        """
        return AppModel(self)

    @property
    def parameters(self) -> dict:
        """The app's parameters and their values, as a snapshot.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.parameters["iapp"], len(app.parameters)
            (0.05, 12)

        See also:
            [whos](../reference/whos.md) in the command window.
        """
        r = self._request("values")
        return dict(zip(r["parameters"], r["parameter_values"]))

    @property
    def initial_conditions(self) -> dict:
        """The app's initial conditions, as a snapshot. ``app.ics`` is the
        same.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.initial_conditions
            {'V': -0.144, 'W': 0.03}

        See also:
            [ic](../reference/ic.md) in the command window.
        """
        r = self._request("values")
        return dict(zip(r["variables"], r["initial_values"]))

    ics = initial_conditions

    @property
    def variables(self) -> list:
        """The variables of the app's model.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.variables
            ['V', 'W']

        See also:
            [`columns`][xpplab.app.App.columns].
        """
        return list(self._request("names")["variables"])

    @property
    def columns(self) -> list:
        """The output columns of the app's model: ``T``, the variables and
        the auxiliary quantities.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> app = xs.app.connect()
            >>> app.columns
            ['T', 'V', 'W', 'CUR', 'ICA', 'IK', 'RATE']

        See also:
            [`variables`][xpplab.app.App.variables].
        """
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
    """The model open in the app, with the methods of a
    [`Model`][xpplab.Model]. [`App.model`][xpplab.app.App.model] returns
    one.

    ``parameters``, ``initial_conditions``, ``set``, ``get``, ``run`` and
    ``last_run`` act in the app. Any other method of a Model
    (``equilibrium``, ``sweep``, ``nullclines``, ``ensemble``, ``auto``,
    ...) computes with a private copy of the model at the app's current
    values (see [`local`][xpplab.app.AppModel.local]): its results come
    back, but the app's plots do not show them. ``command`` runs the app's
    own version where there is one: ``m.command("eq")``.

    Attributes:
        app (App): the connection.
        parameters: the app's parameters, as a live dictionary.
        initial_conditions: the app's initial conditions, as a live
            dictionary; ``ics`` is the same.

    Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

        >>> m = xs.app.connect().model()
        >>> m.parameters["iapp"] = 0.2
        >>> m.run(total=20)
        <Trajectory: 401 points, t=0..20, columns T, V, W, CUR, ICA, IK, RATE>
        >>> m.equilibrium()
        <Equilibrium unstable focus: V=0.0687252, W=0.393797>

    See also:
        [serve](../reference/serve.md) in the command window.
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
        """Set values in the app, as [`App.set`][xpplab.app.App.set] does.

        Args:
            **values: names and values.

        Returns:
            The model itself, so that calls chain.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> m.set(iapp=0.1).get("iapp")
            0.1

        See also:
            [set](../reference/set.md) in the command window.
        """
        self.app.set(**values)
        return self

    def get(self, name: str):
        """A value of the app's model, as [`App.get`][xpplab.app.App.get]
        gives it.

        Args:
            name: a parameter, a variable or an option.

        Returns:
            The value.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> m.get("phi")
            0.333

        See also:
            [get](../reference/get.md) in the command window.
        """
        return self.app.get(name)

    def run(self, **values) -> Trajectory:
        """Run the model in the app, as [`App.run`][xpplab.app.App.run]
        does.

        Args:
            **values: set first, and kept.

        Returns:
            The run, as a [`Trajectory`][xpplab.Trajectory].

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> m.run(total=20)
            <Trajectory: 401 points, t=0..20, columns T, V, W, CUR, ICA, IK, RATE>

        See also:
            [run](../reference/run.md) in the command window.
        """
        return self.app.run(**values)

    def last_run(self) -> Trajectory:
        """The run the app shows, as
        [`App.last_run`][xpplab.app.App.last_run] gives it.

        Returns:
            The run, as a [`Trajectory`][xpplab.Trajectory].

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> sol = m.run(total=20)
            >>> len(m.last_run())
            401

        See also:
            [`run`][xpplab.app.AppModel.run].
        """
        return self.app.last_run()

    def command(self, text: str) -> str:
        """Run a command in the app's command window, as
        [`App.command`][xpplab.app.App.command] does.

        Args:
            text: the command.

        Returns:
            What the command window showed.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> print(m.command("get phi"))
            >> get phi
            phi = 0.333
            <BLANKLINE>

        See also:
            the [command reference](../reference/reference.md).
        """
        return self.app.command(text)

    @property
    def variables(self) -> list:
        """The variables of the app's model.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> m.variables
            ['V', 'W']

        See also:
            [`columns`][xpplab.app.AppModel.columns].
        """
        return self.app.variables

    @property
    def columns(self) -> list:
        """The output columns of the app's model.

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> m.columns
            ['T', 'V', 'W', 'CUR', 'ICA', 'IK', 'RATE']

        See also:
            [`variables`][xpplab.app.AppModel.variables].
        """
        return self.app.columns

    def local(self):
        """A private [`Model`][xpplab.Model] of the same file, at the app's
        current parameters, initial conditions and integration options.

        The methods of a Model that the app's model does not have itself
        are run on it. The same private copy is reused while the app keeps
        the same file open.

        Returns:
            The private [`Model`][xpplab.Model].

        Examples:
        The examples need the desktop app running with ``lecar.ode`` open.

            >>> m = xs.app.connect().model()
            >>> m.set(iapp=0.2).local()
            <Model lecar.ode: 2 variables (V, W), 12 parameters>
            >>> m.local().get("iapp")
            0.2

        See also:
            [`App.model`][xpplab.app.App.model].
        """
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
