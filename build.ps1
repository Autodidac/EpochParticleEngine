[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',

    [ValidateSet('Ninja', 'VisualStudio', 'VisualStudio2026', 'VisualStudio2022')]
    [string]$Generator = 'Ninja',

    [switch]$CpuOnly,
    [switch]$Clean,
    [switch]$SkipTests,
    [switch]$WarningsAsErrors,
    [switch]$Sanitize,
    [string]$EpochGuiPath = '',
    [string]$EpochPlatformPath = ''
)

$ErrorActionPreference = 'Stop'

function ConvertTo-CMakeBool([bool]$Value) {
    if ($Value) { return 'ON' }
    return 'OFF'
}

function Resolve-Generator([string]$Requested, [string]$CMakeExecutable) {
    if ($Requested -eq 'Ninja') { return 'Ninja' }

    $HelpText = (& $CMakeExecutable --help 2>&1 | Out-String)
    $HasVisualStudio2026 = $HelpText.Contains('Visual Studio 18 2026')
    $HasVisualStudio2022 = $HelpText.Contains('Visual Studio 17 2022')

    if ($Requested -eq 'VisualStudio') {
        if ($HasVisualStudio2026) { return 'VisualStudio2026' }
        if ($HasVisualStudio2022) { return 'VisualStudio2022' }
        throw 'This CMake installation exposes no supported Visual Studio generator.'
    }
    if ($Requested -eq 'VisualStudio2026' -and -not $HasVisualStudio2026) {
        throw 'Visual Studio 2026 generation requires CMake 4.2 or newer with the "Visual Studio 18 2026" generator.'
    }
    if ($Requested -eq 'VisualStudio2022' -and -not $HasVisualStudio2022) {
        throw 'This CMake installation does not expose the "Visual Studio 17 2022" generator.'
    }
    return $Requested
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$CMakeExecutable = (Get-Command cmake -ErrorAction Stop).Source
$ResolvedGenerator = Resolve-Generator $Generator $CMakeExecutable
$IsVisualStudio = $ResolvedGenerator -ne 'Ninja'
$GeneratorFlavor = switch ($ResolvedGenerator) {
    'VisualStudio2026' { 'vs2026' }
    'VisualStudio2022' { 'vs2022' }
    default { 'ninja' }
}
$BuildFlavor = if ($CpuOnly) { 'cpu' } else { 'vulkan' }
$BuildDir = Join-Path $Root "out/build/$GeneratorFlavor-$BuildFlavor-$($Configuration.ToLowerInvariant())"

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

$CMakeArgs = @(
    '-S', $Root,
    '-B', $BuildDir,
    "-DEPOCH_PARTICLE_BUILD_VULKAN=$(ConvertTo-CMakeBool (-not $CpuOnly.IsPresent))",
    '-DEPOCH_PARTICLE_BUILD_EXAMPLES=ON',
    "-DEPOCH_PARTICLE_BUILD_TESTS=$(ConvertTo-CMakeBool (-not $SkipTests.IsPresent))",
    "-DEPOCH_PARTICLE_WARNINGS_AS_ERRORS=$(ConvertTo-CMakeBool $WarningsAsErrors.IsPresent)",
    "-DEPOCH_PARTICLE_ENABLE_SANITIZERS=$(ConvertTo-CMakeBool $Sanitize.IsPresent)"
)

switch ($ResolvedGenerator) {
    'VisualStudio2026' {
        $CMakeArgs += @('-G', 'Visual Studio 18 2026', '-A', 'x64')
    }
    'VisualStudio2022' {
        $CMakeArgs += @('-G', 'Visual Studio 17 2022', '-A', 'x64')
    }
    default {
        $CMakeArgs += @('-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Configuration")
    }
}

if (-not $CpuOnly) {
    $Candidates = @()
    if ($env:VCPKG_ROOT) { $Candidates += $env:VCPKG_ROOT }
    $Candidates += @(
        'C:\Users\iammi\source\repos\vcpkg',
        (Join-Path $Root 'external/vcpkg')
    )

    $VcpkgRoot = $Candidates |
        Where-Object { $_ -and (Test-Path (Join-Path $_ 'scripts/buildsystems/vcpkg.cmake')) } |
        Select-Object -First 1

    if ($VcpkgRoot) {
        $Toolchain = Join-Path $VcpkgRoot 'scripts/buildsystems/vcpkg.cmake'
        $CMakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$Toolchain"
        Write-Host "Using vcpkg: $VcpkgRoot"
    } else {
        Write-Warning 'vcpkg was not found. CMake will search system Vulkan and shaderc packages.'
    }
}

if ($EpochGuiPath) {
    $ResolvedEpochGui = (Resolve-Path $EpochGuiPath).Path
    $CMakeArgs += @(
        '-DEPOCH_PARTICLE_WITH_EPOCHGUI=ON',
        "-DEPOCHGUI_SOURCE_DIR=$ResolvedEpochGui"
    )
}

if (-not $CpuOnly) {
    $PlatformCandidate = $EpochPlatformPath
    if (-not $PlatformCandidate) {
        $SiblingPlatform = Join-Path (Split-Path -Parent $Root) 'EpochPlatformEngine'
        if (Test-Path (Join-Path $SiblingPlatform 'CMakeLists.txt')) {
            $PlatformCandidate = $SiblingPlatform
        }
    }
    if ($PlatformCandidate) {
        $ResolvedEpochPlatform = (Resolve-Path $PlatformCandidate).Path
        $CMakeArgs += "-DEPOCH_PLATFORM_SOURCE_DIR=$ResolvedEpochPlatform"
        Write-Host "Using EpochPlatformEngine: $ResolvedEpochPlatform"
    }
}

Write-Host "Configuring $BuildFlavor $Configuration with $ResolvedGenerator in $BuildDir"
& $CMakeExecutable @CMakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$BuildArgs = @('--build', $BuildDir, '--parallel')
if ($IsVisualStudio) { $BuildArgs += @('--config', $Configuration) }
& $CMakeExecutable @BuildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipTests) {
    $CTestArgs = @('--test-dir', $BuildDir, '--output-on-failure')
    if ($IsVisualStudio) { $CTestArgs += @('-C', $Configuration) }
    & ctest @CTestArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Build complete: $BuildDir"
