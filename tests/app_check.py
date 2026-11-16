"""Exit status of scripted runs of the desktop app.

usage: app_check.py xpplab model.ode

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

    # The Kinescope takes pictures of the window, so only the app checks it.
    # The model is copied, since the app works in the model's folder.
    lecar = tmp / "lecar.ode"
    lecar.write_bytes(Path(model).read_bytes())
    (tmp / "rec").mkdir()
    log = tmp / "kine.log"
    timed = [(1, "kinescope play"), (2, "kinescope show 1"),
             (3, "kinescope capture phase"), (5, "kinescope play"),
             (6, "set iapp=0.2"), (6, "run"), (8, "kinescope capture phase"),
             (10, "kinescope save film.gif 300"), (11, "kinescope play abc"),
             (12, "kinescope show 5"), (13, "kinescope play 200"),
             (14, "kinescope clear")]
    args = [app, str(lecar), "--cmd", "show phase", "--record", str(tmp / "rec"),
            "--frames", "16", "--log", str(log)]
    for frame, cmd in timed:
        args += ["--at", str(frame), cmd]
    r = subprocess.run(args, capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        sys.exit(f"the Kinescope run exited {r.returncode}: {r.stderr}")
    text = log.read_text()
    want = ["error: the Kinescope has 0 frames; it plays two or more",
            "error: the Kinescope has no frames yet.",
            "error: the Kinescope has 1 frame; it plays two or more",
            "Saved film.gif: 2 frames of",
            "error: kinescope play takes the milliseconds per frame",
            "error: the Kinescope has 2 frames; show takes 1 to 2",
            "Kinescope emptied."]
    for w in want:
        if w not in text:
            sys.exit(f"the Kinescope run did not say {w!r}:\n{text}")
    if (tmp / "film.gif").read_bytes()[:6] != b"GIF89a":
        sys.exit("kinescope save wrote no GIF")
    if "error:" in text.split(">> kinescope play 200")[1]:
        sys.exit("kinescope play 200 with two frames failed:\n" + text)
    print("the Kinescope: captures, a GIF, and a message for each slip")
