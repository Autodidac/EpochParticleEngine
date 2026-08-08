[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [ValidateSet('Ninja', 'VisualStudio', 'VisualStudio2026', 'VisualStudio2022')]
    [string]$Generator = 'VisualStudio',
    [string]$VcpkgRoot = '',
    [string]$EpochGuiPath = '',
    [string]$EpochPlatformPath = '',
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ApplicationArguments
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path

$BuildParameters = @{
    Configuration = $Configuration
    Generator = $Generator
    SkipTests = $true
}
if ($VcpkgRoot) { $BuildParameters.VcpkgRoot = $VcpkgRoot }
if ($EpochGuiPath) { $BuildParameters.EpochGuiPath = $EpochGuiPath }
if ($EpochPlatformPath) { $BuildParameters.EpochPlatformPath = $EpochPlatformPath }
& (Join-Path $Root 'build.ps1') @BuildParameters
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$BuildStatePath = Join-Path $Root 'out/build/last-build.json'
if (-not (Test-Path $BuildStatePath)) {
    throw "Build state was not written to $BuildStatePath"
}
$BuildState = Get-Content -Raw $BuildStatePath | ConvertFrom-Json
$BuildDir = $BuildState.buildDirectory
$IsVisualStudio = [bool]$BuildState.isVisualStudio

$Candidates = if ($IsVisualStudio) {
    @((Join-Path $BuildDir "$Configuration/EpochParticleLab.exe"))
} else {
    @(
        (Join-Path $BuildDir 'EpochParticleLab.exe'),
        (Join-Path $BuildDir 'EpochParticleLab')
    )
}
$Executable = $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Executable) { throw "EpochParticleLab was not found in $BuildDir" }
& $Executable @ApplicationArguments
exit $LASTEXITCODE
