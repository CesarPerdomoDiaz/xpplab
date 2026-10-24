"""Exit status of scripted runs of the desktop app.

usage: app_check.py xppstudio model.ode

A --screenshot run that works exits 0 and writes the PNG. A --record run into
a folder that does not exist exits non-zero and writes <folder>.err.
Exits 77 (skipped) when the machine cannot open a window.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

app, model = sys.argv[1], sys.argv[2]


def run(*args):
    return subprocess.run([app, model, *args], capture_output=True,
                          text=True, timeout=90)


with tempfile.TemporaryDirectory() as t:
    tmp = Path(t)

    shot = tmp / "ok.png"
    r = run("--screenshot", str(shot), "--frames", "4")
    err = Path(str(shot) + ".err")
    if r.returncode != 0 and err.exists() and any(
            k in err.read_text() for k in
            ("SDL_Init", "SDL_CreateWindow", "SDL_CreateRenderer")):
        print("no window available: " + err.read_text().strip())
        sys.exit(77)
    if r.returncode != 0:
        sys.exit(f"--screenshot exited {r.returncode}: {r.stderr}")
    if not shot.exists() or shot.read_bytes()[:4] != b"\x89PNG":
        sys.exit("--screenshot wrote no PNG")
    print("--screenshot: exit 0, PNG written")

    missing = tmp / "no_such_folder"
    r = run("--record", str(missing), "--frames", "2")
    err = Path(str(missing) + ".err")
    if r.returncode == 0:
        sys.exit("--record into a missing folder exited 0")
    if not err.exists() or not err.read_text().strip():
        sys.exit("--record into a missing folder wrote no .err file")
    print(f"--record into a missing folder: exit {r.returncode}, "
          f"{err.name} written")
