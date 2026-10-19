param(
    [ValidateSet("licensed", "local-dev")]
    [string]$Profile = "licensed",

    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectRoot

if ([string]::IsNullOrWhiteSpace($Version)) {
    $cmakeContents = Get-Content (Join-Path $projectRoot "CMakeLists.txt") -Raw
    $versionMatch = [regex]::Match($cmakeContents, '(?i)project\s*\(\s*StringLife\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})(?=\s|\))')
    if (-not $versionMatch.Success) {
        throw "Could not read the StringLife project version from CMakeLists.txt"
    }
    $Version = $versionMatch.Groups[1].Value
}

$buildRoot = Join-Path $projectRoot "build\$Profile"
$templatePath = Join-Path $projectRoot "packaging\windows\StringLife.iss.in"
$generatedDir = Join-Path $buildRoot "installer"
$generatedIss = Join-Path $generatedDir "StringLife-$Profile.iss"

if (-not (Test-Path $templatePath)) {
    throw "Missing template: $templatePath"
}

$requiredPaths = @(
    (Join-Path $buildRoot "Standalone\StringLife.exe"),
    (Join-Path $buildRoot "VST3\StringLife.vst3")
)

$runningOnWindows = $PSVersionTable.Platform -eq "Win32NT" -or $env:OS -eq "Windows_NT"

foreach ($path in $requiredPaths) {
    if (-not (Test-Path $path)) {
        if ($runningOnWindows) {
            throw "Missing build artefact: $path`nBuild the Windows targets first so build\$Profile contains StringLife.exe and StringLife.vst3."
        }

        throw "Missing build artefact: $path`nThis helper packages Windows outputs only. On macOS/Linux you will not have StringLife.exe unless you built the project for Windows separately.`nUse this script after creating a real Windows build tree, or run it on a Windows machine."
    }
}

New-Item -ItemType Directory -Force -Path $generatedDir | Out-Null

$template = Get-Content $templatePath -Raw
$resolvedBuildRoot = $buildRoot.Replace('\', '\\')
$content = $template.Replace('@APP_VERSION@', $Version).
    Replace('@PROFILE@', $Profile).
    Replace('@BUILD_ROOT@', $resolvedBuildRoot)

Set-Content -Path $generatedIss -Value $content -NoNewline

$iscc = $env:INNO_SETUP_COMPILER

if ([string]::IsNullOrWhiteSpace($iscc)) {
    $defaultIscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if (Test-Path $defaultIscc) {
        $iscc = $defaultIscc
    }
}

if ([string]::IsNullOrWhiteSpace($iscc) -or -not (Test-Path $iscc)) {
    Write-Host "Generated Inno Setup script:"
    Write-Host "  $generatedIss"
    Write-Host
    Write-Host "Install Inno Setup 6 and then run:"
    Write-Host "  `"$generatedIss`" with ISCC.exe"
    Write-Host
    Write-Host "Or set INNO_SETUP_COMPILER to the full path of ISCC.exe."
    exit 0
}

& $iscc $generatedIss
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

Write-Host
Write-Host "==> Windows installer created under:"
Write-Host "    $generatedDir"
