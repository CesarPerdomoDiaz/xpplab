# theme

Switch the app's colours between dark and light.

## Syntax

```
theme dark
theme light
```

- **In the app:** View > **Dark theme**.
- **In Python:** matplotlib's styles: `plt.style.use("dark_background")`.

## Description

`theme dark` gives the app light text and curves on a dark background;
`theme light` dark on light. The plots' colours change with it, so that
curves stay readable on either.

## Examples

### For a projector

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Switch to the light theme](cmd:theme light).

```
theme light
```

How to read it. A light theme usually reads better on a projector or in
print; a dark one is easier on the eyes on a screen at night.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `dark` or `light` | the command | The theme. | Light for slides and paper. |

## Results

- **Every window** is redrawn in the theme's colours.

## More about

**The theme is kept** between sessions, with the text size and the other
preferences.

## Tips

- Try the light theme for figures meant for slides or paper.

## Algorithms

The colours are changed; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: write theme dark or theme light` | Another word. | Write `dark` or `light`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [layout](layout.md), [export](export.md).

## Version history

Introduced in 0.1.0. Words other than `dark` and `light` are refused since
0.1.4.
