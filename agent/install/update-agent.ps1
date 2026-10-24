<#
.SYNOPSIS
    Updates the installed maintainer agent from the checkout and restarts it.

.DESCRIPTION
    Run it as an administrator (in an elevated PowerShell, or over SSH as an
    administrator):

        powershell -ExecutionPolicy Bypass -File agent\install\update-agent.ps1

    It pulls the checkout it is run from, waits for the agent to finish what
    it is doing, copies the new code and prompts to the agent's home, and
    starts it again. The settings, secrets, state and clone are kept. To
    change the scheduled task or the account, run install-agent.ps1 instead.
#>
param(
    [string]$AgentHome = "C:\xpps-agent",
    [string]$TaskName = "XPP Studio maintainer agent",
    [switch]$NoPull
)
$ErrorActionPreference = "Stop"
function Say($text) { Write-Host "==> $text" -ForegroundColor Cyan }

$source = Split-Path -Parent $PSScriptRoot          # ...\agent
$checkout = Split-Path -Parent $source
if (-not $NoPull) {
    Say "Pulling $checkout"
    # Over SSH there is no signed-in GitHub account: borrow the agent's own
    # token from its secrets, for this one command only.
    $secrets = Join-Path $AgentHome "secrets.env"
    $token = Get-Content $secrets -ErrorAction SilentlyContinue |
        Where-Object { $_ -match '^\s*GH_TOKEN\s*=' } |
        ForEach-Object { ($_ -split '=', 2)[1].Trim() } | Select-Object -First 1
    $saved = $env:GH_TOKEN
    if ($token) { $env:GH_TOKEN = $token }
    git -C $checkout -c credential.helper= -c "credential.helper=!gh auth git-credential" pull --ff-only
    $code = $LASTEXITCODE
    $env:GH_TOKEN = $saved
    if ($code -ne 0) { throw "git pull failed; nothing was changed." }
}

$task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if (-not $task) { throw "No scheduled task '$TaskName': run install-agent.ps1 first." }
$wasRunning = $task.State -eq "Running"
if ($wasRunning) {
    # A STOP file makes the agent idle at its next pass instead of stopping
    # in the middle of a Claude run or a push.
    $stop = Join-Path $AgentHome "STOP"
    if (-not (Test-Path $stop)) { New-Item -ItemType File $stop | Out-Null; $made = $true }
    Say "Waiting for the agent to finish its current step (at most 70 minutes)"
    $deadline = (Get-Date).AddMinutes(70)
    $log = Join-Path $AgentHome "agent.log"
    do {
        Start-Sleep -Seconds 15
        $last = if (Test-Path $log) { Get-Content $log -Tail 1 } else { "" }
    } until ($last -match "exists: doing nothing" -or (Get-Date) -gt $deadline)
    Stop-ScheduledTask -TaskName $TaskName
    if ($made) { Remove-Item $stop }
}

Say "Copying the new code to $AgentHome\app"
$app = Join-Path $AgentHome "app"
foreach ($part in @("xppagent", "prompts")) {
    $target = Join-Path $app $part
    if (Test-Path $target) { Remove-Item -Recurse -Force $target }
    Copy-Item -Recurse (Join-Path $source $part) $app
}
Get-ChildItem -Recurse -Directory -Filter "__pycache__" $app | Remove-Item -Recurse -Force

Start-ScheduledTask -TaskName $TaskName
Say "Updated and started. The log: Get-Content $AgentHome\agent.log -Tail 20 -Wait"
