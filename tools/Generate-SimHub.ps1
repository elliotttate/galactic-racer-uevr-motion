param([Parameter(Mandatory=$true)][string]$SimHubDirectory)
$ErrorActionPreference='Stop'
# Run with Windows PowerShell 5.1 so SimHub's .NET Framework dependencies load.
$sourceRoot=Split-Path -Parent $PSScriptRoot
$assemblyPath=Join-Path $SimHubDirectory 'SimHub.Plugins.dll'
[void][Reflection.Assembly]::LoadFrom((Join-Path $SimHubDirectory 'Newtonsoft.Json.dll'))
$assembly=[Reflection.Assembly]::LoadFrom($assemblyPath)
$type=$assembly.GetType('SimHub.Plugins.ExternalSims.ExternalSimManifest',$true)
$definitionPath=Join-Path $sourceRoot 'simhub\GalacticRacer.simdef'
$inputDefinition=Get-Content -LiteralPath $definitionPath -Raw | ConvertFrom-Json
$manifest=New-Object SimHub.Plugins.ExternalSims.ExternalSimManifest
$manifest.Name=$inputDefinition.Name
$manifest.UniqueId=[guid]$inputDefinition.UniqueId
$manifest.DefaultUDPPort=$inputDefinition.DefaultUDPPort
$manifest.TelemetryBehaviour.GameFamily=$inputDefinition.TelemetryBehaviour.GameFamily
$manifest.TelemetryBehaviour.DefaultMaxSpeed=$inputDefinition.TelemetryBehaviour.DefaultMaxSpeed
$manifest.TelemetryBehaviour.HasRumbleStripsContactSurface=$false
foreach($field in $inputDefinition.TelemetryDefinition.Fields){
    if($field.TypeName -eq 'StandardField'){
        $typed=New-Object SimHub.Plugins.ExternalSims.Fields.StandardFields.StandardField
        $typed.FieldType=[Enum]::Parse([SimHub.Plugins.ExternalSims.Fields.StandardFields.StandardFieldType],$field.FieldType)
    }else{
        $typed=New-Object SimHub.Plugins.ExternalSims.Fields.CustomFields.CustomTelemetryField
        $typed.Name=$field.Name
        $typed.Comment=$field.Comment
        if($field.Name -eq 'Airborne'){$typed.Comment='VehicleState.IsInAir flag, 0 or 1'}
        $typed.BinaryType=$field.BinaryType
    }
    $manifest.TelemetryDefinition.Fields.Add($typed)
}
foreach($process in $inputDefinition.DetectionProcesses){
    $detection=New-Object SimHub.Plugins.ExternalSims.ExternalSimProcessDetection
    $detection.ProcessName=$process.ProcessName
    $manifest.DetectionProcesses.Add($detection)
}
$generator=New-Object SimHub.Plugins.ExternalSims.CodeGen.CppWin32Generator
$generated=$generator.Generate($manifest)
$utf8=New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $sourceRoot 'simhub\GalacticRacer.generated.cpp'),$generated,$utf8)
$manifest.SaveToFile($definitionPath)
$constants=[regex]::Match($generated,'(?s)    struct SHTelemetryConstants\s*\{.*?\n    \};').Value
$packet=[regex]::Match($generated,'(?s)    #pragma pack\(push, 1\).*?#pragma pack\(pop\)').Value
$assert=[regex]::Match($generated,'(?s)    static_assert\(sizeof\(SHTelemetryPacket\).*?;').Value
if(-not $constants -or -not $packet -or -not $assert){throw 'Unexpected SimHub generator output.'}
$header="#pragma once`n// Extracted unchanged from SimHub's generated sender.`n#include <cstdint>`nnamespace SimHubExternalTelemetrySender {`n"+$constants+"`n"+$packet+"`n"+$assert+"`n}`n"
[IO.File]::WriteAllText((Join-Path $sourceRoot 'src\SimHubGenerated.hpp'),$header,$utf8)
Write-Output ('GameSignature={0:X8} TelemetrySignature={1:X8} PacketLength={2}' -f $manifest.GameSignature,$manifest.TelemetrySignature,$manifest.PacketLength)
