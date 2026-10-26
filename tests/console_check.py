"""The command window's Python console.

usage: console_check.py xpplab model.ode

Starts the app with lines of Python for its console (py ...), run by the
Python that runs this script, and reads the command window when the last
line has made the app quit. The output must be there, an exception must
come back as a traceback, a block (for ...) must run, and m.run() must
have run the model in the app. Exits 77 (skipped) when this Python has no
NumPy or the machine cannot open a window.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

app_exe, model = str(Path(sys.argv[1]).resolve()), Path(sys.argv[2]).resolve()
try:
    import numpy  # noqa: F401  (the console needs it)
except ImportError:
    print("skipped: this Python has no NumPy")
    sys.exit(77)

lines = ["print(6*7)",
         "m.parameters['iapp'] = 0.1",
         "sol = m.run(total=50)",
         "print('rows', len(sol), 'iapp', m.get('iapp'))",
         "for k in range(2):",
         "    print('k', k)",
         "",
         "1/0",
         "app.command('quit')"]
failures = []


def check(ok, what):
    print(("ok    " if ok else "FAIL  ") + what)
    if not ok:
        failures.append(what)


with tempfile.TemporaryDirectory() as t:
    tmp = Path(t)
    # Names without a value, or starting with "=", can make Windows refuse
    # the whole environment ("cannot find the file specified").
    env = {k: v for k, v in os.environ.items() if k and not k.startswith("=") and v}
    env.update(XPPLAB_PYTHON=sys.executable, XPPLAB_CHANNEL=str(tmp / "channel.json"))
    log, shot = tmp / "app.log", tmp / "shot.png"
    args = [app_exe, str(model), "--serve", "--cmd", "py"]
    for line in lines:
        args += ["--cmd", "py " + line if line else "py"]
    args += ["--screenshot", str(shot), "--frames", "100000", "--log", str(log)]
    try:
        subprocess.run(args, env=env, timeout=120, capture_output=True)
    except subprocess.TimeoutExpired:
        check(False, "the app quits when the console's last line says so")
    err = Path(str(shot) + ".err")
    if err.exists() and any(k in err.read_text() for k in
                            ("SDL_Init", "SDL_CreateWindow", "SDL_CreateRenderer")):
        print("skipped: no window can be opened here")
        sys.exit(77)
    text = log.read_text(errors="replace") if log.exists() else ""
    out = text[text.find("is ready"):] if "is ready" in text else ""
    check("is ready" in text, "Python starts in its own process")
    check("\n42\n" in out, "print() shows in the command window")
    check("rows 1001 iapp 0.1" in out, "m.parameters and m.run() act in the app")
    check('>>> m.parameters["iapp"] = 0.1' in out, "the app writes the action's Python")
    check("k 0\nk 1" in out, "a block runs once it is ended")
    check("ZeroDivisionError: division by zero" in out and 'File "<console>"' in out,
          "an exception comes back as a traceback")
    if failures:
        print(text[-3000:])

print("%d failure(s)" % len(failures) if failures else "all console checks passed")
sys.exit(1 if failures else 0)
