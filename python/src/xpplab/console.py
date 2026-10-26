"""The Python behind the desktop app's console.

The app starts ``python -m xpplab.console`` in a process of its own and
talks to it through standard input and output: one JSON object per line
each way. The code runs in a namespace that already holds

    xs   the xpplab package          np   NumPy
    app  the running app (xs.app)    m    the app's model: m.run() runs it
                                          in the app, whose plots follow

so the Python the app writes for each action runs here unchanged.

Requests (from the app): {"id": n, "op": "exec", "code": "..."} runs code
and answers {"id": n, "more": true} when it is not complete yet;
"complete" with "text" answers the possible completions; "vars" lists the
variables. While code runs, its output comes as {"id": n, "stream":
"out" | "err", "text": "..."}; {"id": n, "done": true} ends it.
"""
from __future__ import annotations

import codeop
import io
import json
import sys
import time
import traceback

_wire = sys.stdout  # the app reads this; the user's prints are redirected


def _send(obj) -> None:
    _wire.write(json.dumps(obj) + "\n")
    _wire.flush()


class _Stream(io.TextIOBase):
    """The user's sys.stdout or sys.stderr while a request runs."""

    def __init__(self, name: str):
        self.name, self.id = name, None

    def writable(self) -> bool:
        return True

    def write(self, text: str) -> int:
        if text:
            _send({"id": self.id, "stream": self.name, "text": text})
        return len(text)


def _describe(value) -> str:
    """A one-line summary of a variable for the app's Workspace."""
    try:
        import numpy as np
        if isinstance(value, np.ndarray):
            return "array %s %s" % ("x".join(map(str, value.shape)) or "scalar", value.dtype)
    except ImportError:
        pass
    kind = type(value).__name__
    if hasattr(value, "columns") and hasattr(value, "__len__"):
        try:
            return "%s: %d rows, columns %s" % (kind, len(value), ", ".join(value.columns)[:60])
        except Exception:
            pass
    if isinstance(value, (int, float, complex, bool)):
        return "%s %r" % (kind, value)
    if isinstance(value, str):
        return "str %r" % (value[:40] + ("..." if len(value) > 40 else ""))
    if isinstance(value, (list, tuple, dict, set)):
        return "%s of %d" % (kind, len(value))
    text = repr(value)
    return text if len(text) <= 70 else text[:67] + "..."


def main() -> int:
    out, err = _Stream("out"), _Stream("err")
    ns = {"__name__": "__console__", "__builtins__": __builtins__}
    startup = set()
    try:
        import numpy as np
        import xpplab as xs
    except ImportError as e:
        _send({"ready": False, "error": "this Python cannot import %s; install it "
               "with: pip install numpy" % (e.name or e)})
        return 1
    ns.update(xs=xs, np=np)
    app_note = ""
    for _ in range(50):  # the app may still be opening its channel
        try:
            app = xs.app.connect(timeout=60, quiet=True)
            ns.update(app=app, m=app.model())
            break
        except xs.app.AppError as e:
            app_note = str(e)
            time.sleep(0.1)
    else:
        app_note = "app and m are not defined: " + app_note
    startup.update(ns)
    _send({"ready": True, "python": sys.version.split()[0], "xpplab": xs.__version__,
           "names": sorted(k for k in ns if not k.startswith("__")),
           "note": app_note})
    compiler = codeop.CommandCompiler()
    for line in sys.stdin:
        try:
            req = json.loads(line)
        except ValueError:
            continue
        rid, op = req.get("id"), req.get("op")
        if op == "exec":
            code = req.get("code", "")
            try:
                try:
                    compiled = compiler(code, "<console>", "single")
                except SyntaxError as e:
                    if "multiple statements" not in str(e):
                        raise
                    compiled = compile(code, "<console>", "exec")  # pasted lines
            except (SyntaxError, OverflowError, ValueError):
                out.id = err.id = rid
                err.write(traceback.format_exc(limit=0))
                _send({"id": rid, "done": True, "error": True})
                continue
            if compiled is None:
                _send({"id": rid, "more": True})
                continue
            out.id = err.id = rid
            old = sys.stdout, sys.stderr
            sys.stdout, sys.stderr = out, err
            failed = False
            try:
                exec(compiled, ns)
            except SystemExit:
                pass
            except BaseException:
                failed = True
                etype, value, tb = sys.exc_info()
                # Without this module's own frame.
                err.write("".join(traceback.format_exception(etype, value, tb.tb_next)))
            finally:
                sys.stdout, sys.stderr = old
            _send({"id": rid, "done": True, "error": failed})
        elif op == "complete":
            import rlcompleter
            text, found = req.get("text", ""), []
            c = rlcompleter.Completer(ns)
            for i in range(200):
                s = c.complete(text, i)
                if s is None:
                    break
                found.append(s)
            _send({"id": rid, "matches": found})
        elif op == "vars":
            rows = []
            for k, v in ns.items():
                if k.startswith("_") or (k in startup and k in ("xs", "np")):
                    continue
                if type(v).__name__ in ("module", "function", "type", "builtin_function_or_method"):
                    continue
                rows.append([k, type(v).__name__, _describe(v)])
            _send({"id": rid, "vars": sorted(rows)})
    return 0


if __name__ == "__main__":
    sys.exit(main())
