import pytest

import xpplab as xs


def test_example_returns_the_path_of_a_shipped_model():
    p = xs.example("lecar.ode")
    assert p.is_file() and p.name == "lecar.ode"
    assert xs.example("lecar") == p          # the ending may be left out
    assert "iapp" in xs.load(p).parameters       # and the path loads


def test_example_prefers_the_environment_folder(tmp_path, monkeypatch):
    (tmp_path / "mine.ode").write_text("x'=-x\ninit x=1\ndone\n")
    monkeypatch.setenv("XPPLAB_EXAMPLES", str(tmp_path))
    assert xs.example("mine.ode") == tmp_path / "mine.ode"
    # a model that is not in that folder is still found in the others
    assert xs.example("lecar.ode").name == "lecar.ode"


def test_example_names_the_close_ones_when_there_is_none():
    with pytest.raises(FileNotFoundError) as e:
        xs.example("lecarr.ode")
    msg = str(e.value)
    assert "lecarr.ode" in msg and "close names:" in msg
    assert "lecar.ode" in msg.split("close names:")[1]
    with pytest.raises(FileNotFoundError) as e:
        xs.example("zzzzzzzz-no-such-model")
    assert "close names" not in str(e.value)
