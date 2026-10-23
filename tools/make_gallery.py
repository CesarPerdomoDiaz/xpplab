"""Build the examples gallery for the documentation website.

Every XPPAUT example model that runs in batch mode is integrated with the
xppstudio package and drawn as a small plot. The page lists each model with
the description from its leading comments.

    python tools/make_gallery.py [--limit N]

Writes docs/gallery/index.md and docs/gallery/img/*.png (not committed).
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

import matplotlib  # noqa: E402

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

try:  # an installed package (with its engine) wins over the source tree
    import xppstudio as xs
except ImportError:
    sys.path.insert(0, str(ROOT / "python" / "src"))
    import xppstudio as xs

ODE = ROOT / "engine" / "xppaut" / "ode"
OUT = ROOT / "docs" / "gallery"
REPO = "https://github.com/xppstudio/xppstudio/blob/main/engine/xppaut/ode/"


def describe(path: Path) -> str:
    lines = []
    for line in path.read_text(errors="replace").splitlines():
        s = line.strip()
        if not s.startswith("#"):
            if lines:
                break
            continue
        s = s.lstrip("#").strip()
        if s:
            lines.append(s)
        if len(lines) >= 4:
            break
    return " ".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()
    index = json.loads((ROOT / "tests" / "golden" / "index.json").read_text())
    (OUT / "img").mkdir(parents=True, exist_ok=True)
    entries = []
    for path in sorted(ODE.glob("*.ode"), key=lambda p: p.name.lower()):
        info = index.get(f"ode/{path.stem}", {})
        if info.get("status") != "ok" or info.get("seconds", 99) > 3:
            continue
        try:
            with xs.load(path) as m:
                sol = m.run()
                cols = [c for c in m.variables[:3]]
                fig, ax = plt.subplots(figsize=(3.2, 1.9), dpi=80)
                for c in cols:
                    ax.plot(sol.t, sol[c], lw=1.1, label=c)
                ax.set_xlabel("t", fontsize=7)
                ax.tick_params(labelsize=6)
                if len(cols) > 1:
                    ax.legend(fontsize=6, loc="upper right", frameon=False)
                fig.tight_layout(pad=0.3)
                fig.savefig(OUT / "img" / f"{path.stem}.png")
                plt.close(fig)
                entries.append((path, m.variables, m._parameter_names))
        except Exception as e:  # a model that does not run is skipped
            print(f"skip {path.name}: {str(e).splitlines()[0]}")
        if args.limit and len(entries) >= args.limit:
            break
    lines = ["# Examples gallery", "",
             f"The {len(entries)} example models that ship with XPPAUT, run by "
             "XPP Studio. Open any of them from the app's **Models** window, "
             "or in Python with `xs.load(\"name.ode\")`.", ""]
    for path, variables, params in entries:
        about = describe(path) or "No description in the file."
        lines += [f"## {path.stem}", "",
                  f"![{path.stem}](img/{path.stem}.png)", "",
                  about, "",
                  f"Variables: {', '.join(variables[:10])}"
                  f"{' ...' if len(variables) > 10 else ''}. "
                  f"Parameters: {', '.join(params[:10]) or 'none'}"
                  f"{' ...' if len(params) > 10 else ''}. "
                  f"[Model file]({REPO}{path.name})", ""]
    (OUT / "index.md").write_text("\n".join(lines))
    print(f"gallery: {len(entries)} models")


if __name__ == "__main__":
    main()
