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
