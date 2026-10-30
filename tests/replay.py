#!/usr/bin/env python3
"""Check that a desktop session saved as Python repeats it, number for number.

Each case starts the app with a model and command-window commands, some of
which export data as CSV or MATLAB .mat: the last run, the bifurcation diagram, a Range, an
Analysis result. The app then saves the session as a Python script (script
FILE.py). The case runs that script with the xpplab package from this
repository and the app's own engine library: the script writes the same CSV
files from its own results, and every number must equal the app's, bit for
bit (a .mat file must be the same bytes).

    python tests/replay.py --app build/app/xpplab.exe
    python3 tests/replay.py --app build/app/xpplab.app/Contents/MacOS/xpplab
    python tests/replay.py --only auto,range      # some cases

Each case opens a window for a few seconds. The scripts need numpy.
"""
import argparse
import ast
import math
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "engine" / "xppaut" / "ode"


def default_app():
    base = ROOT / "build" / "app"
    if sys.platform == "win32":
        return base / "xpplab.exe"
    if sys.platform == "darwin":
        return base / "xpplab.app" / "Contents" / "MacOS" / "xpplab"
    return base / "xpplab"


def engine_of(app):
    """The engine library the app loads, so that the script uses the same."""
    for name in ("libxppe.dll", "libxppe.dylib", "libxppe.so"):
        for d in (app.parent, app.parent.parent / "Frameworks",
                  app.parent.parent / "lib"):
            if (d / name).exists():
                return d / name
    return None


# A folder with non-ASCII characters in its name, made in every case (no
# space: the command window splits words at spaces).
UNICODE_DIR = "\u00f1and\u00fa_\u00e9"

