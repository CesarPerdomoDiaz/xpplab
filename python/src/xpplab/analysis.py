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


_mat_engine = None


def write_mat(path, names, columns, params=None) -> None:
    """Write named columns (and a ``params`` struct of the numbers in the
    dictionary ``params``, if given) as a MATLAB Level 5 .mat file. The
    engine writes it, so the desktop app's ``export FILE.mat`` produces the
    same bytes. Names are made valid MATLAB names: every character that is
    not a letter, digit or underscore becomes ``_``; ``x`` goes in front if
    the name does not start with a letter; names are cut to 63 characters;
    repeated names get ``_2``, ``_3``, ..."""
    global _mat_engine
    cols = [np.ascontiguousarray(c, dtype=np.float64).reshape(-1) for c in columns]
    names = [str(n) for n in names]
    n = len(cols)
    rows = len(cols[0]) if cols else 0
    if any(len(c) != rows for c in cols):
        raise ValueError("the columns of a .mat file must have the same length")
    params = dict(params) if params else {}
    pnames = list(params)
    pvals = (ctypes.c_double * max(len(pnames), 1))(*[float(params[k]) for k in pnames])
    if _mat_engine is None:
        _mat_engine = E.Engine()
    c_names = (ctypes.c_char_p * max(n, 1))(*[s.encode("utf-8") for s in names])
    c_cols = (ctypes.POINTER(ctypes.c_double) * max(n, 1))(
        *[c.ctypes.data_as(ctypes.POINTER(ctypes.c_double)) for c in cols])
    c_pn = (ctypes.c_char_p * max(len(pnames), 1))(*[s.encode("utf-8") for s in pnames])
    _mat_engine.call("xppe_write_mat", os.fsencode(os.fspath(path)), n, c_names,
                     c_cols, rows, len(pnames), c_pn, pvals)


