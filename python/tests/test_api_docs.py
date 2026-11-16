"""The Python reference (docs/python/reference.md) to the standard of the
command reference (#124): every public function, method and property of the
classes the page lists has a summary, its arguments, what it returns, an
example and a "See also" that links the command reference page of the same
action. The examples are run here, as written, with their output.

Members not yet written to this standard are listed in api_pending.txt. The
list may only shrink: a member that meets the standard must leave it.
"""
import doctest
import inspect
import os
import re
import sys
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs
from xpplab import docs

ROOT = Path(__file__).resolve().parents[2]
PAGE = ROOT / "docs" / "python" / "reference.md"
PENDING = Path(__file__).with_name("api_pending.txt")
SECTION = re.compile(r"^(\w[\w ]*):\s*$", re.M)


def documented():
    """The objects the reference page shows, by their import path."""
    out = []
    for path in re.findall(r"^::: (\S+)", PAGE.read_text(encoding="utf-8"), re.M):
        mod, _, name = path.rpartition(".")
        obj = __import__(mod, fromlist=[name])
        out.append((path.replace("xpplab.", "", 1), getattr(obj, name)))
    return out


def members():
    """(name, object, function) for each public member: the classes, their
    methods and properties (inherited ones too, as the page shows them), and
    the module-level functions. Aliases (``ics``) are skipped."""
    out = []
    for name, obj in documented():
        out.append((name, obj, obj if not inspect.isclass(obj) else obj.__init__))
        if not inspect.isclass(obj):
            continue
        seen = set()
        for klass in obj.__mro__:
            if not klass.__module__.startswith("xpplab"):
                continue
            for attr, value in vars(klass).items():
                if attr.startswith("_") or attr in seen:
                    continue
                seen.add(attr)
                fn = value.fget if isinstance(value, property) else value
                if isinstance(fn, (classmethod, staticmethod)):
                    fn = fn.__func__
                if not callable(fn):
                    continue
                if getattr(fn, "__name__", attr) != attr:  # an alias
                    continue
                out.append((f"{name}.{attr}", value, fn))
    return out


def sections(text):
    """The Google-style sections of a docstring, by title."""
    parts = SECTION.split(inspect.cleandoc(text or ""))
    out = {"": parts[0]}
    for title, body in zip(parts[1::2], parts[2::2]):
        out[title] = body
    return out


def page_of(name, fn):
    """The command reference page of the same action, as xs.doc finds it."""
    qual = getattr(fn, "__qualname__", "")
    key = docs.METHODS.get(qual, docs.METHODS.get(name.rsplit(".", 1)[-1], ""))
    return key.replace(" ", "-") if key in docs.PAGES else None


def problems(name, obj, fn):
    """What the member's docstring lacks, as a list of phrases."""
    doc = inspect.getdoc(obj if inspect.isclass(obj) or isinstance(obj, property)
                         else fn)
    s = sections(doc)
    bad = []
    if not s[""].strip():
        bad.append("a summary")
    if ">>>" not in s.get("Examples", ""):
        bad.append("an Examples section with >>>")
    if "See also" not in s:
        bad.append("a See also section")
    if re.search(r":(meth|class|func|attr|mod):`", doc or ""):
        bad.append("links instead of Sphinx roles (:meth: and the like), which "
                   "the site does not render")
    page = page_of(name, fn)
    if page and f"../reference/{page}.md" not in s.get("See also", ""):
        bad.append(f"a See also link to ../reference/{page}.md")
    if inspect.isclass(obj) or isinstance(obj, property):
        return bad
    params = [p for p in inspect.signature(fn).parameters.values()
              if p.name not in ("self", "cls")]
    if params:
        args = s.get("Args", "")
        for p in params:
            star = {p.VAR_KEYWORD: "**", p.VAR_POSITIONAL: "*"}.get(p.kind, "")
            if not re.search(rf"^\s*{re.escape(star + p.name)}\b", args, re.M):
                bad.append(f"Args: {star}{p.name}")
    ret = inspect.signature(fn).return_annotation
    if ret not in (None, "None", inspect.Signature.empty) and "Returns" not in s:
        bad.append("a Returns section")
    return bad


