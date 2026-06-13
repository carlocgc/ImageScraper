#include "CppUnitTest.h"
#include "utils/DownloadUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;
    using namespace ImageScraper::DownloadHelpers;

    // ----------------------------------------------------------------------------
    // CreateQueryParamString
    // ----------------------------------------------------------------------------


    TEST_CLASS(DownloadUtilsTests)
    {
    public:
    TEST_METHOD(CreateQueryParamString_Returns_Empty_String_For_No_Params)
    {
        Assert::IsTrue(  CreateQueryParamString( { } ) == "" );
    }
    
    TEST_METHOD(CreateQueryParamString_Formats_A_Single_Param_Correctly)
    {
        std::vector<QueryParam> params = { { "limit", "25" } };
        Assert::IsTrue(  CreateQueryParamString( params ) == "?limit=25" );
    }
    
    TEST_METHOD(CreateQueryParamString_Joins_Multiple_Params_With_Ampersand)
    {
        std::vector<QueryParam> params = { { "limit", "25" }, { "sort", "hot" }, { "t", "all" } };
        Assert::IsTrue(  CreateQueryParamString( params ) == "?limit=25&sort=hot&t=all" );
    }
    
    // ----------------------------------------------------------------------------
    // ExtractFileNameAndExtFromUrl
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(ExtractFileNameAndExtFromUrl_Extracts_Filename_And_Extension)
    {
        Assert::IsTrue(  ExtractFileNameAndExtFromUrl( "https://i.redd.it/abc123.jpg" ) == "abc123.jpg" );
        Assert::IsTrue(  ExtractFileNameAndExtFromUrl( "https://i.redd.it/path/to/image.png" ) == "image.png" );
    }
    
    TEST_METHOD(ExtractFileNameAndExtFromUrl_Returns_Empty_When_No_Slash_Found)
    {
        Assert::IsTrue(  ExtractFileNameAndExtFromUrl( "nodomain" ) == "" );
    }
    
    TEST_METHOD(ExtractFileNameAndExtFromUrl_Returns_Empty_When_No_Extension_Found)
    {
        Assert::IsTrue(  ExtractFileNameAndExtFromUrl( "https://example.com/noextension" ) == "" );
    }
    
    // ----------------------------------------------------------------------------
    // ExtractExtFromFile
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(ExtractExtFromFile_Returns_The_File_Extension)
    {
        Assert::IsTrue(  ExtractExtFromFile( "image.jpg" ) == "jpg" );
        Assert::IsTrue(  ExtractExtFromFile( "video.mp4" ) == "mp4" );
        Assert::IsTrue(  ExtractExtFromFile( "archive.tar.gz" ) == "gz" );
    }
    
    TEST_METHOD(ExtractExtFromFile_Returns_Empty_String_When_No_Extension)
    {
        Assert::IsTrue(  ExtractExtFromFile( "noextension" ) == "" );
    }
    
    // ----------------------------------------------------------------------------
    // UrlToSafeString
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(UrlToSafeString_Strips_Scheme_And_Replaces_Slashes)
    {
        Assert::IsTrue(  UrlToSafeString( "https://i.redd.it/abc/image.jpg" ) == "i.redd.it-abc-image.jpg" );
    }
    
    TEST_METHOD(UrlToSafeString_Strips_Query_Parameters)
    {
        Assert::IsTrue(  UrlToSafeString( "https://example.com/image.jpg?size=large" ) == "example.com-image.jpg" );
    }
    
    TEST_METHOD(UrlToSafeString_Strips_URL_Fragments)
    {
        Assert::IsTrue(  UrlToSafeString( "https://example.com/image.jpg#section" ) == "example.com-image.jpg" );
    }
    
    // ----------------------------------------------------------------------------
    // Download source labels
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(GetProviderName_Extracts_The_Provider_Folder_From_A_Download_Path)
    {
        const std::string redditPath =
            ( std::filesystem::path( "Downloads" ) / "Reddit" / "aww" / "cat.jpg" ).string( );
        const std::string missingDownloadsPath =
            ( std::filesystem::path( "Archives" ) / "Reddit" / "aww" / "cat.jpg" ).string( );
    
        Assert::IsTrue(  GetProviderName( redditPath ) == "Reddit" );
        Assert::IsTrue(  GetProviderName( missingDownloadsPath ).empty( ) );
    }
    
    TEST_METHOD(Download_Labels_Support_An_Explicit_Custom_Download_Root)
    {
        const std::filesystem::path root = std::filesystem::path( "CustomRoot" );
        const std::string filepath =
            ( root / "Reddit" / "Subreddit" / "aww" / "kitten.png" ).string( );
    
        Assert::IsTrue(  GetProviderName( filepath, root ) == "Reddit" );
        Assert::IsTrue(  GetSubfolderLabel( filepath, root ) == "r/aww" );
    }
    
    TEST_METHOD(GetSubfolderLabel_Uses_Reddit_Prefix)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Reddit" / "cats" / "kitten.png" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ) == "r/cats" );
    }

    TEST_METHOD(GetSubfolderLabel_Omits_Reddit_Target_Directory)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Reddit" / "Subreddit" / "aww" / "kitten.png" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ) == "r/aww" );
    }

    TEST_METHOD(GetSubfolderLabel_Uses_Reddit_User_Prefix)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Reddit" / "User" / "spez" / "post.jpg" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ) == "u/spez" );
    }

    TEST_METHOD(GetSubfolderLabel_Uses_Tumblr_Prefix)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Tumblr" / "artist-name" / "post.gif" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ) == "@artist-name" );
    }

    TEST_METHOD(GetSubfolderLabel_Uses_Bluesky_Prefix)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Bluesky" / "alice.bsky.social" / "post.jpg" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ) == "@alice.bsky.social" );
    }

    TEST_METHOD(GetSubfolderLabel_Wraps_FourChan_Board_In_Slashes)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "4chan" / "wg" / "thread.jpg" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ) == "/wg/" );
    }

    TEST_METHOD(GetSubfolderLabel_Uses_Raw_Relative_Subfolder_For_Other_Providers)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "ExampleSource" / "server" / "channel" / "clip.mp4" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ) == "server/channel" );
    }

    TEST_METHOD(GetSubfolderLabel_Is_Empty_For_Files_Directly_Under_Provider_Root)
    {
        const std::string filepath =
            ( std::filesystem::path( "Downloads" ) / "Reddit" / "loose-file.jpg" ).string( );
        Assert::IsTrue(  GetSubfolderLabel( filepath ).empty( ) );
    }
    
    // ----------------------------------------------------------------------------
    // RedirectToPreferredFileTypeUrl
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(RedirectToPreferredFileTypeUrl_Converts_Gifv_To_Mp4)
    {
        Assert::IsTrue(  RedirectToPreferredFileTypeUrl( "https://i.imgur.com/abc.gifv" ) == "https://i.imgur.com/abc.mp4" );
    }
    
    TEST_METHOD(RedirectToPreferredFileTypeUrl_Returns_Empty_For_Non_Gifv_URLs)
    {
        Assert::IsTrue(  RedirectToPreferredFileTypeUrl( "https://i.redd.it/abc.jpg" ) == "" );
        Assert::IsTrue(  RedirectToPreferredFileTypeUrl( "https://i.redd.it/abc.mp4" ) == "" );
        Assert::IsTrue(  RedirectToPreferredFileTypeUrl( "https://i.redd.it/abc.gif" ) == "" );
    }
    
    // ----------------------------------------------------------------------------
    // IsRedditResponseError / IsTumblrResponseError / IsFourChanResponseError
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(IsRedditResponseError_Returns_False_For_Valid_Response)
    {
        RequestResult result;
        result.m_Response = R"({ "data": { "children": [] } })";
        Assert::IsTrue(  IsStandardResponseError(result ) == false );
    }
    
    TEST_METHOD(IsRedditResponseError_Returns_True_And_Sets_Error_Code_When_Error_Field_Present)
    {
        RequestResult result;
        result.m_Response = R"({ "error": 401 })";
        Assert::IsTrue(  IsStandardResponseError(result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
    }
    
    TEST_METHOD(IsRedditResponseError_Sets_Error_String_When_Message_Field_Present)
    {
        RequestResult result;
        result.m_Response = R"({ "message": "Unauthorized" })";
        Assert::IsTrue(  IsStandardResponseError(result ) == false );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "Unauthorized" );
    }
    
    TEST_METHOD(IsRedditResponseError_Returns_True_For_Invalid_JSON)
    {
        RequestResult result;
        result.m_Response = "not valid json{{";
        Assert::IsTrue(  IsStandardResponseError(result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    }
    
    TEST_METHOD(IsTumblrResponseError_Returns_False_For_Valid_Response)
    {
        RequestResult result;
        result.m_Response = R"({ "response": { "posts": [] } })";
        Assert::IsTrue(  IsStandardResponseError(result ) == false );
    }
    
    TEST_METHOD(IsTumblrResponseError_Returns_True_And_Sets_Error_Code_When_Error_Field_Present)
    {
        RequestResult result;
        result.m_Response = R"({ "error": 403 })";
        Assert::IsTrue(  IsStandardResponseError(result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::Forbidden );
    }
    
    TEST_METHOD(IsTumblrResponseError_Returns_True_For_Invalid_JSON)
    {
        RequestResult result;
        result.m_Response = "not valid json{{";
        Assert::IsTrue(  IsStandardResponseError(result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    }
    
    TEST_METHOD(IsStandardResponseError_Returns_False_For_Valid_4chan_Response)
    {
        RequestResult result;
        result.m_Response = R"({ "boards": [] })";
        Assert::IsTrue(  IsStandardResponseError( result ) == false );
    }
    
    TEST_METHOD(IsStandardResponseError_Returns_True_And_Sets_Error_Code_For_4chan_Error_Field)
    {
        RequestResult result;
        result.m_Response = R"({ "error": 404 })";
        Assert::IsTrue(  IsStandardResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::NotFound );
    }
    
    TEST_METHOD(IsStandardResponseError_Returns_True_For_Invalid_4chan_JSON)
    {
        RequestResult result;
        result.m_Response = "not valid json{{";
        Assert::IsTrue(  IsStandardResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    }
    
    // ----------------------------------------------------------------------------
    // IsStandardResponseError (shared implementation for Reddit / Tumblr / 4chan)
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(IsStandardResponseError_Returns_False_For_Valid_Response_With_No_Error_Field)
    {
        RequestResult result;
        result.m_Response = R"({ "data": {} })";
        Assert::IsTrue(  IsStandardResponseError( result ) == false );
    }
    
    TEST_METHOD(IsStandardResponseError_Sets_Error_Code_From_Integer_Error_Field)
    {
        RequestResult result;
        result.m_Response = R"({ "error": 403 })";
        Assert::IsTrue(  IsStandardResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::Forbidden );
    }
    
    TEST_METHOD(IsStandardResponseError_Sets_Message_String_Without_Treating_It_As_Error)
    {
        RequestResult result;
        result.m_Response = R"({ "message": "Rate limited" })";
        Assert::IsTrue(  IsStandardResponseError( result ) == false );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "Rate limited" );
    }
    
    TEST_METHOD(IsStandardResponseError_Returns_True_For_Malformed_JSON)
    {
        RequestResult result;
        result.m_Response = "{bad json";
        Assert::IsTrue(  IsStandardResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    }
    
    // ----------------------------------------------------------------------------
    // IsMastodonResponseError
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(IsMastodonResponseError_Returns_False_For_Valid_Response)
    {
        RequestResult result;
        result.m_Response = R"({ "id": "123", "content": "hello" })";
        Assert::IsTrue(  IsMastodonResponseError( result ) == false );
    }
    
    TEST_METHOD(IsMastodonResponseError_Sets_BadRequest_And_Error_String_For_String_Error_Field)
    {
        RequestResult result;
        result.m_Response = R"({ "error": "The access token is invalid" })";
        Assert::IsTrue(  IsMastodonResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::BadRequest );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "The access token is invalid" );
    }
    
    TEST_METHOD(IsMastodonResponseError_Maps_Integer_Error_Field_To_Error_Code)
    {
        RequestResult result;
        result.m_Response = R"({ "error": 401 })";
        Assert::IsTrue(  IsMastodonResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
    }
    
    TEST_METHOD(IsMastodonResponseError_Sets_Generic_Message_For_Unexpected_Error_Field_Type)
    {
        RequestResult result;
        result.m_Response = R"({ "error": true })";
        Assert::IsTrue(  IsMastodonResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::BadRequest );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "Mastodon API error" );
    }
    
    TEST_METHOD(IsMastodonResponseError_Returns_True_For_Malformed_JSON)
    {
        RequestResult result;
        result.m_Response = "{bad json";
        Assert::IsTrue(  IsMastodonResponseError( result ) == true );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    }
    
    };
}
