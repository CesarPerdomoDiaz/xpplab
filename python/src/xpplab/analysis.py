"""Data analysis on a model's last run: XPPAUT's Numerics > stocHastic and
Averaging tools (Fourier series, spectra, histograms, correlations,
Liapunov exponents, curve fitting, adjoints, phase response curves and H
functions).

Every method analyses the model's most recent integration, as XPPAUT's
menus do, and leaves that run unchanged::

    m.run(total=500)
    m.spectrum("V").plot()              # power spectral density
    m.histogram("V", bins=50).plot()
    prc = m.phase_response()            # one period, adjoint, PRC
"""
from __future__ import annotations

import ctypes
import os
from pathlib import Path

import numpy as np

from . import _engine as E

WINDOWS = {"square": 0, "none": 0, "parzen": 1, "hamming": 2, "bartlett": 3,
           "hann": 4, "hanning": 4}
SPECTRA = {"psd": 0, "power": 0, "cross": 1, "coherence": 2}


def _b(s) -> bytes:
    return str(s).encode("latin-1")


def _csv_cell(v) -> str:
    if v is None:
        return ""
    if isinstance(v, str):
        return v
    if isinstance(v, (bool, np.bool_)):
        return "1" if v else "0"
    if isinstance(v, (int, np.integer)):
        return str(int(v))
    return repr(float(v))


def write_csv(path, columns, rows) -> None:
    """Write a header and rows as CSV with every number exact: the shortest
    text that reads back as the same float, as the desktop app writes it."""
    with open(path, "w", newline="", encoding="utf-8") as f:
        f.write(",".join(columns) + "\n")
        for row in rows:
            f.write(",".join(_csv_cell(v) for v in row) + "\n")


class Table:
    """A table of results: named NumPy columns, the first one the x-axis."""

    def __init__(self, data: dict[str, np.ndarray], title: str = "",
                 log: str = ""):
        self._data = data
        self.title = title
        self.log = log

    @property
    def columns(self) -> list[str]:
        return list(self._data)

    def __getitem__(self, name: str) -> np.ndarray:
        for k, v in self._data.items():
            if k.lower() == name.lower():
                return v
        raise KeyError(f"no column {name!r}; available: {', '.join(self._data)}")

    def __contains__(self, name: str) -> bool:
        return any(k.lower() == name.lower() for k in self._data)

    def __len__(self) -> int:
        return len(next(iter(self._data.values()))) if self._data else 0

    def to_numpy(self) -> np.ndarray:
        return np.column_stack(list(self._data.values()))

    def to_pandas(self):
        """A pandas DataFrame indexed by the first column."""
        import pandas as pd
        df = pd.DataFrame(self._data)
        return df.set_index(self.columns[0])

    @classmethod
    def from_grid(cls, name: str, values, name2: str, values2, columns: dict,
                  title: str = "") -> "Table":
        """A table of results on a grid (see :meth:`Model.sweep_grid`): one
        row per point, going through ``values`` for each of ``values2``, the
        two values first and then each of ``columns``, arrays of shape
        ``(len(values2), len(values))``."""
        v1, v2 = np.asarray(values, dtype=float), np.asarray(values2, dtype=float)
        data = {name: np.tile(v1, len(v2)), name2: np.repeat(v2, len(v1))}
        for key, arr in columns.items():
            data[key] = np.asarray(arr, dtype=float).reshape(len(v2), len(v1)).ravel()
        return cls(data, title=title)

    def to_csv(self, path: str | os.PathLike) -> None:
        """Write the table as CSV, every number exact (the desktop app's
        ``export FILE.csv analysis`` writes the same file)."""
        cols = [np.asarray(v, dtype=float) for v in self._data.values()]
        write_csv(path, self.columns, zip(*cols))

    def plot(self, *y: str, ax=None, **kw):
        """Plot columns (by default the second) against the first."""
        import matplotlib.pyplot as plt
        ax = ax or plt.gca()
        x = self.columns[0]
        names = list(y) or self.columns[1:2]
        for name in names:
            ax.plot(self[x], self[name], label=name, **kw)
        ax.set_xlabel(x)
        if len(names) == 1:
            ax.set_ylabel(names[0])
        else:
            ax.legend()
        if self.title:
            ax.set_title(self.title)
        return ax

    def __repr__(self) -> str:
        what = f" {self.title}" if self.title else ""
        return f"<Table{what}: {len(self)} rows, columns {', '.join(self.columns)}>"


