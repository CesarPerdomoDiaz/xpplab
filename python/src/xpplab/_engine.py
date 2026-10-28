"""Low-level access to the XPPAUT engine library (engine/api/xppe.h).

XPPAUT keeps its state in global variables, so one loaded copy of the
engine library holds one model. Each Engine therefore loads its own private
copy of the library file. The copies are independent: several models can
be open at once, and closing one frees its memory.
"""

from __future__ import annotations

import ctypes
import os
import shutil
import sys
import tempfile
import threading
from contextlib import contextmanager
from pathlib import Path

__all__ = ["Engine", "EngineError", "find_engine_library"]

API_VERSION = 8

OK = 0
ERR_STATE = -1
ERR_LOAD = -2
ERR_FATAL = -3
ERR_NAME = -4
ERR_RANGE = -5
ERR_NUMERIC = -6

VARIABLES = 0
PARAMETERS = 1
COLUMNS = 2
SETS = 3

AUTO_U0 = 0
AUTO_UMAX = 1
AUTO_UMIN = 2
AUTO_UMEAN = 3
AUTO_EIG_RE = 4
AUTO_EIG_IM = 5
AUTO_PARAMS = 6

_ERROR_NAMES = {
    ERR_STATE: "state",
    ERR_LOAD: "load",
    ERR_FATAL: "fatal",
    ERR_NAME: "name",
    ERR_RANGE: "range",
    ERR_NUMERIC: "numeric",
}


class EngineError(RuntimeError):
    """An error reported by the XPPAUT engine.

    ``log`` holds everything the engine printed during the failing call,
    which usually explains the problem (for example a syntax error in the
    model file).
    """

    def __init__(self, message: str, code: int = 0, log: str = ""):
        super().__init__(message)
        self.code = code
        self.kind = _ERROR_NAMES.get(code, "error")
        self.log = log

    def __str__(self) -> str:
        text = super().__str__()
        if self.log.strip():
            tail = "\n".join(self.log.strip().splitlines()[-12:])
            text += "\n\nEngine output (last lines):\n" + tail
        return text


class AutoPointInfo(ctypes.Structure):
    _fields_ = [
        ("branch", ctypes.c_int),
        ("point", ctypes.c_int),
        ("type", ctypes.c_int),
        ("label", ctypes.c_int),
        ("stable", ctypes.c_int),
        ("periodic", ctypes.c_int),
        ("two_parameter", ctypes.c_int),
        ("par1_slot", ctypes.c_int),
        ("par2_slot", ctypes.c_int),
        ("par1", ctypes.c_double),
        ("par2", ctypes.c_double),
        ("norm", ctypes.c_double),
        ("period", ctypes.c_double),
        ("type_name", ctypes.c_char * 4),
    ]


class AniItem(ctypes.Structure):
    """One shape of an animation frame (xppe_ani_item in xppe.h)."""
    _fields_ = [
        ("shape", ctypes.c_int),
        ("command", ctypes.c_int),
        ("filled", ctypes.c_int),
        ("x1", ctypes.c_double),
        ("y1", ctypes.c_double),
        ("x2", ctypes.c_double),
        ("y2", ctypes.c_double),
        ("color", ctypes.c_int),
        ("level", ctypes.c_double),
        ("thickness", ctypes.c_int),
        ("trail", ctypes.c_int),
        ("font_size", ctypes.c_int),
        ("serif", ctypes.c_int),
        ("text", ctypes.c_char * 128),
    ]


def _library_name() -> str:
    if sys.platform == "win32":
        return "libxppe.dll"
    if sys.platform == "darwin":
        return "libxppe.dylib"
    return "libxppe.so"


