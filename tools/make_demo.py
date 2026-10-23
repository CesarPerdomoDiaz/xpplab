"""Record the README's demo animation and screenshots from the real app.

The app is scripted with --cmd/--then and --screenshot; frames are joined
into docs/images/demo.gif with Pillow.

    python tools/make_demo.py [--exe build/app/xppstudio.exe]
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from bmp2png import read_bmp  # noqa: E402

IMAGES = ROOT / "docs" / "images"

FHN = """# FitzHugh-Nagumo neuron: rest or repetitive firing, depending on I.
v' = v - v^3/3 - w + I
w' = eps*(v + a - b*w)
par I=0, a=0.7, b=0.8, eps=0.08
init v=-1.2, w=-0.6
@ total=150, dt=0.05, xp=v, yp=w
@ xlo=-2.4, xhi=2.4, ylo=-0.9, yhi=2.2
done
"""


def shot(exe, out, args, size="1400x860", frames=10):
    cmd = [str(exe), *args, "--screenshot", str(out), "--frames", str(frames),
           "--size", size]
    subprocess.run(cmd, cwd=exe.parent, check=True, timeout=120,
                   stdin=subprocess.DEVNULL)
    return Image.fromarray(read_bmp(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(ROOT / "build" / "app" / "xppstudio.exe"))
    args = ap.parse_args()
    exe = Path(args.exe).resolve()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        model = tmp / "fhn.ode"
        model.write_text(FHN)

        # The demo: the current I rises; the rest state loses stability at a
        # Hopf bifurcation and the neuron starts to fire.
        frames = []
        values = list(np.linspace(0.0, 0.6, 16))
        for k, I in enumerate(values):
            img = shot(exe, tmp / f"f{k}.bmp",
                       [str(model), "--cmd", f"set I={I:.3f}", "--cmd", "nullclines",
                        "--cmd", "eq", "--cmd", "show phase"])
            frames.append(img.resize((980, 602), Image.LANCZOS))
            print(f"frame {k + 1}/{len(values)}")
        frames = frames + frames[-2:0:-1]  # and back down again
        # One palette per frame keeps the plot colours true.
        # (Octree keeps small saturated areas, like the plot lines, true.)
        gif = [f.quantize(colors=255, method=Image.Quantize.FASTOCTREE,
                          dither=Image.Dither.NONE) for f in frames]
        durations = [260] * len(gif)
        durations[len(values) - 1] = 1400
        durations[0] = 1400
        gif[0].save(IMAGES / "demo.gif", save_all=True, append_images=gif[1:],
                    duration=durations, loop=0, optimize=True)

        # Still screenshots for the README.
        lecar = exe.parent / "examples" / "lecar.ode"
        shot(exe, tmp / "ed.bmp", [str(lecar), "--cmd", "show editor"]).save(
            IMAGES / "editor.png")
        shot(exe, tmp / "ts.bmp",
             [str(lecar), "--cmd", "set iapp=0.1 total=200", "--cmd", "show time"]
             ).save(IMAGES / "time-series.png")
        shot(exe, tmp / "dark.bmp",
             [str(lecar), "--cmd", "theme dark", "--cmd", "use hopf",
              "--cmd", "auto set parmax=0.6 nmax=220", "--cmd", "auto steady iapp",
              "--cmd", "auto from 2 periodic", "--cmd", "show auto"]
             ).save(IMAGES / "bifurcation-dark.png")
        shot(exe, tmp / "help.bmp",
             [str(lecar), "--cmd", "help tutorial", "--cmd", "show help"]
             ).save(IMAGES / "help.png")
    print("demo and screenshots written to", IMAGES)


if __name__ == "__main__":
    main()