class Table:
    """A table of results: named NumPy columns, the first one the x-axis.
    ``params`` (a dictionary of parameter values, or None) goes into a
    ``.mat`` file as the struct ``params``."""

    def __init__(self, data: dict[str, np.ndarray], title: str = "",
                 log: str = "", params: dict | None = None):
        self._data = data
        self.title = title
        self.log = log
        self.params = params

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
                  title: str = "", params: dict | None = None) -> "Table":
        """A table of results on a grid (see :meth:`Model.sweep_grid`): one
        row per point, going through ``values`` for each of ``values2``, the
        two values first and then each of ``columns``, arrays of shape
        ``(len(values2), len(values))``."""
        v1, v2 = np.asarray(values, dtype=float), np.asarray(values2, dtype=float)
        data = {name: np.tile(v1, len(v2)), name2: np.repeat(v2, len(v1))}
        for key, arr in columns.items():
            data[key] = np.asarray(arr, dtype=float).reshape(len(v2), len(v1)).ravel()
        return cls(data, title=title, params=params)

    @classmethod
    def from_nullclines(cls, sets, first: int = 0, title: str = "") -> "Table":
        """Nullclines as a table, one row per line segment, as the desktop
        app's ``export FILE.csv nullclines`` writes them: the set (``first``
        for the first pair in ``sets``, then counting up), which nullcline
        (1 the x variable's, 2 the y variable's) and the segment's ends
        x1, y1, x2, y2. ``sets`` holds ``(x_nullcline, y_nullcline)`` pairs
        from :meth:`Model.nullclines` or :meth:`Model.nullcline_sweep`."""
        rows = []
        for k, pair in enumerate(sets):
            for which, segs in enumerate(pair, start=1):
                segs = np.asarray(segs, dtype=float).reshape(-1, 4)
                rows.append(np.column_stack([np.full(len(segs), first + k, float),
                                             np.full(len(segs), which, float), segs]))
        table = np.vstack(rows) if rows else np.zeros((0, 6))
        names = ["set", "nullcline", "x1", "y1", "x2", "y2"]
        return cls({n: table[:, i] for i, n in enumerate(names)}, title=title)

    def to_csv(self, path: str | os.PathLike) -> None:
        """Write the table as CSV, every number exact (the desktop app's
        ``export FILE.csv analysis`` writes the same file)."""
        cols = [np.asarray(v, dtype=float) for v in self._data.values()]
        write_csv(path, self.columns, zip(*cols))

    def to_mat(self, path: str | os.PathLike) -> None:
        """Write the table as a MATLAB .mat file: one column vector per
        column (names made valid MATLAB names, see :func:`write_mat`), and
        the struct ``params`` when the table has parameter values (the
        desktop app's ``export FILE.mat analysis`` writes the same file)."""
        write_mat(path, self.columns, self._data.values(), self.params)

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
    """The analysis methods of [`Model`][xpplab.Model]."""

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
        """The Fourier series of a column over the last run, as XPPAUT's
        *Numerics > stocHastic > Fourier* computes it.

        Mode $k$ has the frequency $k/L$ in cycles per unit time, $L$ being
        the run's length.

        Args:
            column: an output column of the run.

        Returns:
            A [`Table`][xpplab.Table]: ``freq``, then the coefficients
            ``cos`` and ``sin`` of each mode.

        Raises:
            EngineError: there is no run yet, or no such column.

        Examples:
            $x = \\cos 2 \\pi t$ over 8 time units: all of it is in the mode
            of frequency 1.

            >>> m = xs.Model.from_text('''
            ... x' = 2*pi*y
            ... y' = -2*pi*x
            ... init x=1
            ... @ total=8, dt=0.01
            ... done
            ... ''')
            >>> sol = m.run()
            >>> f = m.fourier("X")
            >>> f
            <Table Fourier series of X: 399 rows, columns freq, cos, sin>
            >>> print(f"{f['freq'][8]:g} {f['cos'][8]:.3f}")
            1 1.000

        See also:
            [fourier](../reference/fourier.md) in the command window;
            [`power`][xpplab.Model.power], [`spectrum`][xpplab.Model.spectrum].
        """
        self._engine.call("xppe_fourier", _b(column))
        return self._result(f"Fourier series of {column}")

    def power(self, column: str) -> Table:
        """The Fourier series of a column over the last run, as amplitude
        and phase.

        Args:
            column: an output column of the run.

        Returns:
            A [`Table`][xpplab.Table]: ``freq`` (in cycles per unit time),
            ``amplitude`` and ``phase`` of each mode.

        Raises:
            EngineError: there is no run yet, or no such column.

        Examples:
            >>> m = xs.Model.from_text('''
            ... x' = 2*pi*y
            ... y' = -2*pi*x
            ... init x=1
            ... @ total=8, dt=0.01
            ... done
            ... ''')
            >>> sol = m.run()
            >>> p = m.power("X")
            >>> k = int(np.argmax(p["amplitude"][1:])) + 1
            >>> print(f"{p['freq'][k]:g} {p['amplitude'][k]:.3f}")
            1 1.000

        See also:
            [power](../reference/power.md) in the command window;
            [`fourier`][xpplab.Model.fourier].
        """
        self._engine.call("xppe_power", _b(column))
        return self._result(f"Fourier amplitude of {column}")

    def spectrum(self, column: str, column2: str | None = None, *,
                 kind: str = "psd", length: int | None = None,
                 window: str | int = "hamming") -> Table:
        """The spectral density of the last run by Welch's method, as
        XPPAUT's *Spec.dns* computes it: windows of ``length`` points that
        overlap by half.

        Args:
            column: an output column of the run.
            column2: a second column, for the cross spectrum and the
                coherence.
            kind: ``"psd"`` (the power spectral density of ``column``),
                ``"cross"`` or ``"coherence"``.
            length: the points in each window; by default 512, or fewer for
                a short run.
            window: ``"square"``, ``"parzen"``, ``"hamming"``,
                ``"bartlett"`` or ``"hann"``.

        Returns:
            A [`Table`][xpplab.Table]: ``freq`` in cycles per unit time and
            the spectrum.

        Raises:
            ValueError: ``"cross"`` or ``"coherence"`` without ``column2``.
            EngineError: there is no run yet, or no such column.

        Examples:
            >>> m = xs.Model.from_text('''
            ... x' = 2*pi*y
            ... y' = -2*pi*x
            ... init x=1
            ... @ total=8, dt=0.01
            ... done
            ... ''')
            >>> sol = m.run()
            >>> s = m.spectrum("X")
            >>> s
            <Table psd of X: 256 rows, columns freq, power>
            >>> print(f"{s['freq'][np.argmax(s['power'])]:.2f}")
            0.98

        See also:
            [spectrum](../reference/spectrum.md) in the command window;
            [`fourier`][xpplab.Model.fourier],
            [`correlation`][xpplab.Model.correlation].
        """
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
        """The histogram of a column of the last run, as XPPAUT's
        *Numerics > stocHastic > Histogram* computes it.

        Args:
            column: an output column of the run.
            bins: the number of bins.
            range: ``(low, high)``; by default the column's range in the run.
            condition: an expression in the syntax of an .ode file; only the
                points where it is not zero count, such as ``"V>0"``.

        Returns:
            A [`Table`][xpplab.Table]: ``bin`` (the left edge) and
            ``count``.

        Raises:
            EngineError: there is no run yet, or no such column.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=200)
            >>> h = m.histogram("V", bins=10)
            >>> h
            <Table Histogram of V: 11 rows, columns bin, count>
            >>> int(h["count"].sum()) == len(sol)
            True

        See also:
            [hist](../reference/hist.md) in the command window;
            [`histogram2d`][xpplab.Model.histogram2d].
        """
        lo, hi = self._range(column, range)
        self._engine.call("xppe_histogram", _b(column), int(bins), lo, hi,
                          _b(condition))
        return self._result(f"Histogram of {column}")

    def histogram2d(self, x: str, y: str, bins=(20, 20), range=None) -> Table:
        """The two-dimensional histogram of two columns of the last run.

        Args:
            x: the first column.
            y: the second column.
            bins: the number of bins, one number for both or ``(nx, ny)``.
            range: ``((xlow, xhigh), (ylow, yhigh))``; by default the
                columns' ranges in the run.

        Returns:
            A [`Table`][xpplab.Table], one row per cell: ``x``, ``y`` and
            ``count``.

        Raises:
            EngineError: there is no run yet, or no such column.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=200)
            >>> m.histogram2d("V", "W", bins=5)
            <Table Histogram of V and W: 25 rows, columns x, y, count>

        See also:
            [hist2d](../reference/hist2d.md) in the command window;
            [`histogram`][xpplab.Model.histogram].
        """
        nx, ny = (bins, bins) if np.isscalar(bins) else bins
        (xlo, xhi), (ylo, yhi) = ((self._range(x, None), self._range(y, None))
                                  if range is None else range)
        self._engine.call("xppe_histogram2d", _b(x), int(nx), float(xlo),
                          float(xhi), _b(y), int(ny), float(ylo), float(yhi))
        return self._result(f"Histogram of {x} and {y}")

    def correlation(self, column: str, column2: str | None = None, *,
                    lags: int = 100, fft: bool = False) -> Table:
        """The cross-correlation of two columns of the last run, or the
        autocorrelation of one, as XPPAUT's *stocHastic > Xcorrel* computes
        it.

        Args:
            column: an output column of the run.
            column2: a second column; by default ``column`` itself.
            lags: about how many lags each way.
            fft: compute it by the fast Fourier transform.

        Returns:
            A [`Table`][xpplab.Table]: ``lag`` in time units and
            ``correlation``.

        Raises:
            EngineError: there is no run yet, or no such column.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=200)
            >>> m.correlation("V", lags=50)
            <Table Autocorrelation of V: 52 rows, columns lag, correlation>

        See also:
            [xcorr](../reference/xcorr.md) in the command window;
            [`spectrum`][xpplab.Model.spectrum].
        """
        other = column2 or column
        self._engine.call("xppe_correlation", _b(column), _b(other), int(lags),
                          int(bool(fft)))
        what = (f"Autocorrelation of {column}" if column2 is None
                else f"Correlation of {column} and {column2}")
        return self._result(what)

    def difference_histogram(self, column: str, bins: int = 100,
                             range=None) -> Table:
        """The histogram of every difference between two points of a column
        of the last run, as XPPAUT's *stocHastic > stAutocor* computes it.

        Args:
            column: an output column of the run.
            bins: the number of bins.
            range: ``(low, high)``; by default minus to plus the column's
                spread in the run.

        Returns:
            A [`Table`][xpplab.Table]: ``difference`` and ``count``.

        Raises:
            EngineError: there is no run yet, or no such column.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=200)
            >>> m.difference_histogram("V", bins=10)
            <Table Differences of V: 11 rows, columns difference, count>

        See also:
            [`histogram`][xpplab.Model.histogram],
            [`correlation`][xpplab.Model.correlation].
        """
        if range is None:
            lo, hi = self._column_limits(column)
            d = (hi - lo) or 1.0
            range = (-d, d)
        lo, hi = range
        self._engine.call("xppe_autocorrelation", _b(column), int(bins),
                          float(lo), float(hi))
        return self._result(f"Differences of {column}")

    def mean(self, column: str) -> tuple[float, float]:
        """The mean and the standard deviation of a column over the last
        run, as XPPAUT's *stocHastic > Mean* computes them.

        Args:
            column: an output column of the run.

        Returns:
            ``(mean, standard deviation)``.

        Raises:
            EngineError: there is no run yet, or no such column.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=200)
            >>> mean, sd = m.mean("V")
            >>> print(f"{mean:.4f} {sd:.4f}")
            -0.1843 0.1819

        See also:
            [mean](../reference/mean.md) in the command window;
            [`histogram`][xpplab.Model.histogram].
        """
        m, s = ctypes.c_double(), ctypes.c_double()
        self._engine.call("xppe_mean", _b(column), ctypes.byref(m),
                          ctypes.byref(s))
        return m.value, s.value

    def lyapunov(self, *, stored: bool = False, **values) -> float:
        """The largest Liapunov exponent of a run from the current initial
        conditions.

        The engine integrates from the initial conditions for the total
        time, as [`run`][xpplab.Model.run] does, together with a copy of the state moved
        by ``JAC_EPS`` (0.001 unless set). After each output step (``dt``
        times ``nout``) it adds up the logarithm of the factor by which the
        distance between the two has grown, and puts the copy back at the
        distance ``JAC_EPS``, in the direction it has moved to. The exponent
        is the sum divided by the time. This is XPPAUT's method, computed in
        double precision during the integration. Steps that start before
        ``trans`` carry the perturbation along without counting, and the
        integration stops, as a run does, when a variable passes ``bound``.
        Nothing is stored: the last run stays as it was. Keyword arguments
        are applied with [`set`][xpplab.Model.set] first, as in [`run`][xpplab.Model.run], so after
        ``m.run(...)`` a plain ``m.lyapunov()`` follows the same trajectory.

        With a fixed-step method, a smaller ``jac_eps`` (such as 1e-6)
        comes closer to the exponent of the linearised equations. With an
        adaptive method keep ``tol`` well below ``jac_eps``: the error
        control and the corrector iterations otherwise move the two copies
        apart by about as much as the perturbation (CVODE at its default
        ``tol=0.001`` gives nonsense). Delay equations, integrals and noise
        are refused with an [`EngineError`][xpplab.EngineError].

        ``stored=True`` gives XPPAUT's own estimate instead (stocHastic >
        Liapunov), along the stored last run: each step starts from a stored
        point and is compared with the next one. XPPAUT stores runs, times
        included, in single precision (about seven digits). Once the time
        or a variable is large, as in a run to t = 1000, the rounding of the
        stored values and of the step length is comparable to the
        perturbation and that estimate goes wrong. A folded variable
        (``fold=``) jumps by its period at every turn, which spoils it too.

        Args:
            stored: use XPPAUT's own estimate along the stored last run.
            **values: set first, as by [`set`][xpplab.Model.set], and kept.

        Returns:
            The exponent, per unit time.

        Raises:
            EngineError: a delay equation, an integral or noise.
            TypeError: ``stored=True`` with values.

        Examples:
            For $x' = -x$ every perturbation shrinks as $e^{-t}$:

            >>> m = xs.Model.from_text('''
            ... x' = -x
            ... init x=1
            ... @ total=20
            ... done
            ... ''')
            >>> round(m.lyapunov(), 4)
            -1.0

            The Lorenz attractor's is about 0.9:

            >>> m = xs.load(xs.example("classics/lorenz"))
            >>> bool(0.8 < m.lyapunov(total=500, trans=50) < 1.0)
            True

        See also:
            [mean](../reference/mean.md) in the command window, whose page
            covers the exponent too; [`sweep`][xpplab.Model.sweep], to map
            it over a parameter.
        """
        z = ctypes.c_double()
        if stored:
            if values:
                raise TypeError("lyapunov(stored=True) uses the last run; "
                                "give it no values")
            self._engine.call("xppe_lyapunov", ctypes.byref(z))
            return z.value
        if values:
            self.set(**values)
        self._engine.call("xppe_lyapunov_integrate", 0.0, ctypes.byref(z))
        return z.value

    # -- Periodic orbits, adjoints and phase response --------------------------

    def limit_cycle(self, *, transient: float | None = None,
                    variable: str | None = None, period: float | None = None,
                    total: float | None = None):
        """Find one period of a stable limit cycle and leave it as the run.

        The model is integrated for ``transient`` time units to settle onto
        the cycle; the period is measured from the upward crossings of
        ``variable`` through the middle of its range, over the last few
        cycles; then exactly one period is integrated from a point on the
        cycle, which is what XPPAUT's adjoint needs. If a run shows too few
        cycles, the times are doubled, up to five times. The initial
        conditions and ``TOTAL`` are restored afterwards.

        Args:
            transient: how long to settle; by default ``TOTAL``.
            variable: the variable whose crossings give the period; by
                default the first.
            period: the period, if it is known; then it is not measured.
            total: how long the run that measures the period is; by default
                the longer of ``TOTAL`` and ``transient``.

        Returns:
            ``(period, orbit)``: the period and the one-period run, a
            [`Trajectory`][xpplab.Trajectory].

        Raises:
            EngineError: no oscillation was found, or it is not periodic
                yet; the message says which.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> period, orbit = m.set(iapp=0.1).limit_cycle()
            >>> print(f"{period:.4f}")
            16.4745
            >>> orbit
            <Trajectory: 330 points, t=0..16.45, columns T, V, W, CUR, ICA, IK, RATE>

        See also:
            [`phase_response`][xpplab.Model.phase_response],
            [`adjoint`][xpplab.Model.adjoint];
            [`auto.continue_from`][xpplab.auto.Auto.continue_from], which
            follows limit cycles, unstable ones too.
        """
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
        """The adjoint along the last run, by XPPAUT's own routine, as its
        *Numerics > Averaging > New adjoint* computes it.

        The last run must be exactly one period of a stable limit cycle, as
        [`limit_cycle`][xpplab.Model.limit_cycle] leaves it. The adjoint is
        normalised as XPPAUT does, so that its dot product with the vector
        field is 1: each column is the infinitesimal phase response curve to
        perturbations of that variable, in time units.

        Args:
            max_iterations: the iterations of XPPAUT's method; 0 keeps
                XPPAUT's setting.
            tolerance: its tolerance; 0 keeps XPPAUT's setting.

        Returns:
            A [`Table`][xpplab.Table]: ``T``, then one column per variable.

        Raises:
            EngineError: there is no run, or the method did not converge.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> period, orbit = m.set(iapp=0.1).limit_cycle()
            >>> m.adjoint()
            <Table Adjoint: 330 rows, columns T, V, W>

        See also:
            [prc](../reference/prc.md) in the command window;
            [`phase_response`][xpplab.Model.phase_response], which does
            both steps; [`h_function`][xpplab.Model.h_function].
        """
        self._engine.call("xppe_adjoint", int(max_iterations), float(tolerance))
        return self._result("Adjoint")

    def phase_response(self, *, transient: float | None = None,
                       variable: str | None = None,
                       period: float | None = None) -> Table:
        """The infinitesimal phase response curves of a stable limit cycle.

        One period is found with [`limit_cycle`][xpplab.Model.limit_cycle]
        and the [`adjoint`][xpplab.Model.adjoint] is computed along it. The
        period stays as the engine's last run.

        Args:
            transient: as for [`limit_cycle`][xpplab.Model.limit_cycle].
            variable: as for [`limit_cycle`][xpplab.Model.limit_cycle].
            period: as for [`limit_cycle`][xpplab.Model.limit_cycle].

        Returns:
            A [`Table`][xpplab.Table]: ``phase`` (0 to 1), ``T``, then one
            column per variable; ``period`` holds the period.

        Raises:
            EngineError: no limit cycle was found, or the adjoint did not
                converge.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> prc = m.set(iapp=0.1).phase_response()
            >>> prc
            <Table Phase response: 330 rows, columns phase, T, V, W>
            >>> print(f"{prc.period:.4f}")
            16.4745

        See also:
            [prc](../reference/prc.md) in the command window;
            [`h_function`][xpplab.Model.h_function].
        """
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
        """The averaged interaction function $H(\\phi)$ of two weakly
        coupled copies of the oscillator, as XPPAUT's *Averaging > Make H*
        computes it.

        It needs the [`adjoint`][xpplab.Model.adjoint] (or
        [`phase_response`][xpplab.Model.phase_response]) on the same run
        first.

        Args:
            coupling: one formula per variable, as a dictionary (the
                variables left out get 0) or a list in the model's order. A
                primed name is the other oscillator's variable:
                ``{"V": "V'-V"}`` couples the two through ``V``, diffusively.

        Returns:
            A [`Table`][xpplab.Table]: ``phi`` (in time units, over one
            period), ``H``, and its odd and even parts ``Hodd`` and
            ``Heven``.

        Raises:
            EngineError: no adjoint of this run, or a formula XPPAUT cannot
                parse.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> prc = m.set(iapp=0.1).phase_response()
            >>> H = m.h_function({"V": "V'-V"})
            >>> H
            <Table H function: 330 rows, columns phi, H, Hodd, Heven>

        See also:
            [hfun](../reference/hfun.md) in the command window;
            [`phase_response`][xpplab.Model.phase_response].
        """
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
        method, as its *Bndryval* does.

        The boundary conditions are the model's ``bndry`` (``b``) lines, in
        which a primed name means the variable at the right end. The
        problem is posed on $[T_0, T_0 + \\mathrm{TOTAL}]$, and the initial
        conditions are the first guess. On success they hold the solution's.

        Args:
            max_iterations: Newton's iterations; 0 keeps the model's
                setting.
            tolerance: Newton's tolerance; 0 keeps the model's setting.
            epsilon: the step of the numerical Jacobian; 0 keeps the
                model's setting.

        Returns:
            The solution, as a [`Trajectory`][xpplab.Trajectory].

        Raises:
            EngineError: the model has no boundary conditions, or the
                method did not converge.

        Examples:
            $x'' = -x$ with $x(0) = 0$ and $x(\\pi/2) = 1$ is solved by
            $x = \\sin t$, so $x'(0) = 1$:

            >>> m = xs.Model.from_text('''
            ... x' = y
            ... y' = -x
            ... b x
            ... b x'-1
            ... init x=0, y=0.5
            ... @ total=1.5707963267948966
            ... done
            ... ''')
            >>> sol = m.bvp()
            >>> print(f"{m.get('y'):.3f} {sol['X'][-1]:.3f}")
            1.000 1.000

        See also:
            [bvp](../reference/bvp.md) in the command window;
            [`auto.start`][xpplab.auto.Auto.start], which continues
            boundary-value problems with AUTO.
        """
        if self._engine.call("xppe_bvp_count", check=False) <= 0:
            raise E.EngineError("the model has no boundary conditions "
                                "(bndry statements)")
        self._engine.call("xppe_bvp", int(max_iterations), float(tolerance),
                          float(epsilon))
        return self._collect()

    def animation(self, path):
        """An XPPAUT animation of the last run, from an .ani file.

        Args:
            path: the .ani file; a relative path is taken next to the
                model.

        Returns:
            An [`Animation`][xpplab.Animation], whose frames follow the run.

        Raises:
            EngineError: XPPAUT cannot read the file; the message gives the
                line.

        Examples:
            >>> m = xs.load(xs.example("pend.ode"))
            >>> sol = m.run()
            >>> m.animation("pend.ani")
            <Animation pend.ani: 2 commands, 401 frames>

        See also:
            [ani](../reference/ani.md) in the command window.
        """
        from .animation import Animation
        p = Path(path)
        if not p.is_absolute():
            p = self.path.parent / p
        return Animation(self, p)

    # -- Curve fitting -------------------------------------------------------

    def fit(self, data, *, variables, parameters, columns=None,
            tolerance: float = 0.0, epsilon: float = 0.0,
            max_iterations: int = 0) -> dict[str, float]:
        """Fit parameters and initial conditions to data, by XPPAUT's
        Levenberg-Marquardt method, as its *Numerics > stocHastic > fIt data*
        does.

        The values to vary start from their current ones and end at the
        fitted ones. The initial conditions apply at the first data time.

        Args:
            data: a file of rows, time first, or an array with time in its
                first column. The file may have a header and commas, as the
                CSV [`Trajectory.to_csv`][xpplab.Trajectory.to_csv] and the
                app's ``export`` write.
            variables: the variables to fit, one name or a list.
            parameters: the parameters and initial conditions to vary, one
                name or a list.
            columns: the data's column for each variable, counting from 1
                for time; by default 2, 3, ...
            tolerance: the method's tolerance; 0 keeps XPPAUT's setting.
            epsilon: the step of its numerical derivatives; 0 keeps
                XPPAUT's setting.
            max_iterations: its iterations; 0 keeps XPPAUT's setting.

        Returns:
            The fitted values, by name.

        Raises:
            EngineError: the data cannot be read, or the method failed.
            ValueError: an array that is not two-dimensional.

        Examples:
            A run with ``phi=0.25`` is the data; the fit starts from
            ``phi=0.333`` and finds 0.25 again:

            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(phi=0.25, total=100)
            >>> data = np.column_stack([sol.t, sol["V"]])
            >>> fitted = m.set(phi=0.333).fit(data, variables="V", parameters="phi")
            >>> print(f"{fitted['phi']:.6f}")
            0.250000

        See also:
            [fitdata](../reference/fitdata.md) and [fit](../reference/fit.md)
            in the command window.
        """
        variables = [variables] if isinstance(variables, str) else list(variables)
        parameters = [parameters] if isinstance(parameters, str) else list(parameters)
        if columns is None:
            columns = list(range(2, 2 + len(variables)))
        if isinstance(data, (str, os.PathLike)):
            # The engine counts the rows, and reads a header and commas
            # too, as in the CSV that to_csv() and the app's export write.
            path = Path(data).resolve()
            shape = (0, 0)
        else:
            rows = np.asarray(data, dtype=float)
            if rows.ndim != 2:
                raise ValueError("data must be a 2-D array: time, then columns")
            path = Path(self._engine.workdir) / "fit-array.dat"
            np.savetxt(path, rows, fmt="%.17g")
            shape = rows.shape
        self._engine.call("xppe_fit", _b(path), shape[0], shape[1],
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
