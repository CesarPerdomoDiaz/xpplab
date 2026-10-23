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
  model gives the same output with both, runs the Python tests, and starts
  the app to take a screenshot.

**None of this has run on a Mac yet.** The first CI run will show what
needs fixing.

## Phase 1: build and pass the tests on macOS

- Fix whatever Apple Clang rejects in the 1990s C. Clang is stricter than
  GCC about implicit declarations, and the flags for that are already set.
- Check the parts that differ per OS: loading private engine copies with
  `dlopen` from `$TMPDIR`, `realpath`, `mkdtemp`, and AUTO's files in the
  per-model work folder.
- Check that the numbers match unmodified XPPAUT on the same Mac. Apple
  Clang fuses multiply-adds by default; the build turns that off
  (`-ffp-contract=off`) so results match the reference build.
- Pass the Python tests on macOS.

**Done when:** CI is green on macOS.

## Phase 2: a real Mac app

- **Universal binary** (arm64 and x86_64) with `CMAKE_OSX_ARCHITECTURES`.
  Homebrew's SDL3 is single-architecture, so release builds compile SDL3
  from source as a universal static library (`XPPS_FETCH_SDL3`). That also
  removes the need to bundle a dylib.
- **Deployment target macOS 11**, the first version for Apple Silicon.
- **Open `.ode` files from Finder.** Declare the `.ode` document type and
  UTI in `Info.plist`, so double-clicking a model or dropping it on the Dock
  icon opens it. SDL delivers both as drop events, which the app already
  handles.
- **Mac conventions:**
  - ⌘ instead of Ctrl. ImGui swaps them on macOS; the menus should display
    ⌘O, ⌘S, ⌘N.
  - ⌘Q, ⌘W and ⌘, (settings).
  - Trackpad pinch and two-finger scroll in the plots.
- **Follow the system's light or dark appearance** on first launch
  (`SDL_GetSystemTheme`).
- **Retina:** the app already renders at full pixel density. Check that
  fonts and plot lines are crisp.
- **Optional:** a small Objective-C file for a native application menu
  (About, Hide, Quit) alongside the in-window menus.

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
