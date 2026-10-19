$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

cmake -S . -B build/ci -A x64 -DSTRINGLIFE_ENABLE_LICENSING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build build/ci --config Release --target StringLife_VST3 StringLife_Standalone --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }

$source = 'build/ci/StringLife_artefacts/Release'
New-Item -ItemType Directory -Force build/licensed/Standalone, build/licensed/VST3 | Out-Null
Copy-Item "$source/Standalone/*" build/licensed/Standalone -Recurse -Force
Copy-Item "$source/VST3/*" build/licensed/VST3 -Recurse -Force

$compiler = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
if (-not (Test-Path $compiler)) { throw 'Inno Setup 6 is required on the Windows runner' }
$env:INNO_SETUP_COMPILER = $compiler
./package-windows.ps1 -Profile licensed
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup failed' }
$installers = @(Get-ChildItem build/licensed/installer/*-Setup.exe)
if ($installers.Count -ne 1) { throw 'Expected exactly one Windows installer' }
New-Item -ItemType Directory -Force dist | Out-Null
Copy-Item $installers[0].FullName dist/installer.exe

cmake --build build/ci --config Release --target MothFingerprintTests --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Fingerprint test build failed' }
ctest --test-dir build/ci -C Release -R machine-fingerprint --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Fingerprint test failed' }