# Each case: a model and the commands; {out} is the case's folder. Every
# CSV the commands export is compared with the script's.
CASES = [
    ("runs", "lecar.ode", [
        "set iapp=0.1 total=300", "run", "export {out}/run.csv",
        "continue 50", "export {out}/continued.csv",
        "eq", "ic last", "run", "export {out}/from_end.csv",
        "set meth=stiff dt=0.1", "run", "export {out}/stiff.csv",
        "set meth=5dp", "run", "export {out}/dopri.csv",
        "set meth=discrete", "set meth=euler dt=0.05", "run",
        "export {out}/euler.csv"]),
    ("sets_poincare", "lecar.ode", [
        "use hopf", "run", "export {out}/hopf.csv",
        "set total=400 poimap=section poivar=W poipln=0.2", "run",
        "export {out}/section.csv"]),
    ("saddle", "lecar.ode", [
        "eq", "run", "export {out}/run.csv"]),
    ("auto", "lecar.ode", [
        "use hopf", "auto set parmax=0.6 nmax=220", "auto steady iapp",
        "auto from 2 periodic", "export {out}/diagram.csv auto",
        "auto reset", "auto steady iapp", "export {out}/again.csv auto"]),
    # Counts are echoed as integers (nmax=600, not 600.0).
    ("auto_counts", "lecar.ode", [
        "use hopf", "auto set parmax=0.6 nmax=600 npr=50 ntst=25",
        "auto steady iapp", "export {out}/diagram.csv auto"]),
    ("auto_files", "lecar.ode", [
        "use hopf", "auto set parmax=0.6 nmax=220", "auto steady iapp",
        "auto from 2 periodic", "auto save {out}/d.auto", "auto reset",
        "auto load {out}/d.auto", "export {out}/loaded.csv auto",
        "auto orbit 6", "export {out}/orbit.csv",
        "auto from 17 extend", "export {out}/continued.csv auto"]),
    ("auto_two", "lecar.ode", [
        "use hopf", "auto set parmax=0.6 nmax=220", "auto steady iapp",
        "auto from 2 two", "export {out}/diagram.csv auto"]),
    ("range", "lecar.ode", [
        "range iapp 0 0.5 20 V", "export {out}/maxmin.csv range",
        "export {out}/after_maxmin.csv",
        "range iapp 0 0.5 6 lyap", "export {out}/lyap.csv range",
        "range iapp 0.05 0.3 4 V continue", "export {out}/followed.csv range",
        "range iapp 0 0.3 4", "export {out}/last_of_kept.csv"]),
    ("range_two", "lecar.ode", [
        "range iapp 0 0.5 6 gca 0.8 1.2 5 V", "export {out}/grid.csv range",
        "range iapp 0 0.5 6 gca 0.8 1.2 lyap", "export {out}/line.csv range",
        "range iapp 0.05 0.3 4 gca 0.9 1.1 3 V continue",
        "export {out}/grid_continued.csv range",
        "range iapp 0 0.3 3 V -0.2 0.2 2", "export {out}/last_of_kept.csv"]),
    ("initial_values", "lecar.ode", [
        "set iapp=0.1 total=40", "run", "ic shift", "run",
        "export {out}/shifted.csv", "ic write {out}/ics.dat", "set V=0 W=0",
        "ic read {out}/ics.dat", "run", "export {out}/from_file.csv",
        "ic V = vk + 0.1*gca", "run", "export {out}/formula.csv",
        "click keep", "click -0.3 0.0", "click 0.2 0.3",
        "export {out}/clicked.csv"]),
    ("initial_array", "amari.ode", [
        "set total=5", "ic u[0..199] = 0.8*exp(-([j]-100)^2/50)", "run",
        "export {out}/bump.csv"]),
    ("nullclines", "lecar.ode", [
        "nullclines freeze", "nullclines freeze iapp 0 0.3 4", "set gca=1.2",
        "export {out}/all.csv nullclines", "nullclines off",
        "export {out}/frozen.csv nullclines", "nullclines unfreeze",
        "nullclines freeze gk 1 3 3", "export {out}/again.csv nullclines"]),
    ("ensemble", "fhn_noise.ode", [
        "set total=50", "run", "ensemble 20", "export {out}/ens.csv analysis",
        "export {out}/last.csv", "run", "export {out}/after.csv"]),
    ("analysis", "lecar.ode", [
        "set iapp=0.1 total=400", "run",
        "spectrum V 256", "export {out}/spectrum.csv analysis",
        "spectrum V W coherence 128", "export {out}/coherence.csv analysis",
        "fourier V", "export {out}/fourier.csv analysis",
        "power V", "export {out}/power.csv analysis",
        "hist V 30", "export {out}/hist.csv analysis",
        "hist V 20 -0.4 0.2", "export {out}/hist_range.csv analysis",
        "hist2d V W 20", "export {out}/hist2d.csv analysis",
        "xcorr V W 50", "export {out}/xcorr.csv analysis",
        "xcorr V", "export {out}/autocorr.csv analysis",
        "mean V", "run", "export {out}/run.csv"]),
    ("prc", "lecar.ode", [
        "set iapp=0.1", "prc 200", "export {out}/prc.csv analysis",
        "export {out}/period.csv", "continue 10", "export {out}/continued.csv"]),
    ("hfun", "lecar.ode", [
        "set iapp=0.1", "hfun V'-V; 0", "export {out}/h.csv analysis"]),
    ("bvp", "dumbbvp.ode", ["bvp", "export {out}/bvp.csv"]),
    ("equilibria", "lecar.ode", [
        "range iapp 0 0.3 31 eq V", "export {out}/sweep.csv range",
        "eq all", "export {out}/all.csv equilibria",
        "set iapp=0.2", "eq all 300", "export {out}/more.csv equilibria",
        "run", "export {out}/run.csv"]),
    ("equilibria_3d", "lorenz.ode", [
        "eq all 200", "export {out}/all.csv equilibria",
        "range r 0.5 30 30 eq z", "export {out}/sweep.csv range"]),
    ("linearize", "lecar.ode", [
        "linearize at eq inputs iapp gca", "export {out}/at_eq.csv analysis",
        "set V=-0.1 W=0.2", "linearize at ic iapp",
        "export {out}/at_ic.csv analysis",
        "set iapp=0.08", "linearise", "export {out}/no_inputs.csv analysis",
        "run", "export {out}/run.csv"]),
    ("linearize_map", "del_log.ode", [
        "linearize eps r", "export {out}/map.csv analysis"]),
    # Pole placement: the gain from the engine, the closed loop written as a
    # model, opened with the session's values and run; then the closed loop
    # linearised (its eigenvalues are the poles) and two inputs at once.
    ("pole_placement", "lecar.ode", [
        "set iapp=0.08 phi=0.3 total=60", "linearize inputs iapp",
        "poles -0.5+0.3i -0.5-0.3i", "closedloop {out}/closed.ode",
        "export {out}/closed.csv", "set V=-0.1", "run",
        "export {out}/kicked.csv", "linearize",
        "export {out}/closed_lin.csv analysis"]),
    ("pole_placement_2", "lecar.ode", [
        "linearize inputs gca gk", "poles -0.3 -0.7",
        "closedloop {out}/closed2.ode", "export {out}/run.csv"]),
    # Root loci: over the gain from pole placement, over one state fed
    # back, and over a parameter with the equilibrium followed.
    ("root_locus", "lecar.ode", [
        "set iapp=0.08", "linearize inputs iapp", "poles -0.5+0.3i -0.5-0.3i",
        "locus gain 0 2 51", "export {out}/gain.csv locus",
        "locus feedback iapp V -1 1 41", "export {out}/feedback.csv locus",
        "locus par phi 0.1 1 31", "export {out}/phi.csv locus"]),
    # LQR: the continuous Riccati equation (Morris-Lecar, the closed loop
    # run) and the discrete one (a map, its gain locus).
    ("lqr", "lecar.ode", [
        "set iapp=0.08", "linearize inputs iapp gca", "lqr Q 10 1 R 0.5 2",
        "closedloop {out}/lqr.ode", "set V=-0.1", "run", "export {out}/run.csv"]),
    ("lqr_map", "del_log.ode", [
        "linearize eps r", "lqr Q 1 R 2", "locus gain 0 1 21",
        "export {out}/locus.csv locus"]),
    # Many runs with their stopping flags, shooting along eigenvectors, and
    # a figure built from them (the script must draw and save it too).
    ("many_shoot", "tests/models/kite.ode", [
        "set VW=2.1",
        "many 30 seed 3 gam 0 1.5707963 om -1.3 1.3 keep tens 0 inf",
        "export {out}/many.csv many",
        "figure new 120 60 1 2", "figure many GAM OM split",
        "figure style 1 scale 57.29577951308232 1 label Ground",
        "figure xlabel $\\Gamma$ [deg]",
        "set VW=2.5 gam=0.3 om=0", "shoot time 20",
        "export {out}/shoot.csv many",
        "figure panel 2", "figure many T GAM", "figure function 1/sqrt(x) 0.1 10",
        "figure ylim 0 2", "figure export {out}/fig.pdf",
        "set VW=8", "shoot eps 1e-4 both", "export {out}/both.csv many",
        "run", "export {out}/run.csv"]),
    # MATLAB .mat files: the app's and the script's must be the same bytes.
    ("mat", "lecar.ode", [
        "set iapp=0.1 total=200", "run", "export {out}/run.mat",
        "set gca=1.1", "continue 40", "export {out}/continued.mat",
        "export {out}/same.csv",
        "range iapp 0 0.5 12 V", "export {out}/maxmin.mat range",
        "range iapp 0 0.5 4 gca 0.8 1.2 3 V", "export {out}/grid.mat range",
        "range iapp 0 0.5 4 gca 0.8 1.2 lyap", "export {out}/line.mat range",
        "range iapp 0 0.3 11 eq V", "export {out}/eqsweep.mat range",
        "eq all", "export {out}/all.mat equilibria",
        "use hopf", "auto set parmax=0.6 nmax=220", "auto steady iapp",
        "auto from 2 periodic", "export {out}/diagram.mat auto",
        "run", "export {out}/last.mat"]),
    ("mat_analysis", "lecar.ode", [
        "nullclines freeze", "nullclines freeze iapp 0 0.3 3",
        "export {out}/nc.mat nullclines",
        "set total=50", "ensemble 10", "export {out}/ens.mat analysis",
        "spectrum V 128", "export {out}/spectrum.mat analysis",
        "set iapp=0.08", "linearize inputs iapp", "poles -0.5+0.3i -0.5-0.3i",
        "locus gain 0 2 21", "export {out}/gain.mat locus",
        "locus par phi 0.1 1 11", "export {out}/phi.mat locus"]),
    # A path with accents: the app and the script both write there.
    ("mat_unicode", "lecar.ode", [
        "set iapp=0.1 total=100", "run",
        "export {out}/" + UNICODE_DIR + "/run.mat",
        "range iapp 0 0.5 6 V", "export {out}/" + UNICODE_DIR + "/range.mat range"]),
    # The model, the exports, the session's files and the script all live in
    # a folder with accents ("unicode_load" copies the model there).
    ("unicode_load", "lecar.ode", [
        "set iapp=0.1 total=100", "run",
        "export {out}/" + UNICODE_DIR + "/run.csv",
        "ic write {out}/" + UNICODE_DIR + "/ics.dat", "set V=0 W=0",
        "ic read {out}/" + UNICODE_DIR + "/ics.dat", "run",
        "export {out}/" + UNICODE_DIR + "/from_file.csv",
        "use hopf", "auto set parmax=0.6 nmax=60", "auto steady iapp",
        "auto save {out}/" + UNICODE_DIR + "/d.auto",
        "auto reset", "auto load {out}/" + UNICODE_DIR + "/d.auto",
        "export {out}/" + UNICODE_DIR + "/diagram.csv auto"]),
    ("mat_many", "tests/models/kite.ode", [
        "set VW=2.1",
        "many 6 seed 3 gam 0 1.5707963 om -1.3 1.3 keep tens 0 inf",
        "export {out}/many.mat many", "export {out}/many.csv many",
        "run", "export {out}/run.mat"]),
]


