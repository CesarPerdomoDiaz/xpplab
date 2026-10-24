#!/bin/sh
# Build the unmodified XPPAUT 8.0 sources (the import commit) against the
# real X11 library. The result is the oracle for
#     python tests/golden.py compare --exe <ours> --reference <this>
# on Linux and macOS. Usage: tests/build_reference.sh [output-dir]
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mkdir -p "${1:-$root/build-reference}" && cd "${1:-$root/build-reference}" && pwd)
src="$out/src"
cc=${CC:-cc}

rm -rf "$src"
mkdir -p "$src"
# The first commit holds XPPAUT exactly as distributed.
first=$(git -C "$root" rev-list --max-parents=0 HEAD)
git -C "$root" archive "$first" engine/xppaut | tar -x -C "$src" --strip-components=2

objs=$(sed -n '/^OBJECTS = /,/^$/p' "$src/Makefile" | grep -o '[a-z0-9_-]*\.o')

# No _FORTIFY_SOURCE, which Ubuntu's GCC turns on by default: XPPAUT writes
# one byte past a buffer for every model with an export statement (see
# docs/upstream-issues.md), and the checks would abort those runs.
cflags="-std=gnu99 -O2 -w -fcommon -ffp-contract=off -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
 -DNOERRNO -DNON_UNIX_STDIO \
 -DAUTO -DCVODE_YES -DHAVEDLL -DMYSTR1=8.0 -DMYSTR2=1 $(pkg-config --cflags x11)"
# Newer GCC and Clang reject XPPAUT's pre-C99 code unless told otherwise.
if $cc --version 2>/dev/null | grep -qi clang; then
  cflags="$cflags -Wno-error=implicit-function-declaration -Wno-error=implicit-int \
 -Wno-error=int-conversion -Wno-error=incompatible-pointer-types \
 -Wno-error=incompatible-function-pointer-types -Wno-error=return-type"
else
  cflags="$cflags -fpermissive"
fi

cd "$src"
for o in $objs; do
  $cc $cflags -c "${o%.o}.c" -o "$o"
done
$cc -o "$out/xppaut-reference" $objs $(pkg-config --libs x11) -lm -ldl
echo "built $out/xppaut-reference"
