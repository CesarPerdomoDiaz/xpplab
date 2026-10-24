"""Print the valgrind errors that involve XPP Studio's code.

    python tests/valgrind_report.py vg-*.log

Run valgrind with --fullpath-after= so that stack frames carry full source
paths. Errors raised only inside Python, numpy or the C library are
skipped. Exits with status 1 if any errors remain.
"""
import re
import sys
from pathlib import Path

OURS = ("/engine/", "libxppe", "/tests/", "/cli/", "/app/src/")
ERRORS = re.compile(r"(Invalid |Conditional jump|Use of uninitialised|"
                    r"Syscall param|Mismatched|Source and destination|"
                    r"Argument .* of function)")


def blocks(text):
    """Split a valgrind log into its messages (separated by '==pid== ')."""
    block = []
    for line in text.splitlines():
        m = re.match(r"==\d+== ?(.*)", line)
        if not m:
            continue
        if m.group(1).strip():
            block.append(m.group(1))
        elif block:
            yield block
            block = []
    if block:
        yield block


def main():
    seen, found = set(), 0
    for path in sys.argv[1:]:
        for b in blocks(Path(path).read_text(errors="replace")):
            if not ERRORS.match(b[0]):
                continue
            if not any(k in line for line in b for k in OURS):
                continue
            key = "\n".join(re.sub(r"0x[0-9A-F]+", "", l) for l in b)
            if key in seen:
                continue
            seen.add(key)
            found += 1
            print(f"--- {Path(path).name}")
            print("\n".join(b))
            print()
    print(f"{found} distinct memory errors involving XPP Studio code")
    sys.exit(1 if found else 0)


if __name__ == "__main__":
    main()
