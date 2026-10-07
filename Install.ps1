param([string]$ProfilePath=(Join-Path $env:APPDATA 'UnrealVRMod\SWGR-Win64-Shipping'))
$ErrorActionPreference='Stop'
$dll=Join-Path $PSScriptRoot 'dist\plugins\GalacticRacerMotion.dll'
if(-not(Test-Path -LiteralPath $dll)){$dll=Join-Path $PSScriptRoot 'plugins\GalacticRacerMotion.dll'}
if(-not(Test-Path -LiteralPath $dll)){throw 'Build first with .\Build.ps1.'}
New-Item -ItemType Directory -Force -Path (Join-Path $ProfilePath 'plugins') | Out-Null
$target=Join-Path $ProfilePath 'plugins\GalacticRacerMotion.dll'
$copyNeeded=$true
if(Test-Path -LiteralPath $target){
    if((Get-FileHash -LiteralPath $target).Hash -eq (Get-FileHash -LiteralPath $dll).Hash){$copyNeeded=$false}
    if($copyNeeded){
    Copy-Item -LiteralPath $target -Destination ($target+'.backup-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
    }
}
if($copyNeeded){Copy-Item -LiteralPath $dll -Destination $target -Force}
$config=Join-Path $ProfilePath 'galactic_racer_motion.cfg'
if(-not(Test-Path -LiteralPath $config)){Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'galactic_racer_motion.cfg') -Destination $config}
$simhub=Join-Path $ProfilePath 'simhub'
New-Item -ItemType Directory -Force -Path $simhub | Out-Null
$definitionPath=Join-Path $simhub 'GalacticRacer.simdef'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'simhub\GalacticRacer.simdef') -Destination $definitionPath -Force
$definition=Get-Content -LiteralPath $definitionPath -Raw | ConvertFrom-Json
$registration=Join-Path $env:LOCALAPPDATA 'SimHub\ExternalSims\Registrations'
New-Item -ItemType Directory -Force -Path $registration | Out-Null
[IO.File]::WriteAllText((Join-Path $registration ($definition.UniqueId+'.simlink')),[IO.Path]::GetFullPath($definitionPath)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
Write-Output "Installed motion telemetry in $ProfilePath. UEVR loads it on the next injection."
