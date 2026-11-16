# palette

Open the command palette: type a few letters and pick an action, a command
or a recent file.

## Syntax

```
palette
palette TEXT
palette run TEXT
palette search TEXT
palette list
```

- **In the app:** Ctrl+Shift+P (⌘⇧P on a Mac).
- **In Python:** the package's methods; `xs.doc()` finds a method's page.

## Description

The palette is a search box over everything the app can do: the menus'
actions, the commands of the command window and the recently opened
files. `palette` opens it; `palette TEXT` opens it with `TEXT` already
typed. Enter runs the best match.

From the command line, `palette run TEXT` runs the best match at once,
`palette search TEXT` lists the five best without running any, and
`palette list` lists every entry with its kind, its shortcut and its
description.

## Examples

### Finding the nullcline commands

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Search the palette](cmd:palette search nullc).

```
palette search nullc
```

The command window answers:

```
palette hit 1: Nullclines
palette hit 2: nullclines
palette hit 3: nullclines freeze
palette hit 4: nullclines unfreeze
palette hit 5: nullclines play
```

How to read it. The first hit is the Phase Plane's *Nullclines* action,
the others the commands of the same name. A few letters are enough.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `TEXT` | the command; typed in the box | What to look for. | A few letters of a word. |

## Results

- **The palette** opens; or with `run`, the best match runs; with
  `search` or `list`, the command window lists the entries.

## More about

**Availability.** An entry that needs something not yet there (a model, a
run, a diagram) is not run: `palette run` says it is not available and
why.

## Tips

- Use the palette to find a command whose name you half remember.

## Algorithms

The entries are ranked by how well the text matches them; nothing is
computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: nothing in the palette matches 'TEXT'` | No entry matches. | Try fewer or other letters. |
| `error: NAME is not available: ...` | The best match needs something not there yet. | Do what the reason says first. |
| `error: palette enter needs the app's window` | `palette enter` from the batch tool. | Use `palette run TEXT`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [help](help.md): the commands and their summaries.
- [doc](doc.md): a command's reference page.

## Version history

Introduced in 0.1.0.
