"""Run the XPPAUT models published on ModelDB through XPP Studio and through
unmodified XPPAUT 8.0, and compare.

The models come from tools/fetch_modeldb.py (build-modeldb/models/<id>/,
listed in tests/modeldb/manifest.json). Every model file is run in batch mode
(-silent -outfile) by each executable, each in its own copy of the model's
download, and classified:

    identical     both ran and wrote the same numbers
    fixed         they differ only because XPP Studio fixes an upstream bug
                  (docs/upstream-issues.md), checked by rerunning XPPAUT
                  without the bug's trigger where that can be done
    differs       both ran, the numbers differ
    ours-fails    XPPAUT ran it, XPP Studio did not
    ref-fails     XPP Studio ran it, XPPAUT did not
    both-fail     neither wrote output (the log says why)
    too-slow      both were still running after --timeout seconds

    python -I tests/modeldb_check.py --exe build/cli/xppaut-headless.exe \\
        --reference ../refbuild/xppaut-ref.exe [--only 45513] [--jobs 8]

Results go to build-modeldb/results.json. The model files are untrusted:
nothing in them is executed, and a copy that holds a program or library is
not run (XPPAUT would load or start it from the current directory).
"""
import argparse
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
sys.path.insert(1, str(ROOT / "tools"))
import golden  # noqa: E402
from fetch_modeldb import (BINARY_MAGIC, CACHE, MANIFEST,  # noqa: E402
                           RUNNABLE)

TIMEOUT = 600
RESULTS = CACHE.parent / "results.json"
OUTPUT_OPTION = re.compile(r"^\s*@.*?\boutput\s*=\s*([^\s,]+)", re.I | re.M)
DONE = re.compile(r"^\s*#?done", re.I | re.M)
# An init statement naming a variable of 10 or more characters.
LONG_INIT = re.compile(r"^\s*(i|init|initial)\s[^\n]*?\b[A-Za-z_]\w{9,}\s*=",
                       re.I | re.M)
CLASSES = ["identical", "fixed", "differs", "ours-fails", "ref-fails",
           "both-fail", "too-slow"]


def unsafe_files(folder):
    """Programs and libraries in a model folder (there should be none)."""
    bad = []
    for p in folder.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix.lower() in RUNNABLE:
            bad.append(p)
            continue
        with open(p, "rb") as f:
            if f.read(4).startswith(BINARY_MAGIC):
                bad.append(p)
    return bad


def find_output(work, name):
    """The output files written for -outfile/output=name: name itself, or
    name.0, name.1, ... from a range run (@ range=1)."""
    if (work / name).is_file() and (work / name).stat().st_size:
        return [work / name]
    parts = [p for p in work.glob(glob.escape(name) + ".*")
             if p.suffix[1:].isdigit()]
    return sorted(parts, key=lambda p: int(p.suffix[1:]))


def read_output(paths):
    text = "".join(p.read_text(encoding="latin-1") for p in paths)
    try:
        rows = golden.load_rows(text)
    except ValueError:
        return {"status": "ok", "nonnumeric": True,
                "sha256": hashlib.sha256(text.encode("latin-1")).hexdigest()}, None
    return {"status": "ok", "rows": len(rows),
            "cols": len(rows[0]) if rows else 0,
            "sha256": golden.digest(rows)}, rows


