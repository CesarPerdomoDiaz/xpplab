"""XPP Lab: XPPAUT's simulation and AUTO bifurcation engine for Python.

    import xpplab as xs
    m = xs.load("lecar.ode")
    sol = m.run(total=100)
    d = m.auto.equilibria("iapp")
    d.special()

The numerics are XPPAUT 8.0's own code, compiled into a native
library. No Fortran or C compiler and no X11 server are needed.
"""

from ._engine import EngineError, find_engine_library
from .analysis import Table
from .animation import Animation
from .auto import Diagram, Point
from .control import Linearization
from .sensitivity import Sensitivity
from .model import METHODS, Ensemble, Equilibrium, EquilibriumList, Model, Trajectory, load
from . import app  # xs.app.connect(): drive the running desktop app

__all__ = ["Model", "Trajectory", "Ensemble", "Equilibrium", "EquilibriumList", "Linearization",
           "Diagram", "Point", "Table", "Animation",
           "EngineError", "METHODS", "load", "find_engine_library", "app",
           "__version__"]

__version__ = "0.1.2"
