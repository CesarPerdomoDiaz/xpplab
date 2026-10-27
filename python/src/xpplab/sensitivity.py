"""Sensitivity of a trajectory to the parameters, by variational equations.

For dx/dt = f(x, p, t) the sensitivity S = dx/dp obeys

    dS/dt = (df/dx) S + df/dp,    S(t0) = 0.

The equations are added to the model as new ODEs, so the model's own
integrator, tolerances and step size integrate them together with the model.
The derivatives of f come from differentiating the model's formulas
symbolically (forward mode: a sensitivity state stands for dx_j/dp, so each
formula gives (df/dx) S + df/dp in one pass). The engine is not changed.
"""
from __future__ import annotations

import re

import numpy as np

__all__ = ["Sensitivity", "augmented_text"]


class SensitivityError(ValueError):
    """The model uses something the variational equations cannot be built for."""


class Sensitivity(dict):
    """S(t) = dx/dp for each parameter.

    ``s["gk"]`` is an array of shape (time, variables): row *n* holds the
    derivatives of every variable at ``s.t[n]`` with respect to ``gk``
    (names ignore case). ``variables`` names the columns; ``trajectory`` is
    the run of the augmented model, whose variable columns equal the model's
    own run up to the integrator's step control.
    """

    def __init__(self, t, variables, data, trajectory):
        super().__init__({k.upper(): v for k, v in data.items()})
        self.t = t
        self.variables = list(variables)
        self.trajectory = trajectory

    def __getitem__(self, name):
        return super().__getitem__(name.upper())

    def __contains__(self, name):
        return super().__contains__(str(name).upper())

    def __repr__(self):
        return (f"Sensitivity({len(self.t)} points, variables "
                f"{', '.join(self.variables)}; parameters {', '.join(self)})")


# -- expressions -------------------------------------------------------------

_TOKEN = re.compile(
    r"\s*(?:(?P<num>(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)"
    r"|(?P<id>[A-Za-z_][A-Za-z0-9_]*)"
    r"|(?P<op><=|>=|==|!=|&&|\|\||\*\*|[-+*/^(),<>]))")
_LEVELS = [("||",), ("&&",), ("<", ">", "<=", ">=", "==", "!="),
           ("+", "-"), ("*", "/")]


def _tokens(text):
    out, pos, text = [], 0, text.rstrip()
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m or m.end() == pos:
            raise SensitivityError(f"cannot read {text[pos:].strip()!r} in {text!r}")
        pos = m.end()
        kind = m.lastgroup
        val = m.group(kind)
        out.append(("op", "^") if val == "**" else (kind, val.lower() if kind == "id" else val))
    return out


class _Parser:
    """Nodes: ("num", text), ("id", name), ("neg", a), ("bin", op, a, b),
    ("call", name, [args]), ("if", cond, a, b)."""

    def __init__(self, text):
        self.text, self.t, self.i = text, _tokens(text), 0
        self.node = self.level(0)
        if self.i != len(self.t):
            raise SensitivityError(f"cannot read the end of {text!r}")

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else (None, None)

    def take(self, val=None):
        kind, v = self.peek()
        if kind is None or (val is not None and v != val):
            raise SensitivityError(f"expected {val or 'more'} in {self.text!r}")
        self.i += 1
        return kind, v

    def level(self, k):
        if k == len(_LEVELS):
            return self.unary()
        a = self.level(k + 1)
        while self.peek()[0] == "op" and self.peek()[1] in _LEVELS[k]:
            op = self.take()[1]
            a = ("bin", op, a, self.level(k + 1))
        return a

    def unary(self):
        if self.peek() == ("op", "-"):
            self.take()
            return ("neg", self.unary())
        if self.peek() == ("op", "+"):
            self.take()
            return self.unary()
        a = self.atom()
        if self.peek() == ("op", "^"):
            self.take()
            return ("bin", "^", a, self.unary())
        return a

    def atom(self):
        kind, v = self.take()
        if kind == "num":
            return ("num", v)
        if kind == "op" and v == "(":
            a = self.level(0)
            self.take(")")
            return a
        if kind != "id":
            raise SensitivityError(f"unexpected {v!r} in {self.text!r}")
        if v == "if" and self.peek() == ("op", "("):
            self.take("(")
            cond = self.level(0)
            self.take(")")
            self.take("then")
            self.take("(")
            a = self.level(0)
            self.take(")")
            self.take("else")
            self.take("(")
            b = self.level(0)
            self.take(")")
            return ("if", cond, a, b)
        if self.peek() == ("op", "("):
            self.take("(")
            args = [self.level(0)]
            while self.peek() == ("op", ","):
                self.take()
                args.append(self.level(0))
            self.take(")")
            return ("call", v, args)
        return ("id", v)


