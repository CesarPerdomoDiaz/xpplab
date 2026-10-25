<#
.SYNOPSIS
    Installs the agent's dashboard ("mission control") on the workstation,
    reachable from your devices over Tailscale, with HTTPS and a password.

.DESCRIPTION
    Run it in an elevated PowerShell, after install-agent.ps1:

        powershell -ExecutionPolicy Bypass -File agent\install\install-web.ps1

    It creates a standard local account for the dashboard (xpps-web), so
    that your own GitHub token is readable by neither the agent's account
    nor its Claude runs. It lets that account read the agent's home but not
    its secrets, asks you to choose the dashboard's password, registers a
    scheduled task that keeps the dashboard running, and puts it on your
    tailnet with `tailscale serve` (HTTPS, reachable only from your own
    devices). Run it again to update the task; it keeps the secrets.
#>
param(
    [string]$AgentHome = "C:\xpps-agent",
    [string]$WebHome = "C:\xpps-web",
    [string]$UserName = "xpps-web",
    [string]$TaskName = "XPP Studio dashboard",
    [int]$Port = 8765
)
$ErrorActionPreference = "Stop"
function Say($text) { Write-Host "==> $text" -ForegroundColor Cyan }

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this from an elevated PowerShell (Run as administrator)."
}
$app = Join-Path $AgentHome "app"
$config = Join-Path $AgentHome "config.toml"
if (-not (Test-Path (Join-Path $app "xppagent\web\server.py"))) {
    throw "The agent with its dashboard is not installed in ${AgentHome}: run install-agent.ps1 (or update-agent.ps1) first."
}
$agentTask = Get-ScheduledTask -TaskName "XPP Studio maintainer agent" -ErrorAction SilentlyContinue
$Python = if ($agentTask) { $agentTask.Actions[0].Execute } else { "" }
if (-not $Python -or -not (Test-Path $Python)) { throw "Could not find the agent's Python." }
$tailscale = "C:\Program Files\Tailscale\tailscale.exe"
if (-not (Test-Path $tailscale)) { throw "Tailscale is not installed." }

