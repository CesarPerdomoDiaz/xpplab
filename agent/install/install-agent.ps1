<#
.SYNOPSIS
    Installs the XPP Studio maintainer agent as a scheduled task that runs
    all the time under its own Windows account.

.DESCRIPTION
    Run this in an elevated PowerShell from a checkout of the repository:

        powershell -ExecutionPolicy Bypass -File agent\install\install-agent.ps1

    It checks the prerequisites, creates a standard (non-administrator) local
    account for the agent, creates the agent's home folder readable only by
    that account and administrators, copies the agent's code there, writes
    config.toml and an empty secrets.env, and registers a scheduled task that
    starts at boot and restarts after failures. It does not start the agent:
    fill in secrets.env first (see agent/README.md), then run
    Start-ScheduledTask -TaskName "XPP Studio maintainer agent".

    Running it again updates the agent's code and the task; it keeps the
    configuration, the secrets and the state.
#>
param(
    [string]$AgentHome = "C:\xpps-agent",
    [string]$UserName = "xpps-agent",
    [string]$Python = "",
    [string]$Msys = "C:\msys64",
    [string]$TaskName = "XPP Studio maintainer agent"
)
$ErrorActionPreference = "Stop"

function Say($text) { Write-Host "==> $text" -ForegroundColor Cyan }
function Problem($text) { Write-Host "  missing: $text" -ForegroundColor Yellow; $script:missing++ }

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this from an elevated PowerShell (Run as administrator)."
}
$source = Split-Path -Parent $PSScriptRoot          # ...\agent
if (-not (Test-Path (Join-Path $source "xppagent\supervisor.py"))) {
    throw "Run this script from a checkout of the repository (agent\install)."
}

# -- prerequisites -------------------------------------------------------------
Say "Checking prerequisites"
$script:missing = 0
if (-not $Python) {
    foreach ($candidate in @("py -3.15", "py -3.14", "py -3.13", "py -3.12", "py -3.11")) {
        $parts = $candidate.Split(" ")
        try {
            $exe = & $parts[0] $parts[1] -c "import sys; print(sys.executable)" 2>$null
            if ($LASTEXITCODE -eq 0 -and $exe) { $Python = $exe.Trim(); break }
        } catch { }
    }
}
if (-not $Python -or -not (Test-Path $Python)) {
    Problem "Python 3.11 or newer for all users: winget install -e --id Python.Python.3.14 --scope machine"
} elseif ($Python -like "$env:USERPROFILE*") {
    Problem "Python is installed for your account only ($Python); the agent's account cannot use it. Install it for all users: winget install -e --id Python.Python.3.14 --scope machine"
} else {
    Write-Host "  Python: $Python"
}
foreach ($tool in @(@("git", "Git.Git"), @("gh", "GitHub.cli"))) {
    $found = Get-Command $tool[0] -ErrorAction SilentlyContinue
    if ($found) { Write-Host "  $($tool[0]): $($found.Source)" }
    else { Problem "$($tool[0]): winget install -e --id $($tool[1]) --scope machine" }
}
$bash = Join-Path $Msys "usr\bin\bash.exe"
if (-not (Test-Path $bash)) {
    Problem "MSYS2 at $Msys (https://www.msys2.org), then the packages below"
} else {
    $need = "mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja " +
            "mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-pytest " +
            "mingw-w64-ucrt-x86_64-python-numpy mingw-w64-ucrt-x86_64-sdl3"
    $env:MSYSTEM = "UCRT64"
    $check = & $bash -lc "pacman -Q $need >/dev/null 2>&1 && echo yes"
    if ($check -ne "yes") {
        Problem "MSYS2 packages: in an MSYS2 UCRT64 shell run: pacman -S --needed $need"
    } else {
        Write-Host "  MSYS2 toolchain: $Msys"
    }
}
if ($script:missing) {
    throw "$($script:missing) prerequisite(s) missing; install them and run this again."
}

