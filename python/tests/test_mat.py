"""MATLAB .mat export: the files are read back by a small reader written
here from the Level 5 format description (so the check needs nothing but
NumPy) and, when SciPy is installed, by scipy.io.loadmat."""
import os
import struct
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs
from xpplab.analysis import write_mat

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"

_SIZES = {1: 1, 2: 1, 3: 2, 4: 2, 5: 4, 6: 4, 7: 4, 9: 8}
_DTYPES = {1: "i1", 2: "u1", 3: "<i2", 4: "<u2", 5: "<i4", 6: "<u4", 9: "<f8"}


def _element(buf, pos):
    """(type, data bytes, next position) of the data element at pos."""
    word = struct.unpack_from("<I", buf, pos)[0]
    if word >> 16:  # small element: the size is in the upper half
        size, kind = word >> 16, word & 0xFFFF
        assert size <= 4
        return kind, buf[pos + 4:pos + 4 + size], pos + 8
    kind, size = word, struct.unpack_from("<I", buf, pos + 4)[0]
    data = buf[pos + 8:pos + 8 + size]
    return kind, data, pos + 8 + (size + 7) // 8 * 8


def _matrix(body):
    """(name, class, dims, parts) of a matrix: parts are the remaining
    elements (the data of a double matrix, the fields of a struct)."""
    pos = 0
    kind, flags, pos = _element(body, pos)
    assert kind == 6
    cls = struct.unpack("<I", flags[:4])[0] & 0xFF
    kind, dims, pos = _element(body, pos)
    assert kind == 5
    dims = tuple(np.frombuffer(dims, "<i4"))
    kind, name, pos = _element(body, pos)
    assert kind == 1
    parts = []
    while pos < len(body):
        kind, data, pos = _element(body, pos)
        parts.append((kind, data))
    return name.decode("ascii"), cls, dims, parts


def read_mat(path):
    buf = Path(path).read_bytes()
    assert buf[:19] == b"MATLAB 5.0 MAT-file"
    assert buf[124:128] == b"\x00\x01IM"  # version 0x0100, little-endian
    out, pos = {}, 128
    while pos < len(buf):
        kind, body, pos = _element(buf, pos)
        assert kind == 14  # miMATRIX: nothing is compressed
        name, cls, dims, parts = _matrix(body)
        if cls == 6:
            (kind, data), = parts
            assert kind == 9
            arr = np.frombuffer(data, "<f8")
            assert arr.size == int(np.prod(dims))
            out[name] = arr.copy()
        else:
            assert cls == 2 and dims == (1, 1)
            (_, flen), (_, names), *fields = parts
            width = struct.unpack("<i", flen)[0]
            keys = [names[i:i + width].split(b"\0")[0].decode("ascii")
                    for i in range(0, len(names), width)]
            assert len(keys) == len(fields)
            vals = []
            for kind, data in fields:
                assert kind == 14
                fname, fcls, fdims, fparts = _matrix(data)
                assert fname == "" and fcls == 6 and fdims == (1, 1)
                vals.append(np.frombuffer(fparts[0][1], "<f8")[0])
            out[name] = dict(zip(keys, vals))
    return out


def same_bits(a, b):
    a = np.ascontiguousarray(a, dtype=float).reshape(-1)
    b = np.ascontiguousarray(b, dtype=float).reshape(-1)
    return a.shape == b.shape and a.view(np.uint64).tolist() == b.view(np.uint64).tolist()


@pytest.fixture
def lecar():
    with xs.load(ODE / "lecar.ode") as m:
        yield m


def test_run(lecar, tmp_path):
    sol = lecar.run(total=100)
    sol.to_mat(tmp_path / "run.mat")
    got = read_mat(tmp_path / "run.mat")
    names = ["t"] + [c for c in sol.columns if c != "T"]
    assert list(got)[:-1] == names and list(got)[-1] == "params"
    for name, col in zip(names, sol.columns):
        assert len(got[name]) == len(sol)
        assert same_bits(got[name], sol[col])
    pars = {p: float(lecar.get(p)) for p in lecar.parameters}
    assert list(got["params"]) == list(pars)
    assert same_bits(list(got["params"].values()), list(pars.values()))


