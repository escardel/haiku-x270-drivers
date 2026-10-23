param(
    [string]$Target = 'user@10.0.0.21',
    [int]$Attempts = 8
)

$ErrorActionPreference = 'Stop'
$logDir = Join-Path $PSScriptRoot 'recovery-logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = Join-Path $logDir "x270-$stamp.txt"

for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
    & ssh -o BatchMode=yes -o ConnectTimeout=5 $Target 'uname -a'
    if ($LASTEXITCODE -eq 0) { break }
    if ($attempt -eq $Attempts) { throw "X270 SSH did not respond after $Attempts bounded attempts." }
    Start-Sleep -Seconds ([Math]::Min(3 * $attempt, 15))
}

$remote = @'
date
uname -a
uptime
ifconfig
ls -la /var/log
tail -n 250 /var/log/syslog
ls -l /boot/home/config/non-packaged/add-ons/media
ls -l /boot/home/config/non-packaged/add-ons/kernel/drivers/bin
cd /boot/home/haiku && git status --short && git branch --show-current
'@

& ssh -o BatchMode=yes -o ConnectTimeout=10 $Target $remote 2>&1 |
    Tee-Object -FilePath $log
if ($LASTEXITCODE -ne 0) { throw "Diagnostic collection failed; partial output: $log" }
Write-Output "Saved diagnostic output to $log"
