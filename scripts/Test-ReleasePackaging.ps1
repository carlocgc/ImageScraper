param(
    [string]$RepoRoot = ( Resolve-Path ( Join-Path $PSScriptRoot ".." ) ).ProviderPath
)

$ErrorActionPreference = "Stop"

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Message
    )

    if( -not ( Test-Path -LiteralPath $Path ) ) {
        throw $Message
    }
}

function Assert-PathMissing {
    param(
        [string]$Path,
        [string]$Message
    )

    if( Test-Path -LiteralPath $Path ) {
        throw $Message
    }
}

$stageScriptPath = Join-Path $RepoRoot "scripts\Stage-PortableRelease.ps1"
Assert-PathExists $stageScriptPath "Expected portable release staging script at $stageScriptPath."

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("ImageScraperReleasePackaging-" + [System.Guid]::NewGuid().ToString("N"))
$releaseDir = Join-Path $tempRoot "release"
$stagingDir = Join-Path $tempRoot "staging"

try {
    New-Item -ItemType Directory -Force -Path ( Join-Path $releaseDir "resources\icons" ) | Out-Null

    Set-Content -Path ( Join-Path $releaseDir "ImageScraper.exe" ) -Value "exe"
    Set-Content -Path ( Join-Path $releaseDir "example.dll" ) -Value "dll"
    Set-Content -Path ( Join-Path $releaseDir "curl-ca-bundle.crt" ) -Value "crt"
    Set-Content -Path ( Join-Path $releaseDir "auth.html" ) -Value "html"
    Set-Content -Path ( Join-Path $releaseDir "ImageScraper.pdb" ) -Value "pdb"
    Set-Content -Path ( Join-Path $releaseDir "config.json" ) -Value "{}"
    Set-Content -Path ( Join-Path $releaseDir "imgui.ini" ) -Value "[imgui]"
    Set-Content -Path ( Join-Path $releaseDir "resources\icons\play.png" ) -Value "icon"

    & $stageScriptPath -ReleaseDir $releaseDir -StagingDir $stagingDir -RepoRoot $RepoRoot

    Assert-PathExists ( Join-Path $stagingDir "ImageScraper.exe" ) "Portable release staging did not include ImageScraper.exe."
    Assert-PathExists ( Join-Path $stagingDir "example.dll" ) "Portable release staging did not include DLL dependencies."
    Assert-PathExists ( Join-Path $stagingDir "curl-ca-bundle.crt" ) "Portable release staging did not include curl-ca-bundle.crt."
    Assert-PathExists ( Join-Path $stagingDir "auth.html" ) "Portable release staging did not include auth.html."
    Assert-PathExists ( Join-Path $stagingDir "resources\icons\play.png" ) "Portable release staging did not include media control icons."

    Assert-PathMissing ( Join-Path $stagingDir "ImageScraper.pdb" ) "Portable release staging should exclude PDB files."
    Assert-PathMissing ( Join-Path $stagingDir "config.json" ) "Portable release staging should exclude config.json."
    Assert-PathMissing ( Join-Path $stagingDir "imgui.ini" ) "Portable release staging should exclude imgui.ini."

    $installerScriptPath = Join-Path $RepoRoot "installer\ImageScraper.iss"
    $installerScript = Get-Content -Raw -Path $installerScriptPath

    if( $installerScript -notmatch 'Source:\s*"\{#SourceDir\}\\resources\\icons\\\*"' ) {
        throw "Installer script does not include resources\\icons."
    }

    if( $installerScript -notmatch 'DestDir:\s*"\{app\}\\resources\\icons"' ) {
        throw "Installer script does not install icons into {app}\\resources\\icons."
    }

    Write-Host "Release packaging regression check passed."
}
finally {
    if( Test-Path -LiteralPath $tempRoot ) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
