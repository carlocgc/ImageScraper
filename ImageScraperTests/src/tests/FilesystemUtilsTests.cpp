#include "CppUnitTest.h"
#include "utils/FilesystemUtils.h"

#include <filesystem>
#include <fstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

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
            std::ofstream{ filePath };
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

    TEST_CLASS(FilesystemUtilsTests)
    {
    public:
    TEST_METHOD(NormalisePath_Empty_Path_Returns_Empty)
    {
        const std::filesystem::path result = FilesystemUtils::NormalisePath( {} );
        Assert::IsTrue(  result.empty( ) );
    }
    
    TEST_METHOD(NormalisePath_Already_Canonical_Path_Is_Unchanged)
    {
        TempDir tmp;
        // The temp dir itself is a canonical path on most systems.
        const std::filesystem::path result = FilesystemUtils::NormalisePath( tmp.path );
        Assert::IsTrue(  !result.empty( ) );
        // make_preferred may change separators but must not change directory identity
        Assert::IsTrue(  std::filesystem::equivalent( result, tmp.path ) );
    }
    
    TEST_METHOD(NormalisePath_Path_With_Segments_Is_Resolved)
    {
        TempDir tmp;
        const std::filesystem::path subDir = tmp.MakeSubDir( "sub" );
        const std::filesystem::path dotDotPath = subDir / ".." / "sub";
    
        const std::filesystem::path result = FilesystemUtils::NormalisePath( dotDotPath );
        Assert::IsTrue(  !result.empty( ) );
        Assert::IsTrue(  std::filesystem::equivalent( result, subDir ) );
    }
    
    TEST_METHOD(NormalisePath_Result_Uses_Preferred_Separator_On_Windows)
    {
        TempDir tmp;
        // Build a path with forward slashes and verify make_preferred converts them.
        const std::string forwardSlashStr = FilesystemUtils::PathToUtf8Generic( tmp.path ) + "/sub";
        const std::filesystem::path forwardSlash{ forwardSlashStr };
        tmp.MakeSubDir( "sub" );
    
        const std::filesystem::path result = FilesystemUtils::NormalisePath( forwardSlash );
        Assert::IsTrue(  !result.empty( ) );
        // On Windows the preferred separator is backslash; the result string
        // must not contain forward slashes after make_preferred.
        const std::string resultStr = FilesystemUtils::PathToUtf8( result );
        Assert::IsTrue(  resultStr.find( '/' ) == std::string::npos );
    }
    
    // ---------------------------------------------------------------------------
    // DirectoryExists
    // ---------------------------------------------------------------------------
    TEST_METHOD(DirectoryExists_Non_Existent_Path_Returns_False)
    {
        const std::filesystem::path missing{ "G:\\__does_not_exist_imagescraper_test__" };
        Assert::IsFalse(  FilesystemUtils::DirectoryExists( missing ) );
    }
    
    TEST_METHOD(DirectoryExists_Regular_File_Returns_False)
    {
        TempDir tmp;
        const std::filesystem::path file = tmp.MakeFile( "test.txt" );
        Assert::IsFalse(  FilesystemUtils::DirectoryExists( file ) );
    }
    
    TEST_METHOD(DirectoryExists_Existing_Directory_Returns_True)
    {
        TempDir tmp;
        Assert::IsTrue(  FilesystemUtils::DirectoryExists( tmp.path ) );
    }
    
    // ---------------------------------------------------------------------------
    // DirectoryHasEntries
    // ---------------------------------------------------------------------------
    TEST_METHOD(DirectoryHasEntries_Non_Existent_Path_Returns_False)
    {
        const std::filesystem::path missing{ "G:\\__does_not_exist_imagescraper_test__" };
        Assert::IsFalse(  FilesystemUtils::DirectoryHasEntries( missing ) );
    }
    
    TEST_METHOD(DirectoryHasEntries_Empty_Directory_Returns_False)
    {
        TempDir tmp;
        Assert::IsFalse(  FilesystemUtils::DirectoryHasEntries( tmp.path ) );
    }
    
    TEST_METHOD(DirectoryHasEntries_Directory_With_One_File_Returns_True)
    {
        TempDir tmp;
        tmp.MakeFile( "file.txt" );
        Assert::IsTrue(  FilesystemUtils::DirectoryHasEntries( tmp.path ) );
    }
    
    TEST_METHOD(DirectoryHasEntries_Directory_With_Subdirectory_Returns_True)
    {
        TempDir tmp;
        tmp.MakeSubDir( "child" );
        Assert::IsTrue(  FilesystemUtils::DirectoryHasEntries( tmp.path ) );
    }
    
    TEST_METHOD(DirectoryHasEntries_Regular_File_Path_Returns_False)
    {
        TempDir tmp;
        const std::filesystem::path file = tmp.MakeFile( "test.txt" );
        Assert::IsFalse(  FilesystemUtils::DirectoryHasEntries( file ) );
    }

    // ---------------------------------------------------------------------------
    // PathToUtf8 / PathFromUtf8
    //
    // The name below is "Reze 霊夢 (Chainsaw Man)" - the kanji are unrepresentable
    // in a western ANSI code page, which is exactly what made path::string( )
    // throw std::system_error while building the downloads tree.
    // ---------------------------------------------------------------------------
    static const std::wstring& NonAnsiName( )
    {
        static const std::wstring name = L"Reze \u970A\u5922 (Chainsaw Man)";
        return name;
    }

    TEST_METHOD(PathToUtf8_Empty_Path_Returns_Empty)
    {
        Assert::IsTrue( FilesystemUtils::PathToUtf8( {} ).empty( ) );
    }

    TEST_METHOD(PathFromUtf8_Empty_String_Returns_Empty)
    {
        Assert::IsTrue( FilesystemUtils::PathFromUtf8( {} ).empty( ) );
    }

    TEST_METHOD(PathToUtf8_Ascii_Path_Matches_Native_String)
    {
        const std::filesystem::path path{ L"C:\\Downloads\\Reddit\\file.jpg" };
        Assert::AreEqual( std::string{ "C:\\Downloads\\Reddit\\file.jpg" },
                          FilesystemUtils::PathToUtf8( path ) );
    }

    TEST_METHOD(PathToUtf8_Non_Ansi_Filename_Does_Not_Throw)
    {
        const std::filesystem::path path = std::filesystem::path{ L"C:\\Downloads" } / NonAnsiName( );

        // path::string( ) throws std::system_error here on a western ANSI code page.
        const std::string utf8 = FilesystemUtils::PathToUtf8( path );
        Assert::IsFalse( utf8.empty( ) );
    }

    TEST_METHOD(PathToUtf8_Non_Ansi_Filename_Produces_Utf8_Bytes)
    {
        const std::filesystem::path path{ NonAnsiName( ) };
        const std::string utf8 = FilesystemUtils::PathToUtf8( path );

        // U+970A encodes as E9 9C 8A in UTF-8.
        Assert::IsTrue( utf8.find( "\xE9\x9C\x8A" ) != std::string::npos );
    }

    TEST_METHOD(PathFromUtf8_Round_Trips_Non_Ansi_Filename)
    {
        const std::filesystem::path original = std::filesystem::path{ L"C:\\Downloads" } / NonAnsiName( );
        const std::filesystem::path roundTripped =
            FilesystemUtils::PathFromUtf8( FilesystemUtils::PathToUtf8( original ) );

        Assert::IsTrue( original == roundTripped );
        Assert::IsTrue( original.wstring( ) == roundTripped.wstring( ) );
    }

    TEST_METHOD(PathToUtf8Generic_Uses_Forward_Slashes)
    {
        const std::filesystem::path path = std::filesystem::path{ L"C:\\Downloads" } / NonAnsiName( );
        const std::string generic = FilesystemUtils::PathToUtf8Generic( path );

        Assert::IsTrue( generic.find( '\\' ) == std::string::npos );
        Assert::IsTrue( generic.find( '/' ) != std::string::npos );
        Assert::IsTrue( FilesystemUtils::PathFromUtf8( generic ) == path );
    }

    TEST_METHOD(PathToUtf8_Round_Trips_Non_Ansi_File_On_Disk)
    {
        TempDir tmp;
        const std::filesystem::path file = tmp.path / ( NonAnsiName( ) + L".jpg" );
        std::ofstream{ file };

        const std::string utf8 = FilesystemUtils::PathToUtf8( file );
        Assert::IsTrue( std::filesystem::exists( FilesystemUtils::PathFromUtf8( utf8 ) ) );
    }

    };
}
