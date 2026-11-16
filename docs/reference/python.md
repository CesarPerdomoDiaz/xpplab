# python

Choose, stop or install the Python that the app's console runs.

## Syntax

```
python
python use PATH
python stop
python fetch
python fetch stop
python install PACKAGE ...
```

- **In the app:** the command window offers **Install Python** when it
  finds none; **Stop** beside it stops a running line.
- **In Python:** outside the app you run your own interpreter, with
  `pip install xpplab`.

## Description

`python` alone says whether the console's Python is running, and which
one, and whether XPP Lab's own Python is installed.

`python use PATH` makes the console use the Python at `PATH` (a
`python.exe`, or `python3`), remembered between sessions; `python use`
with no path goes back to looking for one. Without a choice the console
takes `$XPPLAB_PYTHON`, else XPP Lab's own Python if it is installed,
else the first Python on the `PATH`. It needs Python 3 with NumPy.

`python stop` ends the console's Python; its variables are lost and the
next line of [py](py.md) starts a fresh one. `python restart` is another
name for it.

`python fetch` downloads and installs XPP Lab's own Python, a Python 3.13
made to run from any folder, with NumPy and Matplotlib: about 55 MB to
download, once, and about 200 MB on disk, in a folder of its own in XPP
Lab's settings folder. The download is checked against the SHA-256 digest
the app was built with. `python fetch stop` stops it and keeps nothing.

`python install PACKAGE ...` runs pip with the console's Python, for
example `python install pandas scipy`.

## Examples

### Which Python, and a package more

1. [Ask which Python the console uses](cmd:python).
2. Start it with a line of Python, `py import sys; sys.version_info[:2]`.
3. Add a package: `python install pandas`.

```
python
py import sys; sys.version_info[:2]
python install pandas
```

```python
import sys
import numpy as np

print(sys.executable, sys.version_info[:2], np.__version__)
```

Before any Python line, the command window answers:

```
Python is not running (py CODE starts it).
XPP Lab's own Python is not installed (python fetch installs it).
```

After the `py` line it names the Python it started, with its version, and
`python` then says `Python is running: PATH`. `python install` says
`pip install pandas (with PATH) ...` and, when pip is done, that the
running Python can import what is new.

How to read it. The console runs any Python 3 with NumPy; it uses the
copy of the xpplab package that comes with the app, so that the package
and the app always match.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `PATH` | `python use` | The Python to run. | Remembered between sessions. |
| `$XPPLAB_PYTHON` | the environment | The Python, when none was chosen. | |
| `$XPPLAB_PYTHON_URL`, `$XPPLAB_PYTHON_SHA256` | the environment | Where `python fetch` downloads from, and the digest to check. | For a computer without internet: a copy of the archive. |
| `$XPPLAB_RUNTIME_DIR` | the environment | Where `python fetch` installs. | |

## Results

- **The command window** says which Python is in use, or what was
  installed; a bar under it shows how far a download has got.

## More about

**Removing it.** XPP Lab's own Python changes nothing else on the
computer; deleting its folder removes it. `python` shows where it is.

**The download.** It needs curl and tar, which Windows 10 and later,
macOS and most Linux systems have.

## Tips

- On a computer with Python and NumPy already, the console needs nothing
  more.
- Lines of Python typed while `python fetch` runs wait and run as soon as
  it is ready.

## Algorithms

The console's Python is a separate process; the app sends it the lines
and shows what it prints.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: python takes use PATH, stop, fetch [stop] or install PACKAGE ...; python alone says which Python the console uses` | A word that `python` does not take. | Use one of those. |
| `Write: python install PACKAGE ... (for example python install pandas)` | `python install` with no package. | Name the packages. |
| `error: start the console first (py), so that pip runs with its Python` | No Python yet to run pip with. | Run a `py` line first, or `python fetch`. |
| `pip failed (exit status N): ...` | pip could not install the package. | Read pip's last line: a misspelt name, or no internet. |
| `error: wait until this has finished: ...` | A download or another install is under way. | Wait, or `python fetch stop`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [py](py.md): run Python in the command window.
- [serve](serve.md): let a notebook or a script drive the app.

## Version history

Introduced in 0.1.0. Since 0.1.4 a word it does not take is refused
instead of showing the status.
