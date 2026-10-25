"""Record the README's animations and screenshots from the real app.

Each animation is one session of the app, recorded with --record: a script
of timed commands changes parameters, starts trajectories, runs AUTO or
plays an animation, and every frame is saved. Pillow joins the frames into
GIFs in docs/images.

    python tools/make_demo.py [--exe build/app/xpplab.exe] [--only NAME ...]
"""
import argparse
import math
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
IMAGES = ROOT / "docs" / "images"

DUFFING = """# The unforced Duffing oscillator with damping: two wells and a saddle.
x' = y
y' = x - x^3 - d*y
par d=0.3
init x=-1.9, y=1.2
@ total=20, dt=0.02, xp=x, yp=y
@ xlo=-2, xhi=2, ylo=-1.6, yhi=1.6
done
"""


def ease(k, n):
    """0 to 1 over n steps, slow at both ends."""
    return 0.5 - 0.5 * math.cos(math.pi * k / max(n - 1, 1))


def ramp(first, name, a, b, n, fmt="{:.4f}"):
    """Script lines setting name from a to b over n frames, quietly."""
    return [(first + k, f"@set {name}=" + fmt.format(a + (b - a) * ease(k, n)))
            for k in range(n)]


def hero():
    """Morris-Lecar: the injected current rises and falls; the nullclines
    move, the rest state gives way to repetitive firing, and back."""
    lines = [(0, "@set iapp=0.05")]
    lines += ramp(15, "iapp", 0.05, 0.14, 60)
    lines += ramp(98, "iapp", 0.14, 0.05, 45)
    return dict(model="lecar.ode", size="1100x620", fps=15, frames=158, colors=128,
                scale=None,
                cmds=["hide help", "hide console", "layout side", "nullclines",
                      "show phase"], script=lines)


def phase_plane():
    """Duffing: trajectories started one after another fill in the phase
    portrait; then the saddle with its manifolds, and the flow."""
    starts = [(-1.9, 1.2), (1.9, -1.2), (-0.9, 1.55), (0.9, -1.55),
              (-1.95, -0.3), (1.95, 0.3)]
    lines, f = [], 0
    for x, y in starts:
        lines.append((f, f"@set x={x} y={y} total=0.1"))
        lines += ramp(f + 1, "total", 0.1, 20, 12, "{:.2f}")
        lines.append((f + 14, "@freeze"))
        f += 16
    lines += [(f + 2, "@set x=0.02 y=0 total=20"), (f + 3, "@eq"),
              (f + 12, "@dirfield on")]
    return dict(model=None, text=DUFFING, size="900x560", fps=15, frames=f + 40,
                colors=128,
                cmds=["hide help", "hide models", "hide workspace", "hide console",
                      "nullclines", "dirfield off", "show phase"], script=lines)


def bifurcation():
    """AUTO in the Morris-Lecar model: the steady states and their Hopf
    points, then the branch of periodic orbits."""
    lines = [(0, "@clc"), (12, "auto steady iapp"), (40, "auto from 2 periodic")]
    return dict(model="lecar.ode", size="960x600", frames=90,
                cmds=["theme dark", "hide help", "hide models", "hide workspace",
                      "use hopf", "auto set parmax=0.6 nmax=220", "show auto"],
                script=lines)


def view3d():
    """The Lorenz attractor turning in the 3D view: one turn, looped."""
    return dict(model="lorenz.ode", size="880x560", fps=15, frames=100,
                cmds=["hide help", "hide models", "hide workspace", "hide console",
                      "view3d", "view3d spin 54"], script=[])


def animator():
    """The double pendulum in the Animator."""
    return dict(model="doubpend.ode", size="880x560", frames=140,
                cmds=["hide help", "hide models", "hide workspace", "hide console",
                      "ani doubpend.ani", "ani play"], script=[])


DEMOS = {"demo": hero, "phase-plane": phase_plane, "bifurcation": bifurcation,
         "view3d": view3d, "animator": animator}


def record(exe, demo, tmp, name):
    out = tmp / name
    out.mkdir()
    if demo.get("text"):
        model = tmp / f"{name}.ode"
        model.write_text(demo["text"])
    else:
        model = exe.parent / "examples" / demo["model"]
    script = tmp / f"{name}.txt"
    script.write_text("".join(f"{f} {c}\n" for f, c in demo["script"]))
    fps = demo.get("fps", 20)
    args = [str(exe), str(model), "--size", demo["size"], "--record", str(out),
            "--fps", str(fps), "--frames", str(demo["frames"]), "--script", str(script)]
    for c in demo["cmds"]:
        args += ["--cmd", c]
    subprocess.run(args, cwd=exe.parent, check=True, timeout=600,
                   stdin=subprocess.DEVNULL)
    frames = [Image.open(p).convert("RGB") for p in sorted(out.glob("frame_*.png"))]
    if demo.get("scale"):
        w, h = frames[0].size
        size = (round(w * demo["scale"]), round(h * demo["scale"]))
        frames = [f.resize(size, Image.LANCZOS) for f in frames]
    if len(frames) != demo["frames"]:
        raise SystemExit(f"{name}: {len(frames)} frames of {demo['frames']}")
    return frames, fps


def save_gif(frames, path, fps, colors=255):
    """One palette for the whole animation, taken from a sample of its
    frames, so colours do not flicker; Pillow stores only what changes
    from one frame to the next."""
    sample = frames[:: max(len(frames) // 10, 1)]
    w, h = sample[0].size
    sheet = Image.new("RGB", (w, h * len(sample)))
    for k, f in enumerate(sample):
        sheet.paste(f, (0, h * k))
    # Octree keeps small saturated areas, like the plot lines, true.
    palette = sheet.quantize(colors=colors, method=Image.Quantize.FASTOCTREE,
                             dither=Image.Dither.NONE)
    gif = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in frames]
    gif[0].save(path, save_all=True, append_images=gif[1:],
                duration=round(1000 / fps), loop=0, optimize=False, disposal=1)


def shot(exe, out, args, size="1400x860", frames=12):
    subprocess.run([str(exe), *args, "--screenshot", str(out), "--frames", str(frames),
                    "--size", size], cwd=exe.parent, check=True, timeout=120,
                   stdin=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(ROOT / "build" / "app" / "xpplab.exe"))
    ap.add_argument("--only", nargs="*", help=f"some of: {', '.join(DEMOS)}, stills")
    args = ap.parse_args()
    exe = Path(args.exe).resolve()
    only = set(args.only or [*DEMOS, "stills"])
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for name, make in DEMOS.items():
            if name not in only:
                continue
            demo = make()
            frames, fps = record(exe, demo, tmp, name)
            path = IMAGES / f"{name}.gif"
            save_gif(frames, path, fps, demo.get("colors", 255))
            print(f"{path.name}: {len(frames)} frames, {path.stat().st_size / 1e6:.1f} MB")
        if "stills" in only:
            lecar = exe.parent / "examples" / "lecar.ode"
            shot(exe, IMAGES / "editor.png", [str(lecar), "--cmd", "show editor"])
            shot(exe, IMAGES / "time-series.png",
                 [str(lecar), "--cmd", "set iapp=0.1 total=200", "--cmd", "show time"])
            shot(exe, IMAGES / "help.png",
                 [str(lecar), "--cmd", "help tutorial", "--cmd", "show help"])
            print("editor.png, time-series.png, help.png")


if __name__ == "__main__":
    main()
