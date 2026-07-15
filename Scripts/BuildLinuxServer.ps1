#requires -Version 5.1
[CmdletBinding()]
param(
	[string]$Project,
	[string]$EngineDir = $env:UE_ENGINE_DIR,
	[ValidateSet('Debug', 'DebugGame', 'Development', 'Test', 'Shipping')]
	[string]$Configuration = 'Development',
	[string]$ArchiveDir,
	[string[]]$Maps = @(
		'/Game/PingPong/Maps/EntryMap',
		'/Game/PingPong/Maps/TransitionMap',
		'/Game/PingPong/Maps/ClassicPong',
		'/Game/PingPong/Maps/GameMap',
		'/Game/PingPong/Maps/GameMap4PlayersAtari'
	),
	[switch]$CompileOnly,
	[switch]$SkipCook,
	[switch]$SkipStage,
	[switch]$NoArchive,
	[switch]$Clean,
	[switch]$ForceUseSystemCompiler,
	[string[]]$AdditionalUatArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RequiredLinuxToolchain = 'v21_clang-15.0.1-centos7'
$LinuxArchitecture = 'x86_64-unknown-linux-gnu'
$ScriptRoot = if (-not [string]::IsNullOrWhiteSpace($PSScriptRoot)) {
	$PSScriptRoot
} else {
	Split-Path -Parent $MyInvocation.MyCommand.Path
}

function Resolve-FullPath {
	param([Parameter(Mandatory = $true)][string]$Path)

	$ExpandedPath = [Environment]::ExpandEnvironmentVariables($Path)
	if ([System.IO.Path]::IsPathRooted($ExpandedPath)) {
		return [System.IO.Path]::GetFullPath($ExpandedPath)
	}

	return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $ExpandedPath))
}

function Get-HostSdkName {
	switch ([System.Environment]::OSVersion.Platform) {
		'Win32NT' { return 'HostWin64' }
		'MacOSX' { return 'HostMac' }
		default { return 'HostLinux' }
	}
}

function Get-HostBinarySuffix {
	if ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT) {
		return '.exe'
	}

	return ''
}

function Get-EnvironmentValue {
	param([Parameter(Mandatory = $true)][string]$Name)

	$Value = [Environment]::GetEnvironmentVariable($Name, 'Process')
	if (-not [string]::IsNullOrWhiteSpace($Value)) {
		return $Value
	}

	$Value = [Environment]::GetEnvironmentVariable($Name, 'User')
	if (-not [string]::IsNullOrWhiteSpace($Value)) {
		return $Value
	}

	$Value = [Environment]::GetEnvironmentVariable($Name, 'Machine')
	if (-not [string]::IsNullOrWhiteSpace($Value)) {
		return $Value
	}

	return $null
}

function Test-EngineDir {
	param([Parameter(Mandatory = $true)][string]$Candidate)

	$RunUatPath = Join-Path $Candidate 'Engine/Build/BatchFiles/RunUAT.bat'
	$RunUatShellPath = Join-Path $Candidate 'Engine/Build/BatchFiles/RunUAT.sh'
	return (Test-Path -LiteralPath $RunUatPath) -or (Test-Path -LiteralPath $RunUatShellPath)
}

function Resolve-EngineDirFromAssociation {
	param(
		[Parameter(Mandatory = $true)][string]$Association,
		[Parameter(Mandatory = $true)][string]$ProjectRoot
	)

	if ([string]::IsNullOrWhiteSpace($Association)) {
		return $null
	}

	$AssociationExpanded = [Environment]::ExpandEnvironmentVariables($Association)
	if ([System.IO.Path]::IsPathRooted($AssociationExpanded) -and (Test-EngineDir $AssociationExpanded)) {
		return [System.IO.Path]::GetFullPath($AssociationExpanded)
	}

	$RelativeAssociation = Join-Path $ProjectRoot $AssociationExpanded
	if ((Test-Path -LiteralPath $RelativeAssociation) -and (Test-EngineDir $RelativeAssociation)) {
		return [System.IO.Path]::GetFullPath($RelativeAssociation)
	}

	if ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT) {
		$BuildsKey = 'HKCU:\Software\Epic Games\Unreal Engine\Builds'
		$Builds = Get-ItemProperty -LiteralPath $BuildsKey -ErrorAction SilentlyContinue
		if ($null -ne $Builds) {
			$BuildProperty = $Builds.PSObject.Properties | Where-Object { $_.Name -eq $Association } | Select-Object -First 1
			if (($null -ne $BuildProperty) -and (Test-EngineDir $BuildProperty.Value)) {
				return [System.IO.Path]::GetFullPath($BuildProperty.Value)
			}
		}

		$InstalledKey = Join-Path 'HKLM:\SOFTWARE\EpicGames\Unreal Engine' $Association
		$Installed = Get-ItemProperty -LiteralPath $InstalledKey -ErrorAction SilentlyContinue
		if (($null -ne $Installed) -and $Installed.InstalledDirectory -and (Test-EngineDir $Installed.InstalledDirectory)) {
			return [System.IO.Path]::GetFullPath($Installed.InstalledDirectory)
		}
	}

	return $null
}

