[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',

    [ValidateSet('Ninja', 'VisualStudio', 'VisualStudio2026', 'VisualStudio2022')]
    [string]$Generator = 'VisualStudio',

    [switch]$CpuOnly,
    [switch]$Clean,
    [switch]$SkipTests,
    [switch]$WarningsAsErrors,
    [switch]$Sanitize,
    [string]$VcpkgRoot = '',
    [string]$VcpkgTriplet = 'x64-windows-static-md',
    [string]$EpochGuiPath = '',
    [string]$EpochPlatformPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

function ConvertTo-CMakeBool([bool]$Value) {
    if ($Value) { return 'ON' }
    return 'OFF'
}

function Resolve-VcpkgRoot {
    param([string]$ExplicitRoot, [string]$ProjectRoot)

    $Candidates = @()
    if ($ExplicitRoot) { $Candidates += $ExplicitRoot }
    if ($env:VCPKG_ROOT) { $Candidates += $env:VCPKG_ROOT }
    if ($env:USERPROFILE) {
        $Candidates += (Join-Path $env:USERPROFILE 'source/repos/vcpkg')
        $Candidates += (Join-Path $env:USERPROFILE 'vcpkg')
    }
    $Candidates += (Join-Path $ProjectRoot 'external/vcpkg')

    foreach ($Candidate in $Candidates | Select-Object -Unique) {
        if (-not $Candidate) { continue }
        $Toolchain = Join-Path $Candidate 'scripts/buildsystems/vcpkg.cmake'
        if (Test-Path $Toolchain) {
            return (Resolve-Path $Candidate).Path
        }
    }
    return $null
}

function Initialize-Vcpkg {
    param([string]$Root)

    $Bootstrap = Join-Path $Root 'bootstrap-vcpkg.bat'
    $Executable = Join-Path $Root 'vcpkg.exe'
    if (-not (Test-Path $Executable)) {
        if (-not (Test-Path $Bootstrap)) {
            throw "vcpkg exists at '$Root', but bootstrap-vcpkg.bat is missing."
        }
        Write-Host 'Bootstrapping vcpkg...'
        & $Bootstrap -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg bootstrap failed with exit code $LASTEXITCODE."
        }
    }

    # This project has no dependency version constraints, so the manifest uses
    # the selected vcpkg checkout's current ports. Do not mutate or fetch the
    # user's repository merely to configure a build.
    $env:VCPKG_ROOT = $Root
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $Root 'scripts/windows-tools.ps1')

$CMakeExecutable = (Get-Command cmake.exe -ErrorAction Stop).Source
$CTestCommand = Get-Command ctest.exe -ErrorAction Stop
$GeneratorInfo = Resolve-EpochGenerator $Generator $CMakeExecutable
$GeneratorFlavor = $GeneratorInfo.Flavor
$IsVisualStudio = $GeneratorInfo.IsVisualStudio
$BuildFlavor = if ($CpuOnly) { 'cpu' } else { 'vulkan' }
$BuildDir = Join-Path $Root "out/build/$GeneratorFlavor-$BuildFlavor-$($Configuration.ToLowerInvariant())"

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

$CMakeArgs = @(
    '-S', $Root,
    '-B', $BuildDir,
    '-G', $GeneratorInfo.CMakeGenerator,
    "-DEPOCH_PARTICLE_BUILD_VULKAN=$(ConvertTo-CMakeBool (-not $CpuOnly.IsPresent))",
    '-DEPOCH_PARTICLE_BUILD_EXAMPLES=ON',
    "-DEPOCH_PARTICLE_BUILD_TESTS=$(ConvertTo-CMakeBool (-not $SkipTests.IsPresent))",
    "-DEPOCH_PARTICLE_WARNINGS_AS_ERRORS=$(ConvertTo-CMakeBool $WarningsAsErrors.IsPresent)",
    "-DEPOCH_PARTICLE_ENABLE_SANITIZERS=$(ConvertTo-CMakeBool $Sanitize.IsPresent)"
)

if ($IsVisualStudio) {
    $CMakeArgs += @('-A', 'x64')
} else {
    $CMakeArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
    if ($GeneratorInfo.NinjaPath) {
        $CMakeArgs += "-DCMAKE_MAKE_PROGRAM=$($GeneratorInfo.NinjaPath)"
    }
}

if (-not $CpuOnly) {
    $ResolvedVcpkgRoot = Resolve-VcpkgRoot $VcpkgRoot $Root
    if ($ResolvedVcpkgRoot) {
        Initialize-Vcpkg $ResolvedVcpkgRoot
        $Toolchain = Join-Path $ResolvedVcpkgRoot 'scripts/buildsystems/vcpkg.cmake'
        $CMakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$Toolchain"
        if ($VcpkgTriplet) {
            $CMakeArgs += "-DVCPKG_TARGET_TRIPLET=$VcpkgTriplet"
        }
        Write-Host "Using vcpkg: $ResolvedVcpkgRoot"
    } else {
        Write-Warning 'vcpkg was not found. Set VCPKG_ROOT or pass -VcpkgRoot. CMake will search system Vulkan and shaderc packages.'
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

Write-Host "Configuring $BuildFlavor $Configuration with $($GeneratorInfo.Name) in $BuildDir"
& $CMakeExecutable @CMakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$BuildArgs = @('--build', $BuildDir, '--parallel')
if ($IsVisualStudio) { $BuildArgs += @('--config', $Configuration) }
& $CMakeExecutable @BuildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipTests) {
    $CTestArgs = @('--test-dir', $BuildDir, '--output-on-failure')
    if ($IsVisualStudio) { $CTestArgs += @('-C', $Configuration) }
    & $CTestCommand.Source @CTestArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$BuildStateDirectory = Join-Path $Root 'out/build'
New-Item -ItemType Directory -Force -Path $BuildStateDirectory | Out-Null
$BuildState = [ordered]@{
    configuration = $Configuration
    generator = $GeneratorInfo.Name
    generatorFlavor = $GeneratorFlavor
    buildFlavor = $BuildFlavor
    buildDirectory = $BuildDir
    isVisualStudio = $IsVisualStudio
}
$BuildState | ConvertTo-Json | Set-Content (Join-Path $BuildStateDirectory 'last-build.json') -Encoding UTF8

Write-Host "Build complete: $BuildDir"
