# Driving the desktop app

A notebook, a script or a coding agent can drive the XPP Lab window that
is open on the same computer. The app runs each request as if it had been
typed in its command window: its plots, its Workspace and the session's
Python script follow, so you see what the code does as it does it.

```python
import xpplab as xs

app = xs.app.connect()          # the XPP Lab window on this computer
app.parameters                  # {'iapp': 0.05, 'phi': 0.333, ...}
app.set(iapp=0.2)               # the sliders move; in Live mode it reruns
sol = app.run(total=200)        # a Trajectory, every number exact
app.command("nullclines")       # anything the command window takes
app.command("auto steady iapp")
app.close()
```

| Method | What it does |
|---|---|
| `app.command(text)` | Run a command (`run`, `set iapp=0.2`, `range iapp 0 0.3 20 V`, `auto from 2 periodic`, `export d.csv auto`, ...). Returns what the command window showed; a command that fails raises `xs.app.AppError`, whose `log` holds the command window's text. `app.last_python` holds the Python the app wrote for it. |
| `app.set(**values)`, `app.get(name)` | Parameters, initial values and options of the app's model. |
| `app.run(**values)` | Set the values, run in the app, and return the run. |
| `app.last_run()` | The run the app shows. |
| `app.parameters`, `app.initial_conditions` | Their current values, as dicts. |
| `app.open(path)` | Open a model file in the app. |

The run that comes back equals what `xs.load(...).run()` gives for the same
values, bit for bit, since the app and the package share XPPAUT's engine.

## In the app's own console

The app's command window takes Python too (click its `>>` prompt). That
Python runs in a process of its own with `app` already connected and `m =
app.model()`: a Model whose `parameters`, `initial_conditions`, `set()`,
`get()` and `run()` act in the app, so the lines the app writes for each
action run there unchanged. `app.model()` works the same from a notebook.
Other Model methods on it compute with a private copy of the model at the
app's current values (`m.local()` returns that copy).

## How it connects

When the app starts it listens on this computer only (127.0.0.1, a port
the system chooses) and writes the port and a random token to
`~/.xpplab/channel.json`, a file only you can read. `connect()` reads it;
a client without the token is refused. The command `serve off` closes the
channel and `serve on` opens it again; `xpplab --no-serve` starts the app
without it. If the app is not running, `connect()` raises `AppError`
saying so.

Each request is one line of JSON with the token, an operation (`hello`,
`command`, `get`, `values`, `run`) and its text; each reply is one line of
JSON. The app answers one request per client per frame, so it keeps
drawing while a script works.