def _parse(text):
    return _Parser(text).node


def _show(n):
    k = n[0]
    if k in ("num", "id"):
        return n[1]
    if k == "neg":
        return f"(-{_show(n[1])})"
    if k == "bin":
        return f"({_show(n[2])}{n[1]}{_show(n[3])})"
    if k == "call":
        return f"{n[1]}({','.join(_show(a) for a in n[2])})"
    return f"if({_show(n[1])})then({_show(n[2])})else({_show(n[3])})"


def _subst(n, env):
    k = n[0]
    if k == "num":
        return n
    if k == "id":
        return env.get(n[1], n)
    if k == "neg":
        return ("neg", _subst(n[1], env))
    if k == "bin":
        return ("bin", n[1], _subst(n[2], env), _subst(n[3], env))
    if k == "call":
        return ("call", n[1], [_subst(a, env) for a in n[2]])
    return ("if", _subst(n[1], env), _subst(n[2], env), _subst(n[3], env))


def _num(x):
    return ("num", repr(float(x)))


def _add(a, b):
    if a is None:
        return b
    if b is None:
        return a
    return ("bin", "+", a, b)


def _sub(a, b):
    if b is None:
        return a
    if a is None:
        return ("neg", b)
    return ("bin", "-", a, b)


def _mul(a, b):
    if a is None or b is None:
        return None
    return ("bin", "*", a, b)


def _div(a, b):
    return None if a is None else ("bin", "/", a, b)


def _call(name, a):
    return ("call", name, [a])


def _pow(a, k):
    return ("bin", "^", a, _num(k))


# derivative of f(a) with respect to a, for the functions XPPAUT's parser has
_DERIVATIVE = {
    "sin": lambda a: _call("cos", a),
    "cos": lambda a: ("neg", _call("sin", a)),
    "tan": lambda a: _div(_num(1), _pow(_call("cos", a), 2)),
    "exp": lambda a: _call("exp", a),
    "ln": lambda a: _div(_num(1), a),
    "log10": lambda a: _div(_num(1), _mul(a, _num(np.log(10.0)))),
    "sqrt": lambda a: _div(_num(1), _mul(_num(2), _call("sqrt", a))),
    "tanh": lambda a: _sub(_num(1), _pow(_call("tanh", a), 2)),
    "sinh": lambda a: _call("cosh", a),
    "cosh": lambda a: _call("sinh", a),
    "atan": lambda a: _div(_num(1), ("bin", "+", _num(1), _pow(a, 2))),
    "asin": lambda a: _div(_num(1), _call("sqrt", _sub(_num(1), _pow(a, 2)))),
    "acos": lambda a: _div(_num(-1), _call("sqrt", _sub(_num(1), _pow(a, 2)))),
}
_ZERO_DERIVATIVE = {"heav", "sign", "flr", "int"}
_UNSUPPORTED = {"delay", "shift", "ran", "normal", "table", "max", "min",
                "atan2", "mod", "lgamma", "erf", "erfc", "poisson", "hom_bcs"}


