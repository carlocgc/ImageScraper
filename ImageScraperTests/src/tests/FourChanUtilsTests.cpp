#include "catch2/catch_amalgamated.hpp"
#include "utils/FourChanUtils.h"

using namespace ImageScraper::FourChanUtils;

// ----------------------------------------------------------------------------
// GetPageCountForBoard
// ----------------------------------------------------------------------------

TEST_CASE( "GetPageCountForBoard returns 0 when boards field missing", "[FourChanUtils]" )
{
    Json response = { { "other", "value" } };
    REQUIRE( GetPageCountForBoard( "g", response ) == 0 );
}

TEST_CASE( "GetPageCountForBoard returns 0 when board not found", "[FourChanUtils]" )
{
    Json response = {
        { "boards", {
            { { "board", "a" }, { "pages", 10 } }
        } }
    };
    REQUIRE( GetPageCountForBoard( "g", response ) == 0 );
}

TEST_CASE( "GetPageCountForBoard returns page count for matching board", "[FourChanUtils]" )
{
    Json response = {
        { "boards", {
            { { "board", "a" }, { "pages", 10 } },
            { { "board", "g" }, { "pages", 15 } },
            { { "board", "v" }, { "pages", 12 } }
        } }
    };
    REQUIRE( GetPageCountForBoard( "g", response ) == 15 );
}

TEST_CASE( "GetPageCountForBoard returns 0 for empty boards array", "[FourChanUtils]" )
{
    Json response = { { "boards", Json::array( ) } };
    REQUIRE( GetPageCountForBoard( "g", response ) == 0 );
}

// ----------------------------------------------------------------------------
// GetFileNamesFromResponse
// ----------------------------------------------------------------------------

TEST_CASE( "GetFileNamesFromResponse returns empty when threads field missing", "[FourChanUtils]" )
{
    Json response = { { "other", "value" } };
    REQUIRE( GetFileNamesFromResponse( response ).empty( ) );
}

TEST_CASE( "GetFileNamesFromResponse returns empty for empty threads", "[FourChanUtils]" )
{
    Json response = { { "threads", Json::array( ) } };
    REQUIRE( GetFileNamesFromResponse( response ).empty( ) );
}

TEST_CASE( "GetFileNamesFromResponse builds filename from tim and ext", "[FourChanUtils]" )
{
    Json response = {
        { "threads", {
            { { "posts", {
                { { "tim", 1234567890123ull }, { "ext", ".jpg" } }
            } } }
        } }
    };

    auto result = GetFileNamesFromResponse( response );
    REQUIRE( result.size( ) == 1 );
    REQUIRE( result[ 0 ] == "1234567890123.jpg" );
}

TEST_CASE( "GetFileNamesFromResponse skips posts missing tim or ext", "[FourChanUtils]" )
{
    Json response = {
        { "threads", {
            { { "posts", {
                { { "tim", 111ull } },                          // missing ext
                { { "ext", ".png" } },                          // missing tim
                { { "tim", 222ull }, { "ext", ".gif" } }        // valid
            } } }
        } }
    };

    auto result = GetFileNamesFromResponse( response );
    REQUIRE( result.size( ) == 1 );
    REQUIRE( result[ 0 ] == "222.gif" );
}

TEST_CASE( "GetFileNamesFromResponse collects files across multiple threads", "[FourChanUtils]" )
{
    Json response = {
        { "threads", {
            { { "posts", {
                { { "tim", 111ull }, { "ext", ".jpg" } }
            } } },
            { { "posts", {
                { { "tim", 222ull }, { "ext", ".png" } }
            } } }
        } }
    };

    auto result = GetFileNamesFromResponse( response );
    REQUIRE( result.size( ) == 2 );
}

TEST_CASE( "GetFileNamesFromResponse skips threads with no posts field", "[FourChanUtils]" )
{
    Json response = {
        { "threads", {
            { { "no_posts", "here" } }
        } }
    };

    REQUIRE( GetFileNamesFromResponse( response ).empty( ) );
}
