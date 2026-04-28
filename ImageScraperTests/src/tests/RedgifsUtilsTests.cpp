#include "catch2/catch_amalgamated.hpp"
#include "utils/RedgifsUtils.h"

namespace Utils = ImageScraper::RedgifsUtils;

// IsRedgifsUrl ----------------------------------------------------------------

TEST_CASE( "IsRedgifsUrl matches canonical watch URLs", "[RedgifsUtils]" )
{
    REQUIRE( Utils::IsRedgifsUrl( "https://redgifs.com/watch/someslug" ) );
    REQUIRE( Utils::IsRedgifsUrl( "https://www.redgifs.com/watch/SomeSlug" ) );
    REQUIRE( Utils::IsRedgifsUrl( "https://v3.redgifs.com/watch/another" ) );
    REQUIRE( Utils::IsRedgifsUrl( "http://redgifs.com/ifr/embedded" ) );
}

TEST_CASE( "IsRedgifsUrl rejects unrelated URLs", "[RedgifsUtils]" )
{
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "https://gfycat.com/Slug" ) );
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "https://i.redd.it/foo.jpg" ) );
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "" ) );
}

TEST_CASE( "IsRedgifsUrl rejects direct CDN URLs that need no resolution", "[RedgifsUtils]" )
{
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "https://media.redgifs.com/PersonalInfamousCanadagoose.mp4" ) );
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "https://media.redgifs.com/Slug-mobile.mp4" ) );
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "https://media.redgifs.com/Slug-poster.jpg" ) );
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "https://thumbs.redgifs.com/Foo.gif" ) );
    REQUIRE_FALSE( Utils::IsRedgifsUrl( "https://www.redgifs.com/" ) );
}

TEST_CASE( "IsRedgifsUrl is case-insensitive on the host", "[RedgifsUtils]" )
{
    REQUIRE( Utils::IsRedgifsUrl( "https://RedGifs.com/watch/X" ) );
    REQUIRE( Utils::IsRedgifsUrl( "https://WWW.REDGIFS.COM/watch/X" ) );
}

// ExtractSlug -----------------------------------------------------------------

TEST_CASE( "ExtractSlug pulls the lowercased slug from /watch/ URLs", "[RedgifsUtils]" )
{
    REQUIRE( Utils::ExtractSlug( "https://redgifs.com/watch/someslug" ) == "someslug" );
    REQUIRE( Utils::ExtractSlug( "https://www.redgifs.com/watch/MixedCaseSlug" ) == "mixedcaseslug" );
    REQUIRE( Utils::ExtractSlug( "https://v3.redgifs.com/watch/AnotherSlug" ) == "anotherslug" );
}

TEST_CASE( "ExtractSlug supports the /ifr/ embed form", "[RedgifsUtils]" )
{
    REQUIRE( Utils::ExtractSlug( "https://www.redgifs.com/ifr/EmbedSlug" ) == "embedslug" );
}

TEST_CASE( "ExtractSlug strips query string and fragment", "[RedgifsUtils]" )
{
    REQUIRE( Utils::ExtractSlug( "https://redgifs.com/watch/someslug?utm=foo" ) == "someslug" );
    REQUIRE( Utils::ExtractSlug( "https://redgifs.com/watch/someslug#t=10" )    == "someslug" );
}

TEST_CASE( "ExtractSlug strips trailing path segments", "[RedgifsUtils]" )
{
    REQUIRE( Utils::ExtractSlug( "https://redgifs.com/watch/someslug/extra" ) == "someslug" );
    REQUIRE( Utils::ExtractSlug( "https://redgifs.com/watch/someslug/" )      == "someslug" );
}

TEST_CASE( "ExtractSlug returns empty for URLs that don't carry a slug", "[RedgifsUtils]" )
{
    REQUIRE( Utils::ExtractSlug( "https://redgifs.com/" )         == "" );
    REQUIRE( Utils::ExtractSlug( "https://redgifs.com/watch/" )   == "" );
    REQUIRE( Utils::ExtractSlug( "https://gfycat.com/SomeSlug" )  == "" );
    REQUIRE( Utils::ExtractSlug( "" )                             == "" );
}

// ExtractTokenFromAuthResponse ------------------------------------------------

TEST_CASE( "ExtractTokenFromAuthResponse pulls token and ttl from a valid response", "[RedgifsUtils]" )
{
    const std::string body = R"({"token":"abc.def.ghi","expiresIn":7200,"addr":"1.2.3.4"})";
    std::string token{ };
    int ttl = 0;
    REQUIRE( Utils::ExtractTokenFromAuthResponse( body, token, ttl ) );
    REQUIRE( token == "abc.def.ghi" );
    REQUIRE( ttl == 7200 );
}

