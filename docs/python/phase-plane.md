# Phase plane

![m.phase_plane() and m.sweep() on the FitzHugh-Nagumo model](../images/python-phase-plane.png)

## One call

```python
ax = m.phase_plane("v", "w", xlim=(-2.5, 2.5), ylim=(-1, 2.5))
```

draws the trajectory, both nullclines, the direction field and the nearest
equilibrium (filled if stable, open if not). Each part can be switched off:
`nullclines=False`, `field=False`, `trajectory=False`, `equilibrium=False`.
Pass `ax=` to draw into your own axes.

## The pieces

```python
xn, yn = m.nullclines("v", "w", (-2.5, 2.5), (-1, 2.5), grid=150)
```

returns two arrays of line segments, each of shape (n, 4) holding
`x1, y1, x2, y2`. They come from XPPAUT's own contouring algorithm. Draw
them with matplotlib's `LineCollection(xn.reshape(-1, 2, 2))`.

```python
X, Y, U, V = m.vector_field("v", "w", (-2.5, 2.5), (-1, 2.5), n=20)
plt.quiver(X, Y, U, V)
```

```python
f = m.rhs({"v": 0.5, "w": 0.2})     # dv/dt and dw/dt at a point
```

In models with more than two variables, the variables not on the axes keep
their initial values.

## Equilibria

```python
eq = m.equilibrium()                     # start from the initial values
eq = m.equilibrium({"v": 1.0, "w": 0.5}) # or from a guess
eq.values           # {'V': ..., 'W': ...}
eq.eigenvalues      # complex NumPy array
eq.stable, eq.kind  # False, 'unstable focus'
m.equilibrium(set_initial=True)          # also make it the initial value
```

`kind` is one of: stable node, unstable node, stable focus, unstable focus,
saddle, saddle-focus. For maps it is stable or unstable.
