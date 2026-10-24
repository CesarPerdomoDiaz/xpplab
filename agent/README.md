# The XPP Studio maintainer agent

A program that works on XPP Studio around the clock. It takes GitHub issues
that you label `agent`, plans each one on the issue, makes the change with
Claude Code in its own git worktree, runs the build and all the tests, opens
a draft pull request, has a second Claude run review it, fixes what the
review and CI find, and then asks you to review. **It never merges:** you
do. It follows the design of Bart van der Lint's agents
(https://bart.vandelint.net/agents/).

## How you use it

- **Give it work:** open an issue (or pick one) and add the `agent` label.
  Only labels added by you count; issues labelled by anyone else are
  ignored. Add `agent:priority` as well to put it first.
- **Steer it:** it posts a plan on the issue before it starts. Comment on
  the issue to change course. When it is stuck it asks a question on the
  issue (label `agent:blocked`); your reply sets it going again.
- **Review:** when CI is green it marks the pull request ready and mentions
  you. Comment on the pull request and it makes the changes. Merge it when
  you are happy, or close it to drop the work.
- **After a merge** it watches CI on `main` for 12 hours and opens a
  priority issue if `main` breaks.
- **Scheduled work:** `config.toml` lists recurring jobs (a weekly check of
  third-party versions, a weekly check of one manual page against the app,
  a fortnightly check of models that fail to load). Each opens an issue
  that the agent then works on like any other.
- **The daily log:** every morning it comments on the issue "Maintainer
  agent log" with what it did, what is in progress, and how many Claude
  runs it used.
- **Pause it:** add the `agent:paused` label to the log issue (from your
  phone, if you like); remove it to resume. On the workstation, creating
  the file `C:\xpps-agent\STOP` has the same effect.

Only your comments reach Claude. Comments by other people (testers, for
instance) are not shown to it; quote them in a comment of yours if they
matter.

## How it stays within your Max subscription

Claude Code runs under your Max subscription, which has usage limits per
five-hour window, shared with your own use. The agent:

- makes at most `runs_per_window` Claude runs (default 8) in any
  `window_hours` (default 5);
- makes none during `quiet_hours`, if you set them (for example the hours
  you work);
- stops until the reset time when Claude reports a usage limit, and then
  repeats the interrupted step;
- stops and comments on the log issue if Claude is signed out.

A typical issue takes 3 to 6 runs: a plan, the change, a review, and a fix
or two.

## Safety

- The agent runs as its own standard Windows account (not an
  administrator), in a folder only that account and administrators can
  read.
- Claude Code runs without permission prompts
  (`--dangerously-skip-permissions`), but only inside the agent's worktree
  and as that account. The planning and review runs can only read.
- The GitHub token is not passed to Claude's runs or to the tests: only the
  supervisor pushes and talks to GitHub. (Claude's runs could still read
  `secrets.env`, since they run as the same account: keep the token's
  permissions as narrow as below.)
- The agent never merges, never pushes to `main`, and cannot change
  `.github/workflows` (the token below has no Workflows permission).
- Recommended: protect `main` with a ruleset (repository Settings, Rules,
  Rulesets: target `main`, "Restrict deletions", "Block force pushes",
  "Require a pull request before merging"). Nothing then reaches `main`
  without a pull request.

For the strongest separation, give the agent its own GitHub account (a
"machine user"): invite it to the repository with Write access and use its
token. Its comments and pull requests then show as its own, and with a
ruleset that requires one approval, nothing it opens can be merged until
you approve it. In `config.toml`, `owner` stays your account.

## Installing it on the workstation

Do these steps signed in to the workstation as an administrator (Remote
Desktop is fine). Allow about an hour.

### 1. Prerequisites

In an elevated PowerShell:

```powershell
winget install -e --id Git.Git --scope machine
winget install -e --id GitHub.cli --scope machine
winget install -e --id Python.Python.3.12 --scope machine
winget install -e --id MSYS2.MSYS2
```

Then open "MSYS2 UCRT64" from the Start menu and run:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-pytest mingw-w64-ucrt-x86_64-python-numpy mingw-w64-ucrt-x86_64-sdl3
```

### 2. A checkout to install from

In a new PowerShell (so the new programs are on the PATH):

```powershell
gh auth login
gh repo clone CesarPerdomoDiaz/xppstudio C:\src\xppstudio
```

This checkout is only used to install and update the agent; the agent
keeps its own clone.

### 3. Run the installer

In an elevated PowerShell:

```powershell
cd C:\src\xppstudio
powershell -ExecutionPolicy Bypass -File agent\install\install-agent.ps1
```

It checks the prerequisites and creates the local account `xpps-agent`.
Choose a long password for it when asked, and type it again when the
installer asks for it to register the scheduled task. It also creates
`C:\xpps-agent` with `config.toml`, an empty `secrets.env` and the
scheduled task "XPP Studio maintainer agent". It does not start the task.

### 4. Install Claude Code for the agent's account

```powershell
runas /user:xpps-agent powershell
```

In the window that opens (it runs as `xpps-agent`):

```powershell
irm https://claude.ai/install.ps1 | iex
```

This puts `claude.exe` in `C:\Users\xpps-agent\.local\bin`, which is the
path `config.toml` already uses.

### 5. The two tokens

**Claude:** in any PowerShell, run `claude setup-token` (with Claude Code
installed for your own account, or using
`C:\Users\xpps-agent\.local\bin\claude.exe`). It opens the browser. Sign in
with the account that has the Max subscription. It then prints a token
that lasts a year. You can reuse a token you made on another computer.

**GitHub:** on github.com, go to Settings, Developer settings, Personal
access tokens, Fine-grained tokens, and choose "Generate new token":

- Repository access: only `CesarPerdomoDiaz/xppstudio`.
- Permissions: Contents read and write; Issues read and write; Pull
  requests read and write; Actions read; Commit statuses read (Metadata
  read is added automatically). Leave Workflows and Administration off.
- Expiration: up to a year. Put a reminder in your calendar to renew it.

Open the secrets file as administrator:

```powershell
notepad C:\xpps-agent\secrets.env
```

Fill in both lines and save:

```
GH_TOKEN=github_pat_...
CLAUDE_CODE_OAUTH_TOKEN=sk-ant-oat01-...
```

### 6. Check everything

Run the doctor as the agent's account. It also makes one small Claude run.

```powershell
runas /user:xpps-agent "cmd /k cd /d C:\xpps-agent\app && \"C:\Program Files\Python312\python.exe\" -m xppagent doctor --live --config C:\xpps-agent\config.toml"
```

Every line should say `ok`. Fix anything that says `FAIL`, then run it
again.

### 7. Start it

```powershell
Start-ScheduledTask -TaskName "XPP Studio maintainer agent"
Get-Content C:\xpps-agent\agent.log -Tail 20 -Wait
```

On its first pass it creates the `agent` labels and the log issue. From now
on it starts by itself when the workstation boots, and restarts within five
minutes if it stops.

### 8. A first issue

Open an issue with a small, clear task (for example, a sentence to add to
a page of the manual), label it `agent`, and watch it: a plan comment
within a minute, then a draft pull request.

## Running it

| What | How |
| --- | --- |
| What is it doing? | `python -m xppagent status --config C:\xpps-agent\config.toml` (from `C:\xpps-agent\app`), or the log issue |
| Its log | `C:\xpps-agent\agent.log` |
| Each Claude run, in full | `C:\xpps-agent\runs\` |
| Pause, resume | the `agent:paused` label on the log issue, or the file `C:\xpps-agent\STOP` |
| Stop, start | `Stop-ScheduledTask` / `Start-ScheduledTask -TaskName "XPP Studio maintainer agent"` |
| Change settings | edit `C:\xpps-agent\config.toml`, then stop and start the task |
| Update the agent | `git pull` in `C:\src\xppstudio`, then run the installer again (it keeps the settings, secrets and state) |
| Remove it | `agent\install\uninstall-agent.ps1` (add `-Purge` to delete its folder and account too) |

## Trying it on another computer

```powershell
cd agent
python -m pytest -q tests                       # the loop, offline, with fakes
python -m xppagent doctor --config my.toml      # what is missing
python -m xppagent once --dry-run --config my.toml
```

`--dry-run` reads GitHub and does the local work, but writes nothing to
GitHub, pushes nothing and keeps no state.

## How it works

`xppagent/supervisor.py` is the loop. Each pass takes in newly labelled
issues, moves every task on by at most one stage
(`xppagent/pipeline.py`), opens scheduled issues, and posts the daily log.
The stages are:

```
queued → planned → pr → ci → owner → merged → closed
              ↘ blocked (a question on the issue; your reply resumes it)
```

| Stage | What happens |
| --- | --- |
| queued | A read-only Claude run writes a plan, which is posted on the issue. |
| planned | Claude makes the change in a new worktree and commits it. The supervisor runs the checks in `config.toml` and gives failures back to the same Claude session, up to three times. Then it pushes and opens a draft pull request. |
| pr | A fresh, read-only Claude run reviews the diff. High and medium findings go back to the implementer, followed by the checks and another review, up to two rounds. |
| ci | It waits for GitHub Actions. Failures, with the failing log, go back to Claude, up to three rounds. When CI passes it marks the pull request ready and mentions you. |
| owner | It waits for you. Comments go back to Claude. Merge conflicts with `main` are merged in and resolved. A merge moves the task on; closing the pull request drops it. |
| merged | It watches CI on `main` for 12 hours. |

The prompts are in `prompts/`. `prompts/system.md` holds the rules every
run follows, on top of the repository's `CLAUDE.md`.

The state, which survives restarts, is kept in `C:\xpps-agent\state.json`.

CI runs only once per push to an agent branch (for its pull request), and
the agent's pull requests skip the macOS job, whose minutes cost ten times
as much on a private repository. macOS runs again after the merge, and the
post-merge watch catches what it finds.
