#Requires -Version 5.1
<#
.SYNOPSIS
  Finish WSL Ubuntu + C++ toolchain setup for this portfolio repo.

.DESCRIPTION
  Run from Windows PowerShell AFTER rebooting (WSL features need a reboot).
  Keeps the repo at D:\workspace\projects and builds via /mnt/d/workspace/projects.

.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\wsl-finish-setup.ps1
#>
$ErrorActionPreference = "Stop"
$RepoWin = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Distro = "Ubuntu-24.04"

function Write-Step([string]$Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function ConvertTo-WslPath([string]$WinPath) {
    $full = (Resolve-Path $WinPath).Path
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $Matches[1].ToLowerInvariant()
        $rest = ($Matches[2] -replace '\\', '/')
        return "/mnt/$drive/$rest"
    }
    throw "Cannot convert Windows path to WSL path: $WinPath"
}

Write-Step "Checking WSL"
$env:WSL_UTF8 = "1"
& wsl.exe --version
if ($LASTEXITCODE -ne 0) {
    throw "WSL is not usable yet. Reboot Windows, then re-run this script."
}

$rawList = & wsl.exe -l -q 2>$null
$distroList = @()
if ($null -ne $rawList) {
    $distroList = @(
        $rawList | ForEach-Object {
            ($_ -replace "`0", "").ToString().Trim()
        } | Where-Object { $_ -ne "" }
    )
}

$haveDistro = $false
foreach ($name in @($Distro, "Ubuntu", "Ubuntu-22.04", "Ubuntu-26.04")) {
    if ($distroList -contains $name) {
        $Distro = $name
        $haveDistro = $true
        break
    }
}

if (-not $haveDistro) {
    Write-Step "Installing $Distro (may take a few minutes)"
    & wsl.exe --install -d $Distro --no-launch
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Falling back to distro name 'Ubuntu'..." -ForegroundColor Yellow
        $Distro = "Ubuntu"
        & wsl.exe --install -d $Distro --no-launch
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to install an Ubuntu distro. Reboot if you have not yet, then retry."
        }
    }
}

Write-Step "Ensuring distro can start (root bootstrap)"
& wsl.exe -d $Distro -u root -- bash -lc "echo wsl-ok && uname -a"
if ($LASTEXITCODE -ne 0) {
    throw @"
Could not start $Distro.
If install just finished, open 'Ubuntu' once from the Start menu to finish provisioning,
create your Linux username/password, then re-run this script.
"@
}

$RepoWsl = ConvertTo-WslPath $RepoWin
Write-Step "Installing packages and verifying build"
Write-Host "Repo (Windows): $RepoWin"
Write-Host "Repo (WSL):     $RepoWsl"

# Write a Linux-side script to avoid PowerShell/bash quoting fights.
$linuxScriptPathWin = Join-Path $env:TEMP "portfolio-wsl-setup.sh"
$linuxScript = @"
#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y build-essential gdb cmake ninja-build git pkg-config
REPO='$RepoWsl'
echo "Repo WSL path: `$REPO"
test -d "`$REPO/01-ring-buffer"
cd "`$REPO"
rm -rf 01-ring-buffer/build
cmake -S 01-ring-buffer -B 01-ring-buffer/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 01-ring-buffer/build -j"`$(nproc)"
ctest --test-dir 01-ring-buffer/build --output-on-failure
echo
echo "WSL setup OK. Build + tests passed."
g++ --version | head -n1
cmake --version | head -n1
gdb --version | head -n1
"@
# Convert to LF for bash
$linuxScript = $linuxScript -replace "`r`n", "`n"
[System.IO.File]::WriteAllText($linuxScriptPathWin, $linuxScript)

$linuxScriptPathWsl = ConvertTo-WslPath $linuxScriptPathWin
& wsl.exe -d $Distro -u root -- bash "$linuxScriptPathWsl"
if ($LASTEXITCODE -ne 0) {
    throw "Toolchain install or build/test failed inside WSL."
}

Write-Step "Done"
Write-Host @"

Next in Cursor:
  1. Install the WSL remote extension if prompted.
  2. Open folder via WSL: $RepoWsl
     (Command Palette -> 'WSL: Open Folder in WSL...')
  3. Install Anysphere C/C++ in the WSL remote (Extensions).
  4. Ctrl+Shift+B builds; F5 debugs with gdb.

Repo stays at: $RepoWin
WSL path:      $RepoWsl
"@
