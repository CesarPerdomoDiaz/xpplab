<#
.SYNOPSIS
    Stops and removes the XPP Lab maintainer agent's scheduled task.
    With -Purge, also deletes its home folder (state, logs, secrets, clone)
    and its Windows account.
#>
param(
    [string]$AgentHome = "C:\xpps-agent",
    [string]$UserName = "xpps-agent",
    [string]$TaskName = "XPP Studio maintainer agent",
    [switch]$Purge
)
$ErrorActionPreference = "Stop"

$task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if ($task) {
    Stop-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    Write-Host "Removed the scheduled task '$TaskName'."
} else {
    Write-Host "No scheduled task '$TaskName'."
}
if ($Purge) {
    if (Test-Path $AgentHome) {
        Remove-Item -Recurse -Force $AgentHome
        Write-Host "Deleted $AgentHome."
    }
    if (Get-LocalUser -Name $UserName -ErrorAction SilentlyContinue) {
        Remove-LocalUser -Name $UserName
        Write-Host "Deleted the account $UserName (its profile folder under C:\Users stays)."
    }
}
