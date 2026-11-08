#!/usr/bin/env python3
"""The app's help, through xpplab-batch (no window).

    python tests/help_check.py build/app/xpplab-batch.exe engine/xppaut/ode/lecar.ode

Checks that
  1. every page of docs/manual opens from the Help menu (the Help entries of
     the palette) and with `help NAME`, and the Help menu opens no other page;
  2. `help` has an entry for every command of the command table, in the
     table's order, and `help COMMAND` prints that command's entry;
  3. `help` of an unknown topic says so and names every page.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE = re.compile(r"/manual/([A-Za-z0-9_-]+)\s*$")


def batch(exe, model, cmds, *extra):
    args = [str(exe), str(model)]
    for c in cmds:
        args += ["-c", c]
    args += list(extra)
    return subprocess.run(args, capture_output=True, text=True, errors="replace",
                          timeout=120).stdout


def output_of(text):
    """The lines of the output, without the echo of the commands typed."""
    return [l for l in text.splitlines() if not l.startswith(">>")]


def palette_rows(exe, model):
    rows = []
    for line in batch(exe, model, ["palette list"]).splitlines():
        if line.startswith("palette: ") and line.count(" | ") >= 3:
            kind, label, _, _ = line[len("palette: "):].split(" | ", 3)
            rows.append((kind, label))
    return rows


def opens(exe, model, command):
    """The manual page that a command opens, or None."""
    for line in output_of(batch(exe, model, [command], "--keep-going")):
        m = PAGE.search(line)
        if m:
            return m.group(1)
    return None


def is_entry_of(line, label):
    head = "  " + label
    return line.startswith(head) and (len(line) == len(head) or line[len(head)] == " ")


def main():
    exe, model = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    problems = []
    pages = sorted(p.stem for p in (ROOT / "docs" / "manual").glob("*.md"))
    if len(pages) < 15:
        problems.append("docs/manual has only %d pages" % len(pages))

    rows = palette_rows(exe, model)
    for name in pages:
        if opens(exe, model, "help " + name) != name:
            problems.append("help %s does not open its page" % name)

    menu = [l for k, l in rows if k == "action" and l.startswith("Help: ")]
    from_menu = {}
    for label in menu:
        page = opens(exe, model, "palette run " + label)
        if page is None:
            problems.append("Help menu item '%s' opens no page" % label)
        elif page in from_menu:
            problems.append("Help menu items '%s' and '%s' open the same page %s"
                            % (from_menu[page], label, page))
        else:
            from_menu[page] = label
    for name in pages:
        if name not in from_menu:
            problems.append("the page '%s' is not in the Help menu" % name)
    for page in from_menu:
        if page not in pages:
            problems.append("the Help menu opens '%s', which is not in docs/manual" % page)

    commands = [l for k, l in rows if k == "command"]
    if len(commands) < 60:
        problems.append("the command table has only %d commands" % len(commands))
    text = output_of(batch(exe, model, ["help"]))
    entries = [l for l in text if re.match(r"^  \S", l)]
    if len(entries) != len(commands):
        problems.append("help has %d entries for %d commands" % (len(entries), len(commands)))
    for label, line in zip(commands, entries):
        if not is_entry_of(line, label):
            problems.append("help has %r where the command '%s' should be" % (line.strip(), label))
            break
    # Every command by name: the entries of the commands starting with that word.
    for word in sorted({c.split(" ")[0] for c in commands}):
        if word in pages:
            continue  # `help auto` opens the page
        want = [c for c in commands if c == word or c.startswith(word + " ")]
        got = [l for l in output_of(batch(exe, model, ["help " + word]))
               if re.match(r"^  \S", l)]
        if len(got) != len(want) or not all(is_entry_of(l, c) for l, c in zip(got, want)):
            problems.append("help %s prints %r, not the entries of %r" % (word, got, want))

    text = batch(exe, model, ["help nosuchtopic"], "--keep-going")
    if "nosuchtopic" not in text or any(
            not re.search(r"\b%s\b" % re.escape(p), text.split("Pages:")[-1]) for p in pages):
        problems.append("help of an unknown topic does not name the pages: %r" % text[-300:])

    for p in problems:
        print("FAILED: " + p)
    print("help: %s" % ("%d problem(s)" % len(problems) if problems else "ok"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
