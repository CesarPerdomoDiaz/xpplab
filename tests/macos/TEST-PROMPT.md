# Test XPP Lab on a real Mac

*A prompt for Claude Code on a Mac. Open a clone of this repository in
Claude Code and say: "Follow tests/macos/TEST-PROMPT.md".*

You are testing XPP Lab on a real Mac, with a tester beside you. CI already
builds and tests everything on GitHub's Apple Silicon runner
(`docs/development/macos.md`, Phase 1). Your job is what CI cannot see: the
download as a user gets it, the first run, Retina drawing, the trackpad,
Mac shortcuts and Finder. **Report what you find; do not change code.**
The fixes are made elsewhere, from your report.

Read `CLAUDE.md` and `docs/development/macos.md` first. Ask the tester whenever
a step needs a person: a click in a macOS dialog, a password, a sign-in, or
a judgement about how something looks or feels. Never type a password
yourself.

## 0. The machine

Record the Mac model and chip (`sysctl -n machdep.cpu.brand_string`,
`uname -m`), the macOS version (`sw_vers`) and the screen (Retina or not).
Check for the Xcode command-line tools (`xcode-select -p`) and Homebrew. If
either is missing, ask the tester to install it. Then:

```
brew install cmake ninja pkg-config sdl3 libx11 gh python
```

`gh auth status` must show the tester signed in. If not, ask them to run
`gh auth login`.

## 1. The release, as a user gets it

1. Download the newest release's disk image into a new folder:
   `gh release download --repo CesarPerdomoDiaz/xpplab --pattern 'XPPLab-macOS.dmg' --dir ~/xpps-test`.
2. Ask the tester to double-click the `.dmg`, drag the app to *Applications*, and
   open it by double-clicking, as a user would. Note exactly what macOS
   shows and what they had to do (`TESTING.md` describes *Open Anyway* and
   Control-click > *Open*). Did those instructions match? Take a screenshot
   of any dialog: `screencapture -x ~/xpps-test/first-run.png`.
3. With the app installed, run it from the terminal with screenshots. Each
   run exits by itself. For each, check that the PNG exists, note its pixel
   size (`sips -g pixelWidth -g pixelHeight`; Retina gives twice the window
   size), and look at it. Is the text sharp? Are the plots drawn, the
   panels complete, the theme right?

   ```
   A=/Applications/xpplab.app/Contents/MacOS/xpplab
   E=/Applications/xpplab.app/Contents/Resources/examples
   $A $E/lecar.ode --cmd nullclines --cmd "show phase" --screenshot ~/xpps-test/phase.png
   $A $E/lecar.ode --cmd "use hopf" --cmd "auto set parmax=0.6 nmax=220" --cmd "auto steady iapp" --cmd "auto from 2 periodic" --cmd "show auto" --screenshot ~/xpps-test/auto.png
   $A $E/lorenz.ode --cmd view3d --screenshot ~/xpps-test/view3d.png
   $A $E/doubpend.ode --cmd "ani doubpend.ani" --cmd "ani play" --screenshot ~/xpps-test/animator.png --frames 30
   $A $E/lecar.ode --cmd "help tutorial" --cmd "show help" --screenshot ~/xpps-test/help.png
   mkdir -p ~/xpps-test/rec && $A $E/lorenz.ode --cmd view3d --cmd "view3d spin 90" --record ~/xpps-test/rec --frames 40
   ```

   If the examples are elsewhere in the bundle, find them with
   `find /Applications/xpplab.app -name lecar.ode`. If a run fails, read
   the `.err` file it leaves next to the output.

## 2. Built from source

In the repository (time each step):

```
cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build --output-on-failure -E golden_examples
sh tests/build_reference.sh build-reference
python3 tests/golden.py compare --exe build/cli/xppaut-headless --reference build-reference/xppaut-reference
python3 -m venv .venv && . .venv/bin/activate && pip install numpy pytest matplotlib pandas
PYTHONPATH=python/src python -m pytest -q python/tests
pip install . && python -c "import xpplab as xs, os; m = xs.load(os.path.abspath('engine/xppaut/ode/lecar.ode')); print(m.run(total=50))"
```

The golden comparison must report every model as matching. For any
failure, keep the exact output.

## 3. By hand, with the tester (about 15 minutes)

Open the installed app normally. Ask the tester to try each of these, and write
down what happened, in their words where it matters:

- **Live parameters:** in Workspace > Parameters, drag the `iapp` slider of
  `lecar.ode`. Do the plots follow smoothly?
- **Trackpad:** two-finger scroll and pinch in the phase plane and the time
  series. Do they zoom and pan as expected? Does a click start a
  trajectory?
- **Shortcuts:** ⌘O, ⌘S, ⌘N, ⌘Q, ⌘Z / ⌘C / ⌘V in the editor, and F5 or
  the Run button. Do any need Ctrl instead of ⌘?
- **Menus:** the File, Run, View and Help menus in the window, and the
  macOS menu bar (About, Hide, Quit).
- **Finder:** double-click an `.ode` file, and drop one on the app's Dock
  icon. Does it open?
- **Files:** File > Open and File > Save use the macOS dialogs?
- **Look:** light and dark appearance (System Settings), window resize, full
  screen, text size (View > Text size). Is anything blurry, cut off or
  oddly placed?
- **AUTO:** Run > Bifurcation, steady states and then periodic orbits from
  a Hopf point.
- **3D and Animator:** View > 3D View with Spin; View > Animator on
  `doubpend.ode` with `doubpend.ani`.
- **Battery:** with the app open and idle for a minute, its CPU use in
  Activity Monitor (or `top -l 2 -pid $(pgrep -n xpplab)`). An idle app
  should use almost none.

## 4. The report

Write `tests/macos/report-YYYY-MM-DD/README.md` (today's date):

- the machine and versions from step 0;
- a table with one row per check, saying passed, failed or odd, and a
  short note;
- the exact error output of anything that failed;
- the tester's notes from step 3;
- anything that surprised you, and what you would fix first.

Copy the screenshots there too, shrunk to at most 1600 pixels wide
(`sips -Z 1600 file.png`). Copy a few frames of the recording, not all 40.

Then commit only that folder, on a new branch, and push it:

```
git switch -c macos-report
git add tests/macos/report-YYYY-MM-DD
git commit -m "macOS test report, YYYY-MM-DD [skip ci]"
git push -u origin macos-report
```

Rules for the commit:
- `[skip ci]` keeps CI from running for a report.
- Add no `Co-Authored-By` line.
- Do not push to `main` or open a pull request.

Finally, tell the tester the branch is pushed, so that they can pass it on.
