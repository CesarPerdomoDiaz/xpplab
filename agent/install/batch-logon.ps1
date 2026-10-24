# Dot-sourced by the installers: lets a local account run a scheduled task
# whether anyone is signed in or not ("Log on as a batch job").
#
# Registering a task from a script does not grant that right (the Task
# Scheduler window does), and without it the task never starts (error
# 0x80070569). On Windows Server only administrators have it by default.
function Grant-BatchLogon([string]$UserName) {
    $sid = (New-Object Security.Principal.NTAccount($UserName)).Translate(
        [Security.Principal.SecurityIdentifier]).Value
    $inf = Join-Path $env:TEMP "xpps-rights.inf"
    $db = Join-Path $env:TEMP "xpps-rights.sdb"
    secedit /export /cfg $inf /areas USER_RIGHTS | Out-Null
    $line = Get-Content $inf | Where-Object { $_ -match '^SeBatchLogonRight\s*=' }
    # The policy names the account by SID (*S-1-5-...), or by name when it
    # was added by hand in secpol.msc.
    $granted = $line -match [regex]::Escape("*$sid") -or
               $line -match ("(=|,)\s*" + [regex]::Escape($UserName) + "\s*(,|$)")
    if (-not $granted) {
        Write-Host "==> Letting $UserName run as a scheduled task (Log on as a batch job)" -ForegroundColor Cyan
        $members = if ($line) { ($line -split '=', 2)[1].Trim() + ",*$sid" } else { "*$sid" }
        $policy = "[Unicode]`r`nUnicode=yes`r`n[Version]`r`nsignature=`"`$CHICAGO`$`"`r`nRevision=1`r`n" +
                  "[Privilege Rights]`r`nSeBatchLogonRight = $members`r`n"
        [IO.File]::WriteAllText($inf, $policy, [Text.Encoding]::Unicode)
        secedit /configure /db $db /cfg $inf /areas USER_RIGHTS /quiet | Out-Null
    }
    Remove-Item $inf, $db -ErrorAction SilentlyContinue
}
