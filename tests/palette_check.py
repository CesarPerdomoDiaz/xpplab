#!/usr/bin/env python3
"""The command palette, through xpplab-batch (no window).

    python tests/palette_check.py build/app/xpplab-batch.exe engine/xppaut/ode/lecar.ode

Checks that
  1. the app's own consistency check of the action table passes;
  2. every command that `help` lists is in the palette, and every action of
     the table is (the menus are drawn from the same table);
  3. running an entry of the palette does what the menu item or the command
     does: the same exported data, the same saved Python script, the same
     command-window output;
  4. an entry that is not available is refused, with the reason.
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def batch(exe, model, cmds, *extra):
    args = [str(exe), str(model)]
    for c in cmds:
        args += ["-c", c]
    args += list(extra)
    return subprocess.run(args, capture_output=True, text=True, errors="replace",
                          timeout=120)


def listed(exe, model):
    """The palette's entries as (kind, label, shortcut, description)."""
    r = batch(exe, model, ["palette list"])
    rows = []
    for line in r.stdout.splitlines():
        if line.startswith("palette: ") and line.count(" | ") >= 3:
            kind, label, shortcut, desc = line[len("palette: "):].split(" | ", 3)
            rows.append((kind, label, shortcut, desc))
    return rows


def help_commands(exe, model):
    """The first word of each command `help` lists."""
    r = batch(exe, model, ["help"])
    words = []
    for line in r.stdout.splitlines():
        m = re.match(r"^  ([a-z][a-z0-9]*)\b", line)
        if m:
            words.append(m.group(1))
    return words


def output_of(r):
    """The output without the lines that echo the command typed."""
    return [l for l in r.stdout.splitlines() if not l.startswith(">>")]


# (the palette's text, the command it must do the same as, set up first)
SAME = [
    ("integrate", "run", []),
    ("find equilibrium", "eq", ["run"]),
    ("nullclines", "nullclines", ["run"]),
    ("direction field", "dirfield", ["run"]),
    ("continue", "continue", ["run"]),
    ("ic last", "ic last", ["run"]),
    ("erase curves", "erase", ["run", "freeze", "run"]),
    ("find all equilibria", "eq all", ["run"]),
]


def main():
    exe, model = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    problems = []

    r = batch(exe, model, ["palette check"])
    if r.returncode != 0 or "all consistent" not in r.stdout:
        problems.append("palette check: %s" % (r.stdout + r.stderr).strip()[-400:])

    rows = listed(exe, model)
    labels = {(k, l) for k, l, _, _ in rows}
    command_names = {l.split(" ")[0] for k, l, _, _ in rows if k == "command"}
    for word in help_commands(exe, model):
        if word not in command_names:
            problems.append("help lists '%s', the palette does not" % word)
    if not any(k == "action" and l == "Nullclines" for k, l in labels):
        problems.append("the palette has no Nullclines action")
    if len(rows) < 100:
        problems.append("the palette lists only %d entries" % len(rows))
    if any(not r[3] for r in rows):
        problems.append("an entry has no description")

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for i, (text, command, setup) in enumerate(SAME):
            runs = []
            for how, step in (("direct", command), ("palette", "palette run " + text)):
                # A folder each; the scripts and the output mention it.
                d = tmp / ("%d_%s" % (i, how))
                d.mkdir()
                cmds = setup + [step, "export %s/out.csv" % d.as_posix()]
                r = batch(exe, model, cmds, "--script", d.as_posix() + "/session.py")
                csv, py = d / "out.csv", d / "session.py"
                clean = lambda t: re.sub(r"\d+_(direct|palette)", "CASE", t)
                runs.append((r, csv.read_bytes() if csv.exists() else None,
                             clean(py.read_text()) if py.exists() else None,
                             [clean(l) for l in output_of(r)]))
            (rd, cd, pd, od), (rp, cp, pp, op) = runs
            name = "'palette run %s' / '%s'" % (text, command)
            if rd.returncode != rp.returncode:
                problems.append("%s: exit codes %d and %d" % (name, rd.returncode,
                                                               rp.returncode))
            if cd != cp:
                problems.append("%s: the exported data differ" % name)
            if pd != pp:
                problems.append("%s: the saved scripts differ" % name)
            if od != op:
                problems.append("%s: the output differs: %r / %r" % (name, od, op))

    # Not available: AUTO's actions without a branch.
    r = batch(exe, model, ["palette run auto from"], "--keep-going")
    if "is not available" not in r.stdout or "branch" not in r.stdout:
        problems.append("an unavailable entry was not refused with a reason: %r" % r.stdout)
    r = batch(exe, model, ["palette run xyzzyxyzzy"], "--keep-going")
    if "nothing in the palette matches" not in r.stdout:
        problems.append("a text that matches nothing was not reported")

    for p in problems:
        print("FAILED: " + p)
    print("palette: %s" % ("%d problem(s)" % len(problems) if problems else "ok"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