class AnalysisMixin:
    """The analysis methods of :class:`xpplab.Model`."""

    def _result(self, title: str) -> Table:
        eng = self._engine
        rows = eng.call("xppe_result_rows", check=False)
        ncols = eng.call("xppe_result_count", check=False)
        buf = (ctypes.c_double * max(rows, 1))()
        data = {}
        for j in range(ncols):
            name = E._text(eng._lib.xppe_result_name(j)) or f"c{j}"
            n = eng.call("xppe_result_column", j, buf, rows)
            data[name] = np.frombuffer(buf, dtype=float, count=n).copy()
        return Table(data, title=title, log=eng.log)

    def _range(self, column: str, rng) -> tuple[float, float]:
        if rng is not None:
            lo, hi = rng
            return float(lo), float(hi)
        lo, hi = self._column_limits(column)
        if hi <= lo:
            hi = lo + 1.0
        return lo, hi

    def _column_limits(self, column: str) -> tuple[float, float]:
        """The smallest and largest value of a column in the last run."""
        rows = self._engine.call("xppe_rows", check=False)
        buf = (ctypes.c_double * max(rows, 1))()
        lower = [c.lower() for c in self.columns]
        if column.lower() in lower:
            j = lower.index(column.lower())
        elif column.lower() == "t":
            j = -1
        else:
            raise KeyError(f"no column {column!r}; available: "
                           f"{', '.join(self.columns)}")
        n = self._engine.call("xppe_column", j, buf, rows)
        if n < 1:
            raise E.EngineError("integrate the model first (the analysis "
                                "uses the last run)")
        a = np.frombuffer(buf, dtype=float, count=n)
        return float(a.min()), float(a.max())

    # -- Fourier series and spectra ------------------------------------------

    def fourier(self, column: str) -> Table:
        """Fourier series of a column over the last run: freq, cos, sin.

        Frequencies are in cycles per unit time (mode k / run length)."""
        self._engine.call("xppe_fourier", _b(column))
        return self._result(f"Fourier series of {column}")

    def power(self, column: str) -> Table:
        """The Fourier series as amplitude and phase: freq, amplitude, phase."""
        self._engine.call("xppe_power", _b(column))
        return self._result(f"Fourier amplitude of {column}")

    def spectrum(self, column: str, column2: str | None = None, *,
                 kind: str = "psd", length: int | None = None,
                 window: str | int = "hamming") -> Table:
        """Spectral density by Welch's method (windows of ``length`` points,
        50% overlap), as XPPAUT's Spec.dns.

        ``kind`` is "psd" (power spectral density of ``column``), "cross"
        (cross spectrum) or "coherence" (both need ``column2``). ``window``
        is "square", "parzen", "hamming", "bartlett" or "hann". ``length``
        defaults to 512 points, or less for a short run."""
        k = SPECTRA[kind.lower()] if isinstance(kind, str) else int(kind)
        w = WINDOWS[window.lower()] if isinstance(window, str) else int(window)
        if k > 0 and column2 is None:
            raise ValueError(f"a {kind} spectrum needs two columns")
        if length is None:
            rows = self._engine.call("xppe_rows", check=False)
            length = 512
            while length > 4 and (rows - length + 1) // (length // 2) < 1:
                length //= 2
        self._engine.call("xppe_spectrum", k, _b(column), _b(column2 or column),
                          int(length), w)
        return self._result(f"{kind} of {column}" +
                            (f" and {column2}" if column2 else ""))

    # -- Histograms and correlations ------------------------------------------

    def histogram(self, column: str, bins: int = 100, range=None,
                  condition: str = "") -> Table:
        """Histogram of a column: bin (left edge), count.

        ``range=(lo, hi)`` defaults to the column's range in the run.
        ``condition`` is an XPPAUT expression; only points where it is
        nonzero count (e.g. ``"V>0"``)."""
        lo, hi = self._range(column, range)
        self._engine.call("xppe_histogram", _b(column), int(bins), lo, hi,
                          _b(condition))
        return self._result(f"Histogram of {column}")

    def histogram2d(self, x: str, y: str, bins=(20, 20), range=None) -> Table:
        """Two-dimensional histogram of two columns: x, y, count."""
        nx, ny = (bins, bins) if np.isscalar(bins) else bins
        (xlo, xhi), (ylo, yhi) = ((self._range(x, None), self._range(y, None))
                                  if range is None else range)
        self._engine.call("xppe_histogram2d", _b(x), int(nx), float(xlo),
                          float(xhi), _b(y), int(ny), float(ylo), float(yhi))
        return self._result(f"Histogram of {x} and {y}")

    def correlation(self, column: str, column2: str | None = None, *,
                    lags: int = 100, fft: bool = False) -> Table:
        """Cross-correlation of two columns, or the autocorrelation of one,
        over about ``lags`` lags: lag (in time units), correlation."""
        other = column2 or column
        self._engine.call("xppe_correlation", _b(column), _b(other), int(lags),
                          int(bool(fft)))
        what = (f"Autocorrelation of {column}" if column2 is None
                else f"Correlation of {column} and {column2}")
        return self._result(what)

    def difference_histogram(self, column: str, bins: int = 100,
                             range=None) -> Table:
        """Histogram of all pairwise differences of a column (XPPAUT's
        stAutocor): difference, count."""
        if range is None:
            lo, hi = self._column_limits(column)
            d = (hi - lo) or 1.0
            range = (-d, d)
        lo, hi = range
        self._engine.call("xppe_autocorrelation", _b(column), int(bins),
                          float(lo), float(hi))
        return self._result(f"Differences of {column}")

    def mean(self, column: str) -> tuple[float, float]:
        """Mean and standard deviation of a column over the last run."""
        m, s = ctypes.c_double(), ctypes.c_double()
        self._engine.call("xppe_mean", _b(column), ctypes.byref(m),
                          ctypes.byref(s))
        return m.value, s.value

    def lyapunov(self) -> float:
        """The largest Liapunov exponent along the last run, XPPAUT's way:
        a small perturbation is carried along the run one step at a time and
        renormalised, and the logs of its growth are averaged."""
        z = ctypes.c_double()
        self._engine.call("xppe_lyapunov", ctypes.byref(z))
        return z.value

    # -- Periodic orbits, adjoints and phase response --------------------------

    def limit_cycle(self, *, transient: float | None = None,
                    variable: str | None = None, period: float | None = None,
                    total: float | None = None):
        """Find one period of a stable limit cycle and leave it as the run.

        Integrates for ``transient`` time units (default: the model's total)
        to settle onto the cycle, measures the period from the upward
        crossings of ``variable`` (default: the first) through the middle of
        its range (unless ``period`` is given), then
        integrates exactly one period from a point on the cycle, which is
        what XPPAUT's adjoint needs. Initial conditions and the total time
        are restored afterwards. Returns ``(period, Trajectory)``."""
        var = variable or self.variables[0]
        ics = {v: float(self.get(v)) for v in self.variables}
        old_total = float(self.get("TOTAL"))

        def settle_from_end(length):
            sol = self.run(total=float(length))
            self.set(**{v: float(sol[v][-1]) for v in self.variables})
            return sol

        try:
            settle = float(transient if transient is not None else old_total)
            length = float(total if total is not None else max(old_total, settle))
            if period is None:
                # The period is not known in advance: if a run shows too
                # few cycles, keep going with twice the time.
                for attempt in range(6):
                    settle_from_end(settle)
                    sol = settle_from_end(length)
                    try:
                        period = _period(sol.t, sol[var])
                        break
                    except E.EngineError:
                        if attempt == 5:
                            raise
                        settle, length = 2 * settle, 2 * length
            else:
                settle_from_end(settle)
            orbit = self.run(total=float(period))
        finally:
            self.set(TOTAL=old_total, **ics)
        return float(period), orbit

    def adjoint(self, *, max_iterations: int = 0,
                tolerance: float = 0.0) -> Table:
        """XPPAUT's adjoint along the last run, which must be exactly one
        period of a stable limit cycle (see :meth:`limit_cycle`): T, then
        one column per variable.

        It is normalised as XPPAUT does, so that its dot product with the
        vector field is 1: each column is the infinitesimal phase response
        curve to perturbations of that variable, in time units."""
        self._engine.call("xppe_adjoint", int(max_iterations), float(tolerance))
        return self._result("Adjoint")

    def phase_response(self, *, transient: float | None = None,
                       variable: str | None = None,
                       period: float | None = None) -> Table:
        """The infinitesimal phase response curves of a limit cycle.

        Finds one period with :meth:`limit_cycle`, computes the adjoint, and
        returns a table with the phase (0 to 1), the time, and one column
        per variable. ``table.period`` holds the period."""
        per, _ = self.limit_cycle(transient=transient, variable=variable,
                                  period=period)
        adj = self.adjoint()
        t = adj["T"]
        data = {"phase": (t - t[0]) / per}
        data.update({k: v for k, v in adj._data.items()})
        tab = Table(data, title="Phase response", log=adj.log)
        tab.period = per
        return tab

    def h_function(self, coupling) -> Table:
        """The averaged interaction function H(phi) for weak coupling, after
        :meth:`adjoint` (or :meth:`phase_response`) on the same run.

        ``coupling`` gives one formula per variable, as a dict or a list in
        the order of :attr:`variables`; primed names are the other
        oscillator's variables, e.g. ``{"V": "V'-V", "W": "0"}`` for
        diffusive coupling through V. Result: phi, H, and for three or more
        variables also Hodd and Heven."""
        if isinstance(coupling, dict):
            low = {k.lower(): v for k, v in coupling.items()}
            formulas = [str(low.get(v.lower(), "0")) for v in self.variables]
        else:
            formulas = [str(c) for c in coupling]
        arr = (ctypes.c_char_p * len(formulas))(*[_b(f) for f in formulas])
        self._engine.call("xppe_hfunction", len(formulas), arr)
        return self._result("H function")

    # -- Boundary-value problems and animations ---------------------------------

    def bvp(self, *, max_iterations: int = 0, tolerance: float = 0.0,
            epsilon: float = 0.0):
        """Solve the model's boundary-value problem by XPPAUT's shooting
        method (Bndryval), from the current initial conditions as the first
        guess, over [T0, T0 + total].

        The boundary conditions are the model's ``bndry`` statements, in
        which a primed name means the variable at the right end (``b
        v'-1``). On success the initial conditions hold the solution's and
        the solution is returned as a Trajectory."""
        if self._engine.call("xppe_bvp_count", check=False) <= 0:
            raise E.EngineError("the model has no boundary conditions "
                                "(bndry statements)")
        self._engine.call("xppe_bvp", int(max_iterations), float(tolerance),
                          float(epsilon))
        return self._collect()

    def animation(self, path):
        """An XPPAUT animation (.ani file) of the last run: see
        :class:`xpplab.Animation`. A relative path is taken next to the
        model."""
        from .animation import Animation
        p = Path(path)
        if not p.is_absolute():
            p = self.path.parent / p
        return Animation(self, p)

    # -- Curve fitting -------------------------------------------------------

    def fit(self, data, *, variables, parameters, columns=None,
            tolerance: float = 0.0, epsilon: float = 0.0,
            max_iterations: int = 0) -> dict[str, float]:
        """Fit parameters and initial conditions to data, with XPPAUT's
        Levenberg-Marquardt.

        ``data`` is a file of rows (time first) or an array of shape
        (points, columns) with time in the first column. ``variables`` are
        the variables to fit and ``columns`` their columns in the data,
        counting from 1 for time (default 2, 3, ...). ``parameters`` lists
        the parameters and initial conditions to vary; they start from
        their current values and end at the fitted ones, which are also
        returned. The initial conditions apply at the first data time."""
        variables = [variables] if isinstance(variables, str) else list(variables)
        parameters = [parameters] if isinstance(parameters, str) else list(parameters)
        if columns is None:
            columns = list(range(2, 2 + len(variables)))
        if isinstance(data, (str, os.PathLike)):
            path = Path(data).resolve()
            rows = np.loadtxt(path, ndmin=2)
        else:
            rows = np.asarray(data, dtype=float)
            if rows.ndim != 2:
                raise ValueError("data must be a 2-D array: time, then columns")
            path = Path(self._engine.workdir) / "fit-data.dat"
            np.savetxt(path, rows, fmt="%.17g")
        self._engine.call("xppe_fit", _b(path), rows.shape[0], rows.shape[1],
                          _b(",".join(variables)),
                          _b(",".join(str(c) for c in columns)),
                          _b(",".join(parameters)), float(tolerance),
                          float(epsilon), int(max_iterations))
        return {p: float(self.get(p)) for p in parameters}


def _period(t: np.ndarray, x: np.ndarray) -> float:
    """The period of an oscillation from upward crossings of the middle of
    its range, interpolated linearly, averaged over the last few cycles."""
    m = 0.5 * (float(x.max()) + float(x.min()))
    s = x - m
    up = np.nonzero((s[:-1] < 0) & (s[1:] >= 0))[0]
    if len(up) < 3:
        raise E.EngineError("no oscillation found: run longer (total=...) "
                            "or give the period")
    tc = t[up] - s[up] * (t[up + 1] - t[up]) / (s[up + 1] - s[up])
    periods = np.diff(tc)[-min(5, len(tc) - 1):]
    if periods.std() > 0.01 * periods.mean():
        raise E.EngineError("the oscillation is not periodic yet: use a "
                            "longer transient")
    return float(periods.mean())
