[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [ValidateSet('Ninja', 'VisualStudio', 'VisualStudio2026', 'VisualStudio2022')]
    [string]$Generator = 'Ninja',
    [string]$EpochGuiPath = '',
    [string]$EpochPlatformPath = '',
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ApplicationArguments
)

$ErrorActionPreference = 'Stop'

function Resolve-GeneratorFlavor([string]$Requested) {
    if ($Requested -eq 'Ninja') { return 'ninja' }
    if ($Requested -eq 'VisualStudio2026') { return 'vs2026' }
    if ($Requested -eq 'VisualStudio2022') { return 'vs2022' }

    $CMakeExecutable = (Get-Command cmake -ErrorAction Stop).Source
    $HelpText = (& $CMakeExecutable --help 2>&1 | Out-String)
    if ($HelpText.Contains('Visual Studio 18 2026')) { return 'vs2026' }
    if ($HelpText.Contains('Visual Studio 17 2022')) { return 'vs2022' }
    throw 'This CMake installation exposes no supported Visual Studio generator.'
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildParameters = @{ Configuration = $Configuration; Generator = $Generator; SkipTests = $true }
if ($EpochGuiPath) { $BuildParameters.EpochGuiPath = $EpochGuiPath }
if ($EpochPlatformPath) { $BuildParameters.EpochPlatformPath = $EpochPlatformPath }
& (Join-Path $Root 'build.ps1') @BuildParameters
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$GeneratorFlavor = Resolve-GeneratorFlavor $Generator
$IsVisualStudio = $GeneratorFlavor -ne 'ninja'
$BuildDir = Join-Path $Root "out/build/$GeneratorFlavor-vulkan-$($Configuration.ToLowerInvariant())"
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
