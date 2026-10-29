"""The notebooks in notebooks/ must run from a clean kernel without an error,
and must be committed without outputs.

Running needs nbclient, nbformat and ipykernel (the ``test`` extra); where
they are missing the test is skipped.
"""
import os
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
NOTEBOOKS = sorted((ROOT / "notebooks").glob("*.ipynb"))


def test_there_are_notebooks():
    names = [p.name for p in NOTEBOOKS]
    assert names == ["01_first_steps.ipynb", "02_bifurcations.ipynb", "03_control.ipynb"]


@pytest.mark.parametrize("path", NOTEBOOKS, ids=lambda p: p.name)
def test_notebook_has_no_stored_outputs(path):
    nbformat = pytest.importorskip("nbformat", reason="nbformat is not installed")
    nb = nbformat.read(path, as_version=4)
    code = [c for c in nb.cells if c.cell_type == "code"]
    assert code, f"{path.name} has no code cells"
    for c in code:
        assert c.outputs == [], f"{path.name}: a cell has stored outputs"
        assert c.execution_count is None, f"{path.name}: a cell has an execution count"


@pytest.mark.parametrize("path", NOTEBOOKS, ids=lambda p: p.name)
def test_notebook_runs_from_a_clean_kernel(path, tmp_path, monkeypatch):
    nbformat = pytest.importorskip("nbformat", reason="nbformat is not installed")
    nbclient = pytest.importorskip("nbclient", reason="nbclient is not installed")
    pytest.importorskip("ipykernel", reason="ipykernel is not installed")

    # The kernel is a new process: it inherits this environment. It needs the
    # package (from the source tree when it is not installed) and no display.
    src = str(ROOT / "python" / "src")
    monkeypatch.setenv("MPLBACKEND", "Agg")
    monkeypatch.setenv("PYTHONPATH", os.pathsep.join(filter(None, [src, os.environ.get("PYTHONPATH")])))

    nb = nbformat.read(path, as_version=4)
    client = nbclient.NotebookClient(
        nb, timeout=300, kernel_name="python3",
        resources={"metadata": {"path": str(tmp_path)}})
    try:
        client.execute()        # raises CellExecutionError on the first failing cell
    except nbclient.exceptions.CellExecutionError as e:
        pytest.fail(f"{path.name}: {e}")
    # every code cell ran, none of them printed to stderr as an error
    for c in nb.cells:
        if c.cell_type == "code":
            assert c.execution_count is not None
            assert not [o for o in c.outputs if o.output_type == "error"]
