#include "catch2/catch_amalgamated.hpp"
#include "io/JsonFile.h"

#include <filesystem>
#include <string>

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

TEST_CASE( "GetValue returns false for a missing key", "[JsonFile]" )
{
    TempFile tmp;
    JsonFile jf( tmp.path );

    std::string out;
    REQUIRE( jf.GetValue<std::string>( "nonexistent_key", out ) == false );
    REQUIRE( out.empty( ) );
}

TEST_CASE( "GetValue returns true and correct value after SetValue", "[JsonFile]" )
{
    TempFile tmp;
    JsonFile jf( tmp.path );

    jf.SetValue<std::string>( "reddit_client_id", "my_id" );

    std::string out;
    REQUIRE( jf.GetValue<std::string>( "reddit_client_id", out ) == true );
    REQUIRE( out == "my_id" );
}

TEST_CASE( "GetValue handles integer type correctly", "[JsonFile]" )
{
    TempFile tmp;
    JsonFile jf( tmp.path );

    jf.SetValue<int>( "port", 8080 );

    int out = 0;
    REQUIRE( jf.GetValue<int>( "port", out ) == true );
    REQUIRE( out == 8080 );
}

// ---------------------------------------------------------------------------
// SetValue
// ---------------------------------------------------------------------------

TEST_CASE( "SetValue overwrites an existing key", "[JsonFile]" )
{
    TempFile tmp;
    JsonFile jf( tmp.path );

    jf.SetValue<std::string>( "api_key", "first_value" );
    jf.SetValue<std::string>( "api_key", "second_value" );

    std::string out;
    REQUIRE( jf.GetValue<std::string>( "api_key", out ) == true );
    REQUIRE( out == "second_value" );
}

// ---------------------------------------------------------------------------
// Serialise / Deserialise round-trip
// ---------------------------------------------------------------------------

TEST_CASE( "Serialise writes and Deserialise reads back string values", "[JsonFile]" )
{
    TempFile tmp;

    {
        JsonFile writer( tmp.path );
        writer.SetValue<std::string>( "reddit_client_id",     "test_client_id" );
        writer.SetValue<std::string>( "reddit_client_secret", "test_secret" );
        REQUIRE( writer.Serialise( ) == true );
    }

    {
        JsonFile reader( tmp.path );
        REQUIRE( reader.Deserialise( ) == true );

        std::string clientId, clientSecret;
        REQUIRE( reader.GetValue<std::string>( "reddit_client_id",     clientId )     == true );
        REQUIRE( reader.GetValue<std::string>( "reddit_client_secret", clientSecret ) == true );
        REQUIRE( clientId     == "test_client_id" );
        REQUIRE( clientSecret == "test_secret" );
    }
}

TEST_CASE( "Deserialise auto-creates file when it does not exist and returns true", "[JsonFile]" )
{
    TempFile tmp;
    // Ensure the file doesn't exist before the call
    std::error_code ec;
    std::filesystem::remove( tmp.path, ec );

    JsonFile jf( tmp.path );
    REQUIRE( jf.Deserialise( ) == true );
    REQUIRE( std::filesystem::exists( tmp.path ) );
}

TEST_CASE( "GetValue returns false on a freshly created empty config", "[JsonFile]" )
{
    TempFile tmp;
    std::error_code ec;
    std::filesystem::remove( tmp.path, ec );

    JsonFile jf( tmp.path );
    jf.Deserialise( ); // auto-creates an empty JSON object

    std::string out;
    REQUIRE( jf.GetValue<std::string>( "any_key", out ) == false );
}

// ---------------------------------------------------------------------------
// Empty-string credentials check (mirrors HasRequiredCredentials logic)
// ---------------------------------------------------------------------------

TEST_CASE( "Empty string value is retrievable and detected as empty", "[JsonFile]" )
{
    TempFile tmp;
    JsonFile jf( tmp.path );

    jf.SetValue<std::string>( "tumblr_consumer_key", "" );

    std::string out;
    REQUIRE( jf.GetValue<std::string>( "tumblr_consumer_key", out ) == true );
    REQUIRE( out.empty( ) );
}
