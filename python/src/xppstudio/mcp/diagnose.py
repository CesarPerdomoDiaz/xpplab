"""Turn XPPAUT's parser output into errors an agent can act on: the line of
the .ode file, the expression with a caret, and a hint.

XPPAUT reports a bad model by echoing what it was compiling ("illegal
expression: Q", the expression, a caret, "ERROR compiling X'"), not by
line number. These functions find the line that defines the failing name,
as the desktop app's editor does, and suggest names for typos.
"""

from __future__ import annotations

import difflib
import re

# Built-in functions and names of XPPAUT's expression language
# (docs/manual/odefile.md).
BUILTINS = [
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh",
    "tanh", "exp", "ln", "log", "log10", "sqrt", "abs", "heav", "sign", "mod",
    "max", "min", "flr", "ran", "normal", "besselj", "bessely", "erf", "erfc",
    "hom_bcs", "if", "then", "else", "delay", "pi", "t", "shift", "del_shft",
    "sum", "of", "lgamma",
]

_KEYWORDS = {"par", "param", "p", "init", "i", "number", "num", "n", "aux",
             "a", "table", "wiener", "w", "markov", "global", "bdry", "b",
             "set", "done", "d", "volt", "v", "special", "export", "only",
             "options", "o", "solv"}

_DECLARE = {"par", "param", "p", "init", "i", "number", "num", "n"}

_NAME = r"[A-Za-z_][A-Za-z0-9_]*"


def _lines(text: str) -> list[str]:
    return text.splitlines()


def _code(line: str) -> str:
    """A line without its comment, or "" for comment and tutorial lines."""
    s = line.strip()
    if not s or s.startswith("#") or s.startswith('"'):
        return ""
    return s.split("#", 1)[0].strip()


def defined_names(text: str) -> list[str]:
    """Names a model defines (variables, parameters, functions, fixed and
    auxiliary quantities), found with simple patterns. Good enough to
    suggest corrections for a typo; not a parser."""
    names: list[str] = []
    for raw in _lines(text):
        s = _code(raw)
        if not s:
            continue
        parts = s.split(None, 1)
        head = parts[0].lower()
        rest = parts[1] if len(parts) > 1 else ""
        if head in _DECLARE:
            names += re.findall(rf"({_NAME})\s*(?:\(0\))?\s*=", rest)
            continue
        if head in ("aux", "a", "table", "markov"):
            names += re.findall(_NAME, rest)[:1]
            continue
        if head in ("wiener", "w"):
            names += re.findall(_NAME, rest)
            continue
        if head in _KEYWORDS or s.startswith("@"):
            continue
        m = (re.match(rf"d({_NAME})/dt\s*=", s, re.I)
             or re.match(rf"({_NAME})\s*(?:\[[^\]]*\])?\s*'\s*=", s)
             or re.match(rf"({_NAME})\s*(?:\[[^\]]*\])?\s*\(\s*t\s*(?:\+\s*1)?\s*\)\s*=", s, re.I)
             or re.match(rf"!?({_NAME})\s*(?:\[[^\]]*\])?\s*=", s))
        if m:
            names.append(m.group(1))
            continue
        m = re.match(rf"({_NAME})\s*\(([^)]*)\)\s*=", s)
        if m:  # a user function f(x,y)=...
            names.append(m.group(1))
    seen, out = set(), []
    for n in names:
        if n.lower() not in seen:
            seen.add(n.lower())
            out.append(n)
    return out


def definition_line(text: str, name: str) -> int:
    """The 1-based line that defines ``name`` (an equation, a fixed or
    auxiliary quantity), or 0. Array statements such as ``x[1..10]'=...``
    match the names they expand to (X1 ... X10)."""
    name = name.strip().rstrip("'")
    if not name:
        return 0
    stem = re.sub(r"\d+$", "", name)
    pats = [
        rf"^d{re.escape(name)}/dt\s*=",
        rf"^{re.escape(name)}\s*'\s*=",
        rf"^{re.escape(name)}\s*\(\s*t\s*(\+\s*1)?\s*\)\s*=",
        rf"^(aux|a)\s+{re.escape(name)}\s*=",
        rf"^!?{re.escape(name)}\s*=",
        rf"^{re.escape(name)}\s*\([^)]*\)\s*=",
    ]
    lines = _lines(text)
    for pat in pats:
        for i, raw in enumerate(lines, 1):
            if re.match(pat, _code(raw), re.I):
                return i
    if stem and stem != name:  # an array element
        for i, raw in enumerate(lines, 1):
            s = _code(raw)
            if re.match(rf"^(d|aux\s+|a\s+)?{re.escape(stem)}\s*\[", s, re.I):
                return i
    return 0


def _declares(line: str, name: str) -> bool:
    """Whether one line defines or declares ``name``."""
    s = _code(line)
    if not s:
        return False
    if definition_line(s, name):
        return True
    parts = s.split(None, 1)
    if parts[0].lower() in _DECLARE and len(parts) > 1:
        return any(n.lower() == name.lower() for n in
                   re.findall(rf"({_NAME})\s*(?:\(0\))?\s*=", parts[1]))
    return False


