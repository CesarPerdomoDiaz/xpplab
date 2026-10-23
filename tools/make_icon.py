"""Draw the XPP Studio icon: a limit cycle around an unstable equilibrium,
with the two nullclines, on a rounded square.

    python tools/make_icon.py

Writes app/resources/icon.png (512 px), icon.ico (Windows), icon.icns
(macOS) and icon_rgba.bin (raw 64x64 RGBA for the window icon).
"""
import io
import struct
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.patches import FancyBboxPatch  # noqa: E402

OUT = Path(__file__).resolve().parent.parent / "app" / "resources"


def draw(size):
    fig = plt.figure(figsize=(1, 1), dpi=size)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    fig.patch.set_alpha(0)
    # The figure is one inch wide, so sizes in points scale with the icon.
    box = FancyBboxPatch((0.04, 0.04), 0.92, 0.92,
                         boxstyle="round,pad=0,rounding_size=0.2",
                         fc="#1d2b53", ec="none")
    ax.add_patch(box)
    # A FitzHugh-Nagumo-like picture: cubic and linear nullclines, a limit
    # cycle, and the unstable equilibrium where the nullclines cross.
    v = np.linspace(-2.4, 2.4, 300)
    vx = 0.5 + v / 5.6
    for y, color in (((v - v ** 3 / 3) / 5.2, "#ff6b6b"), (v / 6.5, "#51cf66")):
        (line,) = ax.plot(vx, 0.5 + y, color=color, lw=2.2)
        line.set_clip_path(box)
    t = np.linspace(0, 2 * np.pi, 400)
    r = 0.27 + 0.035 * np.cos(3 * t + 0.6)
    ax.plot(0.5 + r * np.cos(t) * 1.1, 0.5 + r * np.sin(t) * 0.78,
            color="#ffd43b", lw=4.2, solid_capstyle="round")
    ax.plot([0.5], [0.5], "o", ms=6.5, mfc="white", mec="#1d2b53", mew=1.2)
    buf = io.BytesIO()
    fig.savefig(buf, format="png", transparent=True)
    plt.close(fig)
    return buf.getvalue()


def rgba(png_bytes):
    return (plt.imread(io.BytesIO(png_bytes)) * 255).astype(np.uint8)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    pngs = {s: draw(s) for s in (16, 32, 48, 64, 128, 256, 512)}
    (OUT / "icon.png").write_bytes(pngs[512])
    # Windows .ico with PNG-compressed entries.
    sizes = [16, 32, 48, 64, 128, 256]
    header = struct.pack("<HHH", 0, 1, len(sizes))
    entries, data, offset = b"", b"", 6 + 16 * len(sizes)
    for s in sizes:
        p = pngs[s]
        entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32,
                               len(p), offset + len(data))
        data += p
    (OUT / "icon.ico").write_bytes(header + entries + data)
    # macOS .icns with PNG entries.
    chunks = b""
    for tag, s in ((b"icp4", 16), (b"icp5", 32), (b"icp6", 64), (b"ic07", 128),
                   (b"ic08", 256), (b"ic09", 512)):
        chunks += tag + struct.pack(">I", 8 + len(pngs[s])) + pngs[s]
    (OUT / "icon.icns").write_bytes(b"icns" + struct.pack(">I", 8 + len(chunks)) + chunks)
    # Raw RGBA for SDL_SetWindowIcon.
    (OUT / "icon_rgba.bin").write_bytes(rgba(pngs[64]).tobytes())
    print("icons written to", OUT)


if __name__ == "__main__":
    main()
