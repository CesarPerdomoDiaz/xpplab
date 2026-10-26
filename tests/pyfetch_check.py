"""python fetch: XPP Lab's own Python for the console, downloaded once.

usage: pyfetch_check.py xpplab fake_python model.ode

Runs without the internet. The archive is made here, laid out as
python-build-standalone's are, with tests/fake_python.c as its Python (it
answers pip itself and hands everything else to the Python running this
script). The app takes it from a file:// URL ($XPPLAB_PYTHON_URL and
$XPPLAB_PYTHON_SHA256) into a folder of the test's ($XPPLAB_RUNTIME_DIR).

Checks: a Python without NumPy makes the console offer XPP Lab's; an
archive with another digest is refused and nothing is left; a
python folder that XPP Lab did not make is left alone; the right archive
is unpacked, tried and moved into place with its marker; the
lines of Python typed while it installed then run in the console with it;
a second python fetch only says it is there. Exits 77 (skipped) when this
Python lacks NumPy or Matplotlib, or the machine cannot open a window.
"""
import hashlib
import io
import os
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

app_exe = str(Path(sys.argv[1]).resolve())
fake = Path(sys.argv[2]).resolve()
model = str(Path(sys.argv[3]).resolve())
try:
    import matplotlib  # noqa: F401  (python fetch checks both)
    import numpy  # noqa: F401
except ImportError as e:
    print("skipped: this Python has no %s" % e.name)
    sys.exit(77)

failures = []


def check(ok, what):
    print(("ok    " if ok else "FAIL  ") + what)
    if not ok:
        failures.append(what)


def make_archive(path):
    """python/python.exe and pythonw.exe on Windows, python/bin/python3 elsewhere."""
    data = fake.read_bytes()
    names = (["python/python.exe", "python/pythonw.exe"] if os.name == "nt"
             else ["python/bin/python3"])
    with tarfile.open(path, "w:gz") as tar:
        for name in names:
            info = tarfile.TarInfo(name)
            info.size, info.mode = len(data), 0o755
            tar.addfile(info, io.BytesIO(data))
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_app(tmp, sha, cmds, frames, python=None):
    env = {k: v for k, v in os.environ.items() if k and not k.startswith("=") and v}
    env.pop("XPPLAB_PYTHON", None)
    if python:
        env["XPPLAB_PYTHON"] = str(python)
    env.update(XPPLAB_PYTHON_URL=(tmp / "python.tar.gz").as_uri(),
               XPPLAB_PYTHON_SHA256=sha, XPPLAB_RUNTIME_DIR=str(tmp / "runtime"),
               XPPLAB_TEST_REAL_PYTHON=sys.executable,
               XPPLAB_CHANNEL=str(tmp / "channel.json"))
    log, shot = tmp / "app.log", tmp / "shot.png"
    log.unlink(missing_ok=True)
    args = [app_exe, model, "--serve"]
    for c in cmds:
        args += ["--cmd", c]
    args += ["--screenshot", str(shot), "--frames", str(frames), "--log", str(log)]
    try:
        subprocess.run(args, env=env, timeout=150, capture_output=True)
    except subprocess.TimeoutExpired:
        check(False, "the app quits by itself (%s)" % " | ".join(cmds))
    err = Path(str(shot) + ".err")
    if err.exists() and any(k in err.read_text() for k in
                            ("SDL_Init", "SDL_CreateWindow", "SDL_CreateRenderer")):
        print("skipped: no window can be opened here")
        sys.exit(77)
    return log.read_text(errors="replace") if log.exists() else ""


with tempfile.TemporaryDirectory() as t:
    tmp = Path(t)
    runtime = tmp / "runtime"
    sha = make_archive(tmp / "python.tar.gz")

    # A Python without NumPy: the console says so and offers XPP Lab's.
    subprocess.run([sys.executable, "-m", "venv", "--without-pip", str(tmp / "bare")],
                   check=True, capture_output=True)
    bare = next(p for p in [tmp / "bare" / "Scripts" / "python.exe",
                            tmp / "bare" / "bin" / "python"] if p.exists())
    text = run_app(tmp, sha, ["py"], 600, python=bare)
    check("it needs a Python 3 with NumPy" in text and "python fetch      download" in text,
          "a Python without NumPy brings the offer of XPP Lab's")

    text = run_app(tmp, "0" * 64, ["python fetch"], 600)
    check("is not the expected file" in text, "an archive with another digest is refused")
    check(not (runtime / "python").exists() and not (runtime / "python-fetch").exists(),
          "and nothing of it is left")

    other = runtime / "python" / "mine.txt"  # a folder XPP Lab did not make
    other.parent.mkdir(parents=True)
    other.write_text("not XPP Lab's")
    text = run_app(tmp, sha, ["python fetch"], 30)
    check("is in the way" in text and other.exists(), "a python folder of someone else's is left alone")
    other.unlink()
    other.parent.rmdir()

    text = run_app(tmp, sha, ["python fetch", "py print(6*7)",
                              "py app.command('quit')"], 100000)
    marker = runtime / "python" / "xpplab-runtime.txt"
    check("is installed in" in text, "the right archive is unpacked, tried and installed")
    check(marker.exists(), "the install has its marker")
    check(not (runtime / "python-fetch").exists(), "the download's folder is gone")
    start = text.find("Starting Python (")
    started = text[start:text.find("\n", start)].replace("\\", "/")
    check(start >= 0 and (runtime / "python").as_posix() in started,
          "the console starts XPP Lab's Python")
    check("\n42\n" in text[start:], "a line typed while it installed runs once it is ready")

    text = run_app(tmp, sha, ["python fetch", "python"], 30)
    check("XPP Lab's Python is installed" in text, "a second python fetch downloads nothing")
    check("XPP Lab's own Python: " in text, "python says where it is")
    if failures:
        print(text[-3000:])

print("%d failure(s)" % len(failures) if failures else "all python fetch checks passed")
sys.exit(1 if failures else 0)
