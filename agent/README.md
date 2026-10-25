# The XPP Lab maintainer agent

A program that works on XPP Lab around the clock. It takes GitHub issues
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
- **Proposals:** once a week, and once a day while it has nothing to do,
  a read-only run reads `docs/vision.md`, `ROADMAP.md`, the open issues
  and recent merges, and drafts up to three pieces of work. Each becomes
  an issue labelled `agent:proposal`, in a milestone, saying why, what
  and when it is done. Approve one in the dashboard and it gets the
  `agent` label, so the pipeline takes it; decline it with a reason, and
  the next run reads the reason. At most five wait for you at once. A big
  feature is proposed as a design note (`docs/design/<name>.md`) first.
  `python -m xppagent propose --dry-run` shows what it would propose.
- **Explanations:** comment `/explain` on any issue or pull request,
  with a question after it or nothing for a short brief (the problem, the
  plan, the risks, and whether to accept it). A read-only run reads the
  thread and the code, and the agent answers in a comment within minutes.
  It works on a thread of its own, so a long task does not hold it up;
  quiet hours do not apply, and at most 10 answers are given per usage
  window (`explain_per_window`). The dashboard's **Explain…** buttons
  post the comment for you.
- **The daily log:** every morning it comments on the issue "Maintainer
  agent log" with what it did, what is in progress, and how many Claude
  runs it used.
- **Pause it:** add the `agent:paused` label to the log issue (from your
  phone, if you like); remove it to resume. On the workstation, creating
  the file `C:\xpps-agent\STOP` has the same effect.

Only your comments reach Claude. Comments by other people (testers, for
instance) are not shown to it; quote them in a comment of yours if they
matter. The one exception is an explanation: that run cannot change
anything, and it reads the whole thread, told that only your words are
instructions.

No Claude run may read the secrets file, or the sign-ins of Claude Code
and gh. Nothing that looks like a token is ever pushed or posted: the
supervisor checks every change, including its unpushed commits, and every
comment, before they leave the workstation.

## How it stays within your Max subscription

Claude Code runs under your Max subscription, which has usage limits per
five-hour window and per week, shared with your own use. The agent:

- makes at most `runs_per_window` Claude runs (default 16) in any
  `window_hours` (default 5);
- leaves you the rest of the subscription: every run reports how much of
  the five-hour window and of the week is used (by you and the agent
  together), and above `max_five_hour_percent` (default 60) or
  `max_week_percent` (default 75) the agent starts no run until that
  window resets. The dashboard and the daily log show the last reading;
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

## The agent's own GitHub account

The agent works best under a GitHub account of its own, a "machine user".
GitHub allows each person one free machine account besides their own. Its
commits, comments and pull requests then show under its name and picture,
and it appears in the repository's collaborators and contributors next to
you. When you merge its work, the history shows you and the bot. It also
keeps your own token off the workstation.

1. **Create the account.** Sign out of GitHub, or use a private browser
   window, and sign up with the bot's name as the username (for example
   `limit-cyclist`). Use an email address of your own with a `+` tag, such
   as `you+bot@gmail.com`; Gmail delivers it to you. Give it a display name
   and a bio saying what it is: "XPP Lab's maintainer agent (Claude Code).
   Works on issues labelled agent; César merges."
2. **Keep its email private.** In the bot's Settings > Emails, tick "Keep my
   email addresses private". Its commits then use the address
   `ID+NAME@users.noreply.github.com` shown there.
3. **Invite it.** As yourself, in the repository's Settings > Collaborators,
   add the bot with the Write role. As the bot, accept the invitation (from
   its email, or on the repository's page).
4. **Make its token.** As the bot, go to Settings > Developer settings >
   Personal access tokens > Tokens (classic), and generate a token with only
   the `repo` scope. A fine-grained token cannot reach a repository owned by
   another person. Because the account can reach nothing else, `repo`
   covers only this repository. Leave `workflow` unticked, so the agent
   cannot change CI. Set an expiry date and a reminder to renew it.
5. **Tell the agent.** Put the token in `secrets.env` as `GH_TOKEN`, and set
   the bot's name and noreply address in `config.toml` (`git_name`,
   `git_email`). `owner` stays your account: the agent still takes work
   only from you.

With a ruleset on `main` that requires one approving review, nothing the
bot opens can be merged until you approve it.

## Installing it on the workstation

Do these steps signed in to the workstation as an administrator (Remote
Desktop is fine). Allow about an hour.

### 1. Prerequisites

In an elevated PowerShell:

```powershell
winget install -e --id Git.Git --scope machine
winget install -e --id GitHub.cli --scope machine
winget install -e --id Python.Python.3.14 --scope machine
winget install -e --id MSYS2.MSYS2
```

