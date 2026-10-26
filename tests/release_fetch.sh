#!/bin/sh
# python fetch from a packaged app, with the real download: the Python
# pinned for this computer, checked against its digest, then NumPy and
# Matplotlib from PyPI, and a line of Python run with it in the console.
# The release workflow runs it on each package, so that a release never
# ships a pinned Python that cannot be fetched.
#
# usage: tests/release_fetch.sh MODEL.ode COMMAND...
#   COMMAND starts the packaged app: ./xpplab, xvfb-run -a ./xpplab,
#   arch -x86_64 XPPLab.app/Contents/MacOS/xpplab, ...
set -u
model=$1
shift
dir=$(mktemp -d)
log="$dir/app.log"
XPPLAB_RUNTIME_DIR="$dir/runtime" XPPLAB_CHANNEL="$dir/channel.json" \
  "$@" "$model" --serve \
  --cmd "python fetch" \
  --cmd "py import sys, numpy, matplotlib; print('fetched', sys.version.split()[0], numpy.__version__, matplotlib.__version__)" \
  --cmd "py app.command('quit')" \
  --screenshot "$dir/shot.png" --frames 100000000 --log "$log"
cat "$dir/shot.png.err" 2>/dev/null
if grep -q "is installed in" "$log" 2>/dev/null && grep -q "^fetched 3\." "$log" &&
   test -f "$dir/runtime/python/xpplab-runtime.txt"; then
  grep -E "^(Downloading|  checked|Python|fetched|Starting)" "$log"
  rm -rf "$dir"
  exit 0
fi
echo "python fetch did not work; the command window said:"
cat "$log" 2>/dev/null || echo "(no log)"
exit 1
