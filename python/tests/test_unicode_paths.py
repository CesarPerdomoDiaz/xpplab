"""Files in a folder whose name is not ASCII (C:\\Users\\José): the model is
loaded from there, and the results and the AUTO diagram are written there
and read back."""
import shutil
from pathlib import Path

import numpy as np

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"


def _folder(tmp_path):
    folder = tmp_path / "Jos\u00e9 \u00f1and\u00fa"
    folder.mkdir()
    return folder


def test_load_run_csv_and_auto_files_in_an_accented_folder(tmp_path):
    folder = _folder(tmp_path)
    ode = folder / "l\u00e9car.ode"
    shutil.copy(ODE / "lecar.ode", ode)

    with xs.load(ODE / "lecar.ode") as ref:
        expected = ref.run(total=50)["V"]

    with xs.load(ode) as m:
        sol = m.run(total=50)
        assert np.array_equal(sol["V"], expected)
        sol.to_csv(folder / "run.csv")
        header = (folder / "run.csv").read_text().splitlines()[0].split(",")
        table = np.loadtxt(folder / "run.csv", delimiter=",", skiprows=1)
        assert np.array_equal(table[:, header.index("V")], expected)

        m.apply_set("hopf")
        a = m.auto
        a.set(parmax=0.6, nmax=120)
        a.equilibria("iapp")
        before = a.diagram
        assert len(before) > 0
        a.save(folder / "d\u00e9.auto")
        assert (folder / "d\u00e9.auto").stat().st_size > 0
        a.reset()
        assert len(a.diagram) == 0
        after = a.load(folder / "d\u00e9.auto")
        assert [(p.value, p.norm) for p in after] == \
               [(p.value, p.norm) for p in before]
        a.diagram.to_csv(folder / "diagram.csv")

    assert {p.name for p in folder.iterdir()} == {
        "d\u00e9.auto", "diagram.csv", "l\u00e9car.ode", "run.csv"}
