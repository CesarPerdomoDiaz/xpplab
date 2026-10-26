"""Python drives the running app through its local channel.

usage: channel_check.py xpplab model.ode

Starts the app (scripted, with --serve), connects with xpplab.app.connect(),
sets values and runs: the run the app returns must equal the same run made
by the Python package directly, bit for bit. A failing command must raise,
a wrong token must be refused, and the app must remove its channel file
when it quits. Exits 77 (skipped) when the machine cannot open a window.
"""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python" / "src"))
app_exe, model = str(Path(sys.argv[1]).resolve()), Path(sys.argv[2]).resolve()
failures = []


def check(ok, what):
    print(("ok    " if ok else "FAIL  ") + what)
    if not ok:
        failures.append(what)


with tempfile.TemporaryDirectory() as t:
    tmp = Path(t)
    chan = tmp / "channel.json"
    os.environ["XPPLAB_CHANNEL"] = str(chan)
    shot = tmp / "shot.png"
    proc = subprocess.Popen([app_exe, str(model), "--serve", "--screenshot", str(shot),
                             "--frames", "100000"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(600):
        if chan.exists() or proc.poll() is not None:
            break
        time.sleep(0.05)
    err = Path(str(shot) + ".err")
    if not chan.exists():
        proc.kill()
        if err.exists() and any(k in err.read_text() for k in
                                ("SDL_Init", "SDL_CreateWindow", "SDL_CreateRenderer")):
            print("skipped: no window can be opened here")
            sys.exit(77)
        print("FAIL  the app wrote no channel file")
        sys.exit(1)

    try:
        import numpy as np
        import xpplab as xs
    except ImportError as e:
        proc.kill()
        print("skipped: %s" % e)
        sys.exit(77)

    engine = None
    for d in (Path(app_exe).parent, Path(app_exe).parent.parent / "Resources"):
        for name in ("libxppe.dll", "libxppe.dylib", "libxppe.so"):
            if (d / name).exists():
                engine = d / name
    if engine:
        os.environ["XPPLAB_ENGINE"] = str(engine)

    try:
        with xs.app.connect() as app:
            check(Path(app.model_path).name == model.name, "connect: %r" % app)
            check(app.parameters.get("iapp") == 0.05, "the app's parameters are read")
            sol = app.run(iapp=0.1, total=50)
            check(app.get("iapp") == 0.1 and "sol = m.run()" in app.last_python,
                  "set and run go through the app, echoed as Python")
            with xs.load(model) as m:
                ref = m.run(iapp=0.1, total=50)
            check(sol.columns == ref.columns and len(sol) == len(ref) and
                  all(np.array_equal(sol[c], ref[c]) for c in ref.columns),
                  "the app's run equals the package's, bit for bit (%d rows)" % len(sol))
            try:
                app.command("auto from 99 periodic")
                check(False, "a failing command raises AppError")
            except xs.app.AppError as e:
                check("no point labelled 99" in str(e), "a failing command raises AppError")

            info = json.loads(chan.read_text())
            s = socket.create_connection(("127.0.0.1", info["port"]), timeout=10)
            s.sendall(b'{"token": "0000", "op": "hello"}\n')
            reply = s.makefile().readline()
            s.close()
            check('"wrong token"' in reply, "a wrong token is refused")
            try:
                app.command("quit")
            except xs.app.AppError:
                pass  # the app closes the connection as it quits
    finally:
        try:
            proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            proc.kill()
    check(not chan.exists(), "the channel file is removed when the app quits")

print("%d failure(s)" % len(failures) if failures else "all channel checks passed")
sys.exit(1 if failures else 0)