def cells_equal(x, y):
    if x == y:
        return True
    try:
        a, b = float(x), float(y)
    except ValueError:
        return False
    return a == b or (math.isnan(a) and math.isnan(b))


def compare(app_csv, py_csv):
    """None if the files hold the same numbers, else what differs."""
    if py_csv.suffix == ".mat":
        if not py_csv.exists():
            return "%s: the script did not write it" % py_csv.name
        if py_csv.read_bytes() != app_csv.read_bytes():
            return "%s: the app's file and the script's differ" % py_csv.name
        return None
    if not py_csv.exists():
        return "the script did not write %s" % py_csv.name
    a = app_csv.read_text().splitlines()
    b = py_csv.read_text().splitlines()
    if a[:1] != b[:1]:
        return "%s: headers differ: %r / %r" % (py_csv.name, a[:1], b[:1])
    if len(a) != len(b):
        return "%s: %d rows in the app, %d from the script" % (
            py_csv.name, len(a) - 1, len(b) - 1)
    bad, first = 0, None
    for i, (ra, rb) in enumerate(zip(a[1:], b[1:]), start=1):
        ca, cb = ra.split(","), rb.split(",")
        if len(ca) != len(cb):
            return "%s: row %d has %d values in the app, %d from the script" % (
                py_csv.name, i, len(ca), len(cb))
        for j, (x, y) in enumerate(zip(ca, cb)):
            if not cells_equal(x, y):
                bad += 1
                if first is None:
                    first = "row %d, column %s: app %s, script %s" % (
                        i, a[0].split(",")[j], x, y)
    if bad:
        return "%s: %d values differ, first at %s" % (py_csv.name, bad, first)
    return None


