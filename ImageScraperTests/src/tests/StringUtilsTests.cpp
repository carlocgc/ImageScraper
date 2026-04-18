#include "catch2/catch_amalgamated.hpp"
#include "utils/StringUtils.h"

using namespace ImageScraper;

// ---------------------------------------------------------------------------
// ExtractQueryParam
// ---------------------------------------------------------------------------

TEST_CASE( "ExtractQueryParam returns value for first parameter", "[StringUtils]" )
{
    const std::string request = "GET /?code=abc123&state=xyz HTTP/1.1";
    REQUIRE( StringUtils::ExtractQueryParam( request, "code" ) == "abc123" );
}

TEST_CASE( "ExtractQueryParam returns value for middle parameter", "[StringUtils]" )
{
    const std::string request = "GET /?error=access_denied&state=xyz&foo=bar HTTP/1.1";
    REQUIRE( StringUtils::ExtractQueryParam( request, "state" ) == "xyz" );
}

TEST_CASE( "ExtractQueryParam returns value for last parameter before HTTP version", "[StringUtils]" )
{
    const std::string request = "GET /?code=abc123&state=xyz HTTP/1.1";
    REQUIRE( StringUtils::ExtractQueryParam( request, "state" ) == "xyz" );
}

TEST_CASE( "ExtractQueryParam returns empty string when key is absent", "[StringUtils]" )
{
    const std::string request = "GET /?code=abc123&state=xyz HTTP/1.1";
    REQUIRE( StringUtils::ExtractQueryParam( request, "refresh_token" ).empty( ) );
}

TEST_CASE( "ExtractQueryParam does not partially match a longer key", "[StringUtils]" )
{
    // 'error' must not match 'error_description'
    const std::string request = "GET /?error_description=some+message&state=xyz HTTP/1.1";
    REQUIRE( StringUtils::ExtractQueryParam( request, "error" ).empty( ) );
}

TEST_CASE( "ExtractQueryParam returns empty string for empty input", "[StringUtils]" )
{
    REQUIRE( StringUtils::ExtractQueryParam( "", "code" ).empty( ) );
}

// ---------------------------------------------------------------------------
// UrlEncode
// ---------------------------------------------------------------------------

TEST_CASE( "UrlEncode leaves unreserved characters unchanged", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlEncode( "abc-_.~" ) == "abc-_.~" );
}

TEST_CASE( "UrlEncode percent-encodes reserved characters", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlEncode( "http://localhost:8080" ) == "http%3A%2F%2Flocalhost%3A8080" );
}

TEST_CASE( "UrlEncode percent-encodes spaces", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlEncode( "basic offline_access" ) == "basic%20offline_access" );
}

TEST_CASE( "UrlEncode returns empty string for empty input", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlEncode( "" ).empty( ) );
}

// ---------------------------------------------------------------------------
// UrlDecode
// ---------------------------------------------------------------------------

TEST_CASE( "UrlDecode converts plus signs to spaces", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlDecode( "hello+world" ) == "hello world" );
}

TEST_CASE( "UrlDecode converts percent-encoded sequences", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlDecode( "http%3A%2F%2Flocalhost%3A8080" ) == "http://localhost:8080" );
}

TEST_CASE( "UrlDecode converts percent-encoded space", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlDecode( "hello%20world" ) == "hello world" );
}

TEST_CASE( "UrlDecode leaves plain text unchanged", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlDecode( "basic" ) == "basic" );
}

TEST_CASE( "UrlDecode returns empty string for empty input", "[StringUtils]" )
{
    REQUIRE( StringUtils::UrlDecode( "" ).empty( ) );
}

TEST_CASE( "UrlEncode and UrlDecode are inverse operations", "[StringUtils]" )
{
    const std::string original = "http://localhost:8080/callback?foo=bar&baz=qux";
    REQUIRE( StringUtils::UrlDecode( StringUtils::UrlEncode( original ) ) == original );
}
