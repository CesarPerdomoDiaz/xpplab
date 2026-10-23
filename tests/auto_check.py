"""Run a headless AUTO continuation (auto_smoke) and compare the computed
bifurcation diagram with a recorded one.

    python tests/auto_check.py <auto_smoke> <model.ode> <parameter> <set> <golden>
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RTOL = 1e-7


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
    if len(got) != len(ref):
        sys.exit(f"{len(got)} diagram points, expected {len(ref)}")
    for i, (a, b) in enumerate(zip(ref, got)):
        for j, (x, y) in enumerate(zip(a, b)):
            if abs(x - y) > RTOL * max(1.0, abs(x), abs(y)):
                sys.exit(f"point {i} column {j}: expected {x!r}, got {y!r}")
    print(f"diagram matches ({len(ref)} points)")


if __name__ == "__main__":
    main()