def run_xpp(exe, work, model, out, timeout):
    """golden.run_model() for a model that may sit in a folder below the
    working directory, with this check's time limit."""
    env = dict(os.environ, HOME=str(work))  # keep ~/.xpprc out of the run
    try:
        proc = subprocess.run(
            [str(exe), Path(os.path.relpath(model, work)).as_posix(),
             "-silent", "-outfile", out],
            cwd=work, env=env, capture_output=True, timeout=timeout,
            stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        return {"status": "timeout"}, None
    log = (proc.stdout + proc.stderr).decode("latin-1")
    if proc.returncode != 0:
        return {"status": "exit", "code": proc.returncode,
                "log": log[-400:]}, None
    if not (work / out).is_file() or not (work / out).stat().st_size:
        return {"status": "nooutput", "log": log[-400:]}, None
    return read_output([work / out])


def run(exe, source, run_in, model, tmp, timeout, edit=None):
    """Run one model in a fresh copy of its folder; return (info, rows).
    edit(text) -> text changes the copy of the model file first."""
    top = Path(tempfile.mkdtemp(dir=tmp))
    try:
        # The whole download: some models write to ../output or similar.
        shutil.copytree(source, top, dirs_exist_ok=True)
        if unsafe_files(top):
            return {"status": "unsafe"}, None
        work, target = top / run_in, top / model
        if edit:
            text = target.read_text(encoding="latin-1")
            target.write_text(edit(text), encoding="latin-1", newline="")
        out = target.stem + ".golden.dat"
        info, rows = run_xpp(exe, work, target, out, timeout)
        if info["status"] == "nooutput":
            # A range run writes out.0, out.1, ...; an "@ output=file" line
            # in the model overrides -outfile.
            text = target.read_text(encoding="latin-1")
            names = OUTPUT_OPTION.findall(text)  # quotes stay in the name
            for name in [out] + names + [s.strip("'\"") for s in names]:
                files = find_output(work, name)
                if files:
                    info, rows = read_output(files)
                    info["output"] = files[0].name + (
                        f" (+{len(files) - 1})" if len(files) > 1 else "")
                    break
        return info, rows
    finally:
        shutil.rmtree(top, ignore_errors=True)


def first_line(log):
    """The most telling line of an XPPAUT log, for the report."""
    lines = [s.strip() for s in (log or "").splitlines() if s.strip()]
    for s in reversed(lines):
        if re.search(r"error|cannot|can't|unable|illegal|unknown|not |"
                     r"too many|bad|fail|duplicate|problem", s, re.I):
            return s[:160]
    return lines[-1][:160] if lines else ""


def same(a, b):
    return a["status"] == b["status"] == "ok" and a["sha256"] == b["sha256"]


def explain(text, ours, ref, args, source, m, tmp):
    """Check whether a known, fixed upstream bug explains the difference."""
    reasons = []
    if not DONE.search(text):
        # Unmodified XPPAUT parses the last line twice when there is no
        # "done" (form_ode.c read_a_line). Give it one and run it again.
        r2, _ = run(args.reference, source, m["run_in"], m["name"], tmp,
                    args.timeout, edit=lambda t: t.rstrip("\r\n") + "\ndone\n")
        if same(ours, r2):
            return "fixed", "no `done` line: XPPAUT parses the last line twice"
        reasons.append("no `done` line (adding one does not make them agree)")
    export = re.search(r"^\s*export\s*\{", text, re.I | re.M)
    if re.search(r"\bint\s*\[", text) or export:
        # Unmodified XPPAUT reads int[mu] from an unterminated buffer, and
        # export outputs from uninitialised memory when the library is
        # missing: its results vary from run to run.
        for _ in range(5):
            r2, _ = run(args.reference, source, m["run_in"], m["name"], tmp,
                        args.timeout)
            if same(ours, r2):
                return "fixed", (
                    "export without its library: XPPAUT copies uninitialised "
                    "memory into the exported variables" if export else
                    "int[mu]: XPPAUT reads mu from an unterminated buffer "
                    "and varies run to run")
        reasons.append("uses export" if export else "uses int[mu]")
    if ours["status"] != "ok":
        return None, "; ".join(reasons)
    if re.search(r"\bdel(mmult|sparse)\b", text, re.I):
        return "fixed", "delmmult/delsparse: XPPAUT uses the delayed value " \
                        "of the next variable"
    if export and ref["status"] == "exit":
        return "fixed", "export: XPPAUT overflows the heap and crashes"
    if LONG_INIT.search(text):
        return "fixed", "init with a 10-character name: XPPAUT overruns " \
                        "name[10] in compile_em(), then crashes or drops the " \
                        "rest of the line"
    opts = options(text)
    try:
        small_dt = abs(float(opts.get("dt", ".05"))) < abs(float(opts["dtmin"]))
    except (KeyError, ValueError):
        small_dt = False
    if (opts.get("meth", opts.get("method", "")).lower().startswith("cv")
            and small_dt and ref["status"] == "exit"):
        return "fixed", "CVODE with dt below dtmin: XPPAUT frees CVODE " \
                        "memory it never allocated and crashes"
    return None, "; ".join(reasons)


def options(text):
    """The model's @ options, lower-case names; the last setting wins."""
    opts = {}
    for line in re.findall(r"^\s*@(.*)$", text, re.M):
        for name, value in re.findall(r"(\w+)\s*=\s*([^\s,]+)", line):
            opts[name.lower()] = value
    return opts


def check_one(job, args, tmp):
    entry, m = job
    source = args.cache / str(entry["id"])
    model = source / m["name"]
    text = model.read_text(encoding="latin-1")
    t0 = time.time()
    a, ra = run(args.exe, source, m["run_in"], m["name"], tmp, args.timeout)
    b, rb = run(args.reference, source, m["run_in"], m["name"], tmp,
                args.timeout)
    res = {"id": entry["id"], "model": m["name"], "ours": a, "ref": b}
    for side in (a, b):
        side["why"] = first_line(side.pop("log", ""))
        if not side["why"]:
            del side["why"]
    if same(a, b):
        res["class"] = "identical"
    elif a["status"] == "ok" and b["status"] == "ok":
        res["class"] = "differs"
        res["detail"] = (golden.compare(rb, ra, 0.0) if ra and rb else
                         "outputs differ (not all numbers)")
        # Is either side different from one run to the next?
        a2, _ = run(args.exe, source, m["run_in"], m["name"], tmp, args.timeout)
        if not same(a, a2):
            res["ours_varies"] = True
    elif a["status"] == "ok":
        res["class"] = "ref-fails"
    elif b["status"] == "ok":
        res["class"] = "ours-fails"
    elif a["status"] == b["status"] == "timeout":
        res["class"] = "too-slow"
    else:
        res["class"] = "both-fail"
        if a["status"] != b["status"]:
            res["detail"] = f"ours: {a['status']}, reference: {b['status']}"
    if res["class"] in ("differs", "ref-fails", "ours-fails") \
            and not res.get("ours_varies"):
        cls, why = explain(text, a, b, args, source, m, tmp)
        if cls:
            res["class"], res["fixed"] = cls, why
        elif why:
            res["note"] = why
    res["seconds"] = round(time.time() - t0, 1)
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--exe", type=Path, required=True,
                    help="XPP Studio's build/cli/xppaut-headless")
    ap.add_argument("--reference", type=Path, required=True,
                    help="unmodified XPPAUT 8.0")
    ap.add_argument("--cache", type=Path, default=CACHE)
    ap.add_argument("--manifest", type=Path, default=MANIFEST)
    ap.add_argument("--only", action="append",
                    help="model id, or text in the model's path (repeatable)")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) // 2))
    ap.add_argument("--timeout", type=int, default=TIMEOUT)
    ap.add_argument("--results", type=Path, default=RESULTS)
    args = ap.parse_args()
    for name in ("exe", "reference"):
        p = getattr(args, name).resolve()
        if not p.exists() and p.with_suffix(".exe").exists():
            p = p.with_suffix(".exe")
        setattr(args, name, p)

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    jobs = [(e, m) for e in manifest["models"] for m in e["models"]
            if not args.only or any(o == str(e["id"]) or o in m["name"]
                                    for o in args.only)]
    no_files = [e for e in manifest["models"] if not e["models"]]
    missing = [e["id"] for e, _ in jobs
               if not (args.cache / str(e["id"])).is_dir()]
    if missing:
        sys.exit(f"not fetched: {sorted(set(missing))}; "
                 f"run python -I tools/fetch_modeldb.py")

    results = []
    with tempfile.TemporaryDirectory(prefix="xpps-modeldb-") as tmp, \
            ThreadPoolExecutor(args.jobs) as pool:
        futures = [pool.submit(check_one, j, args, tmp) for j in jobs]
        for fut in as_completed(futures):
            res = fut.result()
            results.append(res)
            extra = res.get("fixed") or res.get("detail") or res.get("note") \
                or res["ours"].get("why") or ""
            print(f"{res['class']:10s} {res['id']:>7} {res['model']}  "
                  f"{extra}", flush=True)
    results.sort(key=lambda r: (r["id"], r["model"]))

    counts = {c: sum(r["class"] == c for r in results) for c in CLASSES}
    summary = {"models": len(results), "entries": len(manifest["models"]),
               "entries_without_model_files": [e["id"] for e in no_files],
               "counts": counts}
    args.results.parent.mkdir(parents=True, exist_ok=True)
    args.results.write_text(json.dumps({"summary": summary,
                                        "results": results}, indent=1) + "\n")
    print("summary:", json.dumps(counts))
    print(f"{len(no_files)} ModelDB entries have no model files (web links)")
    bad = counts["differs"] + counts["ours-fails"]
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
