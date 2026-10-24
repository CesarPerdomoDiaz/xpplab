#!/usr/bin/env python3
"""Check the desktop app's features on this machine.

Each case starts the app with a model and command-window commands, takes a
screenshot and writes the command window to a log (--log). The case passes
when the app exits normally, the log has no "error:" line and holds what is
expected, the files the commands write exist, and the screenshot is as large
as the window in pixels. The same cases run on Windows, macOS and Linux, so
the result tables can be set side by side (docs/development/macos.md).

    python tests/parity.py --app build/app/xppstudio.exe
    python3 tests/parity.py --app build/app/xppstudio.app/Contents/MacOS/xppstudio
    python3 tests/parity.py --app ... --all-examples   # also open every example

Each case opens a window for a few seconds. Only the standard library is
used; a C compiler (cc, gcc or clang) is needed for the user-library case.
"""
import argparse
import json
import os
import platform
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "engine" / "xppaut" / "ode"


def default_app():
    base = ROOT / "build" / "app"
    if sys.platform == "win32":
        return base / "xppstudio.exe"
    if sys.platform == "darwin":
        return base / "xppstudio.app" / "Contents" / "MacOS" / "xppstudio"
    return base / "xppstudio"


# ---- Checks on the files a case writes ------------------------------------

def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", head[16:24])


def csv_rows(min_rows, last_t=None):
    """A CSV export with at least min_rows numeric rows, all finite; with
    last_t, the first column of the last row equals it."""
    def check(path):
        lines = [l for l in path.read_text().splitlines() if l.strip()]
        rows = []
        for l in lines[1:]:
            try:
                rows.append([float(v) for v in l.split(",")])
            except ValueError:
                return "a row that is not numbers: %r" % l[:60]
        if len(rows) < min_rows:
            return "%d rows, expected at least %d" % (len(rows), min_rows)
        for r in rows:
            if any(v != v or v in (float("inf"), float("-inf")) for v in r):
                return "a value that is not finite"
        if last_t is not None and abs(rows[-1][0] - last_t) > 1e-6:
            return "ends at t = %g, expected %g" % (rows[-1][0], last_t)
        return None
    return check


def svg_ok(path):
    text = path.read_text()
    if "<svg" not in text or "</svg>" not in text:
        return "not a complete SVG"
    if "<polyline" not in text and "<path" not in text:
        return "no curves in the SVG"
    return None


def png_ok(path):
    size = png_size(path)
    if not size or size[0] < 100 or size[1] < 100:
        return "not a PNG of a figure"
    return None


# ---- Fixtures --------------------------------------------------------------

def find_cc():
    for cc in ("cc", "gcc", "clang"):
        if shutil.which(cc):
            return cc
    return None


def prepare(out):
    """Models copied out of the examples: under a path with spaces and
    accents, a user library to compile, a model to save."""
    fx = out / "fixtures"
    if fx.exists():
        shutil.rmtree(fx)
    odd = fx / "Mis modelos ñ"
    odd.mkdir(parents=True)
    shutil.copy(EXAMPLES / "lecar.ode", odd / "lecar copia.ode")
    shutil.copy(EXAMPLES / "lecar.ode", fx / "tosave.ode")
    dll = fx / "dll"
    dll.mkdir()
    shutil.copy(EXAMPLES / "tstdll.ode", dll / "tstdll.ode")
    shutil.copy(EXAMPLES / "example.c", dll / "example.c")
    cc = find_cc()
    note = "no C compiler found"
    if cc:
        # example.c is old C (functions without a return type); clang 16
        # and later refuse that unless told which C it is.
        cmd = [cc, "-std=gnu89", "-shared", "-fPIC", "-o", "ex.so", "example.c"]
        if sys.platform == "darwin":
            cmd[2] = "-dynamiclib"
        r = subprocess.run(cmd, cwd=dll, capture_output=True, text=True)
        note = "compiled with %s" % " ".join(cmd) if r.returncode == 0 else \
            "compile failed: " + r.stderr.strip()[:300]
    return fx, note


# ---- The cases -------------------------------------------------------------
# model: a name in the examples, or a path; cmds: --cmd; then: --then (once
# the windows are drawn: figure exports); expect: regular expressions the
# log must contain; files: {name: checker} written into the output folder.
# {out} in a command is the output folder.

METHODS = ["rungekutta", "discrete", "euler", "modeuler", "adams", "gear",
           "volterra", "backeuler", "qualrk", "stiff", "cvode", "5dp", "83dp",
           "2rb", "ymp"]
# Rows lecar.ode gives (total=30, dt=0.05) where not 601, as XPPAUT gives
# them: discrete is a map (one row per step, dt=1); ymp is symplectic and
# leaves this dissipative model, so XPPAUT stops at its bound.
MIN_ROWS = {"discrete": 25, "ymp": 150}