class _Model:
    """What the sensitivity equations need from the .ode text."""

    def __init__(self, text, states, parameters):
        self.states = [s.lower() for s in states]
        self.parameters = [p.lower() for p in parameters]
        self.odes = {}      # state -> node
        self.fixed = []     # [(name, node)] in file order
        self.functions = {}  # name -> (args, node)
        self.names = set(self.states) | set(self.parameters)
        self._read(text)

    def _read(self, text):
        text = text.replace("\r", "")
        text = re.sub(r"\\\n", "", text)
        for raw in text.split("\n"):
            line = raw.strip()
            self.names.update(w.lower() for w in re.findall(r"[A-Za-z_]\w*", line))
            if not line or line[0] in '#@"':
                continue
            low = line.lower()
            if re.match(r"done\b", low) or re.match(
                    r"(par|param|params|parameter|parameters|p|init|i|aux|set|"
                    r"number|numbers|n|bdry|global|g|option|options|o)\s+[^=\s]",
                    low) or low.startswith(("set ", "aux ")):
                continue
            if re.match(r"[a-z_]\w*\(\s*0\s*\)\s*=", low):
                continue  # x(0)=...: an initial condition, which stays fixed
            if re.match(r"(table|wiener|markov|volterra|0\s*=|solv|vol|special|"
                        r"import|export|fourier|spec)\b", low):
                raise SensitivityError(f"not supported by sensitivity(): {line!r}")
            m = (re.match(r"d([a-z_]\w*)/dt\s*=\s*(.*)$", low)
                 or re.match(r"([a-z_]\w*)'\s*=\s*(.*)$", low))
            if m:
                self.odes[m.group(1)] = _parse(m.group(2))
                continue
            m = re.match(r"([a-z_]\w*)\s*\(([^)]*)\)\s*=\s*(.*)$", low)
            if m:
                args = [a.strip() for a in m.group(2).split(",")]
                if not all(re.fullmatch(r"[a-z_]\w*", a) for a in args):
                    raise SensitivityError(
                        f"not an ODE model (a map or a difference equation): {line!r}")
                self.functions[m.group(1)] = (args, _parse(m.group(3)))
                continue
            m = re.match(r"([a-z_]\w*)\s*=\s*(.*)$", low)
            if m:
                self.fixed.append((m.group(1), _parse(m.group(2))))
                continue
            raise SensitivityError(f"cannot read the line {line!r}")
        missing = [s for s in self.states if s not in self.odes]
        if missing:
            raise SensitivityError(
                f"no ODE found for {', '.join(missing)} (is it a map or a "
                f"model with algebraic equations?)")


def _unique_prefix(names):
    prefix = "sn"
    while any(n.startswith(prefix) for n in names):
        prefix += "x"
    return prefix