# -- the agent's account -------------------------------------------------------
$user = Get-LocalUser -Name $UserName -ErrorAction SilentlyContinue
if (-not $user) {
    Say "Creating the local account $UserName (a standard user, not an administrator)"
    Write-Host "  Choose a long password for it. You will type it once more below for the"
    Write-Host "  scheduled task; nobody needs to remember it after that."
    $password = Read-Host -AsSecureString "Password for $UserName"
    New-LocalUser -Name $UserName -Password $password -PasswordNeverExpires `
        -UserMayNotChangePassword -Description "Runs the XPP Studio maintainer agent" | Out-Null
} else {
    Say "The account $UserName exists"
}

# A scheduled task that runs whether anyone is signed in or not logs its
# account on "as a batch job". Registering the task from a script does not
# grant that right (the Task Scheduler window does), and without it the task
# never starts (error 0x80070569). On Windows Server only administrators have
# it by default.
$sid = (New-Object Security.Principal.NTAccount($UserName)).Translate(
    [Security.Principal.SecurityIdentifier]).Value
$inf = Join-Path $env:TEMP "xpps-rights.inf"
secedit /export /cfg $inf /areas USER_RIGHTS | Out-Null
$line = Get-Content $inf | Where-Object { $_ -match '^SeBatchLogonRight\s*=' }
# The policy may name the account by its SID (*S-1-5-...) or, when it was
# added by hand in secpol.msc, by its name.
$granted = $line -match [regex]::Escape("*$sid") -or
           $line -match ("(=|,)\s*" + [regex]::Escape($UserName) + "\s*(,|$)")
if (-not $granted) {
    Say "Letting $UserName run as a scheduled task (Log on as a batch job)"
    $members = if ($line) { ($line -split '=', 2)[1].Trim() + ",*$sid" } else { "*$sid" }
    $policy = "[Unicode]`r`nUnicode=yes`r`n[Version]`r`nsignature=`"`$CHICAGO`$`"`r`nRevision=1`r`n" +
              "[Privilege Rights]`r`nSeBatchLogonRight = $members`r`n"
    [IO.File]::WriteAllText($inf, $policy, [Text.Encoding]::Unicode)
    secedit /configure /db (Join-Path $env:TEMP "xpps-rights.sdb") /cfg $inf /areas USER_RIGHTS /quiet | Out-Null
}
Remove-Item $inf, (Join-Path $env:TEMP "xpps-rights.sdb") -ErrorAction SilentlyContinue

# -- the home folder -----------------------------------------------------------
Say "Preparing $AgentHome"
New-Item -ItemType Directory -Force $AgentHome | Out-Null
# Only the agent, administrators and SYSTEM may read it: secrets.env lives here.
$acl = New-Object System.Security.AccessControl.DirectorySecurity
$acl.SetAccessRuleProtection($true, $false)
foreach ($who in @("$env:COMPUTERNAME\$UserName", "BUILTIN\Administrators", "NT AUTHORITY\SYSTEM")) {
    $rule = New-Object System.Security.AccessControl.FileSystemAccessRule(
        $who, "FullControl", "ContainerInherit,ObjectInherit", "None", "Allow")
    $acl.AddAccessRule($rule)
}
Set-Acl -Path $AgentHome -AclObject $acl

# Run again over an installed agent, first let it finish its step and stop:
# its process holds the app folder, and it must not run half-old, half-new
# code. (A STOP file makes it idle at its next pass rather than in the middle
# of a Claude run or a push.)
$wasRunning = $false
$existing = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if ($existing -and $existing.State -eq "Running") {
    $wasRunning = $true
    $stop = Join-Path $AgentHome "STOP"
    $madeStop = -not (Test-Path $stop)
    if ($madeStop) { New-Item -ItemType File $stop | Out-Null }
    Say "Waiting for the running agent to finish its current step (at most 70 minutes)"
    $log = Join-Path $AgentHome "agent.log"
    $deadline = (Get-Date).AddMinutes(70)
    do {
        Start-Sleep -Seconds 15
        $last = if (Test-Path $log) { Get-Content $log -Tail 1 } else { "" }
    } until ($last -match "exists: doing nothing" -or (Get-Date) -gt $deadline)
    Stop-ScheduledTask -TaskName $TaskName
    Get-CimInstance Win32_Process -Filter "Name='python.exe'" |
        Where-Object { $_.CommandLine -match "xppagent" -and $_.CommandLine -match [regex]::Escape($AgentHome) } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
    if ($madeStop) { Remove-Item $stop }
}

