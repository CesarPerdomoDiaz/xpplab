"""The batch command-line tool, xpplab-batch.

usage: batch_check.py xpplab-batch xppaut-headless examples-folder

Without commands it is XPPAUT's -silent: its output.dat must hold the same
numbers as unmodified XPPAUT's batch mode, token for token. With commands it
runs the app's command language: a file of commands with a Range, AUTO and
exports must succeed; a failing command stops it with exit status 1.
"""
import re
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
    shutil.copy(examples / "cable.ode", work)
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

    # Window and file commands that did nothing, or the opposite, with a
    # wrong word; and quit, which a script went past
    r = run(["lecar.ode", "--keep-going", "-c", "show zz", "-c", "layout x",
             "-c", "theme x", "-c", "open", "-c", "show phase", "-c", "quit",
             "-c", "whos"], work)
    check("error: no window called zz; the windows are time, phase" in r.stdout,
          "show names its windows: " + r.stdout[-500:])
    check("error: write layout side or layout tabs" in r.stdout
          and "error: write theme dark or theme light" in r.stdout
          and "Write: open FILE.ode" in r.stdout, "layout, theme, open: " + r.stdout[-500:])
    check(">> whos" not in r.stdout, "quit ends the commands: " + r.stdout[-300:])

    # fitdata reads the CSV that export writes (a header, commas)
    r = run(["lecar.ode", "--keep-going", "-c", "set iapp=0.1", "-c", "run",
             "-c", "export data.csv", "-c", "set gca=1.2 gk=1.8",
             "-c", "fitdata data.csv V 2 gca,gk", "-c", "get gca", "-c", "get gk"], work)
    check("Fitted: gca = 1 gk = 2" in r.stdout and "gca = 0.9999999999" in r.stdout,
          "fitdata on an exported CSV: " + r.stdout[-500:])

    # bvp tol=...: the shooting's tolerance, which only Python could set.
    # The sealed cable on [0, 1] has v'(0) = -tanh(1) = -0.761594156;
    # XPPAUT's tolerance of 1e-5 stops at -0.7615917.
    r = run(["cable.ode", "--keep-going", "-c", "set total=1 dt=0.01 a=0 b=1",
             "-c", "bvp", "-c", "get vx", "-c", "bvp tol=1e-8", "-c", "get vx",
             "-c", "bvp tol=abc"], work)
    check("vx = -0.761591676" in r.stdout and "vx = -0.761594155" in r.stdout,
          "bvp tol= tightens the shooting: " + r.stdout[-500:])
    check("error: write bvp [tol=T]" in r.stdout, "bvp refuses a bad setting: " + r.stdout[-300:])

    # freeze keeps a copy and leaves the run: ic last, continue and export
    # still have it (they said there was no run, or started a new one)
    r = run(["lecar.ode", "-c", "freeze", "-c", "ic last", "-c", "continue 10",
             "-c", "export frozen.csv"], work)
    check(r.returncode == 0 and "There is no run yet" not in r.stdout,
          "freeze leaves the run: " + r.stdout[-400:])
    check("Initial values set to the end of the last run." in r.stdout
          and "Continued to t = 40" in r.stdout and (work / "frozen.csv").exists(),
          "ic last, continue and export after freeze: " + r.stdout[-400:])

    # An analysis of two columns names both
    r = run(["lecar.ode", "-c", "xcorr V W 200", "-c", "xcorr V 200"], work)
    check("Correlation of V and W: 202 rows." in r.stdout
          and "Correlation of V: 202 rows." in r.stdout,
          "xcorr names its columns: " + r.stdout[-400:])

    # ic NAME = FORMULA: a variable in the formula is its initial value, not
    # the state the first run or the equilibrium left in XPPAUT's evaluator
    r = run(["lecar.ode", "-c", "eq", "-c", "ic w = v/10", "-c", "get w"], work)
    check("w = -0.0144" in r.stdout, "ic w = v/10 uses v's initial value: " + r.stdout[-300:])

    # set: a parameter's or a variable's value must be a number; a typo or a
    # formula is refused, not read as 0 or as its first number
    r = run(["lecar.ode", "--keep-going", "-c", "set iapp=abc", "-c", "get iapp",
             "-c", "set iapp=2*0.05", "-c", "get iapp", "-c", "set v=-0.3x",
             "-c", "get v", "-c", "set iapp=0.1 v=-0.2", "-c", "get iapp",
             "-c", "get v", "-c", "set meth=stiff", "-c", "get meth"], work)
    check(r.stdout.count("the value must be a number") == 3,
          "set refuses abc, 2*0.05 and -0.3x: " + r.stdout[-600:])
    check("iapp = 0.05" in r.stdout and "v = -0.144" in r.stdout,
          "a refused value leaves the old one: " + r.stdout[-600:])
    check("iapp = 0.1" in r.stdout and "v = -0.2" in r.stdout,
          "numbers are still set: " + r.stdout[-600:])
    check("meth = stiff" in r.stdout, "get meth names the method: " + r.stdout[-300:])

    # Options: a word for a number, and a method no first letter matches,
    # were taken without a word (total=abc made the total 0)
    r = run(["lecar.ode", "--keep-going", "-c", "set total=abc", "-c", "get total",
             "-c", "set meth=nope", "-c", "get meth"], work)
    check("error: total=abc: the value must be a number" in r.stdout
          and "total = 30" in r.stdout, "set total refuses a word: " + r.stdout[-400:])
    check("error: no method called nope" in r.stdout and "meth = rungekutta" in r.stdout,
          "set meth refuses an unknown method: " + r.stdout[-400:])

    # use without a set did nothing, without a word: it names the sets
    r = run(["lecar.ode", "--keep-going", "-c", "use"], work)
    check("write use SET, with one of: vvst pp snic hopf hom cool" in r.stdout,
          "use alone lists the sets: " + r.stdout[-300:])

    # Errors: the first one stops, with exit status 1
    r = run(["lecar.ode", "-c", "export x.png phase", "-c", "run"], work)
    check(r.returncode == 1 and "error:" in r.stdout and ">> run" not in r.stdout,
          "a failing command stops the run with exit status 1")
    r = run(["lecar.ode", "--keep-going", "--quiet", "-c", "kinescope capture",
             "-c", "export y.png time"], work)
    check(r.returncode == 1 and r.stderr.count("error:") == 2 and not r.stdout.strip(),
          "--keep-going goes on; --quiet prints only the errors")
    # Annotations: bad values are refused with a message and add nothing;
    # a plot window's SVG has the text, the arrow's line and head, and a
    # marker that is not a circle; copied to a figure panel, text with no
    # size takes the figure's size and lands on a panel with no notes.
    note = tmp / "notes"
    note.mkdir()
    shutil.copy(examples / "lecar.ode", note)
    r = run(["lecar.ode", "--keep-going", "-c", "run",
             "-c", "annotate phase text 0 0 color red hi",
             "-c", "annotate phase text 0 0 size abc hi",
             "-c", "annotate phase text 0 0 size -3 hi",
             "-c", "annotate phase text 0 0 color #12345 hi",
             "-c", "annotate phase arrow 0 0 1 1 color #12345678",
             "-c", "annotate phase text -0.2 0.2 color #1f77b4 size 14 saddle point",
             "-c", "annotate phase arrow -0.4 0 -0.2 0.2",
             "-c", "annotate phase marker 0 0.1 s",
             "-c", "annotate phase marker 0.1 0.1 ^",
             "-c", "annotate phase text 0.1 0.3 plain",
             "-c", "figure new 86 65 1 2",
             "-c", "annotate figure text 0 0 mine",
             "-c", "figure from phase",
             "-c", "export phase.svg phase", "--script", "notes.py"], note)
    errors = [l for l in (r.stdout + r.stderr).splitlines() if l.startswith("error")]
    for bad in ("'red'", "'abc'", "'-3'", "'#12345'", "'#12345678'"):
        check(any(bad in l for l in errors), "annotate refuses the value %s and says so" % bad)
    svg = (note / "phase.svg").read_text() if (note / "phase.svg").exists() else ""
    clipped = re.findall(r"<(text|line|polygon|rect|circle|path) clip-path='url\(#plot\)'", svg)
    check(re.search(r"<text clip-path='url\(#plot\)'[^>]*fill='#1f77b4'>saddle point</text>", svg),
          "the SVG has the text, in its colour")
    check(re.search(r"<line clip-path='url\(#plot\)'[^>]*stroke-width='1.5'/>", svg)
          and re.search(r"<polygon clip-path='url\(#plot\)' points='[-\d.]+,[-\d.]+ "
                        r"[-\d.]+,[-\d.]+ [-\d.]+,[-\d.]+' fill=", svg),
          "the SVG has the arrow's line and head")
    check("rect" in clipped and clipped.count("polygon") >= 2,
          "the SVG draws the square and triangle markers as such, clipped to the plot")
    check("circle" not in clipped, "no annotation became a circle")
    script = (note / "notes.py").read_text() if (note / "notes.py").exists() else ""
    check(script.count('notes["phase"].text(') == 2 and
          'notes["phase"].text(-0.2, 0.2, "saddle point", size=14.0, color="#1f77b4")' in script,
          "only the valid texts reached the script")
    check('notes["phase"].text(0.1, 0.3, "plain")' in script,
          "text with no size is written without one")
    check('fig[1].run(' in script and 'fig[0].text(0.0, 0.0, "mine")' in script and
          'fig[1].text(0.1, 0.3, "plain")' in script,
          "the plot's notes went to a panel with no notes, with no size added")

    # A full plot (16 annotations): the 17th is refused with one message,
    # and "figure from phase" copies all 16 with no error and no warning:
    # it picks a panel with no notes, so the copy always fits.
    full = tmp / "full"
    full.mkdir()
    shutil.copy(examples / "lecar.ode", full)
    cmds = ["run"] + ["annotate phase text 0.%d 0.1 note%d" % (i % 10, i) for i in range(17)]
    args = ["lecar.ode", "--keep-going"]
    for c in cmds + ["figure from phase"]:
        args += ["-c", c]
    r = run(args + ["--script", "full.py"], full)
    lines = (r.stdout + r.stderr).splitlines()
    refused = [l for l in lines if "annotations already" in l]
    check(len(refused) == 1 and "phase plot has 16" in refused[0],
          "the 17th annotation of a plot is refused once: %r" % refused)
    check(not [l for l in lines if "left out" in l or l.startswith("error: panel")],
          "copying 16 annotations to a figure reports nothing left out")
    script = (full / "full.py").read_text() if (full / "full.py").exists() else ""
    check(len(re.findall(r'^fig\[0\]\.text\(', script, re.M)) == 16 and
          'note16' not in script,
          "all 16 annotations reached the figure panel, and the refused one did not")

    r = run(["lecar.ode", "--no-such-option"], work)
    check(r.returncode == 2, "an unknown option gives exit status 2")

print("%d failure(s)" % len(failures) if failures else "all batch checks passed")
sys.exit(1 if failures else 0)
