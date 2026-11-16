# help

List the commands, show one command's summary, or open a chapter of the
manual.

## Syntax

```
help
help COMMAND
help CHAPTER
```

- **In the app:** the Help window (F1), and the menus' tooltips.
- **In Python:** `help(m.sweep)`, Python's own help on the package.

## Description

`help` alone lists every command of the command window, with its
arguments, a one-line description and, in brackets, XPPAUT's keys for it
where the app has them.

`help COMMAND` gives that command's summary: its syntax, what it does, and
the forms it takes, followed by a pointer to its reference page when it
has one. `help CHAPTER` opens a chapter of the manual, such as `odefile`
or `auto`.

## Examples

### The summary of range

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Show the summary of range](cmd:help range).

```
help range
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
print(m.sweep.__doc__.splitlines()[0])
```

The command window shows the syntax, `range NAME FROM TO N [NAME2 FROM2 TO2
[N2]] [VAR|lyap|eq] [continue]`, a paragraph on what each word does, and
"doc range opens its reference page, with worked examples."

How to read it. The summary is enough to recall a command's words; the
reference page ([doc](doc.md)) explains them with examples. In Python the
methods' docstrings play the role of the summaries.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `COMMAND` or `CHAPTER` | the command | What to show. | Leave it out for the list of commands. |

## Results

- **The command window** shows the list or the summary; a chapter opens in
  the Help window.

## More about

**XPPAUT's keys.** XPPAUT is driven by two-letter keys (`I G` to run,
`N N` for nullclines). The app answers to the common ones, listed in
brackets by `help`, for those who know them by heart.

## Tips

- `help` alone, to find a command's exact words.
- [doc](doc.md) for the full page.

## Algorithms

The summaries come from the app's command table; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no command or manual page 'NAME'. Pages: ...` | Neither a command nor a chapter. | Pick one from the list given. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [doc](doc.md): the reference page of a command.
- [Commands and shortcuts](../manual/commands.md): the commands in the
  manual.

## Version history

Introduced in 0.1.0.
