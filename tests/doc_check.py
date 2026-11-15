"""The doc command and the reference pages, through xpplab-batch (no window).

    python tests/doc_check.py build/app/xpplab-batch.exe engine/xppaut/ode/lecar.ode

- `doc`, `doc NAME`, another name of a command, a chapter of the manual, a
  misspelt name and a name that is nothing.
- Every command of the command table has a reference page
  (docs/reference/NAME-WITH-DASHES.md) or is in tests/doc_pending.txt, the
  list of pages still to write. The list may only shrink: a page that exists
  must leave it, and a new command needs a page or a line there.
- Every reference page has the same sections, in
  their order (docs/development/reference-pages.md says what goes in each).
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "docs" / "reference"
SITE = "https://cesarperdomodiaz.github.io/xpplab/"
SECTIONS = ["Syntax", "Description", "Examples", "Settings", "Results",
            "More about", "Tips", "Algorithms", "When it fails", "References",
            "See also", "Version history"]


def batch(exe, model, *cmds):
    args = [str(exe), str(model)]
    for c in cmds:
        args += ["-c", c]
    out = subprocess.run(args, capture_output=True, text=True, errors="replace",
                         timeout=120).stdout
    return out.split(">> " + cmds[0], 1)[1]


def commands():
    text = (ROOT / "app" / "src" / "commands.c").read_text(encoding="utf-8")
    table = text[text.index("const CommandInfo app_command_table[]"):]
    table = table[:table.index("\n};")]
    return re.findall(r'^\s*\{"([^"]+)",', table, re.M)


def main(exe, model):
    bad = []

    def check(ok, what):
        if not ok:
            bad.append(what)

    out = batch(exe, model, "doc")
    check(SITE + "reference/reference" in out, "doc alone opens the index: " + out)
    for name in ("auto steady", "AUTO  Steady"):
        out = batch(exe, model, "doc " + name)
        check(SITE + "reference/auto-steady" in out, f"doc {name}: {out}")
    out = batch(exe, model, "doc auto")
    check(SITE + "manual/auto" in out and "reference/" not in out,
          "doc auto opens the chapter: " + out)
    out = batch(exe, model, "doc sweep")
    check("sweep is another name for range" in out or "range has no reference page yet" in out,
          "doc sweep names range: " + out)
    out = batch(exe, model, "doc auto stedy")
    check("error: no reference page 'auto stedy'" in out and "auto steady" in out,
          "a misspelt name gets the closest: " + out)
    out = batch(exe, model, "doc qqqqzzzz")
    check("error: no reference page 'qqqqzzzz'" in out and "Closest" not in out,
          "a name that is nothing: " + out)
    # help stays the short text: it names the page, and does not open it
    out = batch(exe, model, "help auto steady")
    check("doc auto steady opens its reference page" in out and SITE not in out,
          "help auto steady points to doc: " + out)
    out = batch(exe, model, "help doc")
    check("reference page" in out, "help doc: " + out)

    # every command has a page or is pending
    names = commands()
    check("doc" in names and "auto steady" in names, "the command table was not read")
    have = {n for n in names if (REFERENCE / (n.replace(" ", "-") + ".md")).exists()}
    pending_file = ROOT / "tests" / "doc_pending.txt"
    pending = [l.strip() for l in pending_file.read_text(encoding="utf-8").splitlines()
               if l.strip() and not l.startswith("#")]
    for n in names:
        if n not in have and n not in pending:
            bad.append(f"command '{n}' has no reference page and is not in doc_pending.txt")
    for n in pending:
        if n in have:
            bad.append(f"'{n}' has a reference page now: remove it from doc_pending.txt")
        if n not in names:
            bad.append(f"doc_pending.txt lists '{n}', which is not a command")

    # every page has the sections, in order
    for page in sorted(REFERENCE.glob("*.md")):
        if page.stem in ("reference", "template"):
            continue
        text = page.read_text(encoding="utf-8")
        heads = re.findall(r"^## (.+)$", text, re.M)
        # the app's Help window shows these pages and cannot typeset LaTeX yet
        prose = re.sub(r"```.*?```", "", text, flags=re.S)
        if "$" in prose.replace(r"\$", ""):
            bad.append(f"{page.name}: LaTeX ($...$) in a page the app's Help window shows")
        if heads != SECTIONS:
            bad.append(f"{page.name}: sections {heads}, expected {SECTIONS}")
        check((page.stem.replace("-", " ")) in names,
              f"{page.name} is not the page of a command")
        index = (REFERENCE / "reference.md").read_text(encoding="utf-8")
        check(f"]({page.name})" in index, f"{page.name} is not in the reference index")

    for b in bad:
        print("FAIL:", b)
    print("doc: %s (%d commands, %d with a page)" % ("ok" if not bad else "FAILED",
                                                    len(names), len(have)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