def _token_at(expr: str, col: int) -> str:
    """The identifier in ``expr`` that covers column ``col``."""
    for m in re.finditer(_NAME, expr):
        if m.start() <= col < m.end() + 1:
            return m.group(0)
    return ""


def _suggest(token: str, candidates: list[str]) -> list[str]:
    pool = {c.lower(): c for c in candidates}
    hits = difflib.get_close_matches(token.lower(), list(pool), n=3, cutoff=0.6)
    return [pool[h] for h in hits]


def _line_containing(text: str, needle: str) -> int:
    needle = needle.replace(" ", "").lower()
    if not needle:
        return 0
    for i, raw in enumerate(_lines(text), 1):
        if needle in _code(raw).replace(" ", "").lower():
            return i
    return 0


def diagnose(text: str, message: str, log: str) -> dict:
    """An error report for a model that failed to load.

    ``message`` is the engine's one-line error, ``log`` everything it
    printed. The result has ``message``, and when they can be found
    ``line``, ``source`` (that line), ``expression`` and ``caret``, the
    unknown ``name`` and a ``hint``."""
    message = re.sub(r"^could not load \S+?:\s*", "", message.strip())
    out: dict = {"message": message}
    loglines = [ln.rstrip() for ln in log.splitlines()]
    known = defined_names(text)

    # The failing statement: "ERROR compiling X'" / "ERROR compiling X".
    compiling = ""
    for ln in loglines:
        m = re.search(r"ERROR compiling (\S+)", ln) or re.search(
            r"Error (?:allocating or compiling|in parsing) (\S+)", ln)
        if m:
            compiling = m.group(1)
    line = definition_line(text, compiling) if compiling else 0

    for k, ln in enumerate(loglines):
        m = re.search(r"illegal expression:\s*(\S*)", ln) or re.search(
            r"Illegal syntax", ln)
        if not m:
            continue
        out["message"] = ln.strip()
        if k + 2 < len(loglines) and loglines[k + 2].strip() == "^":
            expr, caret = loglines[k + 1], loglines[k + 2]
            out["expression"] = expr
            out["caret"] = caret
            col = caret.index("^")
            token = _token_at(expr, col)
            if "illegal expression" in ln and token:
                out["name"] = token
                close = _suggest(token, known + BUILTINS)
                if close:
                    out["hint"] = (f"{token} is not a parameter, variable, function "
                                   f"or built-in; did you mean {' or '.join(close)}?")
                else:
                    out["hint"] = (f"{token} is not defined: declare it with "
                                   f"'par {token.lower()}=<value>', or define it "
                                   f"as a variable or fixed quantity.")
            elif "Illegal syntax" in ln:
                out["hint"] = ("the expression is malformed near the caret, "
                               "e.g. two operators in a row or a missing operand.")
            if not line:
                line = _line_containing(text, expr)
        break

    for ln in loglines:
        m = re.search(r"(\S+) is a duplicate name", ln)
        if m:
            dup = m.group(1)
            out["name"] = dup
            hits = [i for i, raw in enumerate(_lines(text), 1)
                    if _declares(raw, dup)]
            if hits:  # the second definition is the one XPPAUT rejects
                line = hits[1] if len(hits) > 1 else hits[0]
            out["hint"] = (f"{dup} is defined twice (as an equation, parameter, "
                           f"function or auxiliary). Names ignore case.")
        if "parentheses don't match" in ln:
            out["message"] = "parentheses don't match" + (
                f" in the equation for {compiling}" if compiling else "")
            out["hint"] = "count the ( and ) in that line."
        if "does not terminate" in ln:
            out["hint"] = "a ] is probably missing in an array statement."

    if not line:
        m = re.search(r"\bline (\d+)", log)
        if m:
            line = int(m.group(1))
    if line:
        out["line"] = line
        src = _lines(text)
        if 0 < line <= len(src):
            out["source"] = src[line - 1].rstrip()
    return out


def lint(text: str) -> list[str]:
    """Warnings for things XPPAUT accepts silently but are probably not
    meant: a declaration without a value, a missing ``done``."""
    warnings = []
    lines = _lines(text)
    for i, raw in enumerate(lines, 1):
        s = _code(raw)
        if not s:
            continue
        head = s.split(None, 1)[0].lower()
        if head in ("par", "param", "p", "init", "i", "number", "num", "n"):
            for m in re.finditer(rf"({_NAME})\s*=\s*(?=,|$)", s):
                warnings.append(f"line {i}: {m.group(1)} has no value after '=' "
                                f"(XPPAUT sets it to 0)")
    if not any(_code(raw).lower() == "done" for raw in lines):
        warnings.append("the file does not end with 'done' (XPPAUT accepts "
                        "this, but other tools may not)")
    return warnings
