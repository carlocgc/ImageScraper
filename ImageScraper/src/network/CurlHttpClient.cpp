#include "network/CurlHttpClient.h"
#include "log/Logger.h"

#include "curlpp/Options.hpp"
#include "curlpp/Infos.hpp"
#include <curlpp/Easy.hpp>
#include <curlpp/cURLpp.hpp>

#include <sstream>
#include <list>

ImageScraper::HttpResponse ImageScraper::CurlHttpClient::Get( const HttpRequest& request )
{
    return Execute( request, false );
}

ImageScraper::HttpResponse ImageScraper::CurlHttpClient::Post( const HttpRequest& request )
{
    return Execute( request, true );
}

ImageScraper::HttpResponse ImageScraper::CurlHttpClient::Execute( const HttpRequest& request, bool isPost )
{
    HttpResponse response{ };

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy easy{ };
        std::ostringstream body{ };

        easy.setOpt( new curlpp::options::Url( request.m_Url ) );
        easy.setOpt( new curlpp::options::UserAgent( request.m_UserAgent ) );
        easy.setOpt( new curlpp::options::CaInfo( request.m_CaBundle ) );
        easy.setOpt( new curlpp::options::WriteStream( &body ) );
        easy.setOpt( new curlpp::options::FollowLocation( true ) );
        easy.setOpt( new curlpp::options::Timeout( request.m_TimeoutSeconds ) );

        if( !request.m_Headers.empty( ) )
        {
            std::list<std::string> headers( request.m_Headers.begin( ), request.m_Headers.end( ) );
            easy.setOpt( new curlpp::options::HttpHeader( headers ) );
        }

        if( isPost )
        {
            easy.setOpt( new curlpp::options::PostFields( request.m_Body ) );
            easy.setOpt( new curlpp::options::PostFieldSize( static_cast<long>( request.m_Body.size( ) ) ) );
        }

        easy.perform( );

        response.m_Body = body.str( );
        response.m_StatusCode = curlpp::infos::ResponseCode::get( easy );
        response.m_Success = ( response.m_StatusCode >= 200 && response.m_StatusCode < 300 );
    }
    catch( curlpp::RuntimeError& error )
    {
        response.m_Error = error.what( );
        DebugLog( "[%s] CurlHttpClient runtime error: %s", __FUNCTION__, response.m_Error.c_str( ) );
    }
    catch( curlpp::LogicError& error )
    {
        response.m_Error = error.what( );
        DebugLog( "[%s] CurlHttpClient logic error: %s", __FUNCTION__, response.m_Error.c_str( ) );
    }

    return response;
}
