Review a change that another agent made for XPP Lab issue #$number:
$title

$body

The change, as a diff against $base:

```diff
$diff
```

Review it as a careful maintainer would. Read the surrounding code as
needed, but do not change anything. Look for:

- bugs and unhandled cases;
- anything that could change XPPAUT's numbers (the golden tests must stay
  bit-identical);
- edits in `engine/xppaut/` without an `XPP Lab fix:` comment, or a fix
  without a row in `docs/upstream-issues.md`;
- missing or weak tests, and documentation the change makes wrong;
- anything that does not do what the issue asks.

Report real problems only, not matters of taste. Finish with a JSON block:

```json
{"verdict": "approve", "findings": [{"severity": "high", "file": "path", "line": 1, "problem": "...", "fix": "..."}]}
```

with `"verdict": "changes"` if any finding is high or medium, and an empty
`findings` list if there is nothing to report.
