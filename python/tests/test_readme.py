"""The Python examples a newcomer copies first, run as written from an empty
folder, as after `pip install xpplab`: the block in the README's Python
section and the first block of the package's own README (its PyPI page).
A README edit that breaks either example fails here."""
import os
import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


def first_python_block(path, after=None):
    """The first ```python block of a Markdown file, or the first one after
    the heading `after`."""
    text = path.read_text(encoding="utf-8")
    if after is not None:
        text = text[text.index(after):]
    m = re.search(r"^```python\n(.*?)^```", text, re.S | re.M)
    assert m, f"no python block in {path}"
    return m.group(1)


@pytest.mark.parametrize("readme, after", [
    ("README.md", "## Python"),
    ("python/README.md", None),
])
def test_readme_example_runs_from_an_empty_folder(readme, after, tmp_path):
    pytest.importorskip("matplotlib")
    code = first_python_block(ROOT / readme, after)
    script = tmp_path / "example.py"
    script.write_text(code, encoding="utf-8")
    # The script runs in an empty folder: relative paths in the environment
    # (PYTHONPATH=python/src, as in the build instructions) would no longer
    # point anywhere, so the package and the engine are given absolutely.
    env = dict(os.environ, MPLBACKEND="Agg")
    paths = [str(ROOT / "python" / "src")]
    paths += [os.path.abspath(p) for p in env.get("PYTHONPATH", "").split(os.pathsep) if p]
    env["PYTHONPATH"] = os.pathsep.join(paths)
    if env.get("XPPLAB_ENGINE"):
        env["XPPLAB_ENGINE"] = os.path.abspath(env["XPPLAB_ENGINE"])
    out = subprocess.run([sys.executable, str(script)], cwd=tmp_path, env=env,
                         capture_output=True, text=True, timeout=600)
    assert out.returncode == 0, out.stderr[-3000:]
