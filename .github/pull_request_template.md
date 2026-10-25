<!-- Thank you! A few lines here make the review quick. -->

**What this changes, and why:**

**How you tested it:**

- [ ] `python tests/golden.py check --exe build/cli/xppaut-headless` still reports `{"nooutput": 10, "ok": 187}`: XPPAUT's numbers do not change
- [ ] `ctest --test-dir build` and `PYTHONPATH=python/src python -m pytest python/tests` pass
- [ ] Any change in `engine/xppaut/` is minimal, marked with an `XPP Lab` comment, and listed in `docs/upstream-issues.md`
- [ ] The manual (`docs/manual/`) or the Python guide says what changed, if users will notice it
