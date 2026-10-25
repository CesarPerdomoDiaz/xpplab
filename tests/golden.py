"""Golden-output regression tests for the XPPAUT engine.

Every example model shipped with XPPAUT is integrated in batch mode and the
resulting output.dat is compared against a recorded reference. The
references were produced by the unmodified XPPAUT 8.0 engine, so any
difference means a change in numerical behaviour.

For each model the index stores the SHA-256 of the full output (the
bit-identical check) and a sample of evenly spaced rows, fewer for wide
models (used for tolerance checks and to show where a run diverged).

    python tests/golden.py record --exe build/cli/xppaut-headless
    python tests/golden.py check  --exe build/cli/xppaut-headless
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODEL_DIRS = ["ode", "canonical", "tstauto"]
UPSTREAM = ROOT / "engine" / "xppaut"
GOLDEN = ROOT / "tests" / "golden"
TIMEOUT = 60

# Models whose output differs from run to run in unmodified XPPAUT (fixed
# in XPP Lab; see docs/upstream-issues.md). In compare mode a mismatch
# with the reference is reported but does not fail the suite.
REFERENCE_NONDETERMINISTIC = {
    "ode/mlvolt": "unmodified XPPAUT reads int[.5] with stray digits about "
                  "1 run in 30",
}
SAMPLE_ROWS = 200
SAMPLE_CELLS = 4000  # wide models get fewer sampled rows


def load_rows(text):
    rows = []
    for line in text.splitlines():
        parts = line.split()
        if parts:
            rows.append([float(x) for x in parts])
    return rows


def sample(rows):
    """Evenly spaced rows, always including the first and last."""
    n = len(rows)
    want = max(5, min(SAMPLE_ROWS, SAMPLE_CELLS // max(1, len(rows[0]))))
    if n <= want:
        return list(range(n))
    step = (n - 1) / (want - 1)
    return sorted({round(k * step) for k in range(want)})


def digest(rows):
    h = hashlib.sha256()
    for r in rows:
        h.update(repr(r).encode())
    return h.hexdigest()


def compare(ref, out, rtol):
    """Return None if equal within rtol, else a description of the first problem."""
    if len(ref) != len(out):
        return f"row count {len(out)} != reference {len(ref)}"
    worst, where = 0.0, None
    for i, (a, b) in enumerate(zip(ref, out)):
        if len(a) != len(b):
            return f"row {i}: {len(b)} columns != reference {len(a)}"
        for j, (x, y) in enumerate(zip(a, b)):
            d = abs(x - y) / max(1.0, abs(x), abs(y))
            if d > worst:
                worst, where = d, (i, j, x, y)
    if worst > rtol:
        i, j, x, y = where
        return f"max rel diff {worst:.3e} at row {i} col {j}: ref {x!r} got {y!r}"
    return None


def run_model(exe, workdir, model):
    out = workdir / (model.stem + ".golden.dat")
    if out.exists():
        out.unlink()
    env = dict(os.environ)
    env["HOME"] = str(workdir)  # keep the user's ~/.xpprc out of the run
    start = time.time()
    try:
        proc = subprocess.run(
            [str(exe), model.name, "-silent", "-outfile", out.name],
            cwd=workdir, env=env, capture_output=True, timeout=TIMEOUT,
            stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        return {"status": "timeout"}, None
    elapsed = time.time() - start
    log = (proc.stdout + proc.stderr).decode("latin-1")
    if proc.returncode != 0:
        return {"status": "exit", "code": proc.returncode,
                "log": log[-400:]}, None
    if not out.exists() or out.stat().st_size == 0:
        return {"status": "nooutput", "log": log[-400:]}, None
    rows = load_rows(out.read_text(encoding="latin-1"))
    return {"status": "ok", "rows": len(rows),
            "cols": len(rows[0]) if rows else 0,
            "seconds": round(elapsed, 3), "sha256": digest(rows)}, rows


def models():
    for d in MODEL_DIRS:
        for m in sorted((UPSTREAM / d).glob("*.ode")):
            yield d, m


def uses_export(model):
    """Does the model link external C code with an export statement?"""
    text = model.read_text(errors="replace")
    return re.search(r"^\s*export\s*\{", text, re.I | re.M) is not None


def compare_executables(exe, reference, rtol, only):
    """Run every example with both executables and compare the outputs.
    Each executable gets its own copy of the model directories so that
    files one writes cannot affect the other."""
    failures, same = [], 0
    with tempfile.TemporaryDirectory(prefix="xpps-cmp-") as tmp:
        tmp = Path(tmp)
        for side in ("ours", "ref"):
            for d in MODEL_DIRS:
                shutil.copytree(UPSTREAM / d, tmp / side / d)
        for d, model in models():
            key = f"{d}/{model.stem}"
            if only and only not in key:
                continue
            a, ra = run_model(exe, tmp / "ours" / d, tmp / "ours" / d / model.name)
            b, rb = run_model(reference, tmp / "ref" / d, tmp / "ref" / d / model.name)
            if (b["status"] == "exit" and a["status"] != "exit"
                    and uses_export(model)):
                # Unmodified XPPAUT overflows the heap on export lists.
                print(f"fixed     {key}: unmodified XPPAUT crashed "
                      f"(export bug, see docs/upstream-issues.md)")
                same += 1
                continue
            if a["status"] != b["status"]:
                problem = f"status {a['status']} != reference {b['status']}"
            elif ra is None:
                problem = None
            else:
                problem = compare(rb, ra, rtol)
            if problem and key in REFERENCE_NONDETERMINISTIC:
                print(f"flaky     {key}: {REFERENCE_NONDETERMINISTIC[key]}")
            elif problem:
                failures.append(f"{key}: {problem}")
                print(f"FAIL      {key}: {problem}")
            else:
                same += 1
                print(f"same      {key} ({a['status']})")
    print(f"summary: {same} same, {len(failures)} different")
    for f in failures:
        print("  " + f)
    return 1 if failures else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["record", "check", "compare"],
                    help="record/check against tests/golden, or compare two "
                         "executables side by side")
    ap.add_argument("--exe", required=True)
    ap.add_argument("--reference",
                    help="compare mode: the unmodified XPPAUT executable")
    ap.add_argument("--rtol", type=float, default=0.0,
                    help="relative tolerance (default: bit-identical)")
    ap.add_argument("--only", help="run only models whose name contains this")
    args = ap.parse_args()

    exe = Path(args.exe).resolve()
    if not exe.exists() and exe.with_suffix(".exe").exists():
        exe = exe.with_suffix(".exe")
    if args.mode == "compare":
        if not args.reference:
            ap.error("compare mode needs --reference")
        sys.exit(compare_executables(exe, Path(args.reference).resolve(),
                                     args.rtol, args.only))

    index_path = GOLDEN / "index.json"
    index = json.loads(index_path.read_text()) if index_path.exists() else {}

    failures = []
    counts = {}
    with tempfile.TemporaryDirectory(prefix="xpps-golden-") as tmp:
        tmp = Path(tmp)
        for d in MODEL_DIRS:
            shutil.copytree(UPSTREAM / d, tmp / d)
        for d, model in models():
            key = f"{d}/{model.stem}"
            if args.only and args.only not in key:
                continue
            info, rows = run_model(exe, tmp / d, tmp / d / model.name)
            counts[info["status"]] = counts.get(info["status"], 0) + 1
            if args.mode == "record":
                if rows is not None:
                    idx = sample(rows)
                    info["sample_index"] = idx
                    info["sample"] = [rows[i] for i in idx]
                index[key] = info
                print(f"{info['status']:9s} {key}")
                continue
            ref_info = index.get(key)
            if ref_info is None:
                print(f"NEW       {key} ({info['status']})")
                continue
            if ref_info["status"] != info["status"]:
                failures.append(f"{key}: status {info['status']} != "
                                f"reference {ref_info['status']}")
                print(f"FAIL      {key}")
                continue
            if rows is None:
                print(f"same      {key} ({info['status']})")
                continue
            if info["sha256"] == ref_info["sha256"]:
                print(f"identical {key}")
                continue
            if len(rows) != ref_info["rows"]:
                problem = f"row count {len(rows)} != reference {ref_info['rows']}"
            else:
                got = [rows[i] for i in ref_info["sample_index"]]
                problem = compare(ref_info["sample"], got, args.rtol)
            if problem:
                failures.append(f"{key}: {problem}")
                print(f"FAIL      {key}: {problem}")
            else:
                print(f"close     {key} (within rtol on sampled rows)")

    if args.mode == "record":
        GOLDEN.mkdir(parents=True, exist_ok=True)
        index_path.write_text(json.dumps(index, indent=1, sort_keys=True) + "\n")
    print("summary:", json.dumps(counts, sort_keys=True))
    if failures:
        print(f"{len(failures)} failure(s):")
        for f in failures:
            print("  " + f)
        sys.exit(1)


if __name__ == "__main__":
    main()
