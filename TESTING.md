# Testing the XPP Studio preview

Thank you for trying XPP Studio! This is an early preview. The most useful
thing you can do is use it the way you normally use XPPAUT, then tell us
what worked, what broke and what confused you.

## Install

**Windows:** download `XPPStudio-<version>-Windows.zip` from the
[Releases page](https://github.com/CesarPerdomoDiaz/xppstudio/releases),
unzip it anywhere (your Desktop is fine), and run `xppstudio.exe`. Windows
may warn about an unrecognised app the first time. Choose
*More info > Run anyway*. The preview is not signed yet.

**macOS** (11 Big Sur or later, Apple Silicon or Intel): download
`XPPStudio-<version>-Darwin.dmg` from the same page, open it and drag the
app to *Applications*. The preview is not notarized by Apple yet, so the
first time macOS refuses to open it:

- **macOS 15 Sequoia and later:** try to open the app once and dismiss the
  warning, then open *System Settings > Privacy & Security*, scroll down to
  the message about the app, and click *Open Anyway*.
- **macOS 14 and earlier:** Control-click (or right-click) the app in
  *Applications*, choose *Open*, then *Open* again.

After that it opens normally. Mac-specific things worth checking are at
the end of the list below.

**Linux:** see "Building from source" in the README.

## Fifteen minutes of things to try

1. **Your own models.** Open two or three `.ode` files you use with XPPAUT
   (File > Open, or drag them onto the window). Do they load? Do the results
   look like XPPAUT's?
2. **The tutorial.** Press F1 and follow *Tutorial: your first model*. Is any
   step unclear?
3. **Phase plane.** Turn on nullclines and flow, click to start
   trajectories, and drag a parameter slider.
4. **Equilibria.** Press *Equilibrium*. For a saddle the manifolds should be
   drawn.
5. **AUTO.** Open the Bifurcation window, follow steady states, then pick a
   Hopf point and compute periodic orbits. Compare with XPPAUT if you can.
6. **Figures.** Export a plot as PNG and as SVG. Do the SVGs open in your
   tools (Inkscape, Illustrator, PowerPoint, LaTeX)?
7. **Muscle memory.** If you know XPPAUT's keys, try `I G`, `N N`, `S G`
   and `D D`.
8. **On a Mac:** open an `.ode` file from Finder (double-click it, or
   *Open With*), or drop it on the Dock icon; try ⌘O, ⌘S and ⌘Q; pinch and scroll in the plots with the
   trackpad; switch between light and dark appearance; check that text and
   lines are sharp on a Retina screen.

If you teach with XPPAUT, we would especially like to know whether a first
lab session would go more smoothly with XPP Studio, and what is still
missing for that.

## Reporting

Open an [issue](https://github.com/CesarPerdomoDiaz/xppstudio/issues/new/choose)
and pick:

- **Bug report** when something crashes or gives a wrong result;
- **Model does not work** when an `.ode` file that works in XPPAUT fails here
  (please attach the file);
- **Feedback** for anything else: confusing parts, missing features, ideas.

The *Command Window* shows what the program did. Copying its last lines
into the report helps a lot.

## Known gaps in this preview

These parts of XPPAUT are not in the new interface yet: two-parameter
ranges, the Kinescope (saving frames), labels and arrows on plots, and
several plots in one window. AUTO's homoclinic and boundary-value
continuations are reachable but untested. See [ROADMAP.md](ROADMAP.md).

New since the first preview, and worth a try: the **Analysis** window
(spectra, histograms, phase response curves, fitting), **Range** in the
Workspace, **boundary-value problems** (*Run > Boundary-value problem*),
the **3D View** and the **Animator** (open `doubpend.ode`, run it, press
*Animate*).
