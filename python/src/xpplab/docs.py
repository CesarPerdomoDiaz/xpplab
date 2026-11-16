"""xs.doc(): open the reference page of a command or of a method, as the
desktop app's ``doc NAME`` does."""
from __future__ import annotations

import webbrowser

SITE = "https://cesarperdomodiaz.github.io/xpplab/"

#: The reference pages (docs/reference), by the command's name.
PAGES = ("auto steady", "auto from", "auto set", "auto orbit", "auto save",
         "auto load", "auto reset", "run", "eq", "eq all", "nullclines", "range",
         "continue", "ic last", "ic shift", "whos",
         "ic read", "ic write", "use", "sets",
         "dirfield", "click", "freeze", "erase",
         "mean", "hist", "xcorr", "spectrum", "hist2d", "bvp",
         "nullclines freeze", "nullclines play", "nullclines unfreeze",
         "export", "shoot", "view3d", "ani", "prc", "hfun", "fourier", "power", "script", "echo", "many", "set", "get", "open", "new", "save", "doc", "help",
         "show", "hide", "layout", "theme", "palette", "clear", "quit", "annotate",
         "ensemble", "linearize", "poles", "lqr", "locus", "closedloop", "ic", "fit", "fitdata",
         "kinescope capture", "kinescope play", "kinescope clear", "py", "python",
         "serve", "figure")

#: The method of the package that does what a command does.
#: A method of a class, ``Class.method``, wins over a bare name.
METHODS = {"equilibria": "auto steady", "continue_from": "auto from",
           "orbit": "auto orbit", "Auto.save": "auto save", "Auto.load": "auto load",
           "Auto.set": "auto set", "reset": "auto reset", "Model.run": "run",
           "equilibrium": "eq", "find_equilibria": "eq all",
           "Model.nullclines": "nullclines",
           "sweep": "range", "sweep_grid": "range",
           "continue_run": "continue", "ic_last": "ic last",
           "read_ics": "ic read", "write_ics": "ic write",
           "apply_set": "use",
           "vector_field": "dirfield",
           "mean": "mean", "lyapunov": "mean",
           "histogram": "hist",
           "correlation": "xcorr",
           "spectrum": "spectrum",
           "histogram2d": "hist2d",
           "bvp": "bvp",
           "nullcline_sweep": "nullclines freeze",
           "to_csv": "export", "to_mat": "export",
           "shoot": "shoot", "plot3d": "view3d",
           "animation": "ani",
           "phase_response": "prc", "adjoint": "prc",
           "h_function": "hfun",
           "fourier": "fourier", "power": "power",
           "run_many": "many", "sample_initial": "many",
           "Model.set": "set", "Model.get": "get",
           "load": "open", "from_text": "new",
           "ensemble": "ensemble", "linearize": "linearize", "place_poles": "poles",
           "lqr": "lqr", "root_locus": "locus", "closed_loop": "closedloop",
           "ic_formula": "ic", "fit": "fitdata",
           "connect": "serve", "Figure": "figure"}


def doc(name=None, *, open: bool = True) -> str:
    """Open the reference page of a command in the browser, and return its
    address, as the desktop app's ``doc NAME`` does.

    Args:
        name: a command of the desktop app's command window, such as
            ``"auto steady"``; or a method of the package that does the
            same, as a name or as the method itself. A name with no page,
            or none, opens the index of the reference.
        open: open the page; ``False`` only returns the address.

    Returns:
        The page's address.

    Examples:
        >>> xs.doc("auto steady", open=False)
        'https://cesarperdomodiaz.github.io/xpplab/reference/auto-steady/'
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> xs.doc(m.auto.equilibria, open=False)
        'https://cesarperdomodiaz.github.io/xpplab/reference/auto-steady/'
        >>> xs.doc("sweep", open=False)
        'https://cesarperdomodiaz.github.io/xpplab/reference/range/'
        >>> xs.doc(open=False)
        'https://cesarperdomodiaz.github.io/xpplab/reference/reference/'

    See also:
        [doc](../reference/doc.md) in the command window.
    """
    page = "reference"
    if name is not None:
        if isinstance(name, str):
            # A command's name opens its page, though a method may share it
            # (save, fit); otherwise it may be a method's name.
            key = " ".join(name.lower().split())
            if key not in PAGES:
                key = METHODS.get(name.strip(), key)
        else:
            # A method: by its class first (Auto.load is not xs.load).
            qual = getattr(name, "__qualname__", "")
            key = METHODS.get(qual, METHODS.get(getattr(name, "__name__", ""), ""))
        if key in PAGES:
            page = key.replace(" ", "-")
    url = f"{SITE}reference/{page}/"
    if open:
        webbrowser.open(url)
    return url
