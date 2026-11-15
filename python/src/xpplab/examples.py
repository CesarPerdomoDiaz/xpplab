"""Where the example models are, and :func:`example` to get one by name."""
from __future__ import annotations

import difflib
import os
from pathlib import Path

_PACKAGE = Path(__file__).resolve().parent


def example_dirs() -> list[Path]:
    """The folders that may hold the example models, best first:
    ``$XPPLAB_EXAMPLES``, the copy inside the installed package, and
    ``engine/xppaut/ode`` in a source checkout. Only folders with at least
    one ``.ode`` file are listed."""
    env = os.environ.get("XPPLAB_EXAMPLES")
    cands = [Path(env)] if env else []
    cands += [_PACKAGE / "examples", _PACKAGE.parents[2] / "engine" / "xppaut" / "ode"]
    return [c for c in cands if c.is_dir() and any(c.glob("*.ode"))]


def classics() -> list[Path]:
    """The classic models, sorted by name: familiar systems, each with a
    description in its first lines (the documentation's gallery shows
    them)."""
    for d in example_dirs() + [_PACKAGE.parents[2] / "examples"]:
        found = sorted((d / "classics").glob("*.ode"))
        if found:
            return found
    return []


def example(name: str) -> Path:
    """The path of a shipped example model, to pass to :func:`xpplab.load`::

        m = xs.load(xs.example("lecar.ode"))

    ``name`` is the file name; the ``.ode`` ending may be left out. The
    classic models (the Lorenz attractor, the van der Pol oscillator, ...)
    are in the folder ``classics``::

        m = xs.load(xs.example("classics/lorenz"))

    Raises ``FileNotFoundError`` when there is no such model, naming the
    closest ones."""
    name = str(name)
    dirs = example_dirs()
    # in a source checkout the classics are in examples/classics
    for d in dirs + [_PACKAGE.parents[2] / "examples"]:
        for cand in (name, name + ".ode"):
            p = d / cand
            if p.is_file():
                return p
    known = sorted({p.name for d in dirs for p in d.glob("*.ode")}
                   | {"classics/" + p.name for p in classics()})
    stems = {k[:-4]: k for k in known}
    key = name[:-4] if name.endswith(".ode") else name
    close = [stems[s] for s in difflib.get_close_matches(key, list(stems), n=5, cutoff=0.5)]
    where = ", ".join(str(d) for d in dirs) or "no example folder found"
    hint = f"; close names: {', '.join(close)}" if close else ""
    raise FileNotFoundError(f"no example model {name!r} (looked in {where}){hint}")
