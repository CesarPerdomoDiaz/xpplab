"""The tools of the XPP Lab MCP server, as plain Python.

Every public method of :class:`Session` is one tool. It takes JSON-ready
arguments and returns a JSON-ready dict: small summaries inline, big arrays
in CSV files in the work folder, and a ``python`` entry with the code that
reproduces the result with the ``xpplab`` package. Nothing here imports
the MCP SDK, so the tools can be used and tested without it.

State. A model is named by a path, an example name or its text. Each call
starts from the model file's values plus the overrides it passes; the
values are restored afterwards. The one exception is AUTO's bifurcation
diagram, which grows with each continuation until the file changes or the
caller resets it. A continuation from a labelled point reuses the overrides
and AUTO settings of the call that computed that point.
"""

from __future__ import annotations

import csv
import hashlib
import keyword
import math
import os
import re
import tempfile
import threading
from collections import OrderedDict
from contextlib import contextmanager
from dataclasses import dataclass, field
from functools import wraps
from pathlib import Path

import numpy as np

from .. import _engine as E
from ..analysis import _period
from ..auto import _ACTIONS, SETTINGS, TYPE_NAMES
from ..model import METHODS, Model
from .diagnose import diagnose, lint

__all__ = ["Session", "ToolError", "examples_dir", "doc_path"]

# The settings most continuations need, reported by describe_model.
AUTO_MAIN = ["ds", "dsmin", "dsmax", "parmin", "parmax", "nmax", "npr", "ntst",
             "normmax"]
MAX_MODELS = 8        # models kept loaded at once
MAX_POINTS = 2000     # largest downsampled array returned inline
MAX_SHOWN = 12        # variables shown per point before "..."


class ToolError(Exception):
    """A failure the caller can fix; the message says how."""


class _LoadError(ToolError):
    """A model that does not load, with the diagnosis."""

    def __init__(self, message: str, report: dict, tail: list[str]):
        super().__init__(message)
        self.report = report
        self.tail = tail


# -- where things are ---------------------------------------------------------

def _package_dir() -> Path:
    return Path(__file__).resolve().parents[1]


def _repo_dir() -> Path:
    return _package_dir().parents[2]


def examples_dir() -> Path | None:
    """The folder of example models: $XPPLAB_EXAMPLES, the copy inside
    the installed package, or engine/xppaut/ode in a source checkout."""
    env = os.environ.get("XPPLAB_EXAMPLES")
    cands = [Path(env)] if env else []
    cands += [_package_dir() / "examples", _repo_dir() / "engine" / "xppaut" / "ode"]
    for c in cands:
        if c.is_dir() and any(c.glob("*.ode")):
            return c
    return None


def doc_path(name: str) -> Path | None:
    """A page of the manual (``odefile.md``, ``auto.md``, ...), from the
    installed package or the source checkout's docs/manual."""
    for c in (_package_dir() / "_docs" / name, _repo_dir() / "docs" / "manual" / name):
        if c.is_file():
            return c
    return None


def describe_file(path: Path) -> str:
    """The leading comment of a model file: what it is, by convention."""
    out = []
    try:
        with open(path, encoding="latin-1") as f:
            for line in f:
                s = line.strip()
                if not s:
                    if out:
                        break
                    continue
                if not s.startswith("#"):
                    break
                s = s.lstrip("#").strip()
                if s:
                    out.append(s)
                if sum(len(x) for x in out) > 200:
                    break
    except OSError:
        return ""
    return " ".join(out)[:240]


# -- small helpers --------------------------------------------------------------

def _r(x, digits: int = 6):
    """A number for JSON: 6 significant digits; NaN and infinities as text."""
    if isinstance(x, (bool, np.bool_)):
        return bool(x)
    if isinstance(x, (int, np.integer)):
        return int(x)
    x = float(x)
    if not math.isfinite(x):
        return str(x)
    return float(f"{x:.{digits}g}")


def _rlist(a, digits: int = 6) -> list:
    return [_r(v, digits) for v in np.asarray(a, dtype=float).ravel()]


def _key(name: str) -> str:
    return name.strip().upper()


def _kwargs(d: dict) -> str:
    """Python keyword arguments for a dict, or ``**{...}`` when a name is
    not a valid identifier (an XPPAUT parameter may be called ``lambda``)."""
    if not d:
        return ""
    if all(k.isidentifier() and not keyword.iskeyword(k) for k in d):
        return ", ".join(f"{k}={v!r}" for k, v in d.items())
    return "**" + repr(dict(d))


def _guarded(fn):
    """Serialise tool calls (one engine state per model, and the engine
    changes the working directory) and turn library errors into ToolError."""
    @wraps(fn)
    def wrapper(self, *args, **kw):
        with self._lock:
            try:
                return fn(self, *args, **kw)
            except ToolError:
                raise
            except E.EngineError as err:
                raise ToolError(_engine_message(err)) from None
            except (ValueError, KeyError, LookupError) as err:
                msg = err.args[0] if err.args else str(err)
                raise ToolError(str(msg)) from None
    return wrapper


def _engine_message(err: E.EngineError) -> str:
    msg = super(E.EngineError, err).__str__()
    tail = [ln for ln in err.log.strip().splitlines() if ln.strip()][-6:]
    if tail:
        msg += "\nEngine output (last lines):\n" + "\n".join(tail)
    return msg


@dataclass
class _Step:
    """One AUTO computation: how to repeat it, and the points it made."""
    code: str
    start: bool
    values: dict
    parameter_set: str | None
    settings: dict
    first: int
    last: int


@dataclass
class _Entry:
    model: Model
    path: Path
    digest: str
    text: str
    origin: str
    defaults: dict
    options: dict
    auto_defaults: dict
    steps: list = field(default_factory=list)

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def load_code(self) -> str:
        return f"m = xs.load({self.path.as_posix()!r})"