def cases(fx):
    c = []

    def add(name, what, model, cmds=(), then=(), expect=(), files=None,
            frames=12, allow_errors=False):
        c.append(dict(name=name, what=what, model=str(model), cmds=list(cmds),
                      then=list(then), expect=list(expect), files=files or {},
                      frames=frames, allow_errors=allow_errors))

    # Opening models and running them
    add("open_run", "open a model, run, export CSV", "lecar.ode",
        ["run", "export {out}/open_run.csv"],
        expect=[r"Loaded lecar\.ode: 2 variables, 12 parameters"],
        files={"open_run.csv": csv_rows(500, 30)})
    add("odd_path", "a model under a path with spaces and accents",
        fx / "Mis modelos ñ" / "lecar copia.ode",
        ["run", "export {out}/odd_path.csv"],
        expect=[r"Loaded lecar copia\.ode"], files={"odd_path.csv": csv_rows(500)})
    add("continue", "continue a run, start from its end", "lecar.ode",
        ["run", "continue 20", "export {out}/continue.csv", "ic last"],
        expect=[r"Initial values set to the end of the last run"],
        files={"continue.csv": csv_rows(500, 50)})
    add("set_get", "set, get, whos, sets, use", "lecar.ode",
        ["set iapp=0.3 total=40", "get iapp", "get total", "whos", "sets",
         "use hopf", "get iapp"],
        expect=[r"iapp = 0\.3\b", r"total = 40", r"Output columns", r"hopf",
                r"Applied set hopf"])
    add("new_model", "new model", "lecar.ode", ["new"])
    add("save", "save a model, and a copy elsewhere", fx / "tosave.ode",
        ["save", "save {out}/saved_copy.ode"], expect=[r"Saved .*tosave\.ode"],
        files={"saved_copy.ode": lambda p: None if "params iapp" in p.read_text()
               else "not the model's text"})
    # XPPAUT sets dt=1 when the discrete method is chosen, so dt is set back
    # after each choice. Volterra runs alone, last.
    cmds = []
    for m in [m for m in METHODS if m != "volterra"]:
        cmds += ["set meth=%s" % m, "set dt=0.05", "run", "export {out}/meth_%s.csv" % m]
    for i in range(0, len(cmds), 28):
        part = cmds[i:i + 28]
        names = [x.split("_", 1)[1][:-4] for x in part if x.startswith("export")]
        add("methods_%d" % (i // 28 + 1), "integrators: " + ", ".join(names),
            "lecar.ode", part,
            files={"meth_%s.csv" % n: csv_rows(MIN_ROWS.get(n, 500)) for n in names})

    # Phase plane, equilibria, figures
    add("phase", "nullclines, direction field, equilibrium, figure export",
        "lecar.ode", ["nullclines", "dirfield", "eq", "show phase"],
        then=["export {out}/phase.svg phase", "export {out}/phase.png phase",
              "export {out}/time.svg time", "export {out}/time.png time"],
        expect=[r"Equilibrium \(saddle\)", r"manifolds drawn"],
        files={"phase.svg": svg_ok, "phase.png": png_ok, "time.svg": svg_ok,
               "time.png": png_ok}, frames=20)
    add("freeze_erase", "keep curves, erase", "lecar.ode",
        ["run", "freeze", "set iapp=0.2", "run", "erase"])

    # AUTO
    add("auto", "AUTO: steady states, periodic orbits, export", "lecar.ode",
        ["use hopf", "auto set parmax=0.6 nmax=220", "auto steady iapp",
         "auto from 2 periodic", "show auto"],
        then=["export {out}/auto.svg auto", "export {out}/auto.png auto"],
        expect=[r"HB #\d+", r"AUTO computed \d+ points", r"periodic orbit"],
        files={"auto.svg": svg_ok, "auto.png": png_ok}, frames=20)
    # No command shows the Two parameters tab, so its figure cannot be
    # exported from a script ("show the two-parameter first").
    add("auto_two", "AUTO: two-parameter curve of a Hopf point", "lecar.ode",
        ["use hopf", "auto set parmax=0.6 nmax=220", "auto steady iapp",
         "auto from 2 two", "show auto"],
        expect=[r"continuing from HB #2", r"AUTO computed \d+ points"], frames=20)
    add("auto_reset", "AUTO: extend and reset", "lecar.ode",
        ["use hopf", "auto set parmax=0.6 nmax=100", "auto steady iapp",
         "auto from 1 extend", "auto reset"],
        expect=[r"AUTO: diagram cleared"])

    # XPPAUT tools
    add("range", "Range: max/min over a parameter, Liapunov exponent", "lecar.ode",
        ["range iapp 0 0.5 20 V", "range iapp 0 0.5 6 lyap"],
        expect=[r"Range over iapp from 0 to 0\.5 in 20 runs",
                r"Range over iapp from 0 to 0\.5 in 6 runs"])
    add("bvp", "boundary-value problem", "dumbbvp.ode", ["bvp"],
        expect=[r"Solved the boundary-value problem"])
    add("view3d", "3D view, spin", "lorenz.ode",
        ["view3d x y z", "view3d angles 30 60", "view3d spin 45"])
    add("animator", "Animator", "doubpend.ode",
        ["ani doubpend.ani", "ani play"], expect=[r"Loaded the animation"], frames=30)
    add("array", "array plot", "amari.ode", ["show array"])

    # Analysis (XPPAUT's stocHastic and Averaging menus)
    add("analysis", "spectrum, Fourier, histograms, correlation, mean",
        "lecar.ode",
        ["set iapp=0.1 total=400", "run", "spectrum V 256", "fourier V",
         "power V", "hist V 30", "hist2d V W 20", "xcorr V W 50", "xcorr V",
         "mean V", "show analysis"],
        expect=[r"of V", r"mean"])
    add("prc", "phase response curve", "lecar.ode",
        ["set iapp=0.1", "prc 200", "show analysis"], expect=[r"over one period"])
    add("hfun", "H function for weak coupling", "lecar.ode",
        ["set iapp=0.1", "hfun V'-V; 0", "show analysis"], expect=[r"over one period"])

    # Model types
    for name, model, what in [
            ("dde", "delay.ode", "delay equation"),
            ("map", "cobweb2.ode", "map (discrete)"),
            ("sde", "fhn_noise.ode", "stochastic (wiener)"),
            ("volterra", "gausvolt.ode", "Volterra integral equation"),
            ("dae", "dae_ex1.ode", "differential-algebraic"),
            ("table", "amari.ode", "tables from files"),
            ("markov", "kepler.ode", "Markov chain"),
            ("global", "iaf.ode", "global flags (integrate-and-fire)")]:
        add("type_" + name, what, model, ["run", "export {out}/type_%s.csv" % name],
            files={"type_%s.csv" % name: csv_rows(5)})

    # A library the user compiled (XPPAUT's export statement)
    add("user_library", "export: a function from a compiled library",
        fx / "dll" / "tstdll.ode",
        ["run", "export {out}/user_library.csv"],
        files={"user_library.csv": csv_rows(10)})

    # Windows, help, look
    add("windows", "show and hide every window, layouts, dark theme",
        "lecar.ode",
        ["show time", "show phase", "show auto", "show editor", "show data",
         "show analysis", "show 3d", "show animator", "show help",
         "show console", "show workspace", "show models", "hide help",
         "layout side", "layout tabs", "layout side", "theme dark"])
    add("help", "every manual page", "lecar.ode",
        ["help " + t for t in ("tutorial", "auto", "analysis", "odefile",
                               "numerics", "commands", "xppaut", "tools",
                               "about")] + ["show help"])
    # Last: a crash brings up macOS's report dialog, which slows the next
    # windows down for a minute.
    add("meth_volterra", "Volterra integrator on a model without integrals",
        "lecar.ode", ["set meth=volterra", "run", "export {out}/meth_volterra.csv"],
        files={"meth_volterra.csv": csv_rows(500)})
    return c


# ---- Running ---------------------------------------------------------------

LINE = re.compile(r"window (\d+)x(\d+) \((\d+)x(\d+) pixels\), display scale "
                  r"([\d.]+), pixel density ([\d.]+), interface scale ([\d.]+), "
                  r"renderer (\S+)")


def run_case(app, out, case, timeout):
    name = case["name"]
    shot, log = out / (name + ".png"), out / (name + ".log")
    for p in [shot, log] + [out / f for f in case["files"]]:
        if p.exists():
            p.unlink()
    model = case["model"]
    if not os.path.isabs(model):
        model = str(EXAMPLES / model)
    args = [str(app), model]
    for cmd in case["cmds"]:
        args += ["--cmd", cmd.replace("{out}", out.as_posix())]
    for cmd in case["then"]:
        args += ["--then", cmd.replace("{out}", out.as_posix())]
    args += ["--screenshot", str(shot), "--frames", str(case["frames"]), "--log", str(log)]
    t0 = time.time()
    problems, info = [], {}
    try:
        r = subprocess.run(args, capture_output=True, text=True, timeout=timeout,
                           errors="replace")
        code, err = r.returncode, r.stderr
    except subprocess.TimeoutExpired as e:
        code, err = "timeout", (e.stderr or b"").decode(errors="replace") \
            if isinstance(e.stderr, bytes) else (e.stderr or "")
        problems.append("did not exit within %d s" % timeout)
    dt = time.time() - t0
    if code not in (0, "timeout"):
        problems.append("exit code %s" % code)
    m = LINE.search(err or "")
    if m:
        info["window"] = "%sx%s" % m.group(1, 2)
        info["pixels"] = "%sx%s" % m.group(3, 4)
        info["scale"] = "display %s, density %s, interface %s, %s" % m.group(5, 6, 7, 8)
    if shot.exists():
        size = png_size(shot)
        info["screenshot"] = "%dx%d" % size if size else "?"
        if m and size and "%dx%d" % size != info["pixels"]:
            problems.append("screenshot %dx%d, window %s pixels" % (size + (info["pixels"],)))
    else:
        problems.append("no screenshot")
        e = Path(str(shot) + ".err")
        if e.exists():
            problems.append(e.read_text().strip())
    text = log.read_text(errors="replace") if log.exists() else ""
    if not log.exists():
        problems.append("no log")
    errors = [l for l in text.splitlines() if l.lstrip().startswith(("error:", "AUTO: error"))]
    if errors and not case["allow_errors"]:
        problems += errors[:3]
    for pat in case["expect"]:
        if not re.search(pat, text):
            problems.append("log lacks /%s/" % pat)
    for f, check in case["files"].items():
        p = out / f
        if not p.exists() or p.stat().st_size == 0:
            problems.append("no %s" % f)
        else:
            why = check(p)
            if why:
                problems.append("%s: %s" % (f, why))
    warnings = [l.strip() for l in text.splitlines() if l.lstrip().startswith(("warning:", "note:"))]
    return dict(name=name, what=case["what"], ok=not problems, problems=problems,
                warnings=warnings, seconds=round(dt, 1), **info)


def all_examples(app, out, timeout):
    """Open, run and show in 3D every example, one app run each, and sort
    them into loaded, not loaded (the log says why) and crashed."""
    models = sorted(p.name for p in EXAMPLES.glob("*.ode"))
    d = out / "examples"
    if d.exists():
        shutil.rmtree(d)
    d.mkdir()
    loaded, refused, crashed = [], {}, {}
    t0 = time.time()
    for i, m in enumerate(models):
        log, shot = d / ("%03d.log" % i), d / ("%03d.png" % i)
        args = [str(app), str(EXAMPLES / m), "--cmd", "run", "--cmd", "show 3d",
                "--screenshot", str(shot), "--frames", "4", "--size", "640x400",
                "--log", str(log)]
        try:
            code = subprocess.run(args, capture_output=True, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
        text = log.read_text(errors="replace") if log.exists() else ""
        if code != 0:
            crashed[m] = "exit code %s" % code
        elif re.search(r"^Loaded ", text, re.M):
            loaded.append(m)
        else:
            refused[m] = next((l for l in text.splitlines() if l.startswith("error:")), "?")
        if shot.exists():
            shot.unlink()
    dt = time.time() - t0
    problems = ["crashed: %s (%s)" % (m, why) for m, why in crashed.items()]
    return dict(name="all_examples", what="open, run and show in 3D every example",
                ok=not crashed, loaded=len(loaded), total=len(models),
                not_loaded=refused, crashed=crashed, seconds=round(dt, 1),
                problems=problems, warnings=[])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--app", type=Path, default=default_app())
    ap.add_argument("--out", type=Path, default=ROOT / "parity-out")
    ap.add_argument("--only", nargs="*", help="run only these cases")
    ap.add_argument("--all-examples", action="store_true")
    ap.add_argument("--timeout", type=int, default=120)
    a = ap.parse_args()
    if not a.app.exists():
        sys.exit("no app at %s; build it or pass --app" % a.app)
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    fx, cc_note = prepare(out)
    results = []
    for case in cases(fx):
        if a.only and case["name"] not in a.only:
            continue
        r = run_case(a.app.resolve(), out, case, a.timeout)
        results.append(r)
        print("%-4s %-16s %5.1fs  %s" % ("ok" if r["ok"] else "FAIL", r["name"],
                                         r["seconds"], "; ".join(r["problems"])[:200]),
              flush=True)
    if a.all_examples:
        r = all_examples(a.app.resolve(), out, a.timeout)
        results.append(r)
        print("%-4s %-16s %5.1fs  %d of %d loaded, %d refused, %d crashed" % (
              "ok" if r["ok"] else "FAIL", r["name"], r["seconds"], r["loaded"],
              r["total"], len(r["not_loaded"]), len(r["crashed"])), flush=True)
    summary = dict(system=platform.platform(), machine=platform.machine(),
                   app=str(a.app), compiler=cc_note,
                   passed=sum(r["ok"] for r in results), total=len(results),
                   results=results)
    (out / "parity.json").write_text(json.dumps(summary, indent=1, ensure_ascii=False))
    print("%d of %d cases passed; details in %s" % (summary["passed"], summary["total"],
                                                    out / "parity.json"))
    return 0 if summary["passed"] == summary["total"] else 1


if __name__ == "__main__":
    sys.exit(main())
