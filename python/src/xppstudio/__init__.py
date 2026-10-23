"""XPP Studio: XPPAUT's simulation and AUTO bifurcation engine for Python.

    import xppstudio as xs
    m = xs.load("lecar.ode")
    sol = m.run(total=100)
    d = m.auto.equilibria("iapp")
    d.special()

The numerics are XPPAUT 8.0's own code, unchanged, compiled into a native
library. No Fortran or C compiler and no X11 server are needed.
"""

from ._engine import EngineError, find_engine_library
from .auto import Diagram, Point
from .model import METHODS, Equilibrium, Model, Trajectory, load

__all__ = ["Model", "Trajectory", "Equilibrium", "Diagram", "Point",
           "EngineError", "METHODS", "load", "find_engine_library",
           "__version__"]

__version__ = "0.1.0.dev0"