class Session:
    """The state of one MCP connection: loaded models and the work folder."""

    def __init__(self, workdir: str | os.PathLike | None = None,
                 cwd: str | os.PathLike | None = None):
        self.cwd = Path(cwd or os.getcwd()).resolve()
        self._workdir = Path(workdir).resolve() if workdir else None
        self._models: OrderedDict[str, _Entry] = OrderedDict()
        self._lock = threading.RLock()

    # -- files ------------------------------------------------------------------

    @property
    def workdir(self) -> Path:
        """Where CSV files, figures and inline models go: --workdir,
        $XPPLAB_WORKDIR, ./xpplab-out, or a temporary folder when the
        current one is not writable."""
        if self._workdir is None:
            env = os.environ.get("XPPLAB_WORKDIR")
            if env:
                self._workdir = Path(env).resolve()
            elif os.access(self.cwd, os.W_OK):
                self._workdir = self.cwd / "xpplab-out"
            else:
                self._workdir = Path(tempfile.gettempdir()) / "xpplab-out"
        self._workdir.mkdir(parents=True, exist_ok=True)
        return self._workdir

    def _output(self, entry_or_name, kind: str, ext: str) -> Path:
        stem = (entry_or_name.path.stem if isinstance(entry_or_name, _Entry)
                else Path(str(entry_or_name)).stem)
        stem = re.sub(r"[^A-Za-z0-9_.-]", "_", stem) or "model"
        n = 1
        while True:
            p = self.workdir / f"{stem}-{kind}-{n}.{ext}"
            if not p.exists():
                return p
            n += 1

    def _write_csv(self, entry, kind: str, columns: dict) -> str:
        path = self._output(entry, kind, "csv")
        names = list(columns)
        arrays = [np.asarray(columns[n]) for n in names]
        rows = max((len(a) for a in arrays), default=0)
        with open(path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(names)
            for i in range(rows):
                w.writerow([_csv_cell(a[i]) if i < len(a) else "" for a in arrays])
        return str(path)

    # -- models -----------------------------------------------------------------

    def _resolve(self, model: str) -> tuple[Path, str]:
        if not isinstance(model, str) or not model.strip():
            raise ToolError("model is empty: pass a path to an .ode file, the "
                            "name of an example (see find_examples), or the "
                            "text of a model")
        if "\n" in model:
            text = model if model.endswith("\n") else model + "\n"
            digest = hashlib.sha1(text.encode("utf-8")).hexdigest()[:10]
            path = self.workdir / "models" / f"model-{digest}.ode"
            if not path.exists():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="latin-1", errors="replace")
            return path, "text"
        p = Path(model.strip()).expanduser()
        if not p.is_absolute():
            p = self.cwd / p
        if p.is_file():
            return p.resolve(), "file"
        ex = examples_dir()
        if ex is not None:
            for cand in (ex / model.strip(), ex / (model.strip() + ".ode")):
                if cand.is_file():
                    return cand.resolve(), "example"
        where = f"{self.cwd}" + (" or among the examples" if ex else "")
        raise ToolError(f"no model file {model!r} in {where}. Pass a path to "
                        f"an .ode file, the name of an example (find_examples "
                        f"lists them), or the text of a model.")

    def _load(self, path: Path) -> Model:
        try:
            return Model(path)
        except E.EngineError as err:
            text = path.read_text(encoding="latin-1")
            report = diagnose(text, super(E.EngineError, err).__str__(), err.log)
            tail = [ln for ln in err.log.strip().splitlines() if ln.strip()][-8:]
            where = f"line {report['line']}: " if report.get("line") else ""
            msg = (f"{path.name} does not load: {where}{report['message']}"
                   + (f" ({report['hint']})" if report.get("hint") else "")
                   + ". Call check_model for the details.")
            raise _LoadError(msg, report, tail) from None

    def _entry(self, model: str) -> tuple[_Entry, list[str]]:
        path, origin = self._resolve(model)
        data = path.read_bytes()
        digest = hashlib.sha1(data).hexdigest()
        key = os.path.normcase(str(path))
        notes = []
        old = self._models.get(key)
        if old is not None and old.digest == digest:
            self._models.move_to_end(key)
            return old, notes
        m = self._load(path)
        if old is not None:
            if old.steps:
                notes.append(f"{path.name} changed since the last call: it was "
                             f"reloaded and its AUTO diagram cleared")
            old.model.close()
            del self._models[key]
        entry = _Entry(
            model=m, path=path, digest=digest,
            text=data.decode("latin-1"), origin=origin,
            defaults={n: m.get(n) for n in list(m.parameters) + m.variables},
            options=m.options, auto_defaults=m.auto.settings)
        self._models[key] = entry
        while len(self._models) > MAX_MODELS:
            _, gone = self._models.popitem(last=False)
            gone.model.close()
        return entry, notes

    def close(self) -> None:
        with self._lock:
            for e in self._models.values():
                e.model.close()
            self._models.clear()

    # -- per-call values ----------------------------------------------------------

    def _names_hint(self, entry: _Entry) -> str:
        m = entry.model
        pars = ", ".join(list(m.parameters)[:40])
        return (f"parameters: {pars or 'none'}; variables: "
                f"{', '.join(m.variables[:40])}; options: total, dt, t0, trans, "
                f"meth, tol, atol, dtmin, dtmax, bound, nout, maxstor")

    def _apply(self, entry: _Entry, values: dict | None,
               parameter_set: str | None) -> dict:
        m = entry.model
        if parameter_set:
            sets = {s.lower(): s for s in m.sets}
            if parameter_set.lower() not in sets:
                raise ToolError(f"{entry.name} has no parameter set "
                                f"{parameter_set!r}; its sets: "
                                f"{', '.join(m.sets) or 'none'}")
            m.apply_set(sets[parameter_set.lower()])
        known = {_key(n) for n in entry.defaults}
        applied = {}
        for name, value in (values or {}).items():
            if isinstance(value, bool) or not isinstance(value, (int, float, str)):
                raise ToolError(f"the value of {name} must be a number "
                                f"(or a method name for meth), not {value!r}")
            # XPPAUT's own 'set' ignores names it does not know, so check
            # them here: parameters, variables, and the options xppe_get reads.
            if _key(name) not in known:
                try:
                    m.get(name)
                except E.EngineError:
                    hint = (f" {name} is an AUTO setting: pass it in settings "
                            f"(continuation)." if name.lower() in SETTINGS else "")
                    raise ToolError(f"{name} is not a parameter, variable or option "
                                    f"of {entry.name}.{hint} "
                                    f"{self._names_hint(entry)}") from None
            try:
                m.set(**{name: value})
            except ValueError as err:
                raise ToolError(str(err)) from None
            applied[name] = value
        return applied

    @contextmanager
    def _using(self, entry: _Entry, values: dict | None,
               parameter_set: str | None, settings: dict | None = None):
        """Apply a call's values (and AUTO settings), then restore the
        model's own, so that calls do not leak into each other."""
        m = entry.model
        names = list(entry.defaults)
        saved = {n: m.get(n) for n in names}
        saved_opts = m.options
        saved_auto = m.auto.settings
        try:
            self._apply(entry, values, parameter_set)
            if settings:
                bad = [k for k in settings if k.lower() not in SETTINGS]
                if bad:
                    raise ToolError(f"unknown AUTO setting {bad[0]!r}; the settings "
                                    f"are {', '.join(SETTINGS)}")
                m.auto.set(**{k.lower(): float(v) for k, v in settings.items()})
            yield
        finally:
            back = {n: v for n, v in saved.items() if m.get(n) != v}
            if back:
                m.set(**back)
            now = m.options
            back = {k: v for k, v in saved_opts.items() if now.get(k) != v}
            if back:
                m.set(**back)
            now = m.auto.settings
            back = {k: v for k, v in saved_auto.items() if now.get(k) != v}
            if back:
                m.auto.set(**back)

    def _snippet(self, entry: _Entry, values, parameter_set, *body: str) -> str:
        lines = ["import xpplab as xs", entry.load_code]
        if parameter_set:
            lines.append(f"m.apply_set({parameter_set!r})")
        if values:
            lines.append(f"m.set({_kwargs(values)})")
        lines += body
        return "\n".join(lines)

    # -- tools: models ------------------------------------------------------------

    @_guarded
    def check_model(self, model: str) -> dict:
        """Load a model and report parse errors with line numbers."""
        path, origin = self._resolve(model)
        text = path.read_text(encoding="latin-1")
        snippet = f"import xpplab as xs\nm = xs.load({path.as_posix()!r})"
        try:
            entry, notes = self._entry(str(path))
        except _LoadError as err:
            return {"ok": False, "model": path.name, "path": str(path),
                    "errors": [err.report], "engine_output": err.tail,
                    "warnings": lint(text),
                    "next": "fix the line and call check_model again; the "
                            "format is described in the resource "
                            "xpplab://docs/ode-format",
                    "python": snippet}
        m = entry.model
        warnings = lint(text)
        try:
            f = m.rhs()
            bad = [v for v, x in zip(m.variables, f) if not np.isfinite(x)]
            for v in bad[:5]:
                warnings.append(f"the right-hand side of {v} is not finite at the "
                                f"initial conditions: check divisions by zero, "
                                f"logs and powers of negative numbers")
        except E.EngineError:
            pass
        aux = [c for c in m.columns if c not in m.variables and c != "T"]
        out = {"ok": True, "model": entry.name, "path": str(path),
               "origin": origin,
               "variables": m.variables, "parameters": list(m.parameters),
               "auxiliary": aux, "parameter_sets": m.sets,
               "warnings": warnings, "python": snippet}
        if origin == "text":
            out["note"] = ("the text was saved to this path; pass the path as "
                           "model in later calls")
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def describe_model(self, model: str) -> dict:
        """Variables with initial values, parameters with defaults, options."""
        entry, notes = self._entry(model)
        m = entry.model
        var = {v: _r(entry.defaults[v]) for v in m.variables}
        par = {p: _r(entry.defaults[p]) for p in m.parameters}
        aux = [c for c in m.columns if c not in m.variables and c != "T"]
        opts = {k.lower(): (v if isinstance(v, str) else _r(v))
                for k, v in entry.options.items()
                if k in ("TOTAL", "DT", "T0", "TRANS", "METH", "TOL", "ATOL",
                         "BOUND", "NOUT", "MAXSTOR", "DELAY")}
        out = {"model": entry.name, "path": str(entry.path),
               "description": describe_file(entry.path),
               "initial_conditions": _cap(var), "parameters": _cap(par),
               "auxiliary": aux, "parameter_sets": m.sets, "options": opts,
               "auto_settings": {k: _r(entry.auto_defaults[k]) for k in AUTO_MAIN},
               "python": self._snippet(entry, None, None,
                                       "m.parameters, m.initial_conditions, m.options")}
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def find_examples(self, query: str = "", limit: int = 25) -> dict:
        """Example models whose name or leading comment matches the query."""
        ex = examples_dir()
        if ex is None:
            raise ToolError("no example folder found: set XPPLAB_EXAMPLES to "
                            "a folder of .ode files (XPPAUT's ode/ folder)")
        words = [w.lower() for w in query.split()]
        hits = []
        for p in sorted(ex.glob("*.ode"), key=lambda p: p.name.lower()):
            about = describe_file(p)
            hay = (p.name + " " + about).lower()
            if all(w in hay for w in words):
                hits.append({"name": p.name, "about": about})
        limit = max(1, int(limit))
        out = {"folder": str(ex), "matches": len(hits), "examples": hits[:limit]}
        if len(hits) > limit:
            out["more"] = f"{len(hits) - limit} more: narrow the query or raise limit"
        out["usage"] = "pass an example's name as model, e.g. model='lecar.ode'"
        return out

    # -- tools: simulation --------------------------------------------------------

    @_guarded
    # -- the running desktop app (xpplab.app) -------------------------------------

    def _app(self):
        from .. import app as xapp
        try:
            return xapp.connect(timeout=60)
        except xapp.AppError as err:
            raise ToolError(f"{err}. These tools act in the XPP Lab window the "
                            "researcher has open; the other tools need no app.") from None

    def app_status(self) -> dict:
        """Whether the desktop app is open, its model and current values."""
        with self._app() as app:
            return {"app": f"XPP Lab {app.version}", "model": app.model_path,
                    "parameters": {k: _r(v) for k, v in app.parameters.items()},
                    "initial_conditions": {k: _r(v) for k, v in
                                           app.initial_conditions.items()}}

    def app_command(self, command: str) -> dict:
        """A command-window command in the app; what it showed, its Python."""
        from .. import app as xapp
        with self._app() as app:
            try:
                log = app.command(command)
            except xapp.AppError as err:
                raise ToolError(f"{err}\n{err.log}".strip()) from None
            return {"log": log, "python": app.last_python}

    def app_run(self, values: dict | None = None) -> dict:
        """Set values in the app, run there, and summarise the run."""
        with self._app() as app:
            sol = app.run(**(values or {}))
            python = app.last_python
            variables = app.variables
        summary = {}
        for c in sol.columns:
            if c == "T":
                continue
            x = sol[c]
            fin = x[np.isfinite(x)]
            summary[c] = ({"min": _r(fin.min()), "max": _r(fin.max()),
                           "mean": _r(fin.mean()), "final": _r(x[-1])}
                          if len(fin) else {"final": _r(x[-1])})
        return {"points": len(sol), "t": [_r(sol.t[0]), _r(sol.t[-1])],
                "columns": summary,
                "behaviour": _behaviour(sol, variables, False),
                "python": python}

    def simulate(self, model: str, values: dict | None = None,
                 parameter_set: str | None = None,
                 columns: list[str] | None = None, points: int = 0) -> dict:
        """Integrate once; summary statistics, a CSV file, optional arrays."""
        entry, notes = self._entry(model)
        m = entry.model
        with self._using(entry, values, parameter_set):
            sol = m.run()
            total = float(m.get("TOTAL"))
            t0 = float(m.get("T0"))
            discrete = m.get("METH") == "discrete"
            log = m.log
        cols = [c for c in sol.columns if c != "T"]
        if columns:
            missing = [c for c in columns if c not in sol]
            if missing:
                raise ToolError(f"no column {missing[0]!r}; the columns are "
                                f"{', '.join(cols)}")
            cols = [next(k for k in sol.columns if _key(k) == _key(c)) for c in columns]
        if len(sol) == 0:
            raise ToolError("the run produced no data" + _log_hint(log))
        summary = {}
        for c in cols:
            x = sol[c]
            fin = x[np.isfinite(x)]
            summary[c] = ({"min": _r(fin.min()), "max": _r(fin.max()),
                           "mean": _r(fin.mean()), "final": _r(x[-1])}
                          if len(fin) else {"final": _r(x[-1])})
        out = {"model": entry.name, "points": len(sol),
               "t": [_r(sol.t[0]), _r(sol.t[-1])], "columns": summary}
        out["behaviour"] = _behaviour(sol, m.variables, discrete)
        warnings = _run_warnings(sol, t0, total, log)
        if warnings:
            out["warnings"] = warnings
        out["csv"] = self._write_csv(entry, "run", {k: sol[k] for k in sol.columns})
        if points:
            out["arrays"] = _downsample(sol, ["T"] + cols, points)
        out["python"] = self._snippet(entry, values, parameter_set,
                                      "sol = m.run()",
                                      "sol.t, sol[" + repr(cols[0] if cols else "T") + "]")
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def equilibria(self, model: str, values: dict | None = None,
                   parameter_set: str | None = None,
                   guesses: list[dict] | None = None) -> dict:
        """Newton's method from the initial conditions and from each guess."""
        entry, notes = self._entry(model)
        m = entry.model
        found, failures = [], []
        with self._using(entry, values, parameter_set):
            for g in [None] + list(guesses or []):
                if g is not None:
                    bad = [k for k in g if _key(k) not in map(_key, m.variables)]
                    if bad:
                        raise ToolError(f"guess names {bad[0]!r}, which is not a "
                                        f"variable; the variables are "
                                        f"{', '.join(m.variables)}")
                origin = "initial conditions" if g is None else {k: _r(v) for k, v in g.items()}
                try:
                    eq = m.equilibrium(g)
                except E.EngineError as err:
                    failures.append({"from": origin, "error": super(E.EngineError, err).__str__()})
                    continue
                vec = np.array(list(eq.values.values()))
                if not np.all(np.isfinite(vec)):
                    failures.append({"from": origin, "error": "Newton's method diverged"})
                    continue
                # Newton stops within NEWT_TOL, so the same equilibrium found
                # from two guesses differs in the last digits.
                if any(np.allclose(vec, f[0], rtol=1e-4, atol=1e-5) for f in found):
                    continue
                found.append((vec, eq, origin))
        eqs = [_equilibrium_json(eq, origin) for _, eq, origin in found]
        out = {"model": entry.name, "equilibria": eqs}
        if failures:
            out["failed"] = failures
        if not eqs:
            out["hint"] = ("Newton's method did not converge. Give guesses near "
                           "the equilibrium, e.g. the final state of a simulate "
                           "run that settles, or a crossing of the nullclines "
                           "(plot_phase_plane).")
        body = ["eq = m.equilibrium()"]
        for g in guesses or []:
            body.append(f"m.equilibrium({dict(g)!r})")
        body.append("eq.values, eq.eigenvalues, eq.kind")
        out["python"] = self._snippet(entry, values, parameter_set, *body)
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def sweep(self, model: str, parameter: str, start: float | None = None,
              stop: float | None = None, steps: int = 21,
              at: list[float] | None = None, values: dict | None = None,
              parameter_set: str | None = None,
              variables: list[str] | None = None, transient: float = 0.0,
              continue_runs: bool = False, lyapunov: bool = False) -> dict:
        """XPPAUT's Range: one run per value; max/min, period, Liapunov."""
        entry, notes = self._entry(model)
        m = entry.model
        if at is not None:
            grid = np.asarray(at, dtype=float)
        else:
            if start is None or stop is None:
                raise ToolError("give start and stop (and steps), or the values "
                                "themselves as at=[...]")
            steps = int(steps)
            if not 2 <= steps <= 2000:
                raise ToolError("steps must be between 2 and 2000")
            grid = np.linspace(float(start), float(stop), steps)
        names = {_key(n): n for n in list(m.parameters) + m.variables}
        if _key(parameter) not in names:
            raise ToolError(f"{parameter!r} is neither a parameter nor a variable; "
                            f"{self._names_hint(entry)}")
        parameter = names[_key(parameter)]
        var = [_match(v, m.variables, "variable") for v in (variables or m.variables[:3])]
        transient = float(transient)

        def measure(sol):
            keep = sol.t >= sol.t[0] + transient if len(sol) else []
            if not np.any(keep):
                raise ToolError(f"transient={transient} is longer than the run; "
                                f"raise total in values")
            t = sol.t[keep]
            row = []
            for v in var:
                x = sol[v][keep]
                row += [float(np.max(x)), float(np.min(x))]
            x = sol[var[0]][keep]
            try:
                row.append(_period(t, x))
            except E.EngineError:
                row.append(float("nan"))
            # Cycles measured, and the amplitude in each half of the window
            # (a damped oscillation shrinks from one half to the next).
            s = x - 0.5 * (x.max() + x.min())
            row.append(float(np.count_nonzero((s[:-1] < 0) & (s[1:] >= 0))))
            h = len(x) // 2
            row += [float(np.ptp(x[:h])) if h > 1 else 0.0,
                    float(np.ptp(x[h:])) if h > 1 else 0.0]
            if lyapunov:
                row.append(m.lyapunov())
            return row

        with self._using(entry, values, parameter_set):
            table = m.sweep(parameter, grid, measure=measure,
                            continue_runs=bool(continue_runs))
        cols = {parameter: grid}
        for i, v in enumerate(var):
            cols[f"{v}_max"] = table[:, 2 * i]
            cols[f"{v}_min"] = table[:, 2 * i + 1]
        k = 2 * len(var)
        cols["period"] = table[:, k]
        cols["cycles"] = table[:, k + 1]
        halves = table[:, k + 2:k + 4]
        if lyapunov:
            cols["lyapunov"] = table[:, k + 4]
        regimes = _regimes(grid, cols, halves, var[0], lyapunov)
        out = {"model": entry.name, "parameter": parameter, "runs": len(grid),
               "measured": var, "transient": transient, "regimes": regimes}
        kinds = {r["regime"] for r in regimes}
        advice = []
        if "oscillating, too few cycles" in kinds:
            advice.append("some runs hold fewer than 3 cycles after the transient, "
                          "too few to measure a period: raise total in values")
        if "damped" in kinds:
            advice.append("damped runs are still settling: a longer transient (and "
                          "total) shows whether they reach a steady state")
        if advice:
            out["advice"] = advice
        if len(grid) <= 30:
            out["table"] = {"columns": list(cols),
                            "rows": [[_r(cols[c][i]) for c in cols]
                                     for i in range(len(grid))]}
        out["csv"] = self._write_csv(entry, "sweep", cols)
        meas = (f"lambda s: (s[{var[0]!r}][s.t >= s.t[0] + {transient!r}].max(), "
                f"s[{var[0]!r}][s.t >= s.t[0] + {transient!r}].min())")
        out["python"] = self._snippet(
            entry, values, parameter_set, "import numpy as np",
            f"grid = np.array({_rlist(grid, 12)!r})" if at is not None
            else f"grid = np.linspace({float(start)!r}, {float(stop)!r}, {steps})",
            f"maxmin = m.sweep({parameter!r}, grid, measure={meas}"
            + (", continue_runs=True" if continue_runs else "") + ")"
            + ("\nlyap = m.sweep(" + repr(parameter) + ", grid, measure=lambda s: m.lyapunov()"
               + (", continue_runs=True" if continue_runs else "") + ")" if lyapunov else ""))
        if notes:
            out["notes"] = notes
        return out

    # -- tools: AUTO ------------------------------------------------------------------

    @_guarded
    def continuation(self, model: str, parameter: str | None = None,
                     from_label: int | None = None, action: str | None = None,
                     second_parameter: str | None = None,
                     values: dict | None = None, parameter_set: str | None = None,
                     settings: dict | None = None, reset: bool = False) -> dict:
        """AUTO: follow steady states in a parameter, or continue from a
        labelled point of the diagram (periodic orbits from a Hopf point,
        two-parameter curves, branch switching)."""
        entry, notes = self._entry(model)
        m = entry.model
        auto = m.auto
        if reset:
            auto.reset()
            entry.steps.clear()
        diagram_before = auto.diagram if from_label is not None else None
        settings = {k.lower(): v for k, v in (settings or {}).items()}

        if from_label is None:
            if not parameter:
                raise ToolError("give parameter= to follow steady states, or "
                                "from_label= to continue from a labelled point")
            parameter = _match(parameter, list(m.parameters), "parameter")
            eff_values = dict(values or {})
            eff_set = parameter_set
            eff_settings = dict(settings)
            code = f"m.auto.equilibria({parameter!r})"
            start = True
        else:
            try:
                point = diagram_before.label(int(from_label))
            except LookupError:
                labels = [p.label for p in diagram_before.labelled()]
                raise ToolError(
                    f"no point with label {from_label} in the diagram of "
                    f"{entry.name}" + (f"; its labels are {_compress(labels)}"
                                       if labels else
                                       ": the diagram is empty. Follow steady "
                                       "states first (continuation with "
                                       "parameter=...)")) from None
            parent = next((s for s in entry.steps
                           if s.first <= point.index < s.last), None)
            eff_values = dict(parent.values if parent else {})
            eff_values.update(values or {})
            eff_set = parameter_set or (parent.parameter_set if parent else None)
            eff_settings = dict(parent.settings if parent else {})
            eff_settings.update(settings)
            kind = point.type if point.type in _ACTIONS and point.type else (
                "periodic" if point.periodic else "steady")
            choices = list(_ACTIONS[kind])
            action = action or choices[0]
            if action not in choices:
                raise ToolError(f"label {from_label} is a "
                                f"{TYPE_NAMES.get(point.type, kind)}"
                                f"{' on a periodic branch' if point.periodic else ''}; "
                                f"from it you can: {', '.join(choices)}")
            if action == "two-parameter" and not second_parameter:
                raise ToolError(f"label {from_label} is a "
                                f"{TYPE_NAMES.get(point.type, kind)}; a "
                                f"two-parameter continuation from it needs "
                                f"second_parameter=<another parameter>")
            if second_parameter:
                second_parameter = _match(second_parameter, list(m.parameters),
                                          "parameter")
            elif point.two_parameter and point.parameter2:
                second_parameter = point.parameter2  # stay on the curve
            # Name the parameter explicitly: AUTO's current one may belong to
            # another branch computed since.
            parameter = (_match(parameter, list(m.parameters), "parameter")
                         if parameter else point.parameter)
            extra = f", parameter={parameter!r}"
            if second_parameter:
                extra += f", second={second_parameter!r}"
            code = f"m.auto.continue_from({int(from_label)}, {action!r}{extra})"
            start = False

        before = _auto_count(m)
        with self._using(entry, eff_values, eff_set, eff_settings):
            if start:
                pv = float(m.get(parameter))
                lo, hi = auto.get("PARMIN"), auto.get("PARMAX")
                if not lo <= pv <= hi:
                    raise ToolError(
                        f"{parameter} = {_r(pv)} lies outside AUTO's range "
                        f"parmin..parmax = {_r(lo)}..{_r(hi)}, so AUTO would stop "
                        f"at once. Pass settings={{'parmin': ..., 'parmax': ...}} "
                        f"around the range to explore, or set {parameter} inside "
                        f"it with values.")
            hint = (" The start is probably not an equilibrium: check that "
                    "equilibria converges for these values, or start from the "
                    "end of a simulate run that settles." if start else
                    " Try a smaller ds and dsmax in settings (for periodic orbits "
                    "a larger ntst, 50 or 100). To go further along a branch "
                    "from its end point, repeat the computation that made it "
                    "with a larger nmax or a wider parmin..parmax instead.")
            try:
                if start:
                    d = auto.equilibria(parameter)
                else:
                    d = auto.continue_from(point, action, parameter=parameter,
                                           second=second_parameter)
            except E.EngineError as err:
                raise ToolError(f"AUTO failed: {super(E.EngineError, err).__str__()}."
                                + hint + _log_hint(err.log)) from None
            used = {k: auto.get(k) for k in ("PARMIN", "PARMAX", "NMAX", "NORMMAX",
                                              "DS", "NTST")}
            log = m.log
        after = _auto_count(m)
        if after == before:
            raise ToolError("AUTO computed no points." + hint + _log_hint(log))
        step = _Step(code=code, start=start, values=eff_values,
                     parameter_set=eff_set, settings=eff_settings,
                     first=before, last=after)
        entry.steps.append(step)
        out = _branch_report(d, m.variables, used, start)
        out = {"model": entry.name, **out}
        out["csv"] = self._write_csv(entry, "continuation", _diagram_columns(d, m.variables))
        out["python"] = self._chain(entry)
        if notes:
            out["notes"] = notes
        return out

    def _chain(self, entry: _Entry, *tail: str) -> str:
        """The Python that repeats every AUTO step of a model in order."""
        lines = ["import xpplab as xs", entry.load_code]
        starts = sum(1 for s in entry.steps if s.start)
        if starts > 1:
            lines.append("defaults = {**m.parameters, **m.initial_conditions}")
        cur_vals: dict = {}
        cur_set = None
        cur_auto: dict = {}
        n_start = 0
        for k, s in enumerate(entry.steps, 1):
            if s.start:
                n_start += 1
                if n_start > 1:
                    lines.append("m.set(**defaults)")
                    cur_vals, cur_set = {}, None
            if s.parameter_set and s.parameter_set != cur_set:
                lines.append(f"m.apply_set({s.parameter_set!r})")
                cur_set = s.parameter_set
            diff = {n: v for n, v in s.values.items() if cur_vals.get(n) != v}
            for n in cur_vals:
                if n not in s.values:
                    diff[n] = _default_of(entry, n)
            if diff:
                lines.append(f"m.set({_kwargs(diff)})")
            cur_vals = dict(s.values)
            adiff = {n: v for n, v in s.settings.items() if cur_auto.get(n) != v}
            for n in cur_auto:
                if n not in s.settings:
                    adiff[n] = entry.auto_defaults[n]
            if adiff:
                lines.append(f"m.auto.set({_kwargs(adiff)})")
            cur_auto = dict(s.settings)
            lines.append(f"d{k} = {s.code}")
        lines += tail
        return "\n".join(lines)

    # -- tools: analysis -------------------------------------------------------------

    @_guarded
    def spectrum(self, model: str, column: str, values: dict | None = None,
                 parameter_set: str | None = None, column2: str | None = None,
                 kind: str = "psd", length: int | None = None,
                 window: str = "hamming") -> dict:
        """Power spectral density (or cross spectrum, coherence) of a run."""
        from ..analysis import SPECTRA, WINDOWS
        if kind.lower() not in SPECTRA:
            raise ToolError("kind must be 'psd', 'cross' or 'coherence'")
        if window.lower() not in WINDOWS:
            raise ToolError(f"window must be one of {', '.join(WINDOWS)}")
        entry, notes = self._entry(model)
        m = entry.model
        with self._using(entry, values, parameter_set):
            sol = m.run()
            column = _match(column, [c for c in sol.columns], "column")
            if column2:
                column2 = _match(column2, [c for c in sol.columns], "column")
            tab = m.spectrum(column, column2, kind=kind, length=length,
                             window=window)
        x = tab[tab.columns[0]]
        y = tab[tab.columns[1]]
        out = {"model": entry.name, "kind": kind, "column": column,
               "rows": len(tab), "columns": tab.columns,
               "resolution": _r(x[1] - x[0]) if len(x) > 1 else None,
               "peaks": _peaks(x, y)}
        out["csv"] = self._write_csv(entry, "spectrum", {c: tab[c] for c in tab.columns})
        args = repr(column) + (f", {column2!r}" if column2 else "")
        opts = "".join(f", {k}={v!r}" for k, v in (("kind", kind), ("length", length),
                                                   ("window", window))
                       if v is not None and not (k == "kind" and v == "psd")
                       and not (k == "window" and v == "hamming"))
        out["python"] = self._snippet(entry, values, parameter_set, "m.run()",
                                      f"tab = m.spectrum({args}{opts})")
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def lyapunov(self, model: str, values: dict | None = None,
                 parameter_set: str | None = None) -> dict:
        """The largest Liapunov exponent along one run."""
        entry, notes = self._entry(model)
        m = entry.model
        with self._using(entry, values, parameter_set):
            sol = m.run()
            z = m.lyapunov()
            total, t0, log = float(m.get("TOTAL")), float(m.get("T0")), m.log
        if z > 0.01:
            reading = ("positive: nearby trajectories separate, a sign of chaos; "
                       "confirm that it stays positive when total doubles")
        else:
            reading = ("zero or negative: no sign of chaos. A stable equilibrium "
                       "gives a clearly negative value; on a limit cycle the "
                       "exponent is 0, and XPPAUT's estimate comes out near "
                       "zero or somewhat below it (simulate tells the two apart)")
        out = {"model": entry.name, "lyapunov": _r(z), "reading": reading,
               "run": [_r(sol.t[0]), _r(sol.t[-1])],
               "advice": "use a long run that has settled; skip the transient "
                         "with trans in values"}
        warnings = _run_warnings(sol, t0, total, log)
        if warnings:
            out["warnings"] = warnings
        out["python"] = self._snippet(entry, values, parameter_set, "m.run()",
                                      "m.lyapunov()")
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def limit_cycle(self, model: str, values: dict | None = None,
                    parameter_set: str | None = None,
                    transient: float | None = None, variable: str | None = None,
                    period: float | None = None) -> dict:
        """One period of a stable limit cycle: its period, range and data."""
        entry, notes = self._entry(model)
        m = entry.model
        with self._using(entry, values, parameter_set):
            if variable:
                variable = _match(variable, m.variables, "variable")
            try:
                per, orbit = m.limit_cycle(transient=transient, variable=variable,
                                           period=period)
            except E.EngineError as err:
                raise ToolError(f"{super(E.EngineError, err).__str__()}. Check with "
                                f"simulate that the model oscillates for these "
                                f"values; a longer transient helps slow "
                                f"approaches.") from None
        summary = {v: {"min": _r(orbit[v].min()), "max": _r(orbit[v].max()),
                       "mean": _r(orbit[v].mean())} for v in m.variables[:MAX_SHOWN]}
        out = {"model": entry.name, "period": _r(per, 8),
               "frequency": _r(1.0 / per) if per else None,
               "cycle": summary, "points": len(orbit)}
        out["csv"] = self._write_csv(entry, "cycle", {k: orbit[k] for k in orbit.columns})
        args = _kwargs({k: v for k, v in (("transient", transient), ("variable", variable),
                                          ("period", period)) if v is not None})
        out["python"] = self._snippet(entry, values, parameter_set,
                                      f"period, orbit = m.limit_cycle({args})")
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def phase_response(self, model: str, values: dict | None = None,
                       parameter_set: str | None = None,
                       transient: float | None = None,
                       variable: str | None = None,
                       period: float | None = None) -> dict:
        """Infinitesimal phase response curves of a stable limit cycle."""
        entry, notes = self._entry(model)
        m = entry.model
        with self._using(entry, values, parameter_set):
            if variable:
                variable = _match(variable, m.variables, "variable")
            try:
                prc = m.phase_response(transient=transient, variable=variable,
                                       period=period)
            except E.EngineError as err:
                raise ToolError(f"{super(E.EngineError, err).__str__()}. The PRC "
                                f"needs a stable limit cycle: check with simulate "
                                f"or limit_cycle that the model oscillates for "
                                f"these values.") from None
        ph = prc["phase"]
        curves = {}
        for v in m.variables[:MAX_SHOWN]:
            z = prc[v]
            i, j = int(np.argmin(z)), int(np.argmax(z))
            scale = max(abs(z.max()), abs(z.min())) or 1.0
            curves[v] = {"min": _r(z[i]), "min_at_phase": _r(ph[i], 3),
                         "max": _r(z[j]), "max_at_phase": _r(ph[j], 3),
                         "type": "I (advances only)" if z.min() >= -0.05 * scale
                         else "II (advances and delays)"}
        out = {"model": entry.name, "period": _r(prc.period, 8),
               "units": "time advance per unit kick to each variable (divide by "
                        "the period for cycles)",
               "prc": curves, "points": len(prc)}
        out["csv"] = self._write_csv(entry, "prc", {k: prc[k] for k in prc.columns})
        args = _kwargs({k: v for k, v in (("transient", transient), ("variable", variable),
                                          ("period", period)) if v is not None})
        out["python"] = self._snippet(entry, values, parameter_set,
                                      f"prc = m.phase_response({args})",
                                      "prc.period, prc.to_pandas()")
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def fit(self, model: str, data: str, fit_variables: list[str],
            fit_parameters: list[str], columns: list[int] | None = None,
            values: dict | None = None, parameter_set: str | None = None,
            tolerance: float = 0.0, max_iterations: int = 0) -> dict:
        """Fit parameters and initial conditions to data (Levenberg-Marquardt)."""
        entry, notes = self._entry(model)
        m = entry.model
        p = Path(data).expanduser()
        if not p.is_absolute():
            p = self.cwd / p
        if not p.is_file():
            raise ToolError(f"no data file {data!r}: give a text file of rows, "
                            f"time first, then one column per measured quantity")
        try:
            rows = np.loadtxt(p, ndmin=2, delimiter=_delimiter(p))
        except ValueError as err:
            raise ToolError(f"cannot read {p.name} as numbers: {err}. Remove any "
                            f"header line.") from None
        fv = [_match(v, m.variables, "variable") for v in fit_variables]
        names = list(m.parameters) + m.variables
        fp = [_match(v, names, "parameter or variable") for v in fit_parameters]
        with self._using(entry, values, parameter_set):
            start = {n: _r(m.get(n)) for n in fp}
            before = _misfit(m, rows, fv, columns)
            fitted = m.fit(rows, variables=fv, parameters=fp, columns=columns,
                           tolerance=tolerance, max_iterations=max_iterations)
            after = _misfit(m, rows, fv, columns)
        out = {"model": entry.name, "data": str(p), "rows": int(rows.shape[0]),
               "start": start, "fitted": {k: _r(v, 8) for k, v in fitted.items()},
               "rms_error": {"before": _r(before), "after": _r(after)},
               "next": "pass the fitted values in values to use them in later calls"}
        args = f"variables={fv!r}, parameters={fp!r}"
        if columns:
            args += f", columns={list(columns)!r}"
        out["python"] = self._snippet(entry, values, parameter_set,
                                      f"fitted = m.fit({p.as_posix()!r}, {args})")
        if notes:
            out["notes"] = notes
        return out

    @_guarded
    def bvp(self, model: str, values: dict | None = None,
            parameter_set: str | None = None) -> dict:
        """Solve the model's boundary-value problem (shooting)."""
        entry, notes = self._entry(model)
        m = entry.model
        with self._using(entry, values, parameter_set):
            try:
                sol = m.bvp()
            except E.EngineError as err:
                raise ToolError(f"{super(E.EngineError, err).__str__()}. Shooting "
                                f"starts from the initial conditions: give a "
                                f"better guess in values, or check the bndry "
                                f"statements.") from None
            ics = {v: _r(m.get(v), 10) for v in m.variables}
        out = {"model": entry.name, "initial_conditions": _cap(ics),
               "t": [_r(sol.t[0]), _r(sol.t[-1])],
               "columns": {c: {"min": _r(sol[c].min()), "max": _r(sol[c].max()),
                               "final": _r(sol[c][-1])}
                           for c in sol.columns if c != "T"}}
        out["csv"] = self._write_csv(entry, "bvp", {k: sol[k] for k in sol.columns})
        out["python"] = self._snippet(entry, values, parameter_set, "sol = m.bvp()")
        if notes:
            out["notes"] = notes
        return out

    # -- tools: plots --------------------------------------------------------------------

    def _figure(self, ncols: int = 1):
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
        except ImportError:
            raise ToolError("plots need matplotlib: pip install matplotlib "
                            "(it comes with xpplab[mcp])") from None
        fig, ax = plt.subplots(1, ncols, figsize=(7 if ncols == 1 else 11, 4.5))
        return plt, fig, ax

    def _save(self, plt, fig, entry_or_name, kind: str, fmt: str) -> str:
        fmt = fmt.lower()
        if fmt not in ("png", "svg"):
            plt.close(fig)
            raise ToolError("format must be 'png' or 'svg'")
        path = self._output(entry_or_name, kind, fmt)
        fig.tight_layout()
        fig.savefig(path, dpi=110)
        plt.close(fig)
        return str(path)

    @_guarded
    def plot_simulation(self, model: str, y: list[str] | None = None,
                        x: str = "T", values: dict | None = None,
                        parameter_set: str | None = None,
                        format: str = "png") -> dict:
        """Plot columns of a run against time (or against another column)."""
        entry, notes = self._entry(model)
        m = entry.model
        with self._using(entry, values, parameter_set):
            sol = m.run()
        cols = [c for c in sol.columns if c != "T"]
        y = [_match(c, cols + ["T"], "column") for c in (y or m.variables[:2])]
        x = _match(x, sol.columns, "column")
        plt, fig, ax = self._figure()
        sol.plot(*y, x=x, ax=ax)
        ax.set_title(entry.name)
        path = self._save(plt, fig, entry, "plot", format)
        args = ", ".join(repr(c) for c in y) + (f", x={x!r}" if x != "T" else "")
        return {"model": entry.name, "file": path, **({"notes": notes} if notes else {}),
                "python": self._snippet(entry, values, parameter_set,
                                        "import matplotlib.pyplot as plt",
                                        f"m.run().plot({args})",
                                        f"plt.savefig({Path(path).name!r})")}

    @_guarded
    def plot_phase_plane(self, model: str, x: str | None = None,
                         y: str | None = None, values: dict | None = None,
                         parameter_set: str | None = None,
                         xlim: list[float] | None = None,
                         ylim: list[float] | None = None,
                         nullclines: bool = True, field: bool = True,
                         trajectory: bool = True, format: str = "png") -> dict:
        """Phase plane: trajectory, nullclines, direction field, equilibrium."""
        entry, notes = self._entry(model)
        m = entry.model
        if len(m.variables) < 2:
            raise ToolError("a phase plane needs two variables")
        x = _match(x or m.variables[0], m.variables, "variable")
        y = _match(y or m.variables[1], m.variables, "variable")
        plt, fig, ax = self._figure()
        with self._using(entry, values, parameter_set):
            m.phase_plane(x, y, xlim=xlim, ylim=ylim, trajectory=trajectory,
                          nullclines=nullclines, field=field, ax=ax)
        ax.set_title(entry.name)
        path = self._save(plt, fig, entry, "phase", format)
        kw = _kwargs({k: v for k, v in (("xlim", xlim), ("ylim", ylim))
                      if v is not None}
                     | {k: False for k, v in (("nullclines", nullclines),
                                              ("field", field),
                                              ("trajectory", trajectory)) if not v})
        return {"model": entry.name, "file": path, **({"notes": notes} if notes else {}),
                "python": self._snippet(entry, values, parameter_set,
                                        "import matplotlib.pyplot as plt",
                                        f"m.phase_plane({x!r}, {y!r}"
                                        + (f", {kw}" if kw else "") + ")",
                                        f"plt.savefig({Path(path).name!r})")}

    @_guarded
    def plot_bifurcation(self, model: str, y: str | None = None,
                         format: str = "png") -> dict:
        """Plot the AUTO diagram computed so far for a model."""
        entry, notes = self._entry(model)
        m = entry.model
        d = m.auto.diagram
        if not len(d):
            raise ToolError(f"no bifurcation diagram for {entry.name} yet: run "
                            f"continuation first (the diagram is kept until the "
                            f"model file changes)")
        if y and y.lower() not in ("norm", "period"):
            y = _match(y, m.variables, "variable")
        from ..auto import Diagram
        one = Diagram([p for p in d if not p.two_parameter], d.variables)
        two = Diagram([p for p in d if p.two_parameter], d.variables)
        if len(one) and len(two):
            # Branches against one parameter, curves in the parameter plane.
            plt, fig, (ax, ax2) = self._figure(ncols=2)
            one.plot(y, ax=ax)
            two.plot(ax=ax2)
            ax2.set_title("two-parameter curves")
            code = ["import matplotlib.pyplot as plt",
                    "from xpplab import Diagram",
                    "d = m.auto.diagram",
                    "fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4.5))",
                    f"Diagram([p for p in d if not p.two_parameter], d.variables).plot({y!r}, ax=a1)",
                    "Diagram([p for p in d if p.two_parameter], d.variables).plot(ax=a2)"]
        else:
            plt, fig, ax = self._figure()
            d.plot(y, ax=ax)
            code = ["import matplotlib.pyplot as plt",
                    f"m.auto.diagram.plot({y!r})" if y else "m.auto.diagram.plot()"]
        ax.set_title(entry.name)
        path = self._save(plt, fig, entry, "diagram", format)
        return {"model": entry.name, "file": path, "points": len(d),
                "two_parameter_points": len(two),
                **({"notes": notes} if notes else {}),
                "python": self._chain(entry, *code,
                                      f"plt.savefig({Path(path).name!r})")}

    @_guarded
    def plot_table(self, csv_file: str, x: str | None = None,
                   y: list[str] | None = None, logy: bool = False,
                   title: str | None = None, format: str = "png") -> dict:
        """Plot columns of a CSV file (one this server wrote, or any other)."""
        p = Path(csv_file).expanduser()
        if not p.is_absolute():
            p = self.cwd / p
        if not p.is_file():
            raise ToolError(f"no file {csv_file!r}")
        with open(p, newline="") as f:
            reader = csv.reader(f)
            header = next(reader, None)
            rows = [r for r in reader if r]
        if not header or not rows:
            raise ToolError(f"{p.name} has no header line or no rows")
        data = {}
        for j, name in enumerate(header):
            col = []
            for r in rows:
                try:
                    col.append(float(r[j]))
                except (ValueError, IndexError):
                    col.append(float("nan"))
            data[name] = np.array(col)
        x = _match(x or header[0], header, "column")
        y = [_match(c, header, "column") for c in (y or [h for h in header if h != x][:1])]
        plt, fig, ax = self._figure()
        for c in y:
            ax.plot(data[x], data[c], label=c)
        ax.set_xlabel(x)
        if len(y) == 1:
            ax.set_ylabel(y[0])
        else:
            ax.legend()
        if logy:
            ax.set_yscale("log")
        ax.set_title(title or p.name)
        path = self._save(plt, fig, p.name, "table", format)
        return {"file": path, "python": "\n".join([
            "import pandas as pd, matplotlib.pyplot as plt",
            f"df = pd.read_csv({p.as_posix()!r})",
            f"df.plot(x={x!r}, y={y!r}" + (", logy=True" if logy else "") + ")",
            f"plt.savefig({Path(path).name!r})"])}


