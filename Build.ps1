param([switch]$SkipTests)
$ErrorActionPreference='Stop'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'Visual Studio with C++ tools is required.'}
$cmake=Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
if(-not(Test-Path -LiteralPath $cmake)){throw 'Install the Visual Studio CMake component.'}
$version=& $cmake --version
$installationVersion=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
$generator=if([version]$installationVersion -ge [version]'18.0'){'Visual Studio 18 2026'}else{'Visual Studio 17 2022'}
& $cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build') -G $generator -A x64
if($LASTEXITCODE -ne 0){throw 'CMake configure failed.'}
& $cmake --build (Join-Path $PSScriptRoot 'build') --config Release
if($LASTEXITCODE -ne 0){throw 'Build failed.'}
if(-not $SkipTests){
    & $ctest --test-dir (Join-Path $PSScriptRoot 'build') -C Release --output-on-failure
    if($LASTEXITCODE -ne 0){throw 'Motion telemetry tests failed.'}
}
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'dist\plugins') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build\Release\GalacticRacerMotion.dll') -Destination (Join-Path $PSScriptRoot 'dist\plugins\GalacticRacerMotion.dll') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'galactic_racer_motion.cfg') -Destination (Join-Path $PSScriptRoot 'dist\galactic_racer_motion.cfg') -Force
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'dist\simhub') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'simhub\GalacticRacer.simdef') -Destination (Join-Path $PSScriptRoot 'dist\simhub\GalacticRacer.simdef') -Force
