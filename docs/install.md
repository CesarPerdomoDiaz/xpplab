# Installing XPP Studio

XPP Studio comes ready to run on Windows, macOS and Linux, and as a Python
package. Nothing else is needed: no X server, no compiler, no Terminal
commands beyond the ones below.

| Your computer | Download |
|---|---|
| Windows 10 or 11, 64-bit | [XPPStudio-Windows-x64.zip](https://github.com/CesarPerdomoDiaz/xppstudio/releases/latest/download/XPPStudio-Windows-x64.zip) |
| macOS 11 or later, Apple silicon or Intel | [XPPStudio-macOS.dmg](https://github.com/CesarPerdomoDiaz/xppstudio/releases/latest/download/XPPStudio-macOS.dmg) |
| Linux, x86-64 | [XPPStudio-Linux-x86_64.tar.gz](https://github.com/CesarPerdomoDiaz/xppstudio/releases/latest/download/XPPStudio-Linux-x86_64.tar.gz) |

These links always give the newest release. Every release, with its notes
and checksums, is on the
[releases page](https://github.com/CesarPerdomoDiaz/xppstudio/releases).

The apps are not yet signed with a paid certificate (an Authenticode
certificate on Windows, Apple's notarization on macOS). That is why
Windows and macOS ask you to confirm the first time you open them. The
steps below show how.

## Windows

1. Download `XPPStudio-Windows-x64.zip` and unzip it anywhere: your
   Documents, the Desktop or a USB stick. It needs no installation and no
   administrator rights.
2. Open the folder and run `xppstudio.exe`.
3. If Windows shows "Windows protected your PC", click **More info**, then
   **Run anyway**. Windows asks only the first time.

The folder holds the app, the engine (`libxppe.dll`), SDL (`SDL3.dll` and
`libiconv-2.dll`), XPPAUT's example models in `examples`, and the licences.
To remove XPP Studio, delete the folder.

## macOS

1. Download `XPPStudio-macOS.dmg` and open it.
2. Drag the app to **Applications**.
3. Open it from Applications. The first time, macOS says it cannot check
   the app:
    - **macOS 15 and later:** click **Done**, open **System Settings >
      Privacy & Security**, scroll down to the message about the app,
      click **Open Anyway** and confirm.
    - **macOS 14 and earlier:** Control-click the app (or right-click it),
      choose **Open**, then **Open** again.

If macOS instead says the app "is damaged and can't be opened", the
download's quarantine mark is to blame, not the app. Remove the mark in
Terminal, then open the app as above:

```bash
xattr -dr com.apple.quarantine /Applications/xppstudio.app
```

The app runs natively on both Apple silicon and Intel Macs.

## Linux

```bash
tar xzf XPPStudio-Linux-x86_64.tar.gz
XPPStudio-Linux-x86_64/xppstudio
```

The folder holds the app, the engine (`libxppe.so`), SDL (`libSDL3.so`),
the example models and the licences, and it runs from
wherever you put it. It is built on Ubuntu 22.04 and needs a desktop with
X11 or Wayland and OpenGL, which every desktop distribution has. If it does
not start, run it from a terminal: the message says what is missing.

## Python

The Python package runs the same engine, AUTO included, on Python 3.9 or
later on all three systems. Each release has a ready-built wheel for each
system, so nothing is compiled on your machine:

```bash
pip install xppstudio --find-links https://github.com/CesarPerdomoDiaz/xppstudio/releases/expanded_assets/v0.1.0
```

`pip` picks the wheel for your system from that release. For plots and
data frames, add the extras: `pip install "xppstudio[all]" --find-links ...`
with the same link. Then follow the [Python quick start](python/quickstart.md).

## Checking a download

Each release lists the SHA-256 checksum of every file, in its notes and in
the file `SHA256SUMS`. To compare, compute the checksum of your download:

| System | Command |
|---|---|
| Windows (PowerShell) | `Get-FileHash XPPStudio-Windows-x64.zip` |
| macOS | `shasum -a 256 XPPStudio-macOS.dmg` |
| Linux | `sha256sum XPPStudio-Linux-x86_64.tar.gz` |

## Building from source

To build XPP Studio yourself, or on a system without a download, see
[Building from source](https://github.com/CesarPerdomoDiaz/xppstudio#building-from-source)
in the README. You need a C compiler, CMake 3.20 or later, and SDL 3 for
the app.

## Something went wrong?

Please [open an issue](https://github.com/CesarPerdomoDiaz/xppstudio/issues/new/choose)
with your system, the version (in **Help > About XPP Studio**), and what you saw.