function Resolve-EngineDir {
	param(
		[string]$ExplicitEngineDir,
		[Parameter(Mandatory = $true)][string]$ProjectFile
	)

	if (-not [string]::IsNullOrWhiteSpace($ExplicitEngineDir)) {
		$ResolvedExplicitEngineDir = Resolve-FullPath $ExplicitEngineDir
		if (Test-EngineDir $ResolvedExplicitEngineDir) {
			return $ResolvedExplicitEngineDir
		}

		throw "EngineDir '$ResolvedExplicitEngineDir' does not contain Engine/Build/BatchFiles/RunUAT."
	}

	$ProjectRoot = Split-Path -Parent $ProjectFile
	$ProjectJson = Get-Content -Raw -LiteralPath $ProjectFile | ConvertFrom-Json
	$Association = $ProjectJson.EngineAssociation
	$ResolvedAssociatedEngineDir = Resolve-EngineDirFromAssociation -Association $Association -ProjectRoot $ProjectRoot
	if (-not [string]::IsNullOrWhiteSpace($ResolvedAssociatedEngineDir)) {
		return $ResolvedAssociatedEngineDir
	}

	throw @"
Unable to resolve Unreal Engine directory.
Pass -EngineDir, set UE_ENGINE_DIR, or register the EngineAssociation from '$ProjectFile'.
Current EngineAssociation: '$Association'
"@
}

function Get-RunUatPath {
	param([Parameter(Mandatory = $true)][string]$ResolvedEngineDir)

	if ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT) {
		return Join-Path $ResolvedEngineDir 'Engine/Build/BatchFiles/RunUAT.bat'
	}

	return Join-Path $ResolvedEngineDir 'Engine/Build/BatchFiles/RunUAT.sh'
}

function Get-BuildBatchPath {
	param([Parameter(Mandatory = $true)][string]$ResolvedEngineDir)

	if ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT) {
		return Join-Path $ResolvedEngineDir 'Engine/Build/BatchFiles/Build.bat'
	}
	if ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::MacOSX) {
		return Join-Path $ResolvedEngineDir 'Engine/Build/BatchFiles/Mac/Build.sh'
	}

	return Join-Path $ResolvedEngineDir 'Engine/Build/BatchFiles/Linux/Build.sh'
}

