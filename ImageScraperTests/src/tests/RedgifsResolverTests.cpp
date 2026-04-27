#include "catch2/catch_amalgamated.hpp"
#include "network/RedgifsResolver.h"

using ImageScraper::RedgifsResolver;

// IsRedgifsUrl ----------------------------------------------------------------

TEST_CASE( "IsRedgifsUrl matches canonical watch URLs", "[RedgifsResolver]" )
{
    REQUIRE( RedgifsResolver::IsRedgifsUrl( "https://redgifs.com/watch/someslug" ) );
    REQUIRE( RedgifsResolver::IsRedgifsUrl( "https://www.redgifs.com/watch/SomeSlug" ) );
    REQUIRE( RedgifsResolver::IsRedgifsUrl( "https://v3.redgifs.com/watch/another" ) );
    REQUIRE( RedgifsResolver::IsRedgifsUrl( "http://redgifs.com/ifr/embedded" ) );
}

TEST_CASE( "IsRedgifsUrl rejects unrelated URLs", "[RedgifsResolver]" )
{
    REQUIRE_FALSE( RedgifsResolver::IsRedgifsUrl( "https://gfycat.com/Slug" ) );
    REQUIRE_FALSE( RedgifsResolver::IsRedgifsUrl( "https://i.redd.it/foo.jpg" ) );
    REQUIRE_FALSE( RedgifsResolver::IsRedgifsUrl( "" ) );
}

TEST_CASE( "IsRedgifsUrl is case-insensitive on the host", "[RedgifsResolver]" )
{
    REQUIRE( RedgifsResolver::IsRedgifsUrl( "https://RedGifs.com/watch/X" ) );
    REQUIRE( RedgifsResolver::IsRedgifsUrl( "https://WWW.REDGIFS.COM/watch/X" ) );
}

// ExtractSlug -----------------------------------------------------------------

TEST_CASE( "ExtractSlug pulls the lowercased slug from /watch/ URLs", "[RedgifsResolver]" )
{
    REQUIRE( RedgifsResolver::ExtractSlug( "https://redgifs.com/watch/someslug" ) == "someslug" );
    REQUIRE( RedgifsResolver::ExtractSlug( "https://www.redgifs.com/watch/MixedCaseSlug" ) == "mixedcaseslug" );
    REQUIRE( RedgifsResolver::ExtractSlug( "https://v3.redgifs.com/watch/AnotherSlug" ) == "anotherslug" );
}

TEST_CASE( "ExtractSlug supports the /ifr/ embed form", "[RedgifsResolver]" )
{
    REQUIRE( RedgifsResolver::ExtractSlug( "https://www.redgifs.com/ifr/EmbedSlug" ) == "embedslug" );
}

TEST_CASE( "ExtractSlug strips query string and fragment", "[RedgifsResolver]" )
{
    REQUIRE( RedgifsResolver::ExtractSlug( "https://redgifs.com/watch/someslug?utm=foo" ) == "someslug" );
    REQUIRE( RedgifsResolver::ExtractSlug( "https://redgifs.com/watch/someslug#t=10" )    == "someslug" );
}

TEST_CASE( "ExtractSlug strips trailing path segments", "[RedgifsResolver]" )
{
    REQUIRE( RedgifsResolver::ExtractSlug( "https://redgifs.com/watch/someslug/extra" ) == "someslug" );
    REQUIRE( RedgifsResolver::ExtractSlug( "https://redgifs.com/watch/someslug/" )      == "someslug" );
}

TEST_CASE( "ExtractSlug returns empty for URLs that don't carry a slug", "[RedgifsResolver]" )
{
    REQUIRE( RedgifsResolver::ExtractSlug( "https://redgifs.com/" )         == "" );
    REQUIRE( RedgifsResolver::ExtractSlug( "https://redgifs.com/watch/" )   == "" );
    REQUIRE( RedgifsResolver::ExtractSlug( "https://gfycat.com/SomeSlug" )  == "" );
    REQUIRE( RedgifsResolver::ExtractSlug( "" )                             == "" );
}