def find_engine_library() -> Path:
    """Locate the engine library.

    The XPPLAB_ENGINE environment variable wins. Otherwise use the copy
    installed inside the package, then a development build in the
    repository's build/ directory.
    """
    env = os.environ.get("XPPLAB_ENGINE")
    if env:
        return Path(env)
    here = Path(__file__).resolve().parent
    candidates = [here / "_lib" / _library_name()]
    repo = here.parents[2]
    candidates += [repo / d / "engine" / _library_name()
                   for d in ("build", "build-release", "build-debug")]
    for c in candidates:
        if c.exists():
            return c
    raise EngineError(
        "the XPP Lab engine library was not found; set XPPLAB_ENGINE "
        "to the path of " + _library_name())


_SIGNATURES = {
    "xppe_api_version": ([], ctypes.c_int),
    "xppe_version": ([], ctypes.c_char_p),
    "xppe_load": ([ctypes.c_char_p, ctypes.c_int,
                   ctypes.POINTER(ctypes.c_char_p)], ctypes.c_int),
    "xppe_log": ([], ctypes.c_char_p),
    "xppe_error": ([], ctypes.c_char_p),
    "xppe_count": ([ctypes.c_int], ctypes.c_int),
    "xppe_name": ([ctypes.c_int, ctypes.c_int], ctypes.c_char_p),
    "xppe_get": ([ctypes.c_char_p, ctypes.POINTER(ctypes.c_double)],
                 ctypes.c_int),
    "xppe_set": ([ctypes.c_char_p, ctypes.c_char_p], ctypes.c_int),
    "xppe_set_number": ([ctypes.c_char_p, ctypes.c_double], ctypes.c_int),
    "xppe_apply_set": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_run": ([], ctypes.c_int),
    "xppe_rows": ([], ctypes.c_int),
    "xppe_column": ([ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                     ctypes.c_int], ctypes.c_int),
    "xppe_equilibrium": ([ctypes.POINTER(ctypes.c_double)] * 3, ctypes.c_int),
    "xppe_rhs": ([ctypes.c_double, ctypes.POINTER(ctypes.c_double),
                  ctypes.POINTER(ctypes.c_double)], ctypes.c_int),
    "xppe_columns_at": ([ctypes.c_double, ctypes.POINTER(ctypes.c_double),
                         ctypes.POINTER(ctypes.c_double), ctypes.c_int],
                        ctypes.c_int),
    "xppe_flag_count": ([], ctypes.c_int),
    "xppe_flag_condition": ([ctypes.c_int, ctypes.POINTER(ctypes.c_int)],
                            ctypes.c_char_p),
    "xppe_halt_flag": ([], ctypes.c_int),
    "xppe_manifold_count": ([], ctypes.c_int),
    "xppe_manifold": ([ctypes.c_int, ctypes.POINTER(ctypes.c_int)], ctypes.c_int),
    "xppe_equilibrium_sweep": ([ctypes.c_char_p, ctypes.POINTER(ctypes.c_double),
                                ctypes.c_int, ctypes.POINTER(ctypes.c_double)],
                               ctypes.c_int),
    "xppe_find_equilibria": ([ctypes.c_int, ctypes.c_double,
                              ctypes.POINTER(ctypes.c_double),
                              ctypes.POINTER(ctypes.c_double)], ctypes.c_int),
    "xppe_found_equilibrium": ([ctypes.c_int] + [ctypes.POINTER(ctypes.c_double)] * 3,
                               ctypes.c_int),
    "xppe_linearize": ([ctypes.POINTER(ctypes.c_double), ctypes.c_int,
                        ctypes.POINTER(ctypes.c_char_p), ctypes.c_double]
                       + [ctypes.POINTER(ctypes.c_double)] * 4, ctypes.c_int),
    "xppe_eigenvalues": ([ctypes.c_int] + [ctypes.POINTER(ctypes.c_double)] * 3,
                         ctypes.c_int),
    "xppe_eigenvector": ([ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                          ctypes.c_double, ctypes.POINTER(ctypes.c_double)],
                         ctypes.c_int),
    "xppe_controllability": ([ctypes.c_int, ctypes.c_int,
                              ctypes.POINTER(ctypes.c_double),
                              ctypes.POINTER(ctypes.c_double), ctypes.c_double,
                              ctypes.POINTER(ctypes.c_double),
                              ctypes.POINTER(ctypes.c_double)], ctypes.c_int),
    "xppe_fourier": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_power": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_spectrum": ([ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p,
                       ctypes.c_int, ctypes.c_int], ctypes.c_int),
    "xppe_histogram": ([ctypes.c_char_p, ctypes.c_int, ctypes.c_double,
                        ctypes.c_double, ctypes.c_char_p], ctypes.c_int),
    "xppe_histogram2d": ([ctypes.c_char_p, ctypes.c_int, ctypes.c_double,
                          ctypes.c_double, ctypes.c_char_p, ctypes.c_int,
                          ctypes.c_double, ctypes.c_double], ctypes.c_int),
    "xppe_autocorrelation": ([ctypes.c_char_p, ctypes.c_int, ctypes.c_double,
                              ctypes.c_double], ctypes.c_int),
    "xppe_correlation": ([ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                          ctypes.c_int], ctypes.c_int),
    "xppe_mean": ([ctypes.c_char_p, ctypes.POINTER(ctypes.c_double),
                   ctypes.POINTER(ctypes.c_double)], ctypes.c_int),
    "xppe_lyapunov": ([ctypes.POINTER(ctypes.c_double)], ctypes.c_int),
    "xppe_adjoint": ([ctypes.c_int, ctypes.c_double], ctypes.c_int),
    "xppe_hfunction": ([ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)],
                       ctypes.c_int),
    "xppe_fit": ([ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_char_p,
                  ctypes.c_char_p, ctypes.c_char_p, ctypes.c_double,
                  ctypes.c_double, ctypes.c_int], ctypes.c_int),
    "xppe_result_rows": ([], ctypes.c_int),
    "xppe_result_count": ([], ctypes.c_int),
    "xppe_result_name": ([ctypes.c_int], ctypes.c_char_p),
    "xppe_result_column": ([ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                            ctypes.c_int], ctypes.c_int),
    "xppe_ic_last": ([], ctypes.c_int),
    "xppe_ic_shift": ([], ctypes.c_int),
    "xppe_ic_file": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_ic_formula": ([ctypes.c_char_p, ctypes.c_char_p], ctypes.c_int),
    "xppe_ensemble": ([ctypes.c_int], ctypes.c_int),
    "xppe_ensemble_column": ([ctypes.c_int, ctypes.c_int,
                              ctypes.POINTER(ctypes.c_double), ctypes.c_int],
                             ctypes.c_int),
    "xppe_bvp_count": ([], ctypes.c_int),
    "xppe_bvp": ([ctypes.c_int, ctypes.c_double, ctypes.c_double], ctypes.c_int),
    "xppe_ani_load": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_ani_window": ([ctypes.POINTER(ctypes.c_double)] * 4, ctypes.c_int),
    "xppe_ani_frame": ([ctypes.c_int], ctypes.c_int),
    "xppe_ani_get": ([ctypes.c_int, ctypes.POINTER(AniItem)], ctypes.c_int),
    "xppe_nullcline": ([ctypes.c_int, ctypes.c_int, ctypes.c_int,
                        ctypes.c_double, ctypes.c_double, ctypes.c_double,
                        ctypes.c_double, ctypes.c_int,
                        ctypes.POINTER(ctypes.c_float), ctypes.c_int],
                       ctypes.c_int),
    "xppe_auto_get": ([ctypes.c_char_p, ctypes.POINTER(ctypes.c_double)],
                      ctypes.c_int),
    "xppe_auto_set": ([ctypes.c_char_p, ctypes.c_double], ctypes.c_int),
    "xppe_auto_parameters": ([ctypes.c_char_p, ctypes.c_char_p], ctypes.c_int),
    "xppe_auto_set_workdir": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_auto_reset": ([], ctypes.c_int),
    "xppe_auto_run": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_auto_run_form": ([ctypes.c_char_p, ctypes.c_char_p], ctypes.c_int),
    "xppe_auto_grab": ([ctypes.c_int], ctypes.c_int),
    "xppe_auto_ungrab": ([], ctypes.c_int),
    "xppe_auto_save": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_auto_load": ([ctypes.c_char_p], ctypes.c_int),
    "xppe_auto_orbit": ([ctypes.c_int], ctypes.c_int),
    "xppe_auto_parameter_count": ([], ctypes.c_int),
    "xppe_auto_parameter_name": ([ctypes.c_int], ctypes.c_char_p),
    "xppe_auto_main_slot": ([ctypes.c_int], ctypes.c_int),
    "xppe_auto_count": ([], ctypes.c_int),
    "xppe_auto_point": ([ctypes.c_int, ctypes.POINTER(AutoPointInfo)],
                        ctypes.c_int),
    "xppe_auto_point_data": ([ctypes.c_int, ctypes.c_int,
                              ctypes.POINTER(ctypes.c_double), ctypes.c_int],
                             ctypes.c_int),
}

