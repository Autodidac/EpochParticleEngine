Set-StrictMode -Version 3.0

function Get-EpochVisualStudioInstances {
    $Candidates = @()
    if (${env:ProgramFiles(x86)}) {
        $Candidates += (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe')
    }
    if ($env:ProgramFiles) {
        $Candidates += (Join-Path $env:ProgramFiles 'Microsoft Visual Studio/Installer/vswhere.exe')
    }

    $VsWhere = $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $VsWhere) { return @() }

    $JsonLines = @(& $VsWhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json -utf8 2>$null)
    if ($LASTEXITCODE -ne 0 -or $JsonLines.Count -eq 0) { return @() }

    $Json = $JsonLines -join [Environment]::NewLine
    if ([string]::IsNullOrWhiteSpace($Json)) { return @() }

    try {
        $Parsed = ConvertFrom-Json -InputObject $Json
    } catch {
        Write-Warning "vswhere returned invalid JSON: $($_.Exception.Message)"
        return @()
    }

    # Windows PowerShell 5.1 does not enumerate a top-level JSON array when it
    # is passed directly through the pipeline. Normalize each instance first.
    $Instances = @()
    foreach ($Instance in $Parsed) {
        if ($null -eq $Instance) { continue }

        $VersionText = [string]$Instance.installationVersion
        $Version = $null
        try {
            $Version = [version]::Parse($VersionText)
        } catch {
            Write-Warning "Ignoring Visual Studio instance with invalid version '$VersionText'."
            continue
        }

        $InstallationPath = [string]$Instance.installationPath
        if ([string]::IsNullOrWhiteSpace($InstallationPath)) { continue }

        $Instances += [pscustomobject]@{
            installationPath = $InstallationPath
            installationVersion = $VersionText
            parsedVersion = $Version
            majorVersion = $Version.Major
        }
    }

    return @($Instances | Sort-Object -Property parsedVersion -Descending)
}

function Find-EpochNinjaExecutable {
    param([object[]]$VisualStudioInstances)

    $Command = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if ($Command) { return $Command.Source }

    foreach ($Instance in $VisualStudioInstances) {
        $Candidate = Join-Path $Instance.installationPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
        if (Test-Path $Candidate) { return $Candidate }
    }

    return $null
}

function Resolve-EpochVisualStudioGenerator {
    param(
        [string]$Requested,
        [string]$CMakeHelp,
        [object[]]$VisualStudioInstances
    )

    $Supports2026 = $CMakeHelp.Contains('Visual Studio 18 2026')
    $Supports2022 = $CMakeHelp.Contains('Visual Studio 17 2022')
    $VisualStudio2026 = $VisualStudioInstances | Where-Object {
        $_.majorVersion -eq 18
    } | Select-Object -First 1
    $VisualStudio2022 = $VisualStudioInstances | Where-Object {
        $_.majorVersion -eq 17
    } | Select-Object -First 1

    if ($Requested -eq 'VisualStudio2026') {
        if (-not $Supports2026) {
            throw 'CMake does not expose the "Visual Studio 18 2026" generator. Install CMake 4.2 or newer.'
        }
        if ($VisualStudioInstances.Count -gt 0 -and -not $VisualStudio2026) {
            throw 'Visual Studio 2026 with the Desktop development with C++ workload is not installed.'
        }
        return [pscustomobject]@{
            Name = 'VisualStudio2026'
            CMakeGenerator = 'Visual Studio 18 2026'
            Flavor = 'vs2026'
            IsVisualStudio = $true
            NinjaPath = $null
        }
    }

    if ($Requested -eq 'VisualStudio2022') {
        if (-not $Supports2022) {
            throw 'CMake does not expose the "Visual Studio 17 2022" generator.'
        }
        if ($VisualStudioInstances.Count -gt 0 -and -not $VisualStudio2022) {
            throw 'Visual Studio 2022 with the Desktop development with C++ workload is not installed.'
        }
        return [pscustomobject]@{
            Name = 'VisualStudio2022'
            CMakeGenerator = 'Visual Studio 17 2022'
            Flavor = 'vs2022'
            IsVisualStudio = $true
            NinjaPath = $null
        }
    }

    if ($VisualStudio2026 -and $Supports2026) {
        return [pscustomobject]@{
            Name = 'VisualStudio2026'
            CMakeGenerator = 'Visual Studio 18 2026'
            Flavor = 'vs2026'
            IsVisualStudio = $true
            NinjaPath = $null
        }
    }
    if ($VisualStudio2022 -and $Supports2022) {
        return [pscustomobject]@{
            Name = 'VisualStudio2022'
            CMakeGenerator = 'Visual Studio 17 2022'
            Flavor = 'vs2022'
            IsVisualStudio = $true
            NinjaPath = $null
        }
    }

    if ($VisualStudioInstances.Count -eq 0) {
        if ($Supports2026) {
            return [pscustomobject]@{
                Name = 'VisualStudio2026'
                CMakeGenerator = 'Visual Studio 18 2026'
                Flavor = 'vs2026'
                IsVisualStudio = $true
                NinjaPath = $null
            }
        }
        if ($Supports2022) {
            return [pscustomobject]@{
                Name = 'VisualStudio2022'
                CMakeGenerator = 'Visual Studio 17 2022'
                Flavor = 'vs2022'
                IsVisualStudio = $true
                NinjaPath = $null
            }
        }
    }

    throw 'No supported Visual Studio C++ installation was found.'
}

function Resolve-EpochGenerator {
    param(
        [ValidateSet('Ninja', 'VisualStudio', 'VisualStudio2026', 'VisualStudio2022')]
        [string]$Requested,
        [string]$CMakeExecutable
    )

    $CMakeHelp = (& $CMakeExecutable --help 2>&1 | Out-String)
    $VisualStudioInstances = @(Get-EpochVisualStudioInstances)

    if ($Requested -ne 'Ninja') {
        return Resolve-EpochVisualStudioGenerator -Requested $Requested -CMakeHelp $CMakeHelp -VisualStudioInstances $VisualStudioInstances
    }

    $NinjaPath = Find-EpochNinjaExecutable -VisualStudioInstances $VisualStudioInstances
    $Compiler = @('cl.exe', 'clang++.exe', 'g++.exe') | ForEach-Object {
        Get-Command $_ -ErrorAction SilentlyContinue
    } | Select-Object -First 1
    $CompilerAvailable = $null -ne $Compiler

    if ($NinjaPath -and $CompilerAvailable) {
        return [pscustomobject]@{
            Name = 'Ninja'
            CMakeGenerator = 'Ninja'
            Flavor = 'ninja'
            IsVisualStudio = $false
            NinjaPath = $NinjaPath
        }
    }

    $Reason = if (-not $NinjaPath) {
        'Ninja was not found.'
    } else {
        'Ninja was found, but this PowerShell session has no C++ compiler environment.'
    }
    Write-Warning "$Reason Falling back to the newest installed Visual Studio generator."
    return Resolve-EpochVisualStudioGenerator -Requested 'VisualStudio' -CMakeHelp $CMakeHelp -VisualStudioInstances $VisualStudioInstances
}
