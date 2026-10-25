# Working with the maintainer agent

XPP Lab has a maintainer agent, The Limit Cyclist (the GitHub account
[`limit-cyclist`](https://github.com/limit-cyclist)). It works on issues
that the maintainer gives it: it plans a change, makes it, tests it and
opens a pull request. The maintainer reviews every pull request and merges
it. The agent never merges, and never pushes to `main`.

This page says how to call it, what it does with an issue, and how to talk
to it on a pull request.

## Who it listens to

Only the maintainer. It takes an issue only when the maintainer gives it
one (with a label or a mention, below), and the only instructions it
follows are the maintainer's. Comments by anyone else are read as
information, never as instructions.

If you would like the agent to work on something, open an issue as usual.
The maintainer may hand it on.

## Giving it an issue

The maintainer has two ways to do this.

- **The `agent` label.** Add it to the issue. Add `agent:priority` as well
  to have it start on that issue first.
- **A mention.** Comment on the issue with `@limit-cyclist` and what you
  want, in plain words:

  ```
  @limit-cyclist please fix this. The plot freezes only after a resize,
  so start with the resize handler.
  ```

  It puts 👀 on the comment, answers "On it", and adds the `agent` label.
  The comment becomes part of the request, together with the issue's text.

## What happens next

1. **A plan.** It posts a plan on the issue: what it understood, where the
   change goes, how it will test it, and what is unclear. Comment on the
   issue to change course before the work is far along.
2. **The change.** It makes the change on a branch named
   `agent/<issue>-<title>`, builds XPP Lab, runs all the tests (the golden
   tests included: the numbers must not change), and opens a draft pull
   request.
3. **A second review.** A separate review reads the change, and the
   problems it finds go back to be fixed. Its findings are posted on the
   pull request under "Agent review".
4. **CI.** When CI fails, the log goes back to be fixed.
5. **Ready.** When the review and CI pass, it marks the pull request ready
   and mentions the maintainer.
6. **The maintainer's review.** The maintainer comments, which the agent
   answers (next section), and then merges the pull request or closes it.

After a merge it keeps an eye on CI on `main`. If `main` breaks, it opens
an issue for itself, first in line, to fix the break or revert the merge.

## Asking for changes on its pull request

Comment on the pull request with what to change, in plain words. Several
requests in one comment are fine. On its own pull requests there is no
need to mention it.

- It puts 👀 on the comment as soon as it reads it.
- Its reply starts with the comment quoted, then a checklist with one line
  per request: ✅ done, 🟡 partly, ❌ not done (and why), 💬 answered (for a
  question). Then it says what it changed, how it checked it, and which
  commits it added.
- The update gets a fresh review, which checks it against the comment, and
  then CI. Only then does it ask for a review again ("ready for your review
  again"). It does not say "ready" while a comment of the maintainer's is
  unanswered: a comment written while CI runs is answered first.
- When a comment corrects something it would get wrong again (for example
  "check a change to the app in the app, with a screenshot"), it keeps that
  as a rule and follows it in later work.

## When it is stuck

It asks a question on the issue, starting "I need your help", and labels
the issue `agent:blocked`. A reply in a comment sets it going again.

## Questions and second opinions

None of these changes anything. Each is answered in a comment, usually
within minutes.

| Comment | Where | What you get |
| --- | --- | --- |
| `/explain` or `@limit-cyclist explain` | any issue or pull request | A short brief: what it is about, what is wrong or missing, what the change does, the risks, and whether to accept it |
| `/explain <question>` or `@limit-cyclist explain <question>` | any issue or pull request | An answer to the question, after reading the thread and the code |
| `@limit-cyclist review` | a pull request that is not its own | A second-opinion review, with its findings by severity. Add what to look at, e.g. `@limit-cyclist review, especially the caching` |
| `@limit-cyclist <question>` | a pull request that is not its own | An answer, as with `/explain` |

## Proposals

It also suggests work toward the [vision](../vision.md) and the
[roadmap](https://github.com/CesarPerdomoDiaz/xpplab/blob/main/ROADMAP.md):
issues labelled `agent:proposal`, each in a milestone, saying why, what,
and what "done" means. The maintainer approves one by giving it to the
agent (the `agent` label, or a mention), or declines it by closing it with
a reason, which the agent reads before it proposes again.

## Labels

| Label | Meaning |
| --- | --- |
| `agent` | The agent works on this issue (given by the maintainer) |
| `agent:priority` | Start on it first |
| `agent:working` | The agent is working on it |
| `agent:blocked` | The agent asked a question and waits for the answer |
| `agent:proposal` | Work the agent suggests; not started until approved |

## How to recognise its work

Its commits and pull requests are made by `limit-cyclist`. Its comments
mostly start with 🤖 (plans, replies, questions) or 🔍 (reviews). It posts a
summary of its work every day on the issue "Maintainer agent log".
