# Guards the UTF-8 path convention introduced in PR #152.
#
# std::filesystem::path stores wchar_t on Windows, and path::string( ) /
# path::generic_string( ) convert it to the process ANSI code page. That
# conversion throws std::system_error for any character the code page cannot
# represent, so a single stray call crashes the app on non-Latin filenames.
# First-party code must use ImageScraper::FilesystemUtils::PathToUtf8 /
# PathToUtf8Generic / PathFromUtf8 instead, and the wide Win32 APIs rather than
# their ANSI counterparts.
#
# The baseline is zero: every occurrence in first-party code was removed in
# PR #152, so any hit here is a regression.
#
# A deliberate exception can be annotated on the offending line:
#     ShellExecuteA( ... ); // utf8-guard: ok - ASCII URL, not a path

param(
    [string]$RepoRoot = ( Resolve-Path ( Join-Path $PSScriptRoot ".." ) ).ProviderPath
)

$ErrorActionPreference = "Stop"

# Roots holding first-party C++. Anything added under these is covered by default.
$scanRoots = @(
    "ImageScraper\src",
    "ImageScraper\include",
    "ImageScraperTests\src"
)

# Vendored / third-party trees are excluded by path segment. Listing what to
# skip (rather than what to scan) means new first-party folders are covered
# without touching this script.
$excludedSegments = @(
    "imgui",
    "catch2",
    "nlohmann",
    "curl",
    "curlpp",
    "cppcodec",
    "utilspp",
    "GLFW",
    "libavcodec",
    "libavdevice",
    "libavfilter",
    "libavformat",
    "libavutil",
    "libswresample",
    "libswscale"
)

$bannedPatterns = @(
    @{
        Pattern     = '\.\s*string\s*\(\s*\)'
        Description = 'path::string( ) converts to the ANSI code page and throws on non-representable filenames'
        Replacement = 'ImageScraper::FilesystemUtils::PathToUtf8( path )'
    },
    @{
        Pattern     = '\.\s*generic_string\s*\(\s*\)'
        Description = 'path::generic_string( ) converts to the ANSI code page and throws on non-representable filenames'
        Replacement = 'ImageScraper::FilesystemUtils::PathToUtf8Generic( path )'
    },
    @{
        Pattern     = '\bGetModuleFileNameA\b'
        Description = 'ANSI Win32 path API mangles non-representable characters'
        Replacement = 'GetModuleFileNameW'
    },
    @{
        Pattern     = '\bShellExecuteA\b'
        Description = 'ANSI Win32 API reinterprets UTF-8 arguments as the local code page'
        Replacement = 'ShellExecuteW with StringUtils::Utf8ToWideString'
    },
    @{
        Pattern     = '\b(CreateFileA|FindFirstFileA|FindNextFileA|GetTempPathA|SHGetFolderPathA)\b'
        Description = 'ANSI Win32 path API mangles non-representable characters'
        Replacement = 'the matching *W entry point'
    }
)

$suppressionPattern = '//\s*utf8-guard:\s*ok'

function Test-IsExcluded {
    param( [string]$RelativePath )

    foreach( $segment in $excludedSegments ) {
        if( $RelativePath -like "*\$segment\*" ) {
            return $true
        }
    }

    return $false
}

$violations = @()
$scannedFiles = 0

foreach( $scanRoot in $scanRoots ) {
    $rootPath = Join-Path $RepoRoot $scanRoot
    if( -not ( Test-Path -LiteralPath $rootPath ) ) {
        throw "Expected source root at $rootPath."
    }

    $sourceFiles = Get-ChildItem -LiteralPath $rootPath -Recurse -File -Include *.cpp, *.h, *.hpp

    foreach( $sourceFile in $sourceFiles ) {
        $relativePath = $sourceFile.FullName.Substring( $RepoRoot.Length ).TrimStart( '\' )
        if( Test-IsExcluded $relativePath ) {
            continue
        }

        $scannedFiles++
        $lineNumber = 0

        foreach( $line in ( Get-Content -LiteralPath $sourceFile.FullName ) ) {
            $lineNumber++

            if( $line -match $suppressionPattern ) {
                continue
            }

            foreach( $banned in $bannedPatterns ) {
                if( $line -match $banned.Pattern ) {
                    $violations += [PSCustomObject]@{
                        File        = $relativePath
                        Line        = $lineNumber
                        Text        = $line.Trim( )
                        Description = $banned.Description
                        Replacement = $banned.Replacement
                    }
                }
            }
        }
    }
}

if( $scannedFiles -eq 0 ) {
    throw "UTF-8 path guard scanned no files - the scan roots are probably wrong."
}

if( $violations.Count -gt 0 ) {
    Write-Host "UTF-8 path guard found $( $violations.Count ) violation(s):" -ForegroundColor Red
    Write-Host ""

    foreach( $violation in $violations ) {
        Write-Host "  $( $violation.File ):$( $violation.Line )" -ForegroundColor Yellow
        Write-Host "    $( $violation.Text )"
        Write-Host "    $( $violation.Description )."
        Write-Host "    Use $( $violation.Replacement ) instead."
        Write-Host ""
    }

    Write-Host "If a hit is genuinely safe, annotate the line with '// utf8-guard: ok - <reason>'."
    throw "UTF-8 path guard failed with $( $violations.Count ) violation(s)."
}

Write-Host "UTF-8 path guard passed ($scannedFiles first-party source files scanned)."
