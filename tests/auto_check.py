"""Run a headless AUTO continuation (auto_smoke) and compare the computed
bifurcation diagram with a recorded one.

    python tests/auto_check.py <auto_smoke> <model.ode> <parameter> <set> <golden>

The reference was recorded on Windows, and there the whole diagram must
match. On other systems the C maths library differs in the last bits.
The Morris-Lecar test branch is a closed loop of orbits that AUTO circles
many times, and over the laps those differences grow. There, the first
WELL_POSED points (the steady states and the first lap of orbits) must
match closely, with the same special points.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RTOL = 1e-7 if sys.platform == "win32" else 1e-4
WELL_POSED = None if sys.platform == "win32" else 200


def load(path):
    return [[float(x) for x in line.split()]
            for line in Path(path).read_text().splitlines() if line.strip()]


def main():
    smoke, model, par, pset, golden = sys.argv[1:6]
    model = Path(model).resolve()
    with tempfile.TemporaryDirectory(prefix="xpps-auto-") as tmp:
        tmp = Path(tmp)
        shutil.copy(model, tmp / model.name)
        r = subprocess.run([str(Path(smoke).resolve()), model.name, par, pset,
                            "diagram.txt"], cwd=tmp, capture_output=True,
                           text=True, timeout=600)
        print(r.stdout[-3000:])
        if r.returncode != 0:
            sys.exit("auto_smoke failed")
        got, ref = load(tmp / "diagram.txt"), load(golden)
    if WELL_POSED is None:
        if len(got) != len(ref):
            sys.exit(f"{len(got)} diagram points, expected {len(ref)}")
    else:
        got, ref = got[:WELL_POSED], ref[:WELL_POSED]
        if len(got) < len(ref):
            sys.exit(f"only {len(got)} diagram points, expected {len(ref)}")
    # Columns: branch, point, type, label (exact), then parameter, norm,
    # max and min of the first variable, and period (within RTOL).
    for i, (a, b) in enumerate(zip(ref, got)):
        if a[:4] != b[:4]:
            sys.exit(f"point {i}: branch/point/type/label {b[:4]}, expected {a[:4]}")
        for j, (x, y) in enumerate(zip(a[4:], b[4:]), start=4):
            if abs(x - y) > RTOL * max(1.0, abs(x), abs(y)):
                sys.exit(f"point {i} column {j}: expected {x!r}, got {y!r}")
    print(f"diagram matches ({len(ref)} points)")


if __name__ == "__main__":
    main()