function Test-LinuxSdk {
	param([Parameter(Mandatory = $true)][string]$ResolvedEngineDir)

	if ($ForceUseSystemCompiler) {
		return
	}

	$HostSdkName = Get-HostSdkName
	$BinarySuffix = Get-HostBinarySuffix
	$CandidateRoots = New-Object System.Collections.Generic.List[string]
	$CandidateClangPaths = New-Object System.Collections.Generic.List[string]
	$LinuxMultiArchRoot = Get-EnvironmentValue 'LINUX_MULTIARCH_ROOT'
	$LinuxRoot = Get-EnvironmentValue 'LINUX_ROOT'
	$UeSdksRoot = Get-EnvironmentValue 'UE_SDKS_ROOT'

	if (-not [string]::IsNullOrWhiteSpace($LinuxMultiArchRoot)) {
		$CandidateRoots.Add($LinuxMultiArchRoot)
		$env:LINUX_MULTIARCH_ROOT = $LinuxMultiArchRoot
	}
	if (-not [string]::IsNullOrWhiteSpace($LinuxRoot)) {
		$CandidateClangPaths.Add((Join-Path $LinuxRoot "bin/clang++$BinarySuffix"))
		$env:LINUX_ROOT = $LinuxRoot
	}

	if (-not [string]::IsNullOrWhiteSpace($UeSdksRoot)) {
		$CandidateRoots.Add((Join-Path $UeSdksRoot "$HostSdkName/Linux_x64/$RequiredLinuxToolchain"))
		$env:UE_SDKS_ROOT = $UeSdksRoot
	}

	$CandidateRoots.Add((Join-Path $ResolvedEngineDir "Engine/Extras/ThirdPartyNotUE/SDKs/$HostSdkName/Linux_x64/$RequiredLinuxToolchain"))

	foreach ($CandidateRoot in $CandidateRoots) {
		$CandidateClangPaths.Add((Join-Path $CandidateRoot "$LinuxArchitecture/bin/clang++$BinarySuffix"))
	}

	foreach ($CandidateClangPath in $CandidateClangPaths) {
		if (Test-Path -LiteralPath $CandidateClangPath) {
			return
		}
	}

	$CandidatesText = ($CandidateClangPaths | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
	throw @"
Linux toolchain for UE 5.2 was not found.
Expected toolchain: $RequiredLinuxToolchain
Expected architecture: $LinuxArchitecture

Install the matching Unreal Linux toolchain and set LINUX_MULTIARCH_ROOT to the directory that contains '$LinuxArchitecture',
set legacy LINUX_ROOT to the '$LinuxArchitecture' directory, or place it under one of these locations:
$CandidatesText

Use -ForceUseSystemCompiler only when building natively on Linux with a compatible clang/llvm toolchain available in PATH.
"@
}

if ([string]::IsNullOrWhiteSpace($Project)) {
	$Project = Join-Path $ScriptRoot '../PingPong.uproject'
}

$ProjectFile = Resolve-FullPath $Project
if (-not (Test-Path -LiteralPath $ProjectFile)) {
	throw "Project file was not found: $ProjectFile"
}

$ProjectRoot = Split-Path -Parent $ProjectFile
$ResolvedEngineDir = Resolve-EngineDir -ExplicitEngineDir $EngineDir -ProjectFile $ProjectFile

if ([string]::IsNullOrWhiteSpace($ArchiveDir)) {
	$ArchiveDir = Join-Path $ProjectRoot "Saved/StagedBuilds/LinuxServer-$Configuration"
}
$ArchiveDir = Resolve-FullPath $ArchiveDir

Test-LinuxSdk -ResolvedEngineDir $ResolvedEngineDir

if ($CompileOnly) {
	$BuildBatchPath = Get-BuildBatchPath -ResolvedEngineDir $ResolvedEngineDir
	if (-not (Test-Path -LiteralPath $BuildBatchPath)) {
		throw "Build script was not found: $BuildBatchPath"
	}

	$BuildArgs = @(
		'PingPongServer',
		'Linux',
		$Configuration,
		"-Project=$ProjectFile",
		'-WaitMutex',
		'-NoHotReload'
	)

	if ($Clean) {
		$BuildArgs += '-Clean'
	}
	if ($ForceUseSystemCompiler) {
		$BuildArgs += '-ForceUseSystemCompiler'
	}

	Write-Host "Engine: $ResolvedEngineDir"
	Write-Host "Project: $ProjectFile"
	Write-Host "Running: $BuildBatchPath $($BuildArgs -join ' ')"
	& $BuildBatchPath @BuildArgs
	if ($LASTEXITCODE -ne 0) {
		throw "Linux server compile failed with exit code $LASTEXITCODE."
	}

	exit 0
}

$RunUatPath = Get-RunUatPath -ResolvedEngineDir $ResolvedEngineDir
if (-not (Test-Path -LiteralPath $RunUatPath)) {
	throw "RunUAT script was not found: $RunUatPath"
}

$UatArgs = @(
	'BuildCookRun',
	"-project=$ProjectFile",
	'-noP4',
	'-server',
	'-noclient',
	'-serverplatform=Linux',
	"-serverconfig=$Configuration",
	'-target=PingPongServer',
	'-build',
	'-cook',
	'-stage',
	'-pak',
	'-utf8output'
)

if ($Maps.Count -gt 0) {
	$UatArgs += "-MapsToCook=$($Maps -join '+')"
}
if ($Clean) {
	$UatArgs += '-clean'
}
if ($SkipCook) {
	$UatArgs += '-skipcook'
}
if ($SkipStage) {
	$UatArgs += '-skipstage'
}
if (-not $NoArchive) {
	$UatArgs += '-archive'
	$UatArgs += "-archivedirectory=$ArchiveDir"
}
if ($ForceUseSystemCompiler) {
	$UatArgs += '-ubtargs=-ForceUseSystemCompiler'
}
if ($AdditionalUatArgs.Count -gt 0) {
	$UatArgs += $AdditionalUatArgs
}

Write-Host "Engine: $ResolvedEngineDir"
Write-Host "Project: $ProjectFile"
Write-Host "Archive: $ArchiveDir"
Write-Host "Running: $RunUatPath $($UatArgs -join ' ')"

& $RunUatPath @UatArgs
if ($LASTEXITCODE -ne 0) {
	throw "Linux server package failed with exit code $LASTEXITCODE."
}
