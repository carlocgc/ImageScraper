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

TEST_CASE( "GetMediaUrlsFromResponse returns empty for text post with no embedded media", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                { { "type", "text" }, { "body", "some text post with no images" } }
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

// ---------------------------------------------------------------------------
// Text post - HTML body parsing
// ---------------------------------------------------------------------------

TEST_CASE( "GetMediaUrlsFromResponse extracts img src from text post body", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                {
                    { "type", "text" },
                    { "body", R"(<p>Hello</p><img src="https://cdn.tumblr.com/image.jpg" alt="pic">)" }
                }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 1 );
    REQUIRE( result[ 0 ] == "https://cdn.tumblr.com/image.jpg" );
}

TEST_CASE( "GetMediaUrlsFromResponse prefers highest-resolution srcset entry over src", "[TumblrUtils]" )
{
    // srcset is ordered ascending by width - last entry is the original resolution
    const std::string body =
        R"(<img src="https://cdn.tumblr.com/s640x960/img.jpg" )"
        R"(srcset="https://cdn.tumblr.com/s75x75/img.jpg 75w, )"
        R"(https://cdn.tumblr.com/s400x600/img.jpg 400w, )"
        R"(https://cdn.tumblr.com/s640x960/img.jpg 640w, )"
        R"(https://cdn.tumblr.com/original/img.jpg 2448w">)";

    Json response = {
        { "response", {
            { "posts", {
                { { "type", "text" }, { "body", body } }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 1 );
    REQUIRE( result[ 0 ] == "https://cdn.tumblr.com/original/img.jpg" );
}

TEST_CASE( "GetMediaUrlsFromResponse extracts multiple images from text post body", "[TumblrUtils]" )
{
    const std::string body =
        R"(<img src="https://cdn.tumblr.com/img1.jpg">)"
        R"(<p>some text</p>)"
        R"(<img src="https://cdn.tumblr.com/img2.jpg">)";

    Json response = {
        { "response", {
            { "posts", {
                { { "type", "text" }, { "body", body } }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 2 );
    REQUIRE( result[ 0 ] == "https://cdn.tumblr.com/img1.jpg" );
    REQUIRE( result[ 1 ] == "https://cdn.tumblr.com/img2.jpg" );
}

TEST_CASE( "GetMediaUrlsFromResponse extracts video src from text post body", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                {
                    { "type", "text" },
                    { "body", R"(<video src="https://cdn.tumblr.com/video.mp4"></video>)" }
                }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 1 );
    REQUIRE( result[ 0 ] == "https://cdn.tumblr.com/video.mp4" );
}

TEST_CASE( "GetMediaUrlsFromResponse extracts source src from text post body", "[TumblrUtils]" )
{
    const std::string body =
        R"(<video><source src="https://cdn.tumblr.com/video.mp4" type="video/mp4"></video>)";

    Json response = {
        { "response", {
            { "posts", {
                { { "type", "text" }, { "body", body } }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 1 );
    REQUIRE( result[ 0 ] == "https://cdn.tumblr.com/video.mp4" );
}

TEST_CASE( "GetMediaUrlsFromResponse extracts mixed img and video from text post body", "[TumblrUtils]" )
{
    const std::string body =
        R"(<img src="https://cdn.tumblr.com/img.jpg">)"
        R"(<video src="https://cdn.tumblr.com/clip.mp4"></video>)";

    Json response = {
        { "response", {
            { "posts", {
                { { "type", "text" }, { "body", body } }
            } }
        } }
    };

    auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
    REQUIRE( result.size( ) == 2 );
}

TEST_CASE( "GetMediaUrlsFromResponse skips text post without body field", "[TumblrUtils]" )
{
    Json response = {
        { "response", {
            { "posts", {
                { { "type", "text" } }
            } }
        } }
    };

    REQUIRE( GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
}

// ---------------------------------------------------------------------------
// Helper function unit tests
// ---------------------------------------------------------------------------

TEST_CASE( "BestUrlFromSrcset returns the last entry URL", "[TumblrUtils]" )
{
    const std::string srcset =
        "https://cdn.tumblr.com/s75x75/img.jpg 75w, "
        "https://cdn.tumblr.com/s400x600/img.jpg 400w, "
        "https://cdn.tumblr.com/original/img.jpg 2448w";

    REQUIRE( BestUrlFromSrcset( srcset ) == "https://cdn.tumblr.com/original/img.jpg" );
}

TEST_CASE( "BestUrlFromSrcset handles a single entry with no comma", "[TumblrUtils]" )
{
    REQUIRE( BestUrlFromSrcset( "https://cdn.tumblr.com/img.jpg 640w" ) == "https://cdn.tumblr.com/img.jpg" );
}

TEST_CASE( "BestUrlFromSrcset returns empty for an empty srcset", "[TumblrUtils]" )
{
    REQUIRE( BestUrlFromSrcset( "" ).empty( ) );
}

TEST_CASE( "ExtractAttr returns value for double-quoted attribute", "[TumblrUtils]" )
{
    REQUIRE( ExtractAttr( R"(<img src="https://example.com/img.jpg" alt="pic">)", "src" )
             == "https://example.com/img.jpg" );
}

TEST_CASE( "ExtractAttr returns value for single-quoted attribute", "[TumblrUtils]" )
{
    REQUIRE( ExtractAttr( "<img src='https://example.com/img.jpg'>", "src" )
             == "https://example.com/img.jpg" );
}

TEST_CASE( "ExtractAttr returns empty for missing attribute", "[TumblrUtils]" )
{
    REQUIRE( ExtractAttr( R"(<img alt="pic">)", "src" ).empty( ) );
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
