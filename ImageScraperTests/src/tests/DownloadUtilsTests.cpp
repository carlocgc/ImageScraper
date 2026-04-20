#include "catch2/catch_amalgamated.hpp"
#include "utils/DownloadUtils.h"

using namespace ImageScraper;
using namespace ImageScraper::DownloadHelpers;

// ----------------------------------------------------------------------------
// CreateQueryParamString
// ----------------------------------------------------------------------------

TEST_CASE( "CreateQueryParamString returns empty string for no params", "[DownloadUtils]" )
{
    REQUIRE( CreateQueryParamString( { } ) == "" );
}

TEST_CASE( "CreateQueryParamString formats a single param correctly", "[DownloadUtils]" )
{
    std::vector<QueryParam> params = { { "limit", "25" } };
    REQUIRE( CreateQueryParamString( params ) == "?limit=25" );
}

TEST_CASE( "CreateQueryParamString joins multiple params with ampersand", "[DownloadUtils]" )
{
    std::vector<QueryParam> params = { { "limit", "25" }, { "sort", "hot" }, { "t", "all" } };
    REQUIRE( CreateQueryParamString( params ) == "?limit=25&sort=hot&t=all" );
}

// ----------------------------------------------------------------------------
// ExtractFileNameAndExtFromUrl
// ----------------------------------------------------------------------------

TEST_CASE( "ExtractFileNameAndExtFromUrl extracts filename and extension", "[DownloadUtils]" )
{
    REQUIRE( ExtractFileNameAndExtFromUrl( "https://i.redd.it/abc123.jpg" ) == "abc123.jpg" );
    REQUIRE( ExtractFileNameAndExtFromUrl( "https://i.redd.it/path/to/image.png" ) == "image.png" );
}

TEST_CASE( "ExtractFileNameAndExtFromUrl returns empty when no slash found", "[DownloadUtils]" )
{
    REQUIRE( ExtractFileNameAndExtFromUrl( "nodomain" ) == "" );
}

TEST_CASE( "ExtractFileNameAndExtFromUrl returns empty when no extension found", "[DownloadUtils]" )
{
    REQUIRE( ExtractFileNameAndExtFromUrl( "https://example.com/noextension" ) == "" );
}

// ----------------------------------------------------------------------------
// ExtractExtFromFile
// ----------------------------------------------------------------------------

TEST_CASE( "ExtractExtFromFile returns the file extension", "[DownloadUtils]" )
{
    REQUIRE( ExtractExtFromFile( "image.jpg" ) == "jpg" );
    REQUIRE( ExtractExtFromFile( "video.mp4" ) == "mp4" );
    REQUIRE( ExtractExtFromFile( "archive.tar.gz" ) == "gz" );
}

TEST_CASE( "ExtractExtFromFile returns empty string when no extension", "[DownloadUtils]" )
{
    REQUIRE( ExtractExtFromFile( "noextension" ) == "" );
}

// ----------------------------------------------------------------------------
// UrlToSafeString
// ----------------------------------------------------------------------------

TEST_CASE( "UrlToSafeString strips scheme and replaces slashes", "[DownloadUtils]" )
{
    REQUIRE( UrlToSafeString( "https://i.redd.it/abc/image.jpg" ) == "i.redd.it-abc-image.jpg" );
}

TEST_CASE( "UrlToSafeString strips query parameters", "[DownloadUtils]" )
{
    REQUIRE( UrlToSafeString( "https://example.com/image.jpg?size=large" ) == "example.com-image.jpg" );
}

TEST_CASE( "UrlToSafeString strips URL fragments", "[DownloadUtils]" )
{
    REQUIRE( UrlToSafeString( "https://example.com/image.jpg#section" ) == "example.com-image.jpg" );
}

// ----------------------------------------------------------------------------
// Download source labels
// ----------------------------------------------------------------------------

TEST_CASE( "GetProviderName extracts the provider folder from a download path", "[DownloadUtils]" )
{
    const std::string redditPath =
        ( std::filesystem::path( "Downloads" ) / "Reddit" / "aww" / "cat.jpg" ).string( );
    const std::string missingDownloadsPath =
        ( std::filesystem::path( "Archives" ) / "Reddit" / "aww" / "cat.jpg" ).string( );

    REQUIRE( GetProviderName( redditPath ) == "Reddit" );
    REQUIRE( GetProviderName( missingDownloadsPath ).empty( ) );
}

