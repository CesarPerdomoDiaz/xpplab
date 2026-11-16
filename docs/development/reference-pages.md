# Writing a reference page

Every command and action has one page in `docs/reference/`, opened in the
app by `doc NAME`. The pages follow the shape of MATLAB's reference pages,
which is the standard for this project's documentation: a reader who knows
one page knows where to look on all of them.

`docs/reference/auto-steady.md` is the model to copy.

## The file

- The page of the command `auto steady` is `docs/reference/auto-steady.md`:
  the command's name with dashes for spaces.
- No name may also be a file of `docs/manual/`: the app finds a page by its
  file name.
- Add the page to the index (`docs/reference/reference.md`) and to
  `mkdocs.yml`, and take the command out of `tests/doc_pending.txt`.

## The sections

A page starts with `# name` and one sentence that says what the command
does. Then these sections, with these headings, in this order.
`tests/doc_check.py` fails on a page that leaves one out.

1. **Syntax.** Every form of the command in one code block, then the button
   or menu that does the same, and the Python call.
2. **Description.** One paragraph for each form: what it does and when to
   use it. Say where it starts from, what it draws and when it stops.
3. **Examples.** Each under a `###` heading that says what it shows. A
   command with settings or with more than one use has at least two: the
   plain use, and one that shows a setting or a difficulty. A command that
   does one thing (`auto reset`) may have one. Each example has:
   - a shipped model, or a model short enough to type;
   - links that run it in the app: `[Open the model](example:lecar.ode)` and
     `[Follow the steady states](cmd:auto steady iapp)`;
   - the commands in a code block, and the same in Python;
   - what comes out: the numbers in a table, and a figure where there is one;
   - "How to read it": what the result means, in the terms of the model.
4. **Settings.** A table: each argument and each setting that changes the
   result, its name in the window and in the command, what it means, and
   advice on choosing it.
5. **Results.** What appears and where, what the columns, labels and colours
   mean, what Python returns, and how to export it.
6. **More about.** The mathematics a reader needs to understand the result,
   with each term defined. A student should not need another book to follow
   the page.
7. **Tips.** What to do first, how to choose the settings, common mistakes.
8. **Algorithms.** What XPPAUT and AUTO actually do: the method, the
   tolerances and their defaults. Only what the code does; check it there.
9. **When it fails.** A table: each message the command can give, why, and
   what to do.
10. **References.** Textbooks only.
11. **See also.** The related reference pages, and the chapter and tutorial
    that use the command.
12. **Version history.** "Introduced in 0.1.x", and what changed since.

## The rules

- **Every number is computed.** `python/tests/test_reference.py` runs each
  page's Python examples as written and compares the tables with the
  results. A page whose numbers are typed by hand and not tested is not
  finished.
- **Every command shown is run.** The commands in the examples are run
  through `xpplab-batch` by a test, as the tutorials' are.
- **Only what is true of the code.** A claim about an algorithm, a default
  or a message is checked in the source before it is written.
- **Plain, direct sentences.** No marketing language. Define a term the
  first time it appears.
- **The app shows the page too.** Use only what the Help window draws:
  headings, paragraphs, lists, tables, code blocks, links, bold and code,
  and mathematics: `$...$` in a line, and `$$...$$` on lines of its own. The
  Help window typesets formulas with the Figure window's mathtext, a
  subset of LaTeX (Greek letters, sub- and superscripts, `\frac`, `\sqrt`,
  `\int`, `\sum`, `\partial`, accents such as `\dot` and `\hat`,
  `\left( \right)`, `\mathrm` and `\text`); `tests/help_math_check.c`
  checks that every formula on these pages is one it can typeset.