# -- helpers for the tools ---------------------------------------------------------

def _csv_cell(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return v
    if isinstance(v, (bool, np.bool_)):
        return int(v)
    return repr(f) if math.isfinite(f) else str(f)


def _cap(d: dict, n: int = 60) -> dict:
    if len(d) <= n:
        return d
    out = dict(list(d.items())[:n])
    out["..."] = f"{len(d) - n} more"
    return out


def _auto_count(m) -> int:
    """Points in the model's AUTO diagram, without building them."""
    return m._engine.call("xppe_auto_count", check=False)


def _default_of(entry: _Entry, name: str):
    """A name's value in the model file: parameter, variable or option."""
    for k, v in entry.defaults.items():
        if _key(k) == _key(name):
            return v
    v = entry.options.get(_key(name))
    return v if isinstance(v, str) or v is None else float(v)


def _match(name: str, names: list[str], what: str) -> str:
    for n in names:
        if _key(n) == _key(name):
            return n
    raise ToolError(f"no {what} named {name!r}; the choices are "
                    f"{', '.join(names[:60])}")


def _compress(labels: list[int]) -> str:
    if len(labels) <= 20:
        return ", ".join(map(str, labels))
    return ", ".join(map(str, labels[:20])) + f", ... ({len(labels)} labels)"


def _delimiter(path: Path):
    head = path.read_text(errors="replace")[:2000]
    return "," if "," in head else None


def _log_hint(log: str) -> str:
    tail = [ln.strip() for ln in log.strip().splitlines() if ln.strip()][-4:]
    return ("\nEngine output (last lines):\n" + "\n".join(tail)) if tail else ""


def _downsample(sol, cols: list[str], points: int) -> dict:
    points = max(2, min(int(points), MAX_POINTS))
    n = len(sol)
    idx = np.unique(np.linspace(0, n - 1, min(points, n)).round().astype(int))
    return {c: _rlist(sol[c][idx]) for c in cols}


def _run_warnings(sol, t0: float, total: float, log: str) -> list[str]:
    out = []
    data = np.column_stack([sol[c] for c in sol.columns])
    if not np.all(np.isfinite(data)):
        i = int(np.argmax(~np.all(np.isfinite(data), axis=1)))
        out.append(f"values are not finite from t={_r(sol.t[i])}: the solution "
                   f"blew up; check the equations, or try a smaller dt or "
                   f"meth='stiff'")
    end = float(sol.t[-1])
    if total > 0 and end < t0 + total * (1 - 1e-6) - 1e-9:
        reason = ""
        low = log.lower()
        if "bound" in low or "out of" in low:
            reason = (" because a variable exceeded BOUND; raise bound in values "
                      "if the values are real, or look for a blow-up")
        elif "step" in low and ("small" in low or "fail" in low):
            reason = (" because the step size became too small: the system may be "
                      "stiff (try meth='stiff' or 'cvode') or singular")
        out.append(f"the run stopped at t={_r(end)} instead of "
                   f"t={_r(t0 + total)}{reason}")
    return out


def _behaviour(sol, variables: list[str], discrete: bool) -> dict:
    """What the run does in the end: settles, oscillates, or neither."""
    n = len(sol)
    if n < 16:
        return {"kind": "too few points to tell"}
    if discrete:
        return {"kind": "map (look at the values directly)"}
    q = max(4, n // 4)
    rel = {}
    for v in variables:
        x = sol[v]
        if not np.all(np.isfinite(x)):
            return {"kind": "not finite"}
        span = float(np.ptp(x))
        tail = float(np.ptp(x[-q:]))
        floor = 1e-7 * max(1.0, float(np.max(np.abs(x))))
        rel[v] = 0.0 if tail <= floor else tail / span
    v = max(rel, key=rel.get)
    if rel[v] < 1e-4:
        return {"kind": "steady state", "note": "every variable has settled"}
    half = n // 2
    x = sol[v]
    try:
        per = _period(sol.t[half:], x[half:])
    except E.EngineError:
        return {"kind": "not settled",
                "note": "neither a steady state nor a regular oscillation in the "
                        "second half of the run: integrate longer, or compute "
                        "lyapunov to test for chaos"}
    a3 = float(np.ptp(x[-2 * q:-q]))
    a4 = float(np.ptp(x[-q:]))
    out = {"kind": "periodic", "period": _r(per), "variable": v,
           "amplitude": _r(a4)}
    if a3 > 0 and a4 < 0.9 * a3:
        out["kind"] = "damped oscillation"
        out["note"] = "the amplitude still decreases: it may settle to an equilibrium"
    elif a3 > 0 and a4 > 1.1 * a3:
        out["kind"] = "growing oscillation"
    return out


def _equilibrium_json(eq, origin) -> dict:
    ev = np.asarray(eq.eigenvalues)
    order = np.argsort(-ev.real)
    shown = ev[order][:8]
    out = {"values": {k: _r(v, 8) for k, v in list(eq.values.items())[:60]},
           "stable": bool(eq.stable), "kind": eq.kind,
           "eigenvalues": [[_r(z.real), _r(z.imag)] for z in shown],
           "from": origin}
    if len(ev) > len(shown):
        out["eigenvalues_note"] = (f"the {len(shown)} with the largest real parts "
                                   f"of {len(ev)}; {eq.unstable_count} unstable")
    if eq.discrete:
        out["note"] = "a map: stable when every |eigenvalue| < 1"
    return out


def _peaks(freq, power, n: int = 3) -> list[dict]:
    f = np.asarray(freq, dtype=float)
    p = np.asarray(power, dtype=float)
    if len(p) < 3:
        return []
    idx = [i for i in range(1, len(p) - 1) if p[i] >= p[i - 1] and p[i] > p[i + 1]]
    idx.sort(key=lambda i: -p[i])
    return [{"frequency": _r(f[i]), "period": _r(1 / f[i]) if f[i] else None,
             "power": _r(p[i])} for i in idx[:n]]


def _misfit(m, rows, fv, columns) -> float:
    """Root-mean-square difference between the model and the data at the
    data times, with the model's current values."""
    cols = columns or list(range(2, 2 + len(fv)))
    t = rows[:, 0]
    keep = {}
    ics = {v: float(m.get(v)) for v in m.variables}
    old = {k: m.get(k) for k in ("TOTAL", "T0")}
    try:
        m.set(t0=float(t[0]), total=float(t[-1] - t[0]))
        sol = m.run()
        for v, c in zip(fv, cols):
            keep[v] = np.interp(t, sol.t, sol[v]) - rows[:, int(c) - 1]
    except Exception:
        return float("nan")
    finally:
        m.set(**ics, **{k: v for k, v in old.items()})
    err = np.concatenate(list(keep.values()))
    return float(np.sqrt(np.mean(err ** 2)))


def _regimes(grid, cols: dict, halves, v: str, lyap: bool) -> list[dict]:
    """Consecutive runs that behave alike, as ranges of the parameter."""
    amp = cols[f"{v}_max"] - cols[f"{v}_min"]
    big = float(np.nanmax(amp)) if np.any(np.isfinite(amp)) else 0.0
    labels = []
    for i in range(len(grid)):
        a = amp[i]
        mean = abs(0.5 * (cols[f"{v}_max"][i] + cols[f"{v}_min"][i]))
        a1, a2 = halves[i]
        if not np.isfinite(a):
            labels.append("not finite")
        elif a <= max(1e-6 * max(1.0, mean), 1e-3 * big):
            labels.append("steady")
        elif lyap and cols["lyapunov"][i] > 0.01:
            labels.append("chaotic")
        elif cols["cycles"][i] >= 3 and a2 < 0.9 * a1:
            labels.append("damped")
        elif np.isfinite(cols["period"][i]):
            labels.append("periodic")
        elif cols["cycles"][i] < 3:
            labels.append("oscillating, too few cycles")
        else:
            labels.append("irregular")
    out = []
    for i, lab in enumerate(labels):
        if out and out[-1]["regime"] == lab:
            out[-1]["to"] = _r(grid[i])
            out[-1]["_idx"].append(i)
        else:
            out.append({"regime": lab, "from": _r(grid[i]), "to": _r(grid[i]),
                        "_idx": [i]})
    for r in out:
        idx = r.pop("_idx")
        a = amp[idx]
        if r["regime"] != "steady":
            r["amplitude"] = [_r(np.nanmin(a)), _r(np.nanmax(a))]
        if r["regime"] == "periodic":
            per = cols["period"][idx]
            r["period"] = [_r(np.nanmin(per)), _r(np.nanmax(per))]
        if r["regime"] == "steady":
            r[f"{v}"] = [_r(np.nanmin(cols[f"{v}_max"][idx])),
                         _r(np.nanmax(cols[f"{v}_max"][idx]))]
    return out


def _diagram_columns(d, variables: list[str]) -> dict:
    pts = list(d)
    cols: dict = {
        "label": [p.label for p in pts], "type": [p.type for p in pts],
        "branch": [p.branch for p in pts], "point": [p.point for p in pts],
        "stable": [int(p.stable) for p in pts],
        "periodic": [int(p.periodic) for p in pts]}
    par = pts[0].parameter
    cols[par] = [p.value for p in pts]
    p2 = next((p.parameter2 for p in pts if p.two_parameter and p.parameter2), None)
    if p2:
        cols[p2] = [p.value2 if p.value2 is not None else float("nan") for p in pts]
    cols["norm"] = [p.norm for p in pts]
    cols["period"] = [p.period for p in pts]
    for v in variables[:20]:
        cols[f"{v}_max"] = [p.umax[v] for p in pts]
        cols[f"{v}_min"] = [p.umin[v] for p in pts]
    return cols


def _state(p, variables: list[str]) -> dict:
    if p.periodic:
        return {"max": {v: _r(p.umax[v]) for v in variables[:MAX_SHOWN]},
                "min": {v: _r(p.umin[v]) for v in variables[:MAX_SHOWN]}}
    return {"state": {v: _r(p.u0[v]) for v in variables[:MAX_SHOWN]}}


def _branch_report(d, variables: list[str], used: dict, start: bool) -> dict:
    """A compact account of newly computed points: what the branch is, where
    it is stable, its special points (each once), and why it ended."""
    pts = list(d)
    two = any(p.two_parameter for p in pts)
    periodic = any(p.periodic for p in pts)
    par = pts[0].parameter
    vals = np.array([p.value for p in pts])
    report: dict = {"branch": {
        "kind": ("two-parameter curve" if two else
                 "periodic orbits" if periodic else "steady states"),
        "parameter": par, "points": len(pts),
        "range": [_r(vals.min()), _r(vals.max())]}}
    if two:
        p2 = next((p.parameter2 for p in pts if p.parameter2), None)
        v2 = np.array([p.value2 for p in pts if p.value2 is not None])
        report["branch"]["second_parameter"] = p2
        if len(v2):
            report["branch"]["second_range"] = [_r(v2.min()), _r(v2.max())]
    if periodic:
        per = np.array([p.period for p in pts if p.periodic])
        report["branch"]["period_range"] = [_r(per.min()), _r(per.max())]

    # Special points, each listed once with how often AUTO passed it. On a
    # closed loop of orbits AUTO detects the same fold again on every lap,
    # a little elsewhere each time, hence tolerances relative to the branch.
    tol_v = 1e-3 * max(float(np.ptp(vals)), 1e-12)
    v2s = np.array([p.value2 for p in pts if p.value2 is not None] or [0.0])
    tol_v2 = 1e-3 * max(float(np.ptp(v2s)), 1e-12)
    pers = np.array([p.period for p in pts if p.periodic] or [0.0])
    tol_p = 2e-2 * max(float(np.ptp(pers)), 1e-12)

    def same(g, p):
        q = g["_point"]
        return (q.type == p.type and q.periodic == p.periodic
                and q.two_parameter == p.two_parameter
                and abs(q.value - p.value) <= tol_v
                and (not p.two_parameter or q.value2 is None or p.value2 is None
                     or abs(q.value2 - p.value2) <= tol_v2)
                and (not p.periodic or abs(q.period - p.period) <= tol_p))

    groups: list[dict] = []
    for p in d.special():
        g = next((g for g in groups if same(g, p)), None)
        if g:
            g["labels"].append(p.label)
            continue
        e = {"_point": p, "label": p.label, "type": p.type,
             "what": TYPE_NAMES.get(p.type, p.type) + (
                 " of periodic orbits" if p.periodic and p.type == "LP" else ""),
             par: _r(p.value, 8)}
        if p.two_parameter and p.parameter2:
            e[p.parameter2] = _r(p.value2, 8)
        e["stable"] = bool(p.stable)
        if p.periodic:
            e["period"] = _r(p.period)
        elif p.type == "HB" and p.period > 0:
            e["period_at_onset"] = _r(p.period)
        if not two:
            e.update(_state(p, variables))
        kind = p.type if p.type in _ACTIONS else ("periodic" if p.periodic else "steady")
        e["continue_with"] = list(_ACTIONS.get(kind, {}))
        e["labels"] = [p.label]
        groups.append(e)
    repeats = 0
    for g in groups:
        g.pop("_point")
        if len(g["labels"]) == 1:
            g.pop("labels")
        else:
            repeats = max(repeats, len(g["labels"]))
            g["labels"] = _compress(g["labels"])
    if len(groups) > 15:
        report["special_points"] = groups[:15]
        report["special_points_note"] = (f"{len(groups)} distinct special points; "
                                         f"the CSV has every point")
    else:
        report["special_points"] = groups

    # Where the branch is stable. AUTO does not report the stability of the
    # starting point, so a branch's first point joins the segment after it.
    if not two:
        segs = []
        for k, p in enumerate(pts):
            if k == 1 and len(segs) == 1 and len(pts) > 2 and segs[0]["_b"] == p.branch:
                segs[0]["stable"] = bool(p.stable)
            if segs and segs[-1]["stable"] == p.stable and segs[-1]["_b"] == p.branch:
                segs[-1]["to"] = _r(p.value)
                if p.periodic:
                    segs[-1]["_per"].append(p.period)
            else:
                # A change of stability happens between two points: start the
                # new segment where the last one ended.
                begin = segs[-1]["to"] if segs and segs[-1]["_b"] == p.branch else _r(p.value)
                segs.append({"_b": p.branch, "from": begin, "to": _r(p.value),
                             "stable": bool(p.stable),
                             "_per": [p.period] if p.periodic else []})
        for s in segs:
            s.pop("_b")
            per = s.pop("_per")
            if per:
                s["period"] = [_r(min(per)), _r(max(per))]
        if len(segs) > 12:
            report["stability"] = segs[:12]
            report["stability_note"] = f"{len(segs)} segments; the CSV has every point"
        else:
            report["stability"] = segs

    # How it ended (the first point is where it started, often an EP too).
    report["branch"]["start"] = _r(pts[0].value, 8)
    ends = [p for p in pts[1:] if p.type in ("EP", "MX") and p.label]
    endings = []
    for p in ends[-2:]:
        e = {"label": p.label, "type": p.type, par: _r(p.value, 8)}
        if p.type == "MX":
            e["reason"] = ("AUTO failed to converge here: reduce ds and dsmax, or "
                           "raise ntst for periodic orbits")
        # AUTO stops on the first step past the range.
        elif p.value >= used["PARMAX"] - 1e-6 * max(1, abs(used["PARMAX"])):
            e["reason"] = "reached parmax"
        elif p.value <= used["PARMIN"] + 1e-6 * max(1, abs(used["PARMIN"])):
            e["reason"] = "reached parmin"
        elif p.norm >= used["NORMMAX"] * (1 - 1e-6):
            e["reason"] = "reached normmax"
        elif len(pts) >= used["NMAX"] - 1:
            e["reason"] = "reached nmax steps"
        else:
            e["reason"] = "end of the branch"
        endings.append(e)
    report["ends"] = endings
    regular = [p.label for p in d.labelled() if p.type == ""]
    if regular:
        report["regular_labels"] = _compress(regular)

    advice = []
    if repeats >= 3:
        advice.append(f"AUTO went round a closed loop of orbits (special points "
                      f"repeat up to {repeats} times) until nmax: set a smaller "
                      f"nmax in settings")
    if any(e["type"] == "MX" for e in endings):
        advice.append("the branch stopped on a convergence failure (MX)")
    if start and not two and len(pts) > 1:
        first = pts[0].value
        if pts[1].value > first and vals.min() >= first and first > used["PARMIN"]:
            advice.append(f"the branch went towards larger {par} from {_r(first)}; "
                          f"run again with a negative ds in settings to cover "
                          f"{par} < {_r(first)}")
        elif pts[1].value < first and vals.max() <= first and first < used["PARMAX"]:
            advice.append(f"the branch went towards smaller {par} from {_r(first)}; "
                          f"run again with a positive ds to cover {par} > {_r(first)}")
    hb = [g for g in groups if g["type"] == "HB"]
    if hb:
        advice.append(f"continuation(model, from_label={hb[0]['label']}, "
                      f"action='periodic') follows the limit cycles born at "
                      f"that Hopf point (raise ntst for spiky orbits)")
    lp = [g for g in groups if g["type"] == "LP" and not periodic]
    if lp and not two:
        advice.append(f"continuation(model, from_label={lp[0]['label']}, "
                      f"action='two-parameter', second_parameter=...) traces the "
                      f"fold in two parameters")
    advice.append("plot_bifurcation(model) draws the whole diagram")
    report["next"] = advice
    return report