COUNTS = ("nmax", "npr", "ntst")


def count_problems(script):
    """Each count given to <...>.auto.set() in the script must be an int literal."""
    tree = ast.parse(script.read_text(encoding="utf-8"))
    bad = []
    for node in ast.walk(tree):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                and node.func.attr == "set"
                and isinstance(node.func.value, ast.Attribute)
                and node.func.value.attr == "auto"):
            for kw in node.keywords:
                if kw.arg in COUNTS and not (
                        isinstance(kw.value, ast.Constant)
                        and type(kw.value.value) is int):
                    bad.append("auto.set(%s=...) is not an integer literal" % kw.arg)
    return bad


def auto_set_counts(script):
    """The count keywords of the script's auto.set() calls, as {name: value}."""
    found = {}
    for node in ast.walk(ast.parse(script.read_text(encoding="utf-8"))):
        if (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                and node.func.attr == "set"
                and isinstance(node.func.value, ast.Attribute)
                and node.func.value.attr == "auto"):
            for kw in node.keywords:
                if kw.arg in COUNTS and isinstance(kw.value, ast.Constant):
                    found[kw.arg] = kw.value.value
    return found


def run_case(app, engine, out, name, model, cmds, timeout):
    d = out / name
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    (d / UNICODE_DIR).mkdir()
    script, log, shot = d / "session.py", d / "app.log", d / "shot.png"
    model_file = ROOT / model if model.startswith("tests/") else EXAMPLES / model
    if name == "unicode_load":
        script = d / UNICODE_DIR / "séssion.py"
        shutil.copy(model_file, d / UNICODE_DIR / ("lécar" + model_file.suffix))
        model_file = d / UNICODE_DIR / ("lécar" + model_file.suffix)
    args = [str(app), str(model_file)]
    for c in cmds:
        args += ["--cmd", c.replace("{out}", d.as_posix())]
    args += ["--then", "script " + script.as_posix(), "--screenshot", str(shot),
             "--frames", "8", "--log", str(log)]
    problems = []
    try:
        r = subprocess.run(args, capture_output=True, text=True, timeout=timeout,
                           errors="replace")
        if r.returncode != 0:
            problems.append("the app exited with %s: %s" % (
                r.returncode, (r.stderr or "").strip()[-300:]))
    except subprocess.TimeoutExpired:
        return ["the app did not exit within %d s" % timeout]
    text = log.read_text(errors="replace") if log.exists() else ""
    problems += ["app: " + l for l in text.splitlines() if l.startswith("error")]
    if not script.exists():
        return problems + ["the app saved no script"]
    problems += count_problems(script)
    if name == "auto_counts" and auto_set_counts(script) != {
            "nmax": 600, "npr": 50, "ntst": 25}:
        problems.append("auto.set() counts in the script: %r" % auto_set_counts(script))
    exported = sorted(list(d.glob("*.csv")) + list(d.glob("*.mat"))
                      + list(d.glob(UNICODE_DIR + "/*.mat"))
                      + list(d.glob(UNICODE_DIR + "/*.csv")))
    if not exported:
        problems.append("the commands exported nothing")
    renamed = []
    for p in exported:
        q = p.with_name(p.stem + ".app" + p.suffix)
        p.replace(q)
        renamed.append((q, p))
    env = dict(os.environ, PYTHONPATH=str(ROOT / "python" / "src"),
               MPLBACKEND="Agg")
    if engine:
        env["XPPLAB_ENGINE"] = str(engine)
    try:
        r = subprocess.run([sys.executable, str(script)], cwd=d, env=env,
                           capture_output=True, text=True, timeout=timeout,
                           errors="replace")
        if r.returncode != 0:
            tail = "\n    ".join((r.stderr or "").strip().splitlines()[-6:])
            problems.append("the script failed (exit %s):\n    %s" % (r.returncode, tail))
    except subprocess.TimeoutExpired:
        problems.append("the script did not finish within %d s" % timeout)
    for app_csv, py_csv in renamed:
        why = compare(app_csv, py_csv)
        if why:
            problems.append(why)
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--app", type=Path, default=default_app())
    ap.add_argument("--out", type=Path, default=ROOT / "build" / "replay")
    ap.add_argument("--only", help="comma-separated case names")
    ap.add_argument("--timeout", type=int, default=120)
    args = ap.parse_args()
    # Absolute: each script runs in its own case folder.
    args.app, args.out = args.app.resolve(), args.out.resolve()
    if not args.app.exists():
        sys.exit("no app at %s (build it, or give --app)" % args.app)
    engine = engine_of(args.app)
    only = set(args.only.split(",")) if args.only else None
    failed = 0
    for name, model, cmds in CASES:
        if only and name not in only:
            continue
        problems = run_case(args.app, engine, args.out, name, model, cmds,
                            args.timeout)
        print("%-14s %s" % (name, "ok" if not problems else "FAILED"))
        for p in problems:
            print("    " + p)
        failed += bool(problems)
    print("%d case(s) failed" % failed if failed else "all cases repeat exactly")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