def test_run_params_are_those_of_the_run(lecar, tmp_path):
    sol = lecar.run(total=20, iapp=0.123)
    lecar.set(iapp=0.5)
    sol.to_mat(tmp_path / "run.mat")
    assert read_mat(tmp_path / "run.mat")["params"]["iapp"] == 0.123


def test_same_bytes_every_time(lecar, tmp_path):
    sol = lecar.run(total=50)
    sol.to_mat(tmp_path / "a.mat")
    sol.to_mat(tmp_path / "b.mat")
    lecar.run(total=50).to_mat(tmp_path / "c.mat")
    first = (tmp_path / "a.mat").read_bytes()
    assert first == (tmp_path / "b.mat").read_bytes()
    assert first == (tmp_path / "c.mat").read_bytes()
    # fixed header: the text, then blanks, no date
    assert first[:128].rstrip(b"\0") == (
        b"MATLAB 5.0 MAT-file, written by XPP Lab".ljust(116) + bytes(8) + b"\x00\x01IM")
    assert len(first) % 8 == 0


def test_names(tmp_path):
    long = "v" * 70
    names = ["a b", "1x", "", "x", "x", "a.b", "a_b", "a b ", "params", "_u", long,
             long + "2", "hé", "x_2"]
    cols = [np.full(2, float(i)) for i in range(len(names))]
    write_mat(tmp_path / "n.mat", names, cols, {"k 1": 1.5, "k.1": 2.5, "9": 3.5})
    got = read_mat(tmp_path / "n.mat")
    assert list(got) == [
        "a_b",             # "a b"
        "x1x",             # "1x" does not start with a letter
        "x",               # "" is just the prefix
        "x_2",             # a second "x"
        "x_3",             # a third ("x_2" is taken)
        "a_b_2",           # "a.b" is "a_b", taken
        "a_b_3",           # "a_b", taken twice
        "a_b_",            # "a b " has a trailing blank
        "params_2",        # "params" is the struct's name
        "x_u",             # "_u" does not start with a letter
        "v" * 63,          # cut to 63
        "v" * 61 + "_2",   # cut again, with room for the suffix
        "h_",              # one underscore for a two-byte character
        "x_2_2",           # "x_2" is taken
        "params",
    ]
    assert got["params"] == {"k_1": 1.5, "k_1_2": 2.5, "x9": 3.5}
    assert all(len(k) <= 63 for k in got)
    assert same_bits(got["x_3"], cols[4])


def test_names_without_params(tmp_path):
    # "params" is only reserved when a params struct is written
    write_mat(tmp_path / "n.mat", ["params"], [np.ones(1)])
    assert list(read_mat(tmp_path / "n.mat")) == ["params"]


def test_empty_columns_and_no_columns(tmp_path):
    write_mat(tmp_path / "e.mat", ["a", "b"], [np.zeros(0), np.zeros(0)])
    got = read_mat(tmp_path / "e.mat")
    assert list(got) == ["a", "b"] and all(len(v) == 0 for v in got.values())
    write_mat(tmp_path / "n.mat", [], [])
    assert read_mat(tmp_path / "n.mat") == {}
    with pytest.raises(ValueError):
        write_mat(tmp_path / "bad.mat", ["a", "b"], [np.zeros(2), np.zeros(3)])


def test_special_values_are_exact(tmp_path):
    vals = np.array([0.0, -0.0, np.inf, -np.inf, np.nan, 5e-324, 1.7976931348623157e308,
                     0.1, -1 / 3])
    write_mat(tmp_path / "s.mat", ["v"], [vals])
    assert same_bits(read_mat(tmp_path / "s.mat")["v"], vals)


def test_table_with_and_without_params(tmp_path):
    t = xs.Table({"I": np.linspace(0, 1, 5), "V max": np.arange(5) ** 0.5})
    t.to_mat(tmp_path / "t.mat")
    got = read_mat(tmp_path / "t.mat")
    assert list(got) == ["I", "V_max"]
    assert same_bits(got["V_max"], np.arange(5) ** 0.5)
    t.params = {"gk": 8.0}
    t.to_mat(tmp_path / "t2.mat")
    assert read_mat(tmp_path / "t2.mat")["params"] == {"gk": 8.0}


