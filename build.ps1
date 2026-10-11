param([switch]$Test)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

# Locate installed Visual Studio Build Tools; force native Win32, never x64.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio C++ Build Tools (x86/x64 tools and Windows SDK).' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ tools were not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$environment = & $env:ComSpec /d /c "call `"$vcvars`" x86 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the x86 compiler environment.' }
foreach ($line in $environment) {
    if ($line -match '^[^=]+=') {
        $pair = $line -split '=', 2
        [Environment]::SetEnvironmentVariable($pair[0], $pair[1], 'Process')
    }
}
New-Item -ItemType Directory -Force dist/build, tests/bin | Out-Null
$common = @('/nologo', '/std:c++17', '/O2', '/EHsc', '/MT', '/W4', '/DNOMINMAX', '/D_CRT_SECURE_NO_WARNINGS')
# /MT embeds the CRT; no external VC++ runtime DLL is needed. No /Zi or /DEBUG.
& cl @common /GL /Gy /LD src/loader.cpp /Fodist/build/loader.obj /link `
    /MACHINE:X86 /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG:NONE `
    /DYNAMICBASE /NXCOMPAT /DEF:src/exports.def `
    /OUT:dist/ExtendedLanguageLoader.asi /IMPLIB:dist/build/ExtendedLanguageLoader.lib bcrypt.lib
if ($LASTEXITCODE -ne 0) { throw 'ASI compilation failed.' }
& python scripts/package_release.py
if ($LASTEXITCODE -ne 0) { throw 'Release PE/footprint/package validation failed.' }

if ($Test) {
    & cl @common tests/core_test.cpp /Fodist/build/core_test.obj /Fetests/bin/core_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Core test compilation failed.' }
    & ./tests/bin/core_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
    & cl @common tests/install_layout_test.cpp /Fodist/build/install_layout_test.obj /Fetests/bin/install_layout_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Installation layout test compilation failed.' }
    & ./tests/bin/install_layout_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Installation layout tests failed.' }
    & cl @common tests/error_dialog_test.cpp /Fodist/build/error_dialog_test.obj /Fetests/bin/error_dialog_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Error dialog test compilation failed.' }
    & ./tests/bin/error_dialog_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Error dialog format tests failed.' }
    & cl @common tests/extended_font_format_test.cpp /Fodist/build/extended_font_format_test.obj /Fetests/bin/extended_font_format_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Extended font format test compilation failed.' }
    & ./tests/bin/extended_font_format_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Extended font format tests failed.' }
    & python tests/preparation_unit_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Portable/package tests failed.' }
    & python tests/extended_gxt_test.py
    if ($LASTEXITCODE -ne 0) { throw 'Extended GXT conversion tests failed.' }
    & cl @common tests/windows_exports_test.cpp /Fodist/build/windows_exports_test.obj /Fetests/bin/windows_exports_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'DLL smoke-test compilation failed.' }
    & ./tests/bin/windows_exports_test.exe (Join-Path $PSScriptRoot 'dist/ExtendedLanguageLoader.asi')
    if ($LASTEXITCODE -ne 0) { throw 'x86 DLL/export/refusal smoke test failed.' }
    & cl @common tests/windows_error_dialog_test.cpp /Fodist/build/windows_error_dialog_test.obj /Fetests/bin/windows_error_dialog_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Error dialog worker test compilation failed.' }
    & ./tests/bin/windows_error_dialog_test.exe (Join-Path $PSScriptRoot 'dist/ExtendedLanguageLoader.asi')
    if ($LASTEXITCODE -ne 0) { throw 'Error dialog worker/lifetime tests failed.' }
    # Emit actual native build footprint and runtime dependencies into CI logs.
    & dumpbin /nologo /exports /dependents dist/ExtendedLanguageLoader.asi
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect built ASI.' }
}