def augmented_text(text, states, parameters, wanted):
    """The .ode text with the variational equations for the parameters in
    ``wanted`` added; returns (text, {parameter: [state names]})."""
    mod = _Model(text, states, parameters)
    prefix = _unique_prefix(mod.names)
    out = []
    names = {}
    for k, p in enumerate(wanted):
        p = p.lower()
        sname = {s: f"{prefix}{i}x{k}" for i, s in enumerate(mod.states)}
        names[p] = [sname[s] for s in mod.states]
        fixed_d = {}

        def D(n):
            kind = n[0]
            if kind == "num":
                return None
            if kind == "id":
                v = n[1]
                if v in sname:
                    return ("id", sname[v])
                if v == p:
                    return _num(1)
                return ("id", fixed_d[v]) if v in fixed_d else None
            if kind == "neg":
                d = D(n[1])
                return None if d is None else ("neg", d)
            if kind == "if":
                da, db = D(n[2]), D(n[3])
                if da is None and db is None:
                    return None
                return ("if", n[1], da or _num(0), db or _num(0))
            if kind == "call":
                return D_call(n)
            op, a, b = n[1], n[2], n[3]
            if op in ("<", ">", "<=", ">=", "==", "!=", "&&", "||"):
                return None
            da, db = D(a), D(b)
            if op == "+":
                return _add(da, db)
            if op == "-":
                return _sub(da, db)
            if op == "*":
                return _add(_mul(da, b), _mul(a, db))
            if op == "/":
                if db is None:
                    return _div(da, b)
                return _div(_sub(_mul(da, b), _mul(a, db)), _pow(b, 2))
            if db is None:  # a^b with b free of x and p
                return _mul(_mul(b, ("bin", "^", a, _sub(b, _num(1)))), da)
            whole = ("bin", "^", a, b)
            inner = _add(_mul(db, _call("ln", a)), _div(_mul(b, da), a))
            return _mul(whole, inner)

        def D_call(n):
            name, args = n[1], n[2]
            if name in mod.functions:
                formal, body = mod.functions[name]
                if len(formal) != len(args):
                    raise SensitivityError(f"{name} takes {len(formal)} arguments")
                return D(_subst(body, dict(zip(formal, args))))
            if name in _UNSUPPORTED:
                raise SensitivityError(
                    f"{name}() is not supported by sensitivity()")
            if name in _ZERO_DERIVATIVE:
                return None
            if name not in _DERIVATIVE or len(args) != 1:
                raise SensitivityError(f"unknown function {name}() in the model")
            du = D(args[0])
            return None if du is None else _mul(_DERIVATIVE[name](args[0]), du)

        for j, (g, expr) in enumerate(mod.fixed):
            d = D(expr)
            if d is not None:
                fixed_d[g] = f"{prefix}f{j}x{k}"
                out.append(f"{fixed_d[g]}={_show(d)}")
            else:
                fixed_d.pop(g, None)
        for s in mod.states:
            d = D(mod.odes[s]) or _num(0)
            out.append(f"d{sname[s]}/dt={_show(d)}")
            out.append(f"init {sname[s]}=0")
    body = [ln for ln in text.splitlines() if not re.match(r"\s*done\b", ln, re.I)]
    return "\n".join(body + ["# sensitivity equations (XPP Lab)"] + out + ["done", ""]), names


def run(model, params=None, **values):
    """See :meth:`xpplab.Model.sensitivity`."""
    from .model import Model, _key
    if values:
        model.set(**values)
    if params is None:
        wanted = list(model._parameter_names)
    else:
        wanted = [params] if isinstance(params, str) else list(params)
        known = {_key(p): p for p in model._parameter_names}
        for p in wanted:
            if _key(p) not in known:
                raise KeyError(f"no parameter {p!r}; available: "
                               f"{', '.join(model._parameter_names)}")
        wanted = [known[_key(p)] for p in wanted]
    text, names = augmented_text(model.path.read_text(errors="replace"),
                                 model.variables, model._parameter_names, wanted)
    aug = Model.from_text(text, name=model.path.name, options=model._load_options)
    try:
        for p in model._parameter_names:
            aug.set(**{p: model.get(p)})
        for v in model.variables:
            aug.set(**{v: model.get(v)})
        for o in ("T0", "TOTAL", "DT", "TRANS", "BOUND", "TOL", "ATOL", "DTMIN",
                  "DTMAX", "NOUT", "MAXSTOR", "METH", "DELAY", "NEWT_TOL",
                  "NEWT_ITER", "JAC_EPS"):
            try:
                aug.set(**{o: model.get(o)})
            except Exception:
                pass
        # Sensitivities of an oscillator grow with time; the model's BOUND
        # would stop the run when one passes it.
        aug.set(BOUND=1e30)
        traj = aug.run()
        data = {}
        for p in wanted:
            cols = [traj[n.upper()] for n in names[p.lower()]]
            data[p] = np.column_stack(cols)
        return Sensitivity(traj.t, model.variables, data, traj)
    finally:
        aug.close()
