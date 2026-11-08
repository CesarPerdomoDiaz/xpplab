"""Text, arrows and markers on figures and plots (the app's annotate
commands and the calls its echo writes)."""
import io

import numpy as np
import pytest

import xpplab as xs

plt = pytest.importorskip("matplotlib.pyplot")
from matplotlib.text import Annotation  # noqa: E402
RED = "#d62728"


def figure():
    fig = xs.Figure(86, 65)
    fig[0].line([0, 10], [0, 10])
    fig[0].set(xlim=(0, 10), ylim=(0, 10), legend="none")
    return fig


def png_pixels(fig):
    """The figure as an RGB array, and a function from data to pixel."""
    mpl = fig.draw()
    buf = io.BytesIO()
    mpl.savefig(buf, format="png", dpi=200)
    img = plt.imread(io.BytesIO(buf.getvalue()))
    ax = mpl.axes[0]
    scale = 200 / mpl.dpi   # the transform is in the figure's own dpi
    height = img.shape[0]

    def pixel(x, y):
        px, py = ax.transData.transform((x, y))
        return int(round(px * scale)), int(round(height - py * scale))

    plt.close(mpl)
    return img[..., :3], pixel


def red_mask(img):
    return (img[..., 0] > 0.7) & (img[..., 1] < 0.35) & (img[..., 2] < 0.35)


def test_annotations_draw_in_data_coordinates():
    plain, _ = png_pixels(figure())
    fig = figure()
    fig.text(1, 8, "peak", size=14, color=RED)
    fig.arrow(2, 2, 7, 6, color=RED)
    fig.marker(5, 9, "s", color=RED)
    img, pixel = png_pixels(fig)
    assert red_mask(plain).sum() == 0
    assert red_mask(img).sum() > 150
    # the shaft passes the middle of the arrow, the marker sits at its point
    mx, my = pixel(4.5, 4)
    assert red_mask(img)[my - 2:my + 3, mx - 2:mx + 3].any()
    qx, qy = pixel(5, 9)
    assert red_mask(img)[qy - 2:qy + 3, qx - 2:qx + 3].all()


def test_arrow_and_text_are_where_they_were_put():
    fig = figure()
    fig[0].text(1.5, 8.5, "label", size=11)
    fig[0].arrow(2, 3, 7, 6)
    mpl = fig.draw()
    ax = mpl.axes[0]
    text = ax.texts[0]
    assert text.get_text() == "label" and text.get_position() == (1.5, 8.5)
    assert text.get_fontsize() == 11 and text.get_verticalalignment() == "baseline"
    (arrow,) = [t for t in ax.texts if isinstance(t, Annotation)]
    assert arrow.xy == (7, 6) and arrow.xyann == (2, 3)
    plt.close(mpl)


def test_annotations_go_into_every_format(tmp_path):
    sizes = {}
    for ext in ("pdf", "svg", "eps", "png"):
        figure().save(tmp_path / f"plain.{ext}")
        fig = figure()
        fig.text(1, 8, "label", color=RED)
        fig.arrow(2, 2, 7, 6, color=RED)
        fig.marker(5, 9, "^", color=RED)
        fig.save(tmp_path / f"marked.{ext}")
        sizes[ext] = ((tmp_path / f"plain.{ext}").stat().st_size,
                      (tmp_path / f"marked.{ext}").stat().st_size)
    for ext, (plain, marked) in sizes.items():
        assert marked > plain, ext
    # the SVG draws the text as outlines: the arrow's colour is in the file
    assert RED in (tmp_path / "marked.svg").read_text(encoding="utf-8")
    assert RED not in (tmp_path / "plain.svg").read_text(encoding="utf-8")


def test_clear_deletes_one_or_all():
    p = figure()[0]
    p.text(0, 0, "a").arrow(0, 0, 1, 1).marker(1, 1, "o")
    assert [n["kind"] for n in p.notes] == ["text", "arrow", "marker"]
    p.clear_annotations(1)
    assert [n["kind"] for n in p.notes] == ["text", "marker"]
    p.clear_annotations()
    assert p.notes == []
    with pytest.raises(IndexError):
        p.clear_annotations(0)


def test_bad_arguments_are_refused():
    p = figure()[0]
    with pytest.raises(ValueError):
        p.marker(0, 0, "nope")
    with pytest.raises(ValueError):
        p.text(0, 0, "x", size=0)
    with pytest.raises(TypeError):
        p.text(0, 0, "x", color=3)
    assert p.notes == []


def test_a_plot_windows_notes_draw_on_the_script_axes():
    notes = xs.Annotations()
    notes["phase"].text(0.2, 0.3, "saddle", size=14.0, color=RED)
    notes["phase"].arrow(0.0, 0.0, 0.5, 0.5)
    notes["phase"].marker(0.1, 0.1, "*")
    notes["time"].text(1.0, 1.0, "other")
    fig, ax = plt.subplots()
    ax.plot(np.linspace(0, 1, 5), np.linspace(0, 1, 5))
    notes["phase"].draw(ax)
    assert [t.get_text() for t in ax.texts if not isinstance(t, Annotation)] == ["saddle"]
    assert sum(isinstance(t, Annotation) for t in ax.texts) == 1
    assert len(ax.lines) == 2
    plt.close(fig)
    notes["phase"].clear_annotations(0)
    assert len(notes["phase"].notes) == 2 and len(notes["time"].notes) == 1
    with pytest.raises(KeyError):
        notes["phase plane"]