def pending():
    return [ln.strip() for ln in PENDING.read_text(encoding="utf-8").splitlines()
            if ln.strip() and not ln.startswith("#")]


def test_every_member_is_documented_or_pending():
    todo = pending()
    names = {name for name, _, _ in members()}
    bad = []
    for name, obj, fn in members():
        lacks = problems(name, obj, fn)
        if lacks and name not in todo:
            bad.append(f"{name} lacks {', '.join(lacks)}")
        if not lacks and name in todo:
            bad.append(f"{name} is documented now: remove it from api_pending.txt")
    bad += [f"api_pending.txt lists {n}, which is not a documented member"
            for n in todo if n not in names]
    assert not bad, "\n".join(bad)


def test_every_export_is_on_the_page():
    on_page = {obj for _, obj in documented()}
    missing = [n for n in xs.__all__ if n not in ("METHODS", "__version__", "app")
               and getattr(xs, n) not in on_page]
    assert not missing, f"not in docs/python/reference.md: {missing}"


NUMBER = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")


class Checker(doctest.OutputChecker):
    """doctest's comparison, except that numbers agree to a relative 1e-4.

    The outputs shown are the ones this platform gives. AUTO locates special
    points, and the integrators take steps, only to their tolerances, so the
    last digits shown can differ on another platform's arithmetic."""

    def check_output(self, want, got, optionflags):
        if super().check_output(want, got, optionflags):
            return True
        if NUMBER.sub("#", want) != NUMBER.sub("#", got):
            return super().check_output(NUMBER.sub("#", want), NUMBER.sub("#", got),
                                        optionflags) and self._close(want, got)
        return self._close(want, got)

    @staticmethod
    def _close(want, got):
        a, b = NUMBER.findall(want), NUMBER.findall(got)
        return len(a) == len(b) and all(
            abs(float(x) - float(y)) <= 1e-4 * max(abs(float(x)), abs(float(y))) + 1e-9
            for x, y in zip(a, b))


def test_the_checker_allows_only_the_last_digits():
    c, flags = Checker(), doctest.ELLIPSIS | doctest.NORMALIZE_WHITESPACE
    assert c.check_output("period=25.7585\n", "period=25.7587\n", flags)
    assert c.check_output("array([0.  , 0.6419])\n", "array([0.    , 0.6419])\n", flags)
    assert not c.check_output("period=25.7585\n", "period=25.79\n", flags)
    assert not c.check_output("(1, 2)\n", "(1, 3)\n", flags)
    assert not c.check_output("HB #2\n", "LP #2\n", flags)


DONE = [(name, obj, fn) for name, obj, fn in members() if name not in pending()]


@pytest.mark.parametrize("name,obj,fn", DONE, ids=[d[0] for d in DONE])
def test_examples_run_as_written(name, obj, fn, tmp_path, monkeypatch):
    doc = inspect.getdoc(obj if inspect.isclass(obj) or isinstance(obj, property)
                         else fn)
    if ">>>" not in (doc or ""):
        pytest.skip("no example")
    monkeypatch.chdir(tmp_path)
    try:  # the examples that plot draw off screen
        import matplotlib
        matplotlib.use("Agg")
    except ImportError:
        pass
    globs = {"xs": xs, "np": np}
    test = doctest.DocTestParser().get_doctest(doc, globs, name, None, 0)
    runner = doctest.DocTestRunner(
        checker=Checker(), optionflags=doctest.ELLIPSIS | doctest.NORMALIZE_WHITESPACE)
    out = []
    try:
        runner.run(test, out=out.append)
    finally:
        for value in globs.values():
            if isinstance(value, xs.Model):
                value.close()
        if "matplotlib.pyplot" in sys.modules:
            sys.modules["matplotlib.pyplot"].close("all")
    assert runner.failures == 0, "".join(out)