# The engine reads and writes files relative to the working directory, and
# AUTO uses the HOME environment variable. Both are process-wide, so engine
# calls are serialised.
_process_lock = threading.RLock()


def _text(b: bytes | None) -> str:
    return b.decode("latin-1") if b else ""


class Engine:
    """One private copy of the engine library, holding at most one model."""

    def __init__(self, library: str | os.PathLike | None = None):
        source = Path(library) if library else find_engine_library()
        self._tmpdir = Path(tempfile.mkdtemp(prefix="xpplab-"))
        self._path = self._tmpdir / source.name
        shutil.copy2(source, self._path)
        self.workdir = self._tmpdir / "work"
        self.workdir.mkdir()
        self._lib = ctypes.CDLL(str(self._path))
        for name, (args, res) in _SIGNATURES.items():
            fn = getattr(self._lib, name)
            fn.argtypes = args
            fn.restype = res
        if self._lib.xppe_api_version() != API_VERSION:
            raise EngineError("engine library has an incompatible API version")
        self.cwd: Path | None = None
        self._closed = False
        self._batch = 0

    # -- plumbing ---------------------------------------------------------

    @contextmanager
    def batch(self):
        """Hold the engine for many calls in a row (reading a diagram point by
        point): one change of directory instead of two per call."""
        with _process_lock:
            old = os.getcwd()
            if self.cwd is not None:
                os.chdir(self.cwd)
            self._batch += 1
            try:
                yield self
            finally:
                self._batch -= 1
                os.chdir(old)

    def call(self, name: str, *args, check: bool = True):
        """Call an xppe_* function inside the model's working directory and
        raise EngineError on failure."""
        if self._closed:
            raise EngineError("this model has been closed")
        fn = getattr(self._lib, name)
        with _process_lock:
            if self._batch:
                rc = fn(*args)
            else:
                old = os.getcwd()
                if self.cwd is not None:
                    os.chdir(self.cwd)
                try:
                    rc = fn(*args)
                finally:
                    os.chdir(old)
        if check and isinstance(rc, int) and rc < 0:
            raise EngineError(_text(self._lib.xppe_error()) or name + " failed",
                              rc, self.log)
        return rc

    @property
    def log(self) -> str:
        return _text(self._lib.xppe_log())

    @property
    def version(self) -> str:
        return _text(self._lib.xppe_version())

    def names(self, kind: int) -> list[str]:
        n = self._lib.xppe_count(kind)
        return [_text(self._lib.xppe_name(kind, i)) for i in range(n)]

    def close(self) -> None:
        """Unload this copy of the engine and delete its files."""
        if self._closed:
            return
        self._closed = True
        handle = self._lib._handle
        del self._lib
        try:
            if sys.platform == "win32":
                ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(handle))
            else:
                ctypes.CDLL(None).dlclose(ctypes.c_void_p(handle))
        except Exception:
            pass
        shutil.rmtree(self._tmpdir, ignore_errors=True)

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass
