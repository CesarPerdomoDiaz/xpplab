"""xs.doc(): open the reference page of a command or of a method, as the
desktop app's ``doc NAME`` does."""
from __future__ import annotations

import webbrowser

SITE = "https://cesarperdomodiaz.github.io/xpplab/"

#: The reference pages (docs/reference), by the command's name.
PAGES = ("auto steady", "auto from", "auto set", "auto orbit", "auto save",
         "auto load", "auto reset")

#: The method of the package that does what a command does.
METHODS = {"equilibria": "auto steady", "continue_from": "auto from",
           "orbit": "auto orbit", "save": "auto save", "load": "auto load",
           "reset": "auto reset"}


def doc(name=None, *, open: bool = True) -> str:
    """Open the reference page of a command or action in the browser, and
    return its address::

        xs.doc("auto steady")         # Follow steady states
        xs.doc(m.auto.equilibria)     # the same page, from the method
        xs.doc()                      # the index of the reference

    ``name`` is a command of the desktop app's command window, or a method
    of the package that does the same. A name with no page opens the index.
    ``open=False`` only returns the address.
    """
    page = "reference"
    if name is not None:
        key = getattr(name, "__name__", name)
        key = METHODS.get(key, key)
        key = " ".join(str(key).lower().split())
        if key in PAGES:
            page = key.replace(" ", "-")
    url = f"{SITE}reference/{page}/"
    if open:
        webbrowser.open(url)
    return url