# Replace the code inside the app folder, not the folder itself.
$app = Join-Path $AgentHome "app"
New-Item -ItemType Directory -Force $app | Out-Null
foreach ($part in @("xppagent", "prompts")) {
    $target = Join-Path $app $part
    if (Test-Path $target) { Remove-Item -Recurse -Force $target }
    Copy-Item -Recurse (Join-Path $source $part) $app
}
Get-ChildItem -Recurse -Directory -Filter "__pycache__" $app | Remove-Item -Recurse -Force

$config = Join-Path $AgentHome "config.toml"
if (-not (Test-Path $config)) {
    $text = Get-Content -Raw (Join-Path $source "config.example.toml")
    $homeToml = $AgentHome.Replace("\", "/")
    $msysToml = $Msys.Replace("\", "/")
    $claudeExe = "C:/Users/$UserName/.local/bin/claude.exe"
    $text = $text.Replace('home = "C:/xpps-agent"', "home = `"$homeToml`"")
    $text = $text.Replace('claude = "claude"', "claude = `"$claudeExe`"")
    $text = $text.Replace("C:/msys64", $msysToml)
    [IO.File]::WriteAllText($config, $text, (New-Object Text.UTF8Encoding($false)))
    Write-Host "  wrote $config"
} else {
    Write-Host "  kept $config"
}
$secrets = Join-Path $AgentHome "secrets.env"
if (-not (Test-Path $secrets)) {
    $template = @"
# Secrets of the XPP Studio maintainer agent. Readable only by the agent's
# account and administrators. See agent/README.md for how to make each one.
GH_TOKEN=
CLAUDE_CODE_OAUTH_TOKEN=
"@
    [IO.File]::WriteAllText($secrets, $template, (New-Object Text.UTF8Encoding($false)))
    Write-Host "  wrote an empty $secrets"
} else {
    Write-Host "  kept $secrets"
}

# -- the scheduled task ---------------------------------------------------------
Say "Registering the scheduled task '$TaskName'"
$action = New-ScheduledTaskAction -Execute $Python `
    -Argument "-m xppagent run --config `"$config`"" -WorkingDirectory $app
# At boot, and every 5 minutes for ever: "restart on failure" only covers a
# task that fails to start, not a program that dies later. A copy started
# while the agent runs is dropped (IgnoreNew), and the agent's own lock file
# stops any second copy.
$trigger = @((New-ScheduledTaskTrigger -AtStartup),
             (New-ScheduledTaskTrigger -Once -At (Get-Date).Date `
                 -RepetitionInterval (New-TimeSpan -Minutes 5)))
$settings = New-ScheduledTaskSettingsSet -RestartCount 999 `
    -RestartInterval (New-TimeSpan -Minutes 5) -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -MultipleInstances IgnoreNew
Write-Host "  Type the password of $UserName so the task can run without anyone signed in."
$cred = Get-Credential -UserName "$env:COMPUTERNAME\$UserName" -Message "Password of $UserName"
Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
    -Settings $settings -User $cred.UserName `
    -Password $cred.GetNetworkCredential().Password -RunLevel Limited -Force | Out-Null

if ($wasRunning) {
    Start-ScheduledTask -TaskName $TaskName
    Say "Updated, and the agent is running again."
    return
}
Say "Installed. Next steps (agent/README.md has the details):"
Write-Host "  1. Install Claude Code for ${UserName}: runas /user:$UserName powershell"
Write-Host "     then, in that window: irm https://claude.ai/install.ps1 | iex"
Write-Host "  2. Put GH_TOKEN and CLAUDE_CODE_OAUTH_TOKEN in $secrets (notepad, as administrator)."
Write-Host "  3. Check: runas /user:$UserName `"$Python -m xppagent doctor --live --config $config`""
Write-Host "     (run it from $app)"
Write-Host "  4. Start: Start-ScheduledTask -TaskName '$TaskName'"
