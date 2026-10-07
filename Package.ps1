param([string]$Version='1.0.0')
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?$'){throw 'Use a semantic version such as 1.0.0.'}
$dll=Join-Path $PSScriptRoot 'dist\plugins\GalacticRacerMotion.dll'
if(-not(Test-Path -LiteralPath $dll)){throw 'Build first with .\Build.ps1.'}
$releaseRoot=Join-Path $PSScriptRoot 'release'
New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
$staging=Join-Path $releaseRoot ('staging-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging | Out-Null
foreach($file in @('README.md','Build.ps1','Install.ps1','Package.ps1','CMakeLists.txt','galactic_racer_motion.cfg')){
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $staging
}
foreach($directory in @('src','tests','simhub','vendor','docs')){
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $directory) -Destination $staging -Recurse
}
New-Item -ItemType Directory -Path (Join-Path $staging 'tools'),(Join-Path $staging 'plugins') | Out-Null
foreach($file in @('Listen.py','LiveLoad.py','Generate-SimHub.ps1')){
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "tools\$file") -Destination (Join-Path $staging 'tools')
}
Copy-Item -LiteralPath $dll -Destination (Join-Path $staging 'plugins\GalacticRacerMotion.dll')
$archive=Join-Path $releaseRoot "GalacticRacer-UEVR-Motion-v$Version.zip"
Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $archive -Force
Write-Output $archive
Get-FileHash -LiteralPath $archive -Algorithm SHA256
