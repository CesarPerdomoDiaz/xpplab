"""The batch command-line tool, xpplab-batch.

usage: batch_check.py xpplab-batch xppaut-headless examples-folder

Without commands it is XPPAUT's -silent: its output.dat must hold the same
numbers as unmodified XPPAUT's batch mode, token for token. With commands it
runs the app's command language: a file of commands with a Range, AUTO and
exports must succeed; a failing command stops it with exit status 1.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

batch, headless, examples = sys.argv[1], sys.argv[2], Path(sys.argv[3])
failures = []


def check(ok, what):
    print(("ok    " if ok else "FAIL  ") + what)
    if not ok:
        failures.append(what)


def run(args, cwd):
    return subprocess.run([batch, *args], cwd=cwd, capture_output=True, text=True,
                          timeout=300, stdin=subprocess.DEVNULL)


def tokens(path):
    return [line.split() for line in Path(path).read_text().splitlines() if line.strip()]


with tempfile.TemporaryDirectory() as t:
    tmp = Path(t)

    # XPPAUT's -silent, model by model
    for name in ("lecar", "lorenz", "fhn", "kepler"):
        model = examples / (name + ".ode")
        if not model.exists():
            continue
        mine, ref = tmp / ("mine_" + name), tmp / ("ref_" + name)
        mine.mkdir()
        ref.mkdir()
        shutil.copy(model, mine)
        shutil.copy(model, ref)
        r = run([model.name], mine)
        subprocess.run([headless, model.name, "-silent"], cwd=ref, capture_output=True,
                       timeout=300, stdin=subprocess.DEVNULL)
        same = (r.returncode == 0 and (mine / "output.dat").exists() and
                tokens(mine / "output.dat") == tokens(ref / "output.dat"))
        check(same, "%s: output.dat equals xppaut -silent's" % name)

    # The command language, from a file and from -c
    work = tmp / "commands"
    work.mkdir()
    shutil.copy(examples / "lecar.ode", work)
    (work / "cmds.txt").write_text(
        "# a brute-force diagram, then AUTO\n"
        "use hopf\n"
        "range iapp 0 0.5 6 V\n"
        "export range.csv range\n"
        "auto set parmax=0.6 nmax=220\n"
        "auto steady iapp\n"
        "auto from 2 periodic\n"
        "export diagram.csv auto\n"
        "nullclines\n"
        "export phase.svg phase\n"
        "export auto.svg auto\n")
    r = run(["lecar.ode", "cmds.txt", "-c", "set iapp=0.1", "-c", "run",
             "-c", "export run.dat", "--script", "session.py"], work)
    check(r.returncode == 0, "a command file runs to the end (exit %d)" % r.returncode)
    check("HB #2" in r.stdout and "AUTO computed" in r.stdout,
          "AUTO's special points are printed")
    for f, rows in (("range.csv", 7), ("diagram.csv", 200), ("run.dat", 601)):
        p = work / f
        n = len(p.read_text().splitlines()) if p.exists() else 0
        check(n >= rows, "%s has %d lines (at least %d)" % (f, n, rows))
    for f in ("phase.svg", "auto.svg"):
        p = work / f
        text = p.read_text() if p.exists() else ""
        check("</svg>" in text and ("<path" in text or "<polyline" in text),
              "%s is a figure with curves" % f)
    s = work / "session.py"
    check(s.exists() and "m.auto" in s.read_text() and "sol.save(" in s.read_text(),
          "the session is saved as Python")

    # Errors: the first one stops, with exit status 1
    r = run(["lecar.ode", "-c", "export x.png phase", "-c", "run"], work)
    check(r.returncode == 1 and "error:" in r.stdout and ">> run" not in r.stdout,
          "a failing command stops the run with exit status 1")
    r = run(["lecar.ode", "--keep-going", "--quiet", "-c", "kinescope capture",
             "-c", "export y.png time"], work)
    check(r.returncode == 1 and r.stderr.count("error:") == 2 and not r.stdout.strip(),
          "--keep-going goes on; --quiet prints only the errors")
    r = run(["lecar.ode", "--no-such-option"], work)
    check(r.returncode == 2, "an unknown option gives exit status 2")

print("%d failure(s)" % len(failures) if failures else "all batch checks passed")
sys.exit(1 if failures else 0)
