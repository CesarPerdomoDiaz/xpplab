#!/bin/sh
# Builds XPP_Lab-<version>-x86_64.AppImage from the install tree that the
# Linux tarball is made of (the CMake "app" component).
#
# usage: tools/make_appimage.sh BUILD_DIR [OUTPUT_DIR]
#   BUILD_DIR   a finished build (cmake --build), made on the oldest supported
#               system (Ubuntu 22.04): the AppImage has the same glibc baseline
#               as the tarball. OUTPUT_DIR defaults to BUILD_DIR.
# Needs: cmake, curl, sha256sum. Runs appimagetool with --appimage-extract-and-run,
# so FUSE is not needed to build.
set -eu

# appimagetool, pinned. To update: download the new file, compute its
# SHA-256 yourself, and change both lines.
APPIMAGETOOL_VERSION=1.9.0
APPIMAGETOOL_SHA256=46fdd785094c7f6e545b61afcfb0f3d98d8eab243f644b4b17698c01d06083d1
APPIMAGETOOL_URL=https://github.com/AppImage/appimagetool/releases/download/$APPIMAGETOOL_VERSION/appimagetool-x86_64.AppImage

[ $# -ge 1 ] || { echo "usage: $0 BUILD_DIR [OUTPUT_DIR]" >&2; exit 2; }
src=$(cd "$(dirname "$0")/.." && pwd)
build=$(cd "$1" && pwd)
out=${2:-$build}
mkdir -p "$out"
out=$(cd "$out" && pwd)
[ "$(uname -m)" = x86_64 ] || { echo "only x86_64 is supported" >&2; exit 1; }

version=$(sed -n 's/^CMAKE_PROJECT_VERSION:[A-Z]*=//p' "$build/CMakeCache.txt")
[ -n "$version" ] || { echo "no version in $build/CMakeCache.txt" >&2; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
appdir=$work/XPP_Lab.AppDir

# The same files as the tarball, in one folder: the binaries find the engine,
# SDL, examples/ and python/ beside themselves.
cmake --install "$build" --component app --prefix "$appdir/usr/lib/xpplab"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/mime/packages" \
         "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/512x512/apps"
ln -s ../lib/xpplab/xpplab "$appdir/usr/bin/xpplab"

cp "$src/packaging/linux/AppRun" "$appdir/AppRun"
chmod +x "$appdir/AppRun"
cp "$src/packaging/linux/xpplab.desktop" "$appdir/xpplab.desktop"
cp "$src/packaging/linux/xpplab.desktop" "$appdir/usr/share/applications/xpplab.desktop"
cp "$src/packaging/linux/xpplab.xml" "$appdir/usr/share/mime/packages/xpplab.xml"
cp "$src/app/resources/icon.png" "$appdir/xpplab.png"
cp "$src/app/resources/icon.png" "$appdir/.DirIcon"
cp "$src/app/resources/icon.png" "$appdir/usr/share/icons/hicolor/512x512/apps/xpplab.png"

tool=$work/appimagetool.AppImage
curl -fsSL "$APPIMAGETOOL_URL" -o "$tool"
echo "$APPIMAGETOOL_SHA256  $tool" | sha256sum -c -
chmod +x "$tool"

file=$out/XPP_Lab-$version-x86_64.AppImage
ARCH=x86_64 "$tool" --appimage-extract-and-run "$appdir" "$file"
echo "$file"
