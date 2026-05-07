#include "catch2/catch_amalgamated.hpp"
#include "requests/AuthRequestHelpers.h"

using namespace ImageScraper;

static RequestOptions MakeOptions( )
{
    RequestOptions opts{ };
    opts.m_UserAgent = "TestAgent/1.0";
    opts.m_CaBundle  = "ca-bundle.crt";
    return opts;
}

static HttpResponse MakeSuccess( )
{
    HttpResponse r{ };
    r.m_Success    = true;
    r.m_StatusCode = 200;
    r.m_Body       = "ok";
    return r;
}

static HttpResponse MakeFailure( int statusCode, const std::string& error = "http error" )
{
    HttpResponse r{ };
    r.m_Success    = false;
    r.m_StatusCode = statusCode;
    r.m_Error      = error;
    return r;
}

// ---------------------------------------------------------------------------
// ValidateOAuthCredentials
// ---------------------------------------------------------------------------
TEST_CASE( "ValidateOAuthCredentials - empty ClientId returns false", "[AuthRequestHelpers]" )
{
    auto opts = MakeOptions( );
    opts.m_ClientId     = "";
    opts.m_ClientSecret = "secret";

    RequestResult result{ };
    const bool valid = AuthRequestHelpers::ValidateOAuthCredentials( opts, result );

    REQUIRE_FALSE( valid );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    REQUIRE_FALSE( result.m_Success );
}

TEST_CASE( "ValidateOAuthCredentials - empty ClientSecret returns false", "[AuthRequestHelpers]" )
{
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "";

    RequestResult result{ };
    const bool valid = AuthRequestHelpers::ValidateOAuthCredentials( opts, result );

    REQUIRE_FALSE( valid );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
}

TEST_CASE( "ValidateOAuthCredentials - both populated returns true", "[AuthRequestHelpers]" )
{
    auto opts = MakeOptions( );
    opts.m_ClientId     = "id";
    opts.m_ClientSecret = "secret";

    RequestResult result{ };
    const bool valid = AuthRequestHelpers::ValidateOAuthCredentials( opts, result );

    REQUIRE( valid );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::OK );
}

// ---------------------------------------------------------------------------
// BuildQueryString
// ---------------------------------------------------------------------------
TEST_CASE( "BuildQueryString - no params returns base unchanged", "[AuthRequestHelpers]" )
{
    auto opts = MakeOptions( );
    const std::string result = AuthRequestHelpers::BuildQueryString( "grant_type=auth_code", opts );
    REQUIRE( result == "grant_type=auth_code" );
}

TEST_CASE( "BuildQueryString - single param appended correctly", "[AuthRequestHelpers]" )
{
    auto opts = MakeOptions( );
    opts.m_QueryParams = { { "code", "abc123" } };
    const std::string result = AuthRequestHelpers::BuildQueryString( "grant_type=auth_code", opts );
    REQUIRE( result == "grant_type=auth_code&code=abc123" );
}

TEST_CASE( "BuildQueryString - multiple params all appended in order", "[AuthRequestHelpers]" )
{
    auto opts = MakeOptions( );
    opts.m_QueryParams = {
        { "code",         "abc123"           },
        { "redirect_uri", "http://localhost" }
    };
    const std::string result = AuthRequestHelpers::BuildQueryString( "grant_type=auth_code", opts );
    REQUIRE( result == "grant_type=auth_code&code=abc123&redirect_uri=http://localhost" );
}

TEST_CASE( "BuildQueryString - empty base with params produces leading &", "[AuthRequestHelpers]" )
{
    auto opts = MakeOptions( );
    opts.m_QueryParams = { { "key", "val" } };
    const std::string result = AuthRequestHelpers::BuildQueryString( "", opts );
    REQUIRE( result == "&key=val" );
}

// ---------------------------------------------------------------------------
// HandleHttpError
// ---------------------------------------------------------------------------
TEST_CASE( "HandleHttpError - success response returns false and leaves result clean", "[AuthRequestHelpers]" )
{
    RequestResult result{ };
    const bool hadError = AuthRequestHelpers::HandleHttpError( MakeSuccess( ), result );

    REQUIRE_FALSE( hadError );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::OK );
}

TEST_CASE( "HandleHttpError - 401 response returns true and maps Unauthorized", "[AuthRequestHelpers]" )
{
    RequestResult result{ };
    const bool hadError = AuthRequestHelpers::HandleHttpError( MakeFailure( 401, "Unauthorized" ), result );

    REQUIRE( hadError );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
    REQUIRE( result.m_Error.m_ErrorString == "Unauthorized" );
}

TEST_CASE( "HandleHttpError - 404 response returns true and maps NotFound", "[AuthRequestHelpers]" )
{
    RequestResult result{ };
    const bool hadError = AuthRequestHelpers::HandleHttpError( MakeFailure( 404, "not found" ), result );

    REQUIRE( hadError );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::NotFound );
    REQUIRE( result.m_Error.m_ErrorString == "not found" );
}

TEST_CASE( "HandleHttpError - 500 response returns true and maps InternalServerError", "[AuthRequestHelpers]" )
{
    RequestResult result{ };
    const bool hadError = AuthRequestHelpers::HandleHttpError( MakeFailure( 500, "server error" ), result );

    REQUIRE( hadError );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
}
