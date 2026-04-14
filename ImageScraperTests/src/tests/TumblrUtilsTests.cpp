#include "catch2/catch_amalgamated.hpp"
#include "utils/TumblrUtils.h"
#include "services/ServiceOptionTypes.h"

using namespace ImageScraper;
using namespace ImageScraper::TumblrUtils;

TEST_CASE( "GetMediaUrlsFromResponse returns empty for no posts", "[TumblrUtils]" )
{
    Json response = { { "response", { { "posts", Json::array( ) } } } };
    REQUIRE( GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
}

TEST_CASE( "GetMediaUrlsFromResponse extracts photo URLs", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                {
                    { "type", "photo" },
                    { "photos", {
                        { { "original_size", { { "url", "https://example.tumblr.com/photo1.jpg" } } } },
                        { { "original_size", { { "url", "https://example.tumblr.com/photo2.jpg" } } } }
                    } }
                }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 2 );
    REQUIRE( result[ 0 ] == "https://example.tumblr.com/photo1.jpg" );
    REQUIRE( result[ 1 ] == "https://example.tumblr.com/photo2.jpg" );
}

TEST_CASE( "GetMediaUrlsFromResponse extracts video URL", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                {
                    { "type", "video" },
                    { "video_url", "https://example.tumblr.com/video.mp4" }
                }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 1 );
    REQUIRE( result[ 0 ] == "https://example.tumblr.com/video.mp4" );
}

TEST_CASE( "GetMediaUrlsFromResponse skips video posts without video_url", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                { { "type", "video" } }
            } }
        } }
    };

    REQUIRE( GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
}

TEST_CASE( "GetMediaUrlsFromResponse skips photo posts without original_size", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                {
                    { "type", "photo" },
                    { "photos", {
                        { { "alt_size", { { "url", "https://example.tumblr.com/photo.jpg" } } } }
                    } }
                }
            } }
        } }
    };

    REQUIRE( GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
}

TEST_CASE( "GetMediaUrlsFromResponse ignores non photo/video post types", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                { { "type", "text" }, { "body", "some text post" } }
            } }
        } }
    };

    REQUIRE( GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
}

TEST_CASE( "GetMediaUrlsFromResponse handles mixed post types", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                {
                    { "type", "photo" },
                    { "photos", {
                        { { "original_size", { { "url", "https://example.tumblr.com/photo.jpg" } } } }
                    } }
                },
                { { "type", "text" }, { "body", "ignored" } },
                {
                    { "type", "video" },
                    { "video_url", "https://example.tumblr.com/video.mp4" }
                }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 2 );
}

TEST_CASE( "GetMediaUrlsFromResponse respects maxItems limit", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                {
                    { "type", "photo" },
                    { "photos", {
                        { { "original_size", { { "url", "https://example.tumblr.com/photo1.jpg" } } } },
                        { { "original_size", { { "url", "https://example.tumblr.com/photo2.jpg" } } } },
                        { { "original_size", { { "url", "https://example.tumblr.com/photo3.jpg" } } } }
                    } }
                },
                {
                    { "type", "video" },
                    { "video_url", "https://example.tumblr.com/video.mp4" }
                }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, 2 );
    REQUIRE( result.size( ) == 2 );
}
