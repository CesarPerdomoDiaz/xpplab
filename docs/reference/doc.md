# doc

Open the reference page of a command, with its worked examples.

## Syntax

```
doc
doc NAME
```

- **In the app:** the **?** beside *Follow steady states* in the
  Bifurcation window, and the Help window.
- **In Python:** `xs.doc("range")`, or `xs.doc(m.sweep)` from the method.

## Description

`doc NAME` opens the reference page of the command `NAME` in the Help
window: what it does, its settings, worked examples with every number
explained, how to read the results, and what to do when it fails. `doc`
alone opens the index of all the pages.

`NAME` may be another name of a command (`doc sweep` opens `range`), or a
chapter of the manual (`doc odefile`). A misspelt name gets the closest
page as a suggestion. In Python, `xs.doc()` opens the same page on the
website and returns its address; a method of the package opens the page
of the command that does the same.

## Examples

### Finding a page

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Open the page of range](cmd:doc range), then try its other name with
   `doc sweep` and a typing slip with `doc rnage`.

```
doc range
doc sweep
doc rnage
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
print(xs.doc("range", open=False))
print(xs.doc(m.sweep, open=False))
print(xs.doc("no such command", open=False))
```

`doc sweep` says "sweep is another name for range." and opens the same
page; `doc rnage` answers "error: no reference page 'rnage'. Closest:
range. 'doc' alone opens the index." In Python the first two give
`https://cesarperdomodiaz.github.io/xpplab/reference/range/`, and a name
with no page gives the index.

How to read it. Every way into a command, its name, another name, the
package's method, leads to one page, so what is explained there holds for
the app and for Python alike.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `NAME` | the command; the name or method in Python | A command, another name of one, or a manual chapter. | Leave it out for the index. |
| `open` | `open=False` in Python | Only return the address. | For scripts that should not open a browser. |

## Results

- **The Help window** shows the page; from the batch tool, the command
  window gives its address.
- **In Python** `xs.doc()` opens the browser and returns the address.

## More about

**`doc` and `help`.** [help](help.md) gives a command's one-paragraph
summary in the command window; `doc` the full page. Use `help` to recall
the syntax, `doc` to learn what the command does and how to read it.

## Tips

- The **?** beside *Follow steady states* opens the page of
  [auto steady](auto-steady.md) the same way.
- Read a page's examples in the app: their links run each step.

## Algorithms

The name is looked up among the pages, the commands' other names and the
manual's chapters; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no reference page 'NAME'. Closest: ...` | A misspelt name. | Use the suggestion. |
| `error: no reference page 'NAME'. 'doc' alone opens the index, ...` | Nothing is close. | Open the index, or list the commands with `help`. |
| `NAME has no reference page yet; its summary: ...` | The page is still to be written. | The summary follows; the manual's chapters cover the rest. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [help](help.md): the commands and their summaries.
- [Every command and action](reference.md): the index.

## Version history

Introduced in 0.1.4.
