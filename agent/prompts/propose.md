You are choosing what XPP Lab's maintainer agent should propose next.
Read the code as much as you need; change nothing. This run's answer is
the JSON block described at the end, not the usual status block.

The project's vision (docs/vision.md):

$vision

The roadmap (ROADMAP.md). Its unchecked items are candidates, but some
checkboxes are out of date: check the code before you trust one.

$roadmap

Open issues and proposals (do not propose these again):

$open

Merged lately:

$merged

Proposals the maintainer declined, with his reasons. Learn from them, and
do not propose them again:

$declined

Propose $count pieces of work for the maintainer to approve. Each one:

- serves the vision and fits one of these milestones: $milestones;
- is one pull request of work that an agent can do and test alone in a few
  hours. A large feature becomes a design note first: title it "Design: ...",
  and it is done when `docs/design/<name>.md` is merged;
- keeps XPPAUT's numbers bit-identical and can be checked by tests;
- is concrete: names the files or areas, and says what done means.

Prefer, in this order: things that are wrong or broken (code, docs, or a
roadmap that no longer matches the code), then what new users meet first
(installing, the first run, the manual), then new features. Mix the themes
when you can, and look at the code before you propose, so that each proposal
is accurate about what exists and what is missing.

Finish with exactly this JSON block, with $count entries:

```json
{"proposals": [{"title": "a short imperative title", "milestone": "M5 Documentation and examples", "theme": "Learn by doing", "why": "why it matters, in one or two sentences", "what": "what to do, naming files or areas", "done_when": "how anyone can tell it is done", "size": "S, M or L"}]}
```
