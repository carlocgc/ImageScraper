#include "CppUnitTest.h"
#include "utils/RedditUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper::RedditUtils;

    // ----------------------------------------------------------------------------
    // ParseAccessToken
    // ----------------------------------------------------------------------------


    TEST_CLASS(RedditUtilsTests)
    {
    public:
    TEST_METHOD(ParseAccessToken_Returns_Token_And_Expiry_For_Valid_Response)
    {
        Json response = { { "access_token", "abc123" }, { "expires_in", 3600 } };
        auto result = ParseAccessToken( response );
    
        Assert::IsTrue(  result.has_value( ) );
        Assert::IsTrue(  result->m_Token == "abc123" );
        Assert::IsTrue(  result->m_ExpireSeconds == 3600 );
    }
    
    TEST_METHOD(ParseAccessToken_Returns_Nullopt_When_Access_Token_Missing)
    {
        Json response = { { "expires_in", 3600 } };
        Assert::IsTrue(  !ParseAccessToken( response ).has_value( ) );
    }
    
    TEST_METHOD(ParseAccessToken_Returns_Nullopt_When_Expires_In_Missing)
    {
        Json response = { { "access_token", "abc123" } };
        Assert::IsTrue(  !ParseAccessToken( response ).has_value( ) );
    }
    
    TEST_METHOD(ParseAccessToken_Returns_Nullopt_For_Empty_Response)
    {
        Assert::IsTrue(  !ParseAccessToken( Json{ } ).has_value( ) );
    }
    
    // ----------------------------------------------------------------------------
    // ParseRefreshToken
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(ParseRefreshToken_Returns_Token_For_Valid_Response)
    {
        Json response = { { "refresh_token", "refresh_abc" } };
        auto result = ParseRefreshToken( response );
    
        Assert::IsTrue(  result.has_value( ) );
        Assert::IsTrue(  result.value( ) == "refresh_abc" );
    }
    
    TEST_METHOD(ParseRefreshToken_Returns_Nullopt_When_Refresh_Token_Missing)
    {
        Json response = { { "access_token", "abc123" } };
        Assert::IsTrue(  !ParseRefreshToken( response ).has_value( ) );
    }
    
    TEST_METHOD(ParseRefreshToken_Returns_Nullopt_For_Empty_Response)
    {
        Assert::IsTrue(  !ParseRefreshToken( Json{ } ).has_value( ) );
    }
    
    // ----------------------------------------------------------------------------
    // GetMediaUrls
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(NormalizeUserName_Accepts_Bare_Usernames)
    {
        Assert::IsTrue(  NormalizeUserName( "spez" ) == "spez" );
    }
    
    TEST_METHOD(NormalizeUserName_Strips_User_Prefixes)
    {
        Assert::IsTrue(  NormalizeUserName( "u/spez" ) == "spez" );
        Assert::IsTrue(  NormalizeUserName( "/user/spez" ) == "spez" );
    }
    
    TEST_METHOD(NormalizeUserName_Extracts_Usernames_From_Reddit_URLs)
    {
        Assert::IsTrue(  NormalizeUserName( "https://www.reddit.com/user/spez/submitted/" ) == "spez" );
        Assert::IsTrue(  NormalizeUserName( "https://reddit.com/u/spez/?sort=new" ) == "spez" );
    }
    
    TEST_METHOD(NormalizeUserName_Rejects_Empty_Shells)
    {
        Assert::IsTrue(  NormalizeUserName( "/user/" ).empty( ) );
        Assert::IsTrue(  NormalizeUserName( "u/" ).empty( ) );
    }
    
    TEST_METHOD(NormalizeSubredditName_Accepts_Bare_Subreddit_Names)
    {
        Assert::IsTrue(  NormalizeSubredditName( "pics" ) == "pics" );
    }
    
    TEST_METHOD(NormalizeSubredditName_Strips_R_Slash_Prefixes)
    {
        Assert::IsTrue(  NormalizeSubredditName( "r/pics" ) == "pics" );
        Assert::IsTrue(  NormalizeSubredditName( "/r/pics" ) == "pics" );
    }
    
    TEST_METHOD(NormalizeSubredditName_Extracts_Subreddit_Names_From_Reddit_URLs)
    {
        Assert::IsTrue(  NormalizeSubredditName( "https://www.reddit.com/r/pics/top/" ) == "pics" );
        Assert::IsTrue(  NormalizeSubredditName( "https://reddit.com/r/gifs/?sort=hot" ) == "gifs" );
    }
    
    TEST_METHOD(NormalizeSubredditName_Rejects_Empty_Shells)
    {
        Assert::IsTrue(  NormalizeSubredditName( "/r/" ).empty( ) );
        Assert::IsTrue(  NormalizeSubredditName( "r/" ).empty( ) );
    }
    
    TEST_METHOD(GetMediaUrls_Returns_Empty_For_Response_With_No_Data_Field)
    {
        Json response = { { "other", "value" } };
        auto result = GetMediaUrls( response );
    
        Assert::IsTrue(  result.m_Urls.empty( ) );
        Assert::IsTrue(  result.m_AfterParam.empty( ) );
    }
    
    TEST_METHOD(GetMediaUrls_Returns_Empty_For_Response_With_No_Children)
    {
        Json response = { { "data", { } } };
        auto result = GetMediaUrls( response );
    
        Assert::IsTrue(  result.m_Urls.empty( ) );
    }
    
    TEST_METHOD(GetMediaUrls_Extracts_Image_URLs_From_Url_Field)
    {
        Json response = Json::parse( R"({
            "data": {
                "children": [ { "data": { "url": "https://i.redd.it/image.jpg" } } ],
                "after": null
            }
        })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_Urls.size( ) == 1 );
        Assert::IsTrue(  result.m_Urls[ 0 ] == "https://i.redd.it/image.jpg" );
    }
    
    TEST_METHOD(GetMediaUrls_Extracts_Image_URLs_From_Url_Overridden_By_Dest_Field)
    {
        Json response = Json::parse( R"({
            "data": {
                "children": [ { "data": { "url_overridden_by_dest": "https://i.redd.it/image.png" } } ],
                "after": null
            }
        })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_Urls.size( ) == 1 );
        Assert::IsTrue(  result.m_Urls[ 0 ] == "https://i.redd.it/image.png" );
    }
    
    TEST_METHOD(GetMediaUrls_Filters_Out_Non_Media_URLs)
    {
        Json response = Json::parse( R"({
            "data": {
                "children": [ { "data": { "url": "https://www.reddit.com/r/pics/comments/abc" } } ],
                "after": null
            }
        })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_Urls.empty( ) );
    }
    
    TEST_METHOD(GetMediaUrls_Captures_After_Param_For_Pagination)
    {
        Json response = Json::parse( R"({ "data": { "children": [], "after": "t3_xyz789" } })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_AfterParam == "t3_xyz789" );
    }
    
    TEST_METHOD(GetMediaUrls_Clears_After_Param_When_Null)
    {
        Json response = Json::parse( R"({ "data": { "children": [], "after": null } })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_AfterParam.empty( ) );
    }
    
    TEST_METHOD(GetMediaUrls_Bails_On_First_Post_With_No_URL_Key)
    {
        // Documents existing behaviour: returns early on first post with no url/url_overridden_by_dest
        Json response = Json::parse( R"({
            "data": {
                "children": [
                    { "data": { "title": "no url here" } },
                    { "data": { "url": "https://i.redd.it/image.jpg" } }
                ],
                "after": null
            }
        })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_Urls.empty( ) );
    }
    
    TEST_METHOD(GetMediaUrls_Accepts_Redgifs_Watch_URLs_Even_Without_An_Extension)
    {
        Json response = Json::parse( R"({
            "data": {
                "children": [
                    { "data": { "url": "https://redgifs.com/watch/somegifslug" } },
                    { "data": { "url": "https://www.redgifs.com/watch/anotherSlug" } },
                    { "data": { "url": "https://v3.redgifs.com/watch/thirdSlug" } }
                ],
                "after": null
            }
        })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_Urls.size( ) == 3 );
        Assert::IsTrue(  result.m_Urls[ 0 ] == "https://redgifs.com/watch/somegifslug" );
        Assert::IsTrue(  result.m_Urls[ 1 ] == "https://www.redgifs.com/watch/anotherSlug" );
        Assert::IsTrue(  result.m_Urls[ 2 ] == "https://v3.redgifs.com/watch/thirdSlug" );
    }
    
    TEST_METHOD(GetMediaUrls_Still_Rejects_Unrelated_Extension_Less_URLs_E_G_Gfycat)
    {
        Json response = Json::parse( R"({
            "data": {
                "children": [
                    { "data": { "url": "https://gfycat.com/SomeSlug" } },
                    { "data": { "url": "https://reddit.com/r/sub/comments/xyz/title/" } }
                ],
                "after": null
            }
        })" );
    
        auto result = GetMediaUrls( response );
        Assert::IsTrue(  result.m_Urls.empty( ) );
    }
    
    };
}