# -- the dashboard's account -----------------------------------------------------
if (-not (Get-LocalUser -Name $UserName -ErrorAction SilentlyContinue)) {
    Say "Creating the local account $UserName (a standard user) for the dashboard"
    Write-Host "  Choose a long password for it; you type it once more below for the task."
    $password = Read-Host -AsSecureString "Password for $UserName"
    New-LocalUser -Name $UserName -Password $password -PasswordNeverExpires `
        -UserMayNotChangePassword -Description "Runs the XPP Lab agent's dashboard" | Out-Null
} else {
    Say "The account $UserName exists"
}
. (Join-Path $PSScriptRoot "batch-logon.ps1")
Grant-BatchLogon $UserName

# -- who may read what ---------------------------------------------------------------
Say "The dashboard may read $AgentHome, but not its secrets"
$acl = Get-Acl $AgentHome
$acl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule(
    "$env:COMPUTERNAME\$UserName", "ReadAndExecute", "ContainerInherit,ObjectInherit", "None", "Allow")))
Set-Acl -Path $AgentHome -AclObject $acl
$agentSecrets = Join-Path $AgentHome "secrets.env"
if (Test-Path $agentSecrets) {
    $sacl = New-Object System.Security.AccessControl.FileSecurity
    $sacl.SetAccessRuleProtection($true, $false)
    foreach ($who in @("$env:COMPUTERNAME\xpps-agent", "BUILTIN\Administrators", "NT AUTHORITY\SYSTEM")) {
        $sacl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule(
            $who, "FullControl", "Allow")))
    }
    Set-Acl -Path $agentSecrets -AclObject $sacl
}

Say "Preparing $WebHome (the dashboard's secrets and log)"
New-Item -ItemType Directory -Force $WebHome | Out-Null
$wacl = New-Object System.Security.AccessControl.DirectorySecurity
$wacl.SetAccessRuleProtection($true, $false)
foreach ($who in @("$env:COMPUTERNAME\$UserName", "BUILTIN\Administrators", "NT AUTHORITY\SYSTEM")) {
    $wacl.AddAccessRule((New-Object System.Security.AccessControl.FileSystemAccessRule(
        $who, "FullControl", "ContainerInherit,ObjectInherit", "None", "Allow")))
}
Set-Acl -Path $WebHome -AclObject $wacl

$secrets = Join-Path $WebHome "secrets.env"
if (-not (Test-Path $secrets)) {
    $template = @"
# Secrets of the agent's dashboard. Readable only by its account ($UserName)
# and administrators. GH_TOKEN is YOUR fine-grained token (agent/README.md).
GH_TOKEN=
DASHBOARD_PASSWORD=
"@
    [IO.File]::WriteAllText($secrets, $template, (New-Object Text.UTF8Encoding($false)))
}
$lines = Get-Content $secrets
if (-not ($lines | Where-Object { $_ -match '^DASHBOARD_PASSWORD=\S' })) {
    Say "Choose the password you will type to open the dashboard (10 characters or more)"
    Push-Location $app
    $out = & $Python -m xppagent web-password
    Pop-Location
    $hash = $out | Where-Object { $_ -like "DASHBOARD_PASSWORD=*" } | Select-Object -Last 1
    if (-not $hash) { throw "No password was set: $out" }
    $lines = @($lines | Where-Object { $_ -notmatch '^DASHBOARD_PASSWORD=' }) + $hash
    [IO.File]::WriteAllText($secrets, ($lines -join "`r`n") + "`r`n", (New-Object Text.UTF8Encoding($false)))
}
$hasToken = [bool]((Get-Content $secrets) | Where-Object { $_ -match '^GH_TOKEN=\S' })

# -- the scheduled task ----------------------------------------------------------
Say "Registering the scheduled task '$TaskName'"
$existing = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if ($existing -and $existing.State -eq "Running") { Stop-ScheduledTask -TaskName $TaskName }
$action = New-ScheduledTaskAction -Execute $Python -WorkingDirectory $app `
    -Argument "-m xppagent web --config `"$config`" --secrets `"$secrets`" --port $Port"
# At boot and every 5 minutes: a second copy cannot take the port and exits.
$trigger = @((New-ScheduledTaskTrigger -AtStartup),
             (New-ScheduledTaskTrigger -Once -At (Get-Date).Date `
                 -RepetitionInterval (New-TimeSpan -Minutes 5)))
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -MultipleInstances IgnoreNew
Write-Host "  Type the password of $UserName so the dashboard runs without anyone signed in."
$cred = Get-Credential -UserName "$env:COMPUTERNAME\$UserName" -Message "Password of $UserName"
Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
    -Settings $settings -User $cred.UserName `
    -Password $cred.GetNetworkCredential().Password -RunLevel Limited -Force | Out-Null

# -- on the tailnet, over HTTPS ---------------------------------------------------
Say "Putting the dashboard on your tailnet (tailscale serve)"
& $tailscale serve --bg $Port
if ($LASTEXITCODE -ne 0) {
    Write-Host "  tailscale serve failed. In the Tailscale admin console, open DNS and turn on"
    Write-Host "  MagicDNS and HTTPS Certificates, then run this script again." -ForegroundColor Yellow
}
$dns = (& $tailscale status --json | ConvertFrom-Json).Self.DNSName.TrimEnd(".")

if ($hasToken) {
    Start-ScheduledTask -TaskName $TaskName
    Say "The dashboard is running: https://$dns"
} else {
    Say "Almost there. Put your own GitHub token in the dashboard's secrets:"
    Write-Host "  notepad $secrets      (the GH_TOKEN= line; agent/README.md says which token)"
    Write-Host "  then: Start-ScheduledTask -TaskName '$TaskName'"
    Write-Host "  and open https://$dns from any of your devices on Tailscale."
}
