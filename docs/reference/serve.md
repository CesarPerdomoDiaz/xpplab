# serve

Let a notebook, a script or a coding agent on this computer drive the app.

## Syntax

```
serve
serve on
serve off
```

- **In the app:** the channel is open from the start; `xpplab --no-serve`
  starts the app without it.
- **In Python:** `app = xs.app.connect()`, then `app.command(...)`,
  `app.set(...)`, `app.run(...)`.

## Description

The app listens for Python on this computer only, from the moment it
starts. `serve off` closes that channel and drops the clients connected
to it, the console's too: Python can no longer connect. `serve` or
`serve on` opens it again and says how many clients are connected.

A connected script works the app as if it typed in the command window:
the plots, the Workspace and the session's Python script follow each
request, so you see what the code does as it does it. The app's own
console, [py](py.md), connects through the same channel and opens it when
the console starts.

## Examples

### Drive the app from a notebook

1. [Open the Morris-Lecar model](example:lecar.ode) in the app.
2. In a notebook or a script, connect, set the current, run longer, and
   ask for the equilibrium.

```
serve
```

```python
import numpy as np
import xpplab as xs

app = xs.app.connect()          # the XPP Lab window on this computer
app.set(iapp=0.2)
sol = app.run(total=200)
print(np.max(sol["V"]), len(sol))
print(app.command("eq"))
app.close()
```

The app's sliders move to `iapp` = 0.2, it runs for 200 time units, and
the run comes back with 4001 rows and its largest `V`,
0.2983856201171875: bit for bit what `xs.load(...).run(iapp=0.2,
total=200)` gives, since the app and the package share XPPAUT's engine.
`app.command("eq")` returns what the command window showed:

```
>> eq
Equilibrium (unstable focus): V=0.0687252 W=0.393797
  eigenvalues: 0.010419+1.2556i 0.010419-1.2556i
>>> eq = m.equilibrium()
```

How to read it. The script and the window are one session: the lines the
app writes for each request (`>>>`) are the session's Python script, and
`app.last_python` holds the last of them.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `on`, `off` | the command | Open or close the channel. | Off when no script should reach the app. |
| `--no-serve`, `--serve` | the app's command line | Start without the channel; a screenshot or recording opens it only with `--serve`. | |

## Results

- **The command window** says whether Python can connect, and how many
  clients are connected.

## More about

**How it connects.** The app listens on 127.0.0.1, on a port the system
chooses, and writes the port and a random token to
`~/.xpplab/channel.json`, a file only you can read. `connect()` reads it;
a client without the token is refused. Each request is one line of JSON,
and so is each reply. The app answers one request per client per frame,
so it keeps drawing while a script works.

**What a client can do.** `app.command(text)` runs any command and raises
`xs.app.AppError` when it fails; `app.set()`, `app.get()`, `app.run()`,
`app.last_run()`, `app.parameters`, `app.initial_conditions` and
`app.open(path)` cover the usual steps; `app.model()` returns a Model that
acts in the app. See the Python guide's *Driving the desktop app*.

## Tips

- Use `with xs.app.connect() as app:` so that the connection closes when
  the block ends.
- A coding agent with the MCP server (`python -m xpplab.mcp`) drives the
  app the same way, through its `app_command` and `app_run` tools.

## Algorithms

A TCP server on the loopback address, polled once a frame; requests are
run by the same code as the command window.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: write serve, serve on or serve off` | Another word after `serve`. | Use one of those. |
| `error: xpplab-batch has no channel; drive the desktop app instead` | `serve` in `xpplab-batch`. | Start the desktop app. |
| `error: the channel for Python could not be opened` | The port or the channel file could not be made. | Check that `~/.xpplab` can be written. |
| `AppError: XPP Lab is not running, or does not let Python connect` (in Python) | The app is closed, or `serve off`. | Start the app, or `serve on`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [py](py.md): Python in the app's own command window.
- [script](script.md): save the session as a Python script.

## Version history

Introduced in 0.1.0. Since 0.1.4 a word other than `on` or `off` is an
error.
