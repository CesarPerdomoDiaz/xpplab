"""The Windows installer: a silent install and uninstall, and a model opened
from the installed app.

usage: installer_check.py XPPLab-Windows-x64.exe model.ode

Two rounds, each into a folder whose name has spaces and non-ASCII letters:
one with `/ASSOC=1` (the .ode association, over another handler of .ode that
the test sets and the uninstaller must restore) and one with `/DESKTOP=1`
(the desktop shortcut). Each round installs silently, checks the files, the
registry and the shortcuts, opens a copy of the model from the installed
app, started from another folder with the model's path as its first
argument, then uninstalls silently and checks that the folder, the registry
entries and the shortcuts are gone and that .ode is as it was before.

Exits 77 (skipped) when this is not Windows, when the installer is not built
(CPack makes it only when NSIS is installed: `cpack` in the build folder) or
when the machine cannot open a window.
"""
import ctypes
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

if sys.platform != "win32":
    print("the installer is for Windows")
    sys.exit(77)

import winreg

installer, model = str(Path(sys.argv[1]).resolve()), Path(sys.argv[2]).resolve()
if not Path(installer).exists():
    print("no installer was built (it needs NSIS): " + installer)
    sys.exit(77)
UNINSTALL = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\XPPLab"
APP = r"Software\XPPLab"
ODE = r"Software\Classes\.ode"
PROGID = r"Software\Classes\XPPLab.ode"
FOLDER = "ñandú é"


def read(path, name=""):
    """A HKCU value, or None when the key or the value is missing."""
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as k:
            return winreg.QueryValueEx(k, name)[0]
    except FileNotFoundError:
        return None


def exists(path):
    try:
        winreg.CloseKey(winreg.OpenKey(winreg.HKEY_CURRENT_USER, path))
        return True
    except FileNotFoundError:
        return False


def special_folder(csidl):
    buf = ctypes.create_unicode_buffer(1024)
    ctypes.windll.shell32.SHGetFolderPathW(0, csidl, 0, 0, buf)
    return Path(buf.value)


start_menu = special_folder(0x02) / "XPP Lab.lnk"   # CSIDL_PROGRAMS
desktop = special_folder(0x10) / "XPP Lab.lnk"      # CSIDL_DESKTOPDIRECTORY


def state():
    return {"ode": read(ODE), "ode_key": exists(ODE),
            "progid": exists(PROGID), "app": exists(APP),
            "uninstall": exists(UNINSTALL), "start": start_menu.exists(),
            "desktop": desktop.exists()}


def fail(msg):
    sys.exit(msg)


def check(ok, what):
    if not ok:
        fail("FAILED: " + what)
    print("ok: " + what)


def silent_install(option, target):
    # NSIS wants /D= last and unquoted, so the command line is a string.
    cmd = f'"{installer}" /S {option} /D={target}'
    r = subprocess.run(cmd, capture_output=True, timeout=300)
    check(r.returncode == 0, f"the silent install ({option}) exits 0")


def silent_uninstall(target):
    # The uninstaller copies itself to a temporary folder and returns at
    # once, so wait until the folder is gone.
    r = subprocess.run([str(target / "Uninstall.exe"), "/S"],
                       capture_output=True, timeout=300)
    check(r.returncode == 0, "the silent uninstall exits 0")
    deadline = time.time() + 60
    while target.exists() and time.time() < deadline:
        time.sleep(0.5)
    time.sleep(1)  # the registry and shortcut steps follow the files


def open_model(exe, work):
    """Run the installed app on a copy of the model, from another folder."""
    models = work / "modelos" / FOLDER
    src = models / "mi modelo ñandú.ode"
    models.mkdir(parents=True)
    shutil.copyfile(model, src)
    elsewhere = work / "otra carpeta"
    elsewhere.mkdir()
    shot = models / "captura.png"
    log = models / "registro.txt"
    r = subprocess.run([str(exe), str(src), "--screenshot", str(shot),
                        "--log", str(log), "--frames", "4"],
                       cwd=elsewhere, capture_output=True, text=True,
                       timeout=120)
    err = Path(str(shot) + ".err")
    if r.returncode != 0 and err.exists() and any(
            k in err.read_text() for k in
            ("SDL_Init", "SDL_CreateWindow", "SDL_CreateRenderer")):
        print("no window available: " + err.read_text().strip())
        return None
    check(r.returncode == 0, f"the installed app exits 0 ({r.stderr!r})")
    check(shot.read_bytes()[:4] == b"\x89PNG", "the screenshot is a PNG")
    lines = log.read_text(encoding="utf-8").splitlines()
    check(any(line.startswith(f"Loaded {src.name}: ") for line in lines),
          f"the app loaded {src.name} from its first argument")
    return True


def round_(option, assoc, desktop_shortcut):
    # With the association, .ode starts out with another handler, which the
    # uninstaller must put back.
    other = "XPPLabTest.other"
    if assoc and not exists(ODE):
        winreg.SetValue(winreg.HKEY_CURRENT_USER, ODE, winreg.REG_SZ, other)
    try:
        return round_inner(option, assoc, desktop_shortcut)
    finally:
        if read(ODE) == other:
            winreg.DeleteKey(winreg.HKEY_CURRENT_USER, ODE)


def round_inner(option, assoc, desktop_shortcut):
    with tempfile.TemporaryDirectory() as t:
        work = Path(t)
        target = work / FOLDER / "XPP Lab"
        before = state()
        check(not before["uninstall"] and not before["app"],
              "no XPP Lab is installed before the round")
        try:
            silent_install(option, target)
            exe = target / "xpplab.exe"
            check(exe.exists() and (target / "Uninstall.exe").exists(),
                  "the app and the uninstaller are in the folder")
            check((target / "libxppe.dll").exists(), "the engine is there")
            now = state()
            check(now["uninstall"], "it is listed under Installed apps")
            check(read(UNINSTALL, "InstallLocation") == str(target),
                  "the listing records the folder")
            check(now["start"], "the Start menu entry exists")
            check(now["desktop"] == (before["desktop"] or desktop_shortcut),
                  "the desktop shortcut exists only when asked for")
            if assoc:
                cmd = read(PROGID + r"\shell\open\command")
                check(read(ODE) == "XPPLab.ode" and
                      cmd == f'"{exe}" "%1"',
                      ".ode opens with the installed xpplab.exe")
            else:
                check(read(ODE) == before["ode"] and not now["progid"],
                      "without /ASSOC=1 .ode is left alone")
            opened = open_model(exe, work)
        finally:
            uninstaller = target / "Uninstall.exe"
            if uninstaller.exists():
                silent_uninstall(target)
        check(not target.exists(), "the install folder is gone")
        check(state() == before,
              "the registry and shortcuts are as before the install")
        return opened


results = [round_("/ASSOC=1", True, False), round_("/DESKTOP=1", False, True)]
if not all(results):
    sys.exit(77)
