# XPP Studio for macOS: the plan

**Goal:** a native Mac app that researchers download, drag to
Applications and double-click. It should run on Apple Silicon and Intel
alike, be signed and notarized, and need no XQuartz, no Homebrew and no
Terminal. It should look and behave like a Mac app while keeping XPPAUT's
workflow.

This replaces today's route to XPPAUT on a Mac: install XQuartz, download a
command-line binary, `chmod` it, then allow it in *Privacy & Security*.

## Where things stand

The code is already portable:

- **The engine needs no X server.** XPPAUT's X11 calls go to a null
  implementation.
- **The app uses SDL3**, which draws through Metal on macOS and uses the
  native file dialogs (NSOpenPanel and NSSavePanel).
- **The Mac build is set up:**
  - a `.app` bundle with its icon (`icon.icns`), and the engine and examples
    in `Contents/Resources`, where SDL looks for them;
  - SDL3 copied into the bundle with `fixup_bundle`, then an ad-hoc
    signature;
  - a `.dmg` built by CPack.
- **CI has a macOS job (Apple Silicon).** It builds everything, builds
  unmodified XPPAUT 8.0 with X11 from Homebrew, checks that every example
  model gives the same output with both, runs the Python tests, starts
  the app to take a screenshot, and builds the `.dmg`. It passes.

## Phase 1: build and pass the tests on macOS — done

On GitHub's Apple Silicon runner (macOS 26, Apple Clang):

- **Everything compiles**: the engine, the command-line program, the app
  and the tests. Clang is stricter than GCC about implicit declarations in
  the 1990s C; the flags for that were already set.
- **All 197 example models give the same output as unmodified XPPAUT 8.0**,
  built on the same Mac with X11. Apple Clang fuses multiply-adds by
  default; the build turns that off (`-ffp-contract=off`) so the numbers
  match.
- **The engine API tests pass**, as does the AUTO test, which compares the
  Morris-Lecar diagram with one recorded on Windows. Off Windows that test
  compares the steady states and the first lap of periodic orbits: Apple's
  maths library differs from Windows' in the last bits, and over AUTO's
  later laps around the closed branch those differences grow.
- **All 26 Python tests pass.** They load private engine copies with
  `dlopen` from `$TMPDIR` and run AUTO in per-model work folders.
- **The app starts, loads a model, draws its nullclines**, and saves a
  screenshot. CPack builds the `.dmg`.

![XPP Studio on macOS](../images/macos-first-run.png)

*The first run on a Mac: the CI screenshot from GitHub's Apple Silicon
runner, in the system's light appearance.*

## Phase 2: a real Mac app

Already in the code:

- **Universal binary** (arm64 and x86_64). The release workflow sets
  `CMAKE_OSX_ARCHITECTURES` and compiles SDL3 from source
  (`XPPS_FETCH_SDL3`), because Homebrew's SDL3 is built for one
  architecture only. *Not yet run: the first tagged release will show it.*
- **Deployment target macOS 11**, the first version for Apple Silicon
  (`LSMinimumSystemVersion` in `Info.plist`).
- **Open `.ode` files from Finder.** `Info.plist` declares the `.ode`
  document type and its UTI, so double-clicking a model or dropping it on
  the Dock icon opens it. SDL delivers both as drop events, which the app
  handles. *Needs a person to try it.*
- **⌘ shortcuts.** ImGui swaps Ctrl and ⌘ on macOS, and the menus show ⌘O,
  ⌘S, ⌘N and ⌘Q. SDL also gives the app the standard application menu
  (About, Hide, Quit) and Window menu.
- **The system's light or dark appearance** is followed on first launch
  (`SDL_GetSystemTheme`), as the screenshot above shows.

Still to do:

- ⌘W (close the focused window) and ⌘, (settings).
- Trackpad pinch and two-finger scroll in the plots.
- **Retina:** the app renders at full pixel density. Check on a Retina
  screen that fonts and plot lines are crisp.
- **Optional:** put File, Run and View in the Mac menu bar as well as in
  the window, with a small Objective-C file.

**Done when:** a Mac user can install from the `.dmg` and use the app
without reading anything Mac-specific.

## Phase 3: signing, notarization, distribution

Without this, macOS blocks the app on first launch. With it, the app opens
like any other.

1. Join the Apple Developer Program ($99 per year) and create a
   *Developer ID Application* certificate.
2. Sign with the hardened runtime:
   `codesign --options runtime --timestamp --entitlements xppstudio.entitlements --sign "Developer ID Application: ..."`.
   The private engine copies keep their signature when copied, so
   library validation should accept them. If not, the entitlement
   `com.apple.security.cs.disable-library-validation` is the fallback.
3. Notarize and staple: `xcrun notarytool submit XPPStudio.dmg --wait`,
   then `xcrun stapler staple XPPStudio.dmg`.
4. Do all of this in the release workflow, with the certificate and a
   notarytool API key stored as repository secrets.
5. Once the project is public, add a Homebrew cask, so that
   `brew install --cask xppstudio` works.

Until there is a developer account, preview builds are signed ad hoc.
Testers then open the app the first time with right-click > *Open*, and
TESTING.md explains this.

## Phase 4: Python on the Mac

The `xppstudio` package builds a universal2 wheel with cibuildwheel; this
is already configured. Then:

- test it on Apple Silicon and on Intel;
- publish it with the first release, so that `pip install xppstudio` works
  on any Mac with AUTO included.

## Phase 5: a test round with Mac users

- Aim for testers on Apple Silicon (M1 to M4) and on Intel Macs, ideally
  covering a few macOS versions.
- Give them the checklist in TESTING.md, plus Mac-specific checks:
  - the first-launch dialog;
  - Retina rendering;
  - trackpad zoom and pan in plots;
  - ⌘ shortcuts;
  - opening `.ode` files from Finder;
  - light and dark appearance.
- Collect results with the issue templates.

## What is needed from people

- **One or two Mac testers.** CI can build, run the tests and take a
  screenshot, but only a person can judge whether the app feels right on a
  Mac.
- **A decision on the Apple Developer account** for notarized downloads.
