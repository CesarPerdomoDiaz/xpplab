# Contributing to XPP Studio

Thank you for helping! Bug reports, models that do not work, ideas and code
are all welcome. For reports, please use the
[issue templates](https://github.com/CesarPerdomoDiaz/xppstudio/issues/new/choose).

## Build and test

See "Building from source" in the README. Then:

```bash
ctest --test-dir build                        # engine, API and AUTO tests
PYTHONPATH=python/src python -m pytest python/tests
```

## The one rule: XPPAUT's numbers do not change

`engine/xppaut/` is XPPAUT 8.0's source, imported unmodified in the first
commit. `tests/golden.py` checks every example model bit for bit against it.

- New features go in `engine/api/`, `engine/platform/`, `app/` and
  `python/`, never in a rewrite of XPPAUT's numerics.
- If XPPAUT's code must change (a genuine bug, or a guard for embedding),
  keep the change minimal and mark it with an `XPP Studio` comment. Bug fixes
  also get an entry in `docs/upstream-issues.md`.
- If a change alters the golden output on purpose, explain why in the
  commit, and re-record with `python tests/golden.py record`.

## Code style

C99, two-space indentation, and comments that explain why. Follow the code
around you. The app is C on top of cimgui, so keep C++ out of it, apart
from `app/src/gui_backend.cpp`.

## The maintainer agent

Some issues are worked on by the project's maintainer agent, The Limit
Cyclist (the GitHub account `limit-cyclist`). It plans, builds and tests a
change and opens a pull request, which a person reviews and merges. It takes
work only from the maintainer: it acts on an issue only when the maintainer
has given it the `agent` label, and it does not follow instructions in other
people's issues, comments or pull requests. Its comments are marked as its
own. See [agent/README.md](agent/README.md) for how it works.

Your issues and pull requests are read and answered by the maintainer, as
in any project.

## Documentation

The manual lives in `docs/manual/*.md`. It is embedded in the app and
published on the website. Links of the form `[text](example:model.ode)` and
`[text](cmd:run)` open a model or run a command inside the app.
