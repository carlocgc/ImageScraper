param(
    [Parameter(Mandatory = $true)]
    [string]$ReleaseDir,

    [Parameter(Mandatory = $true)]
    [string]$StagingDir,

    [string]$RepoRoot = ( Resolve-Path ( Join-Path $PSScriptRoot ".." ) ).ProviderPath
)

$ErrorActionPreference = "Stop"

$resolvedReleaseDir = ( Resolve-Path $ReleaseDir ).ProviderPath

if( Test-Path -LiteralPath $StagingDir ) {
    Remove-Item -LiteralPath $StagingDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null

$excludedFiles = @(
    "ImageScraper.pdb",
    "config.json",
    "imgui.ini"
)

$releaseFiles = Get-ChildItem -Path $resolvedReleaseDir -File |
    Where-Object { $_.Name -notin $excludedFiles }

foreach( $file in $releaseFiles ) {
    Copy-Item -LiteralPath $file.FullName -Destination $StagingDir
}

$iconsSourceDir = Join-Path $resolvedReleaseDir "resources\icons"
if( Test-Path -LiteralPath $iconsSourceDir ) {
    $iconsDestinationDir = Join-Path $StagingDir "resources\icons"
    New-Item -ItemType Directory -Force -Path $iconsDestinationDir | Out-Null
    Copy-Item -Path ( Join-Path $iconsSourceDir "*" ) -Destination $iconsDestinationDir -Recurse -Force
}

$repoFiles = @(
    "LICENSE.txt",
    "THIRD_PARTY_LICENSES.md",
    "README.MD"
)

foreach( $repoFile in $repoFiles ) {
    Copy-Item -LiteralPath ( Join-Path $RepoRoot $repoFile ) -Destination $StagingDir
}
