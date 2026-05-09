# make_installer.ps1
# Usage: run from repo root: powershell -ExecutionPolicy Bypass -File .\scripts\make_installer.ps1
param(
    [string] $BuildRoot = "build\TICK_artefacts\Release",
    [string] $OutDist = "dist"
)

Write-Host "Staging artifacts..."

# Clean dist
if (Test-Path $OutDist) { Remove-Item $OutDist -Recurse -Force }
New-Item -ItemType Directory -Path $OutDist | Out-Null

# Copy VST3 bundle
$srcVst3 = Join-Path $BuildRoot "VST3\TICK.vst3"
if (-not (Test-Path $srcVst3)) {
    Write-Error "VST3 bundle not found at $srcVst3. Build the project first."
    exit 1
}
Write-Host "Copying VST3..."
Copy-Item -Path $srcVst3 -Destination (Join-Path $OutDist "TICK.vst3") -Recurse -Force

# Copy Standalone if exists
$srcStandalone = Join-Path $BuildRoot "Standalone"
if (Test-Path $srcStandalone) {
    Write-Host "Copying Standalone..."
    Copy-Item -Path $srcStandalone -Destination (Join-Path $OutDist "Standalone") -Recurse -Force
}

# Ensure NSIS script exists
if (-not (Test-Path "installer.nsi")) {
    Write-Error "installer.nsi not found in repo root."
    exit 1
}

# Find makensis (first check PATH, then common install locations)
$makensisCmd = Get-Command makensis -ErrorAction SilentlyContinue
if ($makensisCmd) {
    $makensisPath = $makensisCmd.Path
} else {
    $possible = @(
        "$Env:ProgramFiles\NSIS\makensis.exe",
        "$Env:ProgramFiles(x86)\NSIS\makensis.exe",
        "C:\Program Files\NSIS\makensis.exe",
        "C:\Program Files (x86)\NSIS\makensis.exe"
    )
    $makensisPath = $possible | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $makensisPath) {
    Write-Error 'makensis not found. Install NSIS (https://nsis.sourceforge.io/Download) or add makensis to PATH.
You can install via Chocolatey: choco install nsis -y'
    exit 1
}

Write-Host "Building installer with makensis at: $makensisPath"

# Run makensis
$proc = Start-Process -FilePath $makensisPath -ArgumentList "installer.nsi" -Wait -NoNewWindow -PassThru
if ($proc.ExitCode -ne 0) {
    Write-Error "makensis failed with exit code $($proc.ExitCode)."
    exit $proc.ExitCode
}

Write-Host "Installer created: TICK-Installer.exe"