#include "requests/reddit/AuthenticationRequest.h"
#include "utils/DownloadUtils.h"
#include "curlpp/Options.hpp"
#include "cppcodec/base64_rfc4648.hpp"

const std::string ImageScraper::Reddit::AuthenticationRequest::s_AuthUrl = "https://www.reddit.com/api/v1/access_token";
const std::string ImageScraper::Reddit::AuthenticationRequest::s_AuthData = "grant_type=client_credentials";

ImageScraper::RequestResult ImageScraper::Reddit::AuthenticationRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] AuthenticationRequest started!", __FUNCTION__ );

    RequestResult result{ };

    if( options.m_ClientId == "" )
    {
        result.SetError( ResponseErrorCode::InvalidOptions );
        DebugLog( "[%s] AuthenticationRequest failed, Client ID not provided.", __FUNCTION__ );
        return result;
    }

    if( options.m_ClientSecret == "" )
    {
        result.SetError( ResponseErrorCode::InvalidOptions );
        DebugLog( "[%s] AuthenticationRequest failed, Client Secret not provided.", __FUNCTION__ );
        return result;
    }

    const std::string credentials = options.m_ClientId + ":" + options.m_ClientSecret;
    const std::string encodedCredentials = cppcodec::base64_rfc4648::encode( credentials );
    const std::string authHeader = "Basic " + encodedCredentials;

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };
        std::ostringstream response;

        request.setOpt( new curlpp::options::Url( s_AuthUrl ) );
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );
        request.setOpt( new curlpp::options::Verbose( false ) );
        request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );
        request.setOpt( new curlpp::options::HttpHeader( { "Content-Type: application/x-www-form-urlencoded", "Authorization: " + authHeader } ) );
        request.setOpt( new curlpp::options::PostFields( s_AuthData ) );
        request.setOpt( new curlpp::options::PostFieldSize( static_cast<long>( s_AuthData.length( ) ) ) );
        request.setOpt( new curlpp::options::WriteStream( &response ) );

        // Blocks
        request.perform( );

        result.m_Response = response.str( );
    }
    catch( curlpp::RuntimeError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] AuthenticationRequest failed!", __FUNCTION__, result.m_Error.m_ErrorString.c_str() );
        return result;

    }
    catch( curlpp::LogicError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] AuthenticationRequest failed!", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    if( DownloadHelpers::IsResponseError( result ) )
    {
        DebugLog( "[%s] AuthenticationRequest failed!", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] AuthenticationRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