def test_sweep_table(lecar, tmp_path):
    values = np.linspace(0.05, 0.4, 8)
    amp = lecar.sweep("iapp", values, measure=lambda s: s["V"].max())
    t = xs.Table({"iapp": values, "V_max": amp}, params=dict(lecar.parameters))
    t.to_mat(tmp_path / "sweep.mat")
    got = read_mat(tmp_path / "sweep.mat")
    assert same_bits(got["iapp"], values) and same_bits(got["V_max"], amp)
    assert list(got["params"]) == list(lecar.parameters)


def test_equilibria(lecar, tmp_path):
    lecar.apply_set("hopf")
    eqs = lecar.find_equilibria()
    assert len(eqs) >= 1
    eqs.to_mat(tmp_path / "eq.mat")
    got = read_mat(tmp_path / "eq.mat")
    cols, rows = eqs._table()
    assert list(got) == cols
    rows = np.array(rows, dtype=float)
    for j, name in enumerate(cols):
        assert same_bits(got[name], rows[:, j])


def test_diagram(lecar, tmp_path):
    lecar.apply_set("hopf")
    d = lecar.auto.equilibria("iapp")
    d.to_mat(tmp_path / "d.mat")
    got = read_mat(tmp_path / "d.mat")
    assert len(got["value"]) == len(d)
    assert "type" not in got and "parameter" not in got
    assert same_bits(got["value"], [p.value for p in d])
    assert same_bits(got["label"], [p.label for p in d])
    assert same_bits(got["stable"], [p.stable for p in d])
    assert same_bits(got["value2"], [np.nan] * len(d))
    for v in d.variables:
        assert same_bits(got[v + "_max"], [p.umax[v] for p in d])
        assert same_bits(got[v + "_min"], [p.umin[v] for p in d])


def test_run_list(lecar, tmp_path):
    runs = lecar.run_many([{"v": -20.0}, {"v": 20.0}], total=20)
    runs.to_mat(tmp_path / "r.mat")
    got = read_mat(tmp_path / "r.mat")
    assert list(got)[:3] == ["run", "event", "t"]
    n = [len(s) for s in runs]
    assert same_bits(got["run"], np.repeat([0.0, 1.0], n))
    assert same_bits(got["V"], np.concatenate([s["V"] for s in runs]))
    assert "params" not in got


def test_root_locus(lecar, tmp_path):
    rl = lecar.root_locus("iapp", (0.1, 0.3), steps=11)
    rl.to_mat(tmp_path / "rl.mat")
    got = read_mat(tmp_path / "rl.mat")
    assert same_bits(got["iapp"], rl.values)
    assert same_bits(got["re1"], rl.poles[:, 0].real)
    assert same_bits(got["im1"], rl.poles[:, 0].imag)


def test_scipy_reads_the_files(lecar, tmp_path):
    # SciPy's reader is the check that does not share this file's reading of
    # the format. It must run wherever XPPLAB_REQUIRE_SCIPY is set (CI should
    # set it, after installing the "test" extra); elsewhere it skips without
    # SciPy.
    if os.environ.get("XPPLAB_REQUIRE_SCIPY"):
        import scipy.io as io
    else:
        io = pytest.importorskip("scipy.io")
    sol = lecar.run(total=100)
    sol.to_mat(tmp_path / "run.mat")
    got = io.loadmat(tmp_path / "run.mat", squeeze_me=False, struct_as_record=False)
    mine = read_mat(tmp_path / "run.mat")
    assert got["t"].shape == (len(sol), 1)
    for name, v in mine.items():
        if name == "params":
            for k, x in v.items():
                assert getattr(got["params"][0, 0], k)[0, 0] == x
        else:
            assert same_bits(got[name], v)
    write_mat(tmp_path / "e.mat", ["a", "b"], [np.zeros(0), np.zeros(0)])
    assert io.loadmat(tmp_path / "e.mat")["a"].shape == (0, 1)