TEST_CASE( "GetSubfolderLabel formats provider-specific download labels", "[DownloadUtils]" )
{
    SECTION( "Reddit uses r/ prefix" )
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Reddit" / "cats" / "kitten.png" ).string( );
        REQUIRE( GetSubfolderLabel( filepath ) == "r/cats" );
    }

    SECTION( "Tumblr uses @ prefix" )
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Tumblr" / "artist-name" / "post.gif" ).string( );
        REQUIRE( GetSubfolderLabel( filepath ) == "@artist-name" );
    }

    SECTION( "4chan wraps the board in slashes" )
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "4chan" / "wg" / "thread.jpg" ).string( );
        REQUIRE( GetSubfolderLabel( filepath ) == "/wg/" );
    }

    SECTION( "Other providers use the raw relative subfolder" )
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Discord" / "server" / "channel" / "clip.mp4" ).string( );
        REQUIRE( GetSubfolderLabel( filepath ) == "server/channel" );
    }

    SECTION( "Files stored directly under the provider root have no subfolder label" )
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Reddit" / "loose-file.jpg" ).string( );
        REQUIRE( GetSubfolderLabel( filepath ).empty( ) );
    }
}

// ----------------------------------------------------------------------------
// RedirectToPreferredFileTypeUrl
// ----------------------------------------------------------------------------

TEST_CASE( "RedirectToPreferredFileTypeUrl converts gifv to mp4", "[DownloadUtils]" )
{
    REQUIRE( RedirectToPreferredFileTypeUrl( "https://i.imgur.com/abc.gifv" ) == "https://i.imgur.com/abc.mp4" );
}

TEST_CASE( "RedirectToPreferredFileTypeUrl returns empty for non-gifv URLs", "[DownloadUtils]" )
{
    REQUIRE( RedirectToPreferredFileTypeUrl( "https://i.redd.it/abc.jpg" ) == "" );
    REQUIRE( RedirectToPreferredFileTypeUrl( "https://i.redd.it/abc.mp4" ) == "" );
    REQUIRE( RedirectToPreferredFileTypeUrl( "https://i.redd.it/abc.gif" ) == "" );
}

// ----------------------------------------------------------------------------
// IsRedditResponseError / IsTumblrResponseError / IsFourChanResponseError
// ----------------------------------------------------------------------------

TEST_CASE( "IsRedditResponseError returns false for valid response", "[DownloadUtils][Reddit]" )
{
    RequestResult result;
    result.m_Response = R"({ "data": { "children": [] } })";
    REQUIRE( IsRedditResponseError( result ) == false );
}

TEST_CASE( "IsRedditResponseError returns true and sets error code when error field present", "[DownloadUtils][Reddit]" )
{
    RequestResult result;
    result.m_Response = R"({ "error": 401 })";
    REQUIRE( IsRedditResponseError( result ) == true );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
}

TEST_CASE( "IsRedditResponseError sets error string when message field present", "[DownloadUtils][Reddit]" )
{
    RequestResult result;
    result.m_Response = R"({ "message": "Unauthorized" })";
    REQUIRE( IsRedditResponseError( result ) == false );
    REQUIRE( result.m_Error.m_ErrorString == "Unauthorized" );
}

TEST_CASE( "IsRedditResponseError returns true for invalid JSON", "[DownloadUtils][Reddit]" )
{
    RequestResult result;
    result.m_Response = "not valid json{{";
    REQUIRE( IsRedditResponseError( result ) == true );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
}

TEST_CASE( "IsTumblrResponseError returns false for valid response", "[DownloadUtils][Tumblr]" )
{
    RequestResult result;
    result.m_Response = R"({ "response": { "posts": [] } })";
    REQUIRE( IsTumblrResponseError( result ) == false );
}

TEST_CASE( "IsTumblrResponseError returns true and sets error code when error field present", "[DownloadUtils][Tumblr]" )
{
    RequestResult result;
    result.m_Response = R"({ "error": 403 })";
    REQUIRE( IsTumblrResponseError( result ) == true );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::Forbidden );
}

TEST_CASE( "IsTumblrResponseError returns true for invalid JSON", "[DownloadUtils][Tumblr]" )
{
    RequestResult result;
    result.m_Response = "not valid json{{";
    REQUIRE( IsTumblrResponseError( result ) == true );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
}

TEST_CASE( "IsFourChanResponseError returns false for valid response", "[DownloadUtils][FourChan]" )
{
    RequestResult result;
    result.m_Response = R"({ "boards": [] })";
    REQUIRE( IsFourChanResponseError( result ) == false );
}

TEST_CASE( "IsFourChanResponseError returns true and sets error code when error field present", "[DownloadUtils][FourChan]" )
{
    RequestResult result;
    result.m_Response = R"({ "error": 404 })";
    REQUIRE( IsFourChanResponseError( result ) == true );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::NotFound );
}

TEST_CASE( "IsFourChanResponseError returns true for invalid JSON", "[DownloadUtils][FourChan]" )
{
    RequestResult result;
    result.m_Response = "not valid json{{";
    REQUIRE( IsFourChanResponseError( result ) == true );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
}
