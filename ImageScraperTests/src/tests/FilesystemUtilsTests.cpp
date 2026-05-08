#include "catch2/catch_amalgamated.hpp"
#include "utils/FilesystemUtils.h"

#include <filesystem>
#include <fstream>

using namespace ImageScraper;

// RAII helper: creates a temp directory and removes it (with all contents) on destruction.
struct TempDir
{
    std::filesystem::path path;

    TempDir( )
    {
        path = std::filesystem::temp_directory_path( ) / "FilesystemUtilsTest";
        std::error_code ec;
        std::filesystem::create_directories( path, ec );
    }

    ~TempDir( )
    {
        std::error_code ec;
        std::filesystem::remove_all( path, ec );
    }

    // Create a file inside the temp dir and return its path.
    std::filesystem::path MakeFile( const std::string& name ) const
    {
        const std::filesystem::path filePath = path / name;
        std::ofstream{ filePath.string( ) };
        return filePath;
    }

    // Create a subdirectory inside the temp dir and return its path.
    std::filesystem::path MakeSubDir( const std::string& name ) const
    {
        const std::filesystem::path dirPath = path / name;
        std::error_code ec;
        std::filesystem::create_directories( dirPath, ec );
        return dirPath;
    }
};

// ---------------------------------------------------------------------------
// NormalisePath
// ---------------------------------------------------------------------------
TEST_CASE( "NormalisePath - empty path returns empty", "[FilesystemUtils]" )
{
    const std::filesystem::path result = FilesystemUtils::NormalisePath( {} );
    REQUIRE( result.empty( ) );
}

TEST_CASE( "NormalisePath - already canonical path is unchanged", "[FilesystemUtils]" )
{
    TempDir tmp;
    // The temp dir itself is a canonical path on most systems.
    const std::filesystem::path result = FilesystemUtils::NormalisePath( tmp.path );
    REQUIRE( !result.empty( ) );
    // make_preferred may change separators but must not change directory identity
    REQUIRE( std::filesystem::equivalent( result, tmp.path ) );
}

TEST_CASE( "NormalisePath - path with .. segments is resolved", "[FilesystemUtils]" )
{
    TempDir tmp;
    const std::filesystem::path subDir = tmp.MakeSubDir( "sub" );
    const std::filesystem::path dotDotPath = subDir / ".." / "sub";

    const std::filesystem::path result = FilesystemUtils::NormalisePath( dotDotPath );
    REQUIRE( !result.empty( ) );
    REQUIRE( std::filesystem::equivalent( result, subDir ) );
}

TEST_CASE( "NormalisePath - result uses preferred separator on Windows", "[FilesystemUtils]" )
{
    TempDir tmp;
    // Build a path with forward slashes and verify make_preferred converts them.
    const std::string forwardSlashStr = tmp.path.generic_string( ) + "/sub";
    const std::filesystem::path forwardSlash{ forwardSlashStr };
    tmp.MakeSubDir( "sub" );

    const std::filesystem::path result = FilesystemUtils::NormalisePath( forwardSlash );
    REQUIRE( !result.empty( ) );
    // On Windows the preferred separator is backslash; the result string
    // must not contain forward slashes after make_preferred.
    const std::string resultStr = result.string( );
    REQUIRE( resultStr.find( '/' ) == std::string::npos );
}

// ---------------------------------------------------------------------------
// DirectoryExists
// ---------------------------------------------------------------------------
TEST_CASE( "DirectoryExists - non-existent path returns false", "[FilesystemUtils]" )
{
    const std::filesystem::path missing{ "G:\\__does_not_exist_imagescraper_test__" };
    REQUIRE_FALSE( FilesystemUtils::DirectoryExists( missing ) );
}

TEST_CASE( "DirectoryExists - regular file returns false", "[FilesystemUtils]" )
{
    TempDir tmp;
    const std::filesystem::path file = tmp.MakeFile( "test.txt" );
    REQUIRE_FALSE( FilesystemUtils::DirectoryExists( file ) );
}

TEST_CASE( "DirectoryExists - existing directory returns true", "[FilesystemUtils]" )
{
    TempDir tmp;
    REQUIRE( FilesystemUtils::DirectoryExists( tmp.path ) );
}

// ---------------------------------------------------------------------------
// DirectoryHasEntries
// ---------------------------------------------------------------------------
TEST_CASE( "DirectoryHasEntries - non-existent path returns false", "[FilesystemUtils]" )
{
    const std::filesystem::path missing{ "G:\\__does_not_exist_imagescraper_test__" };
    REQUIRE_FALSE( FilesystemUtils::DirectoryHasEntries( missing ) );
}

TEST_CASE( "DirectoryHasEntries - empty directory returns false", "[FilesystemUtils]" )
{
    TempDir tmp;
    REQUIRE_FALSE( FilesystemUtils::DirectoryHasEntries( tmp.path ) );
}

TEST_CASE( "DirectoryHasEntries - directory with one file returns true", "[FilesystemUtils]" )
{
    TempDir tmp;
    tmp.MakeFile( "file.txt" );
    REQUIRE( FilesystemUtils::DirectoryHasEntries( tmp.path ) );
}

TEST_CASE( "DirectoryHasEntries - directory with subdirectory returns true", "[FilesystemUtils]" )
{
    TempDir tmp;
    tmp.MakeSubDir( "child" );
    REQUIRE( FilesystemUtils::DirectoryHasEntries( tmp.path ) );
}

TEST_CASE( "DirectoryHasEntries - regular file path returns false", "[FilesystemUtils]" )
{
    TempDir tmp;
    const std::filesystem::path file = tmp.MakeFile( "test.txt" );
    REQUIRE_FALSE( FilesystemUtils::DirectoryHasEntries( file ) );
}
