#include "requests/reddit/RefreshAccessTokenRequest.h"
#include "log/Logger.h"
#include "utils/DownloadUtils.h"
#include "curlpp/Options.hpp"
#include "cppcodec/base64_rfc4648.hpp"

const std::string ImageScraper::Reddit::RefreshAccessTokenRequest::s_AuthUrl = "https://www.reddit.com/api/v1/access_token";
const std::string ImageScraper::Reddit::RefreshAccessTokenRequest::s_AuthData = "grant_type=refresh_token";

ImageScraper::RequestResult ImageScraper::Reddit::RefreshAccessTokenRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] RefreshAccessTokenRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_ClientId == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        DebugLog( "[%s] RefreshAccessTokenRequest failed, Client ID not provided.", __FUNCTION__ );
        return result;
    }

    if( options.m_ClientSecret == "" )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        DebugLog( "[%s] RefreshAccessTokenRequest failed, Client Secret not provided.", __FUNCTION__ );
        return result;
    }

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };
        std::ostringstream response;

        // Set up the URL
        request.setOpt( new curlpp::options::Url( s_AuthUrl ) );

        // Use POST method
        request.setOpt( new curlpp::options::Post( true ) );

        // Set up the POST fields
        std::string postData = s_AuthData;
        for( const auto& [key, value] : options.m_QueryParams )
        {
            postData += "&" + key + "=" + value;
        }
        request.setOpt( new curlpp::options::PostFields( postData ) );
        request.setOpt( new curlpp::options::PostFieldSize( static_cast< int >( postData.length( ) ) ) );

        // Set up HTTP Basic Auth
        std::string authCredentials = options.m_ClientId + ":" + options.m_ClientSecret;
        std::string authHeaderValue = "Basic " + cppcodec::base64_rfc4648::encode( authCredentials );;
        std::list<std::string> headers;
        headers.push_back( "Authorization: " + authHeaderValue );
        headers.push_back( "Content-Type: application/x-www-form-urlencoded" );
        headers.push_back( "User-Agent: " + options.m_UserAgent );
        request.setOpt( new curlpp::options::HttpHeader( headers ) );
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );
        request.setOpt( new curlpp::options::Verbose( false ) );

        // Set up output stream
        request.setOpt( new curlpp::options::WriteStream( &response ) );

        // Blocks
        request.perform( );

        result.m_Response = response.str( );
    }
    catch( curlpp::RuntimeError& error )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] RefreshAccessTokenRequest failed!", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }
    catch( curlpp::LogicError& error )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] RefreshAccessTokenRequest failed!", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    if( DownloadHelpers::IsRedditResponseError( result ) )
    {
        DebugLog( "[%s] RefreshAccessTokenRequest failed!", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] RefreshAccessTokenRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
