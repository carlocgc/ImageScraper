#include "CppUnitTest.h"
#include "io/JsonFile.h"

#include <filesystem>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    // Helper: build a temp file path and ensure cleanup after the test
    struct TempFile
    {
        std::string path;

        TempFile( )
        {
            path = ( std::filesystem::temp_directory_path( ) / "imagescraper_test_config.json" ).string( );
        }

        ~TempFile( )
        {
            std::error_code ec;
            std::filesystem::remove( path, ec );
        }
    };

    // ---------------------------------------------------------------------------
    // GetValue
    // ---------------------------------------------------------------------------


    TEST_CLASS(JsonFileTests)
    {
    public:
    TEST_METHOD(GetValue_Returns_False_For_A_Missing_Key)
    {
        TempFile tmp;
        JsonFile jf( tmp.path );
    
        std::string out;
        Assert::IsTrue(  jf.GetValue<std::string>( "nonexistent_key", out ) == false );
        Assert::IsTrue(  out.empty( ) );
    }
    
    TEST_METHOD(GetValue_Returns_True_And_Correct_Value_After_SetValue)
    {
        TempFile tmp;
        JsonFile jf( tmp.path );
    
        jf.SetValue<std::string>( "reddit_client_id", "my_id" );
    
        std::string out;
        Assert::IsTrue(  jf.GetValue<std::string>( "reddit_client_id", out ) == true );
        Assert::IsTrue(  out == "my_id" );
    }
    
    TEST_METHOD(GetValue_Handles_Integer_Type_Correctly)
    {
        TempFile tmp;
        JsonFile jf( tmp.path );
    
        jf.SetValue<int>( "port", 8080 );
    
        int out = 0;
        Assert::IsTrue(  jf.GetValue<int>( "port", out ) == true );
        Assert::IsTrue(  out == 8080 );
    }
    
    // ---------------------------------------------------------------------------
    // SetValue
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(SetValue_Overwrites_An_Existing_Key)
    {
        TempFile tmp;
        JsonFile jf( tmp.path );
    
        jf.SetValue<std::string>( "api_key", "first_value" );
        jf.SetValue<std::string>( "api_key", "second_value" );
    
        std::string out;
        Assert::IsTrue(  jf.GetValue<std::string>( "api_key", out ) == true );
        Assert::IsTrue(  out == "second_value" );
    }
    
    // ---------------------------------------------------------------------------
    // Serialise / Deserialise round-trip
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Serialise_Writes_And_Deserialise_Reads_Back_String_Values)
    {
        TempFile tmp;
    
        {
            JsonFile writer( tmp.path );
            writer.SetValue<std::string>( "reddit_client_id",     "test_client_id" );
            writer.SetValue<std::string>( "reddit_client_secret", "test_secret" );
            Assert::IsTrue(  writer.Serialise( ) == true );
        }
    
        {
            JsonFile reader( tmp.path );
            Assert::IsTrue(  reader.Deserialise( ) == true );
    
            std::string clientId, clientSecret;
            Assert::IsTrue(  reader.GetValue<std::string>( "reddit_client_id",     clientId )     == true );
            Assert::IsTrue(  reader.GetValue<std::string>( "reddit_client_secret", clientSecret ) == true );
            Assert::IsTrue(  clientId     == "test_client_id" );
            Assert::IsTrue(  clientSecret == "test_secret" );
        }
    }
    
    TEST_METHOD(Deserialise_Auto_Creates_File_When_It_Does_Not_Exist_And_Returns_True)
    {
        TempFile tmp;
        // Ensure the file doesn't exist before the call
        std::error_code ec;
        std::filesystem::remove( tmp.path, ec );
    
        JsonFile jf( tmp.path );
        Assert::IsTrue(  jf.Deserialise( ) == true );
        Assert::IsTrue(  std::filesystem::exists( tmp.path ) );
    }
    
    TEST_METHOD(GetValue_Returns_False_On_A_Freshly_Created_Empty_Config)
    {
        TempFile tmp;
        std::error_code ec;
        std::filesystem::remove( tmp.path, ec );
    
        JsonFile jf( tmp.path );
        jf.Deserialise( ); // auto-creates an empty JSON object
    
        std::string out;
        Assert::IsTrue(  jf.GetValue<std::string>( "any_key", out ) == false );
    }
    
    // ---------------------------------------------------------------------------
    // Empty-string credentials check (mirrors HasRequiredCredentials logic)
    // ---------------------------------------------------------------------------
    
    // ---------------------------------------------------------------------------
    // Non-ANSI paths (callers pass UTF-8, e.g. a Cyrillic or CJK Windows username)
    // ---------------------------------------------------------------------------

    TEST_METHOD(Serialise_And_Deserialise_Round_Trip_Under_A_Non_Ansi_Directory)
    {
        const std::filesystem::path dir = std::filesystem::temp_directory_path( ) / L"JsonFileTest_\u0414\u0430\u043D\u043D\u044B\u0435_\u4E34\u65F6";
        const std::filesystem::path file = dir / L"config.json";
        std::error_code ec;
        std::filesystem::remove_all( dir, ec );

        const std::string utf8Path = file.u8string( );

        {
            JsonFile writer( utf8Path );
            Assert::IsTrue( writer.Deserialise( ), L"Deserialise should create the file under the non-ANSI directory" );
            writer.SetValue<std::string>( "download_location", "D:/Pics" );
            Assert::IsTrue( writer.Serialise( ) );
        }

        Assert::IsTrue( std::filesystem::exists( file ), L"File should exist at the real UTF-8 decoded path" );

        {
            JsonFile reader( utf8Path );
            Assert::IsTrue( reader.Deserialise( ) );
            std::string out;
            Assert::IsTrue( reader.GetValue<std::string>( "download_location", out ) );
            Assert::AreEqual( std::string{ "D:/Pics" }, out );
        }

        std::filesystem::remove_all( dir, ec );
    }

    TEST_METHOD(Empty_String_Value_Is_Retrievable_And_Detected_As_Empty)
    {
        TempFile tmp;
        JsonFile jf( tmp.path );
    
        jf.SetValue<std::string>( "tumblr_consumer_key", "" );
    
        std::string out;
        Assert::IsTrue(  jf.GetValue<std::string>( "tumblr_consumer_key", out ) == true );
        Assert::IsTrue(  out.empty( ) );
    }
    
    };
}
