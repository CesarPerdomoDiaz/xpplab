"""Run api_smoke on a model and check that the engine API returns exactly
the data that xppaut -silent writes to output.dat.

    python tests/api_check.py <api_smoke> <xppaut-headless> <model.ode>
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def rows(path):
    return [line.split() for line in path.read_text().splitlines() if line.strip()]


def main():
    smoke, headless, model = (Path(a).resolve() for a in sys.argv[1:4])
    with tempfile.TemporaryDirectory(prefix="xpps-api-") as tmp:
        tmp = Path(tmp)
        shutil.copy(model, tmp / model.name)
        r = subprocess.run([str(smoke), model.name], cwd=tmp,
                           capture_output=True, text=True)
        print(r.stdout)
        if r.returncode != 0:
            print(r.stderr)
            sys.exit("api_smoke failed")
        subprocess.run([str(headless), model.name, "-silent", "-outfile",
                        "ref.dat"], cwd=tmp, capture_output=True,
                       stdin=subprocess.DEVNULL, check=True)
        got, ref = rows(tmp / "api_run1.dat"), rows(tmp / "ref.dat")
        if got != ref:
            sys.exit(f"API data differs from xppaut -silent "
                     f"({len(got)} vs {len(ref)} rows)")
        print(f"API output identical to xppaut -silent ({len(ref)} rows)")


if __name__ == "__main__":
    main()
