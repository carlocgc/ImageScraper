#include "catch2/catch_amalgamated.hpp"
#include "requests/RequestTypes.h"

using namespace ImageScraper;

TEST_CASE( "ResponseErrorCodefromInt maps known HTTP codes", "[RequestTypes]" )
{
    REQUIRE( ResponseErrorCodefromInt( 200 ) == ResponseErrorCode::OK );
    REQUIRE( ResponseErrorCodefromInt( 400 ) == ResponseErrorCode::BadRequest );
    REQUIRE( ResponseErrorCodefromInt( 401 ) == ResponseErrorCode::Unauthorized );
    REQUIRE( ResponseErrorCodefromInt( 403 ) == ResponseErrorCode::Forbidden );
    REQUIRE( ResponseErrorCodefromInt( 404 ) == ResponseErrorCode::NotFound );
    REQUIRE( ResponseErrorCodefromInt( 500 ) == ResponseErrorCode::InternalServerError );
}

TEST_CASE( "ResponseErrorCodefromInt returns InternalServerError for unknown codes", "[RequestTypes]" )
{
    REQUIRE( ResponseErrorCodefromInt( 0 ) == ResponseErrorCode::InternalServerError );
    REQUIRE( ResponseErrorCodefromInt( 429 ) == ResponseErrorCode::InternalServerError );
    REQUIRE( ResponseErrorCodefromInt( 503 ) == ResponseErrorCode::InternalServerError );
}

TEST_CASE( "ResponseErrorCodeToString returns correct strings", "[RequestTypes]" )
{
    REQUIRE( ResponseErrorCodeToString( ResponseErrorCode::OK ) == "OK" );
    REQUIRE( ResponseErrorCodeToString( ResponseErrorCode::BadRequest ) == "Bad Request" );
    REQUIRE( ResponseErrorCodeToString( ResponseErrorCode::Unauthorized ) == "Unauthorized" );
    REQUIRE( ResponseErrorCodeToString( ResponseErrorCode::Forbidden ) == "Forbidden" );
    REQUIRE( ResponseErrorCodeToString( ResponseErrorCode::NotFound ) == "Not Found" );
    REQUIRE( ResponseErrorCodeToString( ResponseErrorCode::InternalServerError ) == "Internal Server Error" );
}

TEST_CASE( "RequestResult::SetError sets code and string together", "[RequestTypes]" )
{
    RequestResult result;
    result.SetError( ResponseErrorCode::Unauthorized );

    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
    REQUIRE( result.m_Error.m_ErrorString == "Unauthorized" );
}

TEST_CASE( "RequestResult defaults to unsuccessful with OK error code", "[RequestTypes]" )
{
    RequestResult result;

    REQUIRE( result.m_Success == false );
    REQUIRE( result.m_Response.empty() );
    REQUIRE( result.m_Error.m_ErrorCode == ResponseErrorCode::OK );
    REQUIRE( result.m_Error.m_ErrorString.empty() );
}
