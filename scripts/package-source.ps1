[CmdletBinding()]
param(
    [string]$OutputDirectory = '',
    [switch]$IncludeBuilds
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $Root 'out/packages'
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$VersionHeader = Join-Path $Root 'include/epochengine/particle/version.hpp'
$VersionText = Get-Content -Raw $VersionHeader
$Match = [regex]::Match($VersionText, 'version_string\s*=\s*"([^"]+)"')
if (-not $Match.Success) { throw 'Could not read version_string.' }
$Version = $Match.Groups[1].Value
$Archive = Join-Path $OutputDirectory "EpochParticleEngine-v$Version-source.zip"
if (Test-Path $Archive) { Remove-Item -Force $Archive }

$Staging = Join-Path ([System.IO.Path]::GetTempPath()) "EpochParticleEngine-package-$PID"
if (Test-Path $Staging) { Remove-Item -Recurse -Force $Staging }
New-Item -ItemType Directory -Force -Path $Staging | Out-Null
$Destination = Join-Path $Staging "EpochParticleEngine-v$Version"
New-Item -ItemType Directory -Force -Path $Destination | Out-Null

$Excluded = @('.git', '.vs')
if (-not $IncludeBuilds) { $Excluded += @('build', 'out') }
Get-ChildItem -Force $Root | Where-Object { $Excluded -notcontains $_.Name -and $_.Name -notlike 'build-*' } |
    ForEach-Object { Copy-Item -Recurse -Force $_.FullName $Destination }

Compress-Archive -Path $Destination -DestinationPath $Archive -CompressionLevel Optimal
Remove-Item -Recurse -Force $Staging
$Hash = (Get-FileHash $Archive -Algorithm SHA256).Hash.ToLowerInvariant()
$HashPath = "$Archive.sha256"
"$Hash  $(Split-Path -Leaf $Archive)" | Set-Content $HashPath -NoNewline
Write-Host $Archive
Write-Host $HashPath
