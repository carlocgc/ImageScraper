#include "catch2/catch_amalgamated.hpp"
#include "utils/RedditUtils.h"

using namespace ImageScraper::RedditUtils;

// ----------------------------------------------------------------------------
// ParseAccessToken
// ----------------------------------------------------------------------------

TEST_CASE( "ParseAccessToken returns token and expiry for valid response", "[RedditUtils]" )
{
    Json response = { { "access_token", "abc123" }, { "expires_in", 3600 } };
    auto result = ParseAccessToken( response );

    REQUIRE( result.has_value( ) );
    REQUIRE( result->m_Token == "abc123" );
    REQUIRE( result->m_ExpireSeconds == 3600 );
}

TEST_CASE( "ParseAccessToken returns nullopt when access_token missing", "[RedditUtils]" )
{
    Json response = { { "expires_in", 3600 } };
    REQUIRE( !ParseAccessToken( response ).has_value( ) );
}

TEST_CASE( "ParseAccessToken returns nullopt when expires_in missing", "[RedditUtils]" )
{
    Json response = { { "access_token", "abc123" } };
    REQUIRE( !ParseAccessToken( response ).has_value( ) );
}

TEST_CASE( "ParseAccessToken returns nullopt for empty response", "[RedditUtils]" )
{
    REQUIRE( !ParseAccessToken( Json{ } ).has_value( ) );
}

// ----------------------------------------------------------------------------
// ParseRefreshToken
// ----------------------------------------------------------------------------

TEST_CASE( "ParseRefreshToken returns token for valid response", "[RedditUtils]" )
{
    Json response = { { "refresh_token", "refresh_abc" } };
    auto result = ParseRefreshToken( response );

    REQUIRE( result.has_value( ) );
    REQUIRE( result.value( ) == "refresh_abc" );
}

TEST_CASE( "ParseRefreshToken returns nullopt when refresh_token missing", "[RedditUtils]" )
{
    Json response = { { "access_token", "abc123" } };
    REQUIRE( !ParseRefreshToken( response ).has_value( ) );
}

TEST_CASE( "ParseRefreshToken returns nullopt for empty response", "[RedditUtils]" )
{
    REQUIRE( !ParseRefreshToken( Json{ } ).has_value( ) );
}

// ----------------------------------------------------------------------------
// GetMediaUrls
// ----------------------------------------------------------------------------

TEST_CASE( "GetMediaUrls returns empty for response with no data field", "[RedditUtils]" )
{
    Json response = { { "other", "value" } };
    auto result = GetMediaUrls( response );

    REQUIRE( result.m_Urls.empty( ) );
    REQUIRE( result.m_AfterParam.empty( ) );
}

TEST_CASE( "GetMediaUrls returns empty for response with no children", "[RedditUtils]" )
{
    Json response = { { "data", { } } };
    auto result = GetMediaUrls( response );

    REQUIRE( result.m_Urls.empty( ) );
}

TEST_CASE( "GetMediaUrls extracts image URLs from url field", "[RedditUtils]" )
{
    Json response = Json::parse( R"({
        "data": {
            "children": [ { "data": { "url": "https://i.redd.it/image.jpg" } } ],
            "after": null
        }
    })" );

    auto result = GetMediaUrls( response );
    REQUIRE( result.m_Urls.size( ) == 1 );
    REQUIRE( result.m_Urls[ 0 ] == "https://i.redd.it/image.jpg" );
}

TEST_CASE( "GetMediaUrls extracts image URLs from url_overridden_by_dest field", "[RedditUtils]" )
{
    Json response = Json::parse( R"({
        "data": {
            "children": [ { "data": { "url_overridden_by_dest": "https://i.redd.it/image.png" } } ],
            "after": null
        }
    })" );

    auto result = GetMediaUrls( response );
    REQUIRE( result.m_Urls.size( ) == 1 );
    REQUIRE( result.m_Urls[ 0 ] == "https://i.redd.it/image.png" );
}

TEST_CASE( "GetMediaUrls filters out non-media URLs", "[RedditUtils]" )
{
    Json response = Json::parse( R"({
        "data": {
            "children": [ { "data": { "url": "https://www.reddit.com/r/pics/comments/abc" } } ],
            "after": null
        }
    })" );

    auto result = GetMediaUrls( response );
    REQUIRE( result.m_Urls.empty( ) );
}

TEST_CASE( "GetMediaUrls captures after param for pagination", "[RedditUtils]" )
{
    Json response = Json::parse( R"({ "data": { "children": [], "after": "t3_xyz789" } })" );

    auto result = GetMediaUrls( response );
    REQUIRE( result.m_AfterParam == "t3_xyz789" );
}

TEST_CASE( "GetMediaUrls clears after param when null", "[RedditUtils]" )
{
    Json response = Json::parse( R"({ "data": { "children": [], "after": null } })" );

    auto result = GetMediaUrls( response );
    REQUIRE( result.m_AfterParam.empty( ) );
}

TEST_CASE( "GetMediaUrls bails on first post with no URL key", "[RedditUtils]" )
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
    REQUIRE( result.m_Urls.empty( ) );
}