TEST_CASE( "ExtractTokenFromAuthResponse defaults ttl when expiresIn is absent", "[RedgifsUtils]" )
{
    const std::string body = R"({"token":"abc","addr":"1.2.3.4"})";
    std::string token{ };
    int ttl = 0;
    REQUIRE( Utils::ExtractTokenFromAuthResponse( body, token, ttl ) );
    REQUIRE( token == "abc" );
    REQUIRE( ttl == 3600 );
}

TEST_CASE( "ExtractTokenFromAuthResponse fails when token is missing", "[RedgifsUtils]" )
{
    const std::string body = R"({"addr":"1.2.3.4"})";
    std::string token = "untouched";
    int ttl = 1;
    REQUIRE_FALSE( Utils::ExtractTokenFromAuthResponse( body, token, ttl ) );
    REQUIRE( token == "untouched" );
    REQUIRE( ttl == 1 );
}

TEST_CASE( "ExtractTokenFromAuthResponse fails on malformed JSON", "[RedgifsUtils]" )
{
    std::string token{ };
    int ttl = 0;
    REQUIRE_FALSE( Utils::ExtractTokenFromAuthResponse( "not json at all", token, ttl ) );
    REQUIRE( token.empty( ) );
}

// ExtractMediaUrlFromGifResponse ----------------------------------------------

TEST_CASE( "ExtractMediaUrlFromGifResponse prefers HD over SD", "[RedgifsUtils]" )
{
    const std::string body = R"({"gif":{"id":"slug","urls":{"hd":"https://media.redgifs.com/Slug.mp4","sd":"https://media.redgifs.com/Slug-mobile.mp4"}}})";
    REQUIRE( Utils::ExtractMediaUrlFromGifResponse( body ) == "https://media.redgifs.com/Slug.mp4" );
}

TEST_CASE( "ExtractMediaUrlFromGifResponse falls back to SD when HD is missing", "[RedgifsUtils]" )
{
    const std::string body = R"({"gif":{"urls":{"sd":"https://media.redgifs.com/Slug-mobile.mp4"}}})";
    REQUIRE( Utils::ExtractMediaUrlFromGifResponse( body ) == "https://media.redgifs.com/Slug-mobile.mp4" );
}

TEST_CASE( "ExtractMediaUrlFromGifResponse returns empty when no HD or SD URL is present", "[RedgifsUtils]" )
{
    const std::string body = R"({"gif":{"urls":{"poster":"https://media.redgifs.com/Slug-poster.jpg"}}})";
    REQUIRE( Utils::ExtractMediaUrlFromGifResponse( body ).empty( ) );
}

TEST_CASE( "ExtractMediaUrlFromGifResponse returns empty on missing gif object", "[RedgifsUtils]" )
{
    REQUIRE( Utils::ExtractMediaUrlFromGifResponse( R"({"error":"not found"})" ).empty( ) );
    REQUIRE( Utils::ExtractMediaUrlFromGifResponse( "totally not json" ).empty( ) );
}

// ExtractMediaUrlsFromUserSearchResponse --------------------------------------

TEST_CASE( "ExtractMediaUrlsFromUserSearchResponse returns one URL per gif (HD preferred)", "[RedgifsUtils]" )
{
    const std::string body = R"({
        "page": 1,
        "pages": 3,
        "gifs": [
            { "urls": { "hd": "https://media.redgifs.com/A.mp4", "sd": "https://media.redgifs.com/A-mobile.mp4" } },
            { "urls": { "sd": "https://media.redgifs.com/B-mobile.mp4" } },
            { "urls": { "poster": "https://media.redgifs.com/C-poster.jpg" } }
        ]
    })";

    int totalPages = 0;
    const std::vector<std::string> urls = Utils::ExtractMediaUrlsFromUserSearchResponse( body, totalPages );
    REQUIRE( totalPages == 3 );
    REQUIRE( urls.size( ) == 2 );
    REQUIRE( urls[ 0 ] == "https://media.redgifs.com/A.mp4" );
    REQUIRE( urls[ 1 ] == "https://media.redgifs.com/B-mobile.mp4" );
}

TEST_CASE( "ExtractMediaUrlsFromUserSearchResponse handles empty gifs array", "[RedgifsUtils]" )
{
    int totalPages = 0;
    const std::vector<std::string> urls = Utils::ExtractMediaUrlsFromUserSearchResponse( R"({"page":1,"pages":0,"gifs":[]})", totalPages );
    REQUIRE( urls.empty( ) );
    REQUIRE( totalPages == 0 );
}

TEST_CASE( "ExtractMediaUrlsFromUserSearchResponse returns empty on malformed JSON", "[RedgifsUtils]" )
{
    int totalPages = 0;
    const std::vector<std::string> urls = Utils::ExtractMediaUrlsFromUserSearchResponse( "not json", totalPages );
    REQUIRE( urls.empty( ) );
    REQUIRE( totalPages == 0 );
}
