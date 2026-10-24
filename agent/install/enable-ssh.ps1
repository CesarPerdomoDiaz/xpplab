<#
.SYNOPSIS
    Lets one computer log in to this workstation over SSH, through Tailscale
    only, with a key and never a password.

.DESCRIPTION
    Run it in an elevated PowerShell on the workstation:

        powershell -ExecutionPolicy Bypass -File agent\install\enable-ssh.ps1 -PublicKey "ssh-ed25519 AAAA... name"

    It installs and starts Windows' OpenSSH Server, turns password logins
    off, allows port 22 only from Tailscale addresses (100.64.0.0/10), makes
    PowerShell the shell for SSH sessions, and authorizes the given public
    key for administrators. Run it again to add another key; it keeps the
    ones already there.
#>
param(
    [Parameter(Mandatory = $true)][string]$PublicKey
)
$ErrorActionPreference = "Stop"
function Say($text) { Write-Host "==> $text" -ForegroundColor Cyan }

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this from an elevated PowerShell (Run as administrator)."
}
if ($PublicKey -notmatch '^(ssh-ed25519|ssh-rsa|ecdsa-sha2-\S+) \S+') {
    throw "That does not look like an SSH public key (it should start with ssh-ed25519)."
}
$tailscale = "C:\Program Files\Tailscale\tailscale.exe"
if (-not (Test-Path $tailscale)) {
    throw "Tailscale is not installed. Install it and sign in first (https://tailscale.com/download)."
}
$ip = (& $tailscale ip -4 2>$null | Select-Object -First 1)
if (-not $ip) { throw "Tailscale is not signed in: run 'tailscale up' or sign in from its tray icon." }

Say "OpenSSH Server"
$cap = Get-WindowsCapability -Online -Name "OpenSSH.Server*" | Select-Object -First 1
if ($cap.State -ne "Installed") {
    Write-Host "  installing (this can take a few minutes)"
    Add-WindowsCapability -Online -Name $cap.Name | Out-Null
}
Set-Service -Name sshd -StartupType Automatic
Start-Service sshd   # creates C:\ProgramData\ssh\sshd_config on first start

Say "Key logins only"
$config = "C:\ProgramData\ssh\sshd_config"
$text = Get-Content -Raw $config
foreach ($setting in @("PasswordAuthentication no", "PubkeyAuthentication yes",
                       "PermitEmptyPasswords no")) {
    $name = $setting.Split(" ")[0]
    if ($text -match "(?m)^\s*#?\s*$name\b.*$") {
        $text = [regex]::Replace($text, "(?m)^\s*#?\s*$name\b.*$", $setting)
    } else {
        $text = "$setting`r`n" + $text
    }
}
[IO.File]::WriteAllText($config, $text)

Say "The key"
$keys = "C:\ProgramData\ssh\administrators_authorized_keys"
$existing = if (Test-Path $keys) { Get-Content $keys } else { @() }
if ($existing -notcontains $PublicKey.Trim()) {
    Add-Content -Path $keys -Value $PublicKey.Trim() -Encoding ascii
}
# sshd ignores the file unless only Administrators and SYSTEM can write it.
icacls $keys /inheritance:r /grant "*S-1-5-32-544:F" /grant "SYSTEM:F" | Out-Null

Say "PowerShell as the SSH shell"
New-Item -Path "HKLM:\SOFTWARE\OpenSSH" -Force | Out-Null
New-ItemProperty -Path "HKLM:\SOFTWARE\OpenSSH" -Name DefaultShell -PropertyType String `
    -Value "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -Force | Out-Null

Say "Firewall: port 22 from Tailscale only"
Get-NetFirewallRule -Name "OpenSSH-Server-In-TCP" -ErrorAction SilentlyContinue |
    Disable-NetFirewallRule
Get-NetFirewallRule -Name "XPPS-SSH-Tailscale" -ErrorAction SilentlyContinue |
    Remove-NetFirewallRule
New-NetFirewallRule -Name "XPPS-SSH-Tailscale" -DisplayName "OpenSSH (Tailscale only)" `
    -Direction Inbound -Protocol TCP -LocalPort 22 -RemoteAddress "100.64.0.0/10" `
    -Action Allow | Out-Null

Restart-Service sshd
Say "Done. From the other computer:"
Write-Host "  ssh $env:USERNAME@$ip"
Write-Host "  (or ssh $env:USERNAME@$env:COMPUTERNAME, by its Tailscale name)"
