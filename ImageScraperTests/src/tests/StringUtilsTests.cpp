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

// ---------------------------------------------------------------------------
// ToLower
// ---------------------------------------------------------------------------

TEST_CASE( "ToLower lowercases ASCII letters", "[StringUtils]" )
{
    REQUIRE( StringUtils::ToLower( "Hello World" ) == "hello world" );
    REQUIRE( StringUtils::ToLower( "ALLCAPS" )     == "allcaps" );
    REQUIRE( StringUtils::ToLower( "MixedCase42" ) == "mixedcase42" );
}

TEST_CASE( "ToLower leaves non-ASCII bytes alone", "[StringUtils]" )
{
    // The bytes for u00e9 ("é") in UTF-8 are 0xC3 0xA9 - both must pass through.
    const std::string input  = "Caf\xC3\xA9";
    const std::string output = StringUtils::ToLower( input );
    REQUIRE( output == "caf\xC3\xA9" );
}

TEST_CASE( "ToLower returns empty string for empty input", "[StringUtils]" )
{
    REQUIRE( StringUtils::ToLower( "" ) == "" );
}

// ---------------------------------------------------------------------------
// Trim
// ---------------------------------------------------------------------------

TEST_CASE( "Trim removes leading and trailing whitespace", "[StringUtils]" )
{
    REQUIRE( StringUtils::Trim( "  hello  " )      == "hello" );
    REQUIRE( StringUtils::Trim( "\t\n hello \r\n" ) == "hello" );
}

TEST_CASE( "Trim leaves interior whitespace alone", "[StringUtils]" )
{
    REQUIRE( StringUtils::Trim( "  hello world  " ) == "hello world" );
}

TEST_CASE( "Trim returns empty string when input is whitespace-only", "[StringUtils]" )
{
    REQUIRE( StringUtils::Trim( "   " )    == "" );
    REQUIRE( StringUtils::Trim( "\t\r\n" ) == "" );
    REQUIRE( StringUtils::Trim( "" )       == "" );
}

// ---------------------------------------------------------------------------
// StartsWith
// ---------------------------------------------------------------------------

TEST_CASE( "StartsWith matches a true prefix", "[StringUtils]" )
{
    REQUIRE( StringUtils::StartsWith( "https://example.com", "https://" ) );
    REQUIRE( StringUtils::StartsWith( "abcdef", "abc" ) );
}

TEST_CASE( "StartsWith returns false when prefix doesn't match", "[StringUtils]" )
{
    REQUIRE_FALSE( StringUtils::StartsWith( "https://example.com", "http://" ) );
    REQUIRE_FALSE( StringUtils::StartsWith( "ab", "abc" ) ); // prefix longer than value
}

TEST_CASE( "StartsWith with empty prefix is always true", "[StringUtils]" )
{
    REQUIRE( StringUtils::StartsWith( "anything", "" ) );
    REQUIRE( StringUtils::StartsWith( "", "" ) );
}

// ---------------------------------------------------------------------------
// StripUrlQueryAndFragment
// ---------------------------------------------------------------------------

TEST_CASE( "StripUrlQueryAndFragment removes query string", "[StringUtils]" )
{
    REQUIRE( StringUtils::StripUrlQueryAndFragment( "https://example.com/path?foo=bar" ) == "https://example.com/path" );
}

TEST_CASE( "StripUrlQueryAndFragment removes fragment", "[StringUtils]" )
{
    REQUIRE( StringUtils::StripUrlQueryAndFragment( "https://example.com/path#section" ) == "https://example.com/path" );
}

TEST_CASE( "StripUrlQueryAndFragment removes whichever terminator comes first", "[StringUtils]" )
{
    // Query before fragment.
    REQUIRE( StringUtils::StripUrlQueryAndFragment( "https://x.test/p?q=1#frag" ) == "https://x.test/p" );
    // Fragment before query (rare, but valid byte order).
    REQUIRE( StringUtils::StripUrlQueryAndFragment( "https://x.test/p#frag?q=1" ) == "https://x.test/p" );
}

TEST_CASE( "StripUrlQueryAndFragment passes URLs without terminators through unchanged", "[StringUtils]" )
{
    REQUIRE( StringUtils::StripUrlQueryAndFragment( "https://example.com/path" ) == "https://example.com/path" );
    REQUIRE( StringUtils::StripUrlQueryAndFragment( "" )                         == "" );
}
