# Security

## Reporting a problem

Please report security problems privately, not in a public issue: use
**Report a vulnerability** on the
[Security tab](https://github.com/CesarPerdomoDiaz/xppstudio/security/advisories/new).
Only the maintainer sees the report. You will get an answer within a week,
and credit in the fix if you want it.

Examples of what to report:

- a model file (`.ode`, `.ani`, `.set`) that makes the app or the engine
  read or write files it should not, or crash in a way that looks
  exploitable;
- a release download or a Python wheel that does not match its published
  checksum;
- the maintainer agent (`agent/`) acting on someone other than the
  maintainer, or exposing a secret.

## Opening models from others

As in XPPAUT, a model file can name a compiled library for the engine to
load (the `dll_lib` and `dll_fun` options), and that library runs as a
program does. Treat a model from someone you do not know like any program
from them, and look for `dll_lib` in it before you open it.

## Supported versions

Fixes go into the next release. Please check that a problem still happens
with the [newest one](https://github.com/CesarPerdomoDiaz/xppstudio/releases/latest).