Then open "MSYS2 UCRT64" from the Start menu and run:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-pytest mingw-w64-ucrt-x86_64-python-numpy mingw-w64-ucrt-x86_64-python-matplotlib mingw-w64-ucrt-x86_64-python-pandas mingw-w64-ucrt-x86_64-sdl3
```

### 2. A checkout to install from

In a new PowerShell (so the new programs are on the PATH):

```powershell
gh auth login
gh repo clone CesarPerdomoDiaz/xpplab C:\src\xpplab
```

This checkout is only used to install and update the agent; the agent
keeps its own clone.

### 3. Run the installer

In an elevated PowerShell:

```powershell
cd C:\src\xpplab
powershell -ExecutionPolicy Bypass -File agent\install\install-agent.ps1
```

It checks the prerequisites and creates the local account `xpps-agent`.
Choose a long password for it when asked, and type it again when the
installer asks for it to register the scheduled task. It also creates
`C:\xpps-agent` with `config.toml`, an empty `secrets.env` and the
scheduled task "XPP Studio maintainer agent". It does not start the task.
(The scheduled tasks keep the name the project had before it became XPP
Lab: renaming a task means typing the account's password again.)

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

**GitHub:** the bot's classic token (see "The agent's own GitHub account"
above). Without a bot account, make a token of your own instead: on
github.com, go to Settings, Developer settings, Personal access tokens,
Fine-grained tokens, and choose "Generate new token":

- Repository access: only `CesarPerdomoDiaz/xpplab`.
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
| Update the agent | `agent\install\update-agent.ps1`, as administrator. It pulls `C:\src\xpplab`, lets the agent finish its step, copies the new code and restarts it, keeping the settings, secrets and state |
| Remove it | `agent\install\uninstall-agent.ps1` (add `-Purge` to delete its folder and account too) |

### Looking after it from another computer

`agent\install\enable-ssh.ps1 -PublicKey "ssh-ed25519 ..."`, run once as
administrator, lets one other computer log in over SSH, but only through
Tailscale and only with that key. Password logins are turned off, and port
22 is closed to everything outside the tailnet. From that computer, updates,
logs and restarts then need no Remote Desktop:

```
ssh you@<workstation> "powershell -File C:\src\xpplab\agent\install\update-agent.ps1"
```

## The dashboard

`python -m xppagent web` serves "mission control", a web page for
supervising the bot from any of your devices. It shows:

- **The path:** the roadmap as a graph, to check that the work stays on
  course. Each milestone on `ROADMAP.md`'s "Path:" line is a hub, in order;
  the one in brackets there is marked "you are here", earlier ones with
  open items are marked as passed with items left, and the other
  milestones hang below as side tracks. Each hub's items circle it, done
  ones filled, and the merges that ticked them sit just outside. Click any
  node for its details; drag and scroll to move and zoom. Below it, the
  last merges in order: green moved the path, amber a side track, grey
  ticked nothing (fixes, upkeep or detours). It reads the roadmap's history
  from GitHub, each commit's version once.
- **Right now:** the Claude run in progress, step by step (the files it
  reads and edits, the commands it runs, the errors it hits) as they
  happen, and otherwise what the bot is waiting for.
- **Needs you:**
  - pull requests ready to merge, with their summary, CI checks and the
    agent review;
  - the bot's questions;
  - its proposals;
  - issues opened by other people, which the bot leaves alone until you
    approve them.

  Each one can be handled with a single click: merge, ask for changes,
  answer, close, approve or decline. Each one, and each task on the
  board, also has **Explain…**: ask the bot what it is about (or leave
  the question empty for a short brief), and its answer appears there.
- **Work:** the pipeline with a count at each stage, and a card for each
  task.
- **Recent Claude runs:** each can be opened, to see what it did.
- **The roadmap**, from `ROADMAP.md`; **what happened**; **open pull
  requests**; **Ask the bot**, which opens an issue for it; and **Pause**.

It listens on 127.0.0.1 only. `tailscale serve` makes it reachable over
HTTPS from your own devices on Tailscale, and nothing else can reach it.
A password guards it on top. It runs as its own Windows account,
`xpps-web`, which may read the agent's state and logs but not the agent's
secrets. It acts on GitHub with **your own** token, kept where neither
the agent's account nor its Claude runs can read it, so merges and
comments from the dashboard are yours.

### Installing it on the workstation

1. **Turn on HTTPS for your tailnet:** in the Tailscale admin console, under
   DNS, turn on MagicDNS and HTTPS Certificates.
2. **Make your own token:** on github.com, as yourself, go to Settings,
   Developer settings, Fine-grained tokens:
   - Repository access: only `CesarPerdomoDiaz/xpplab`.
   - Permissions: Contents read and write (for merging); Issues read and
     write; Pull requests read and write; Commit statuses read; Actions
     read.
   - Expiration: up to a year.
3. **Install it.** In an elevated PowerShell:
   ```powershell
   cd C:\src\xpplab
   git pull
   powershell -ExecutionPolicy Bypass -File agent\install\install-web.ps1
   ```
   It creates the `xpps-web` account (type a password for it twice), asks
   you to choose the dashboard's password, and registers the task. It
   then prints the page's address, `https://<workstation>.<your
   tailnet>.ts.net`.
4. **Add the token.** Put your token in `C:\xpps-web\secrets.env` as
   `GH_TOKEN=...`, then start the task with
   `Start-ScheduledTask -TaskName "XPP Studio dashboard"`.

To try it elsewhere, read-only and without a password:
`XPPS_DASHBOARD_TOKEN=... python -m xppagent web --config my.toml --read-only --no-login`,
then open http://127.0.0.1:8765.

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
