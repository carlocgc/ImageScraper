#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "curlpp/Options.hpp"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"

#include <sstream>

ImageScraper::RequestResult ImageScraper::DownloadRequest::Perform( const DownloadOptions& options )
{
    DebugLog( "[%s] DownloadRequest started! URL: %s", __FUNCTION__, options.m_Url.c_str() );

    if( options.m_BufferPtr == nullptr )
    {
        m_Result.SetError( ResponseErrorCode::InvalidOptions );
        DebugLog( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }

    m_BufferPtr = options.m_BufferPtr;

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };

        // Set the write callback
        using namespace std::placeholders;
        curlpp::types::WriteFunctionFunctor functor = std::bind( &DownloadRequest::WriteCallback, this, _1, _2, _3 );
        curlpp::options::WriteFunction* writeCallback = new curlpp::options::WriteFunction( functor );

        request.setOpt( writeCallback );
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );
        request.setOpt( new curlpp::options::Url( options.m_Url ) );
        request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );
        request.setOpt( new curlpp::options::FollowLocation( true ) );

        request.perform( );
    }
    catch( curlpp::RuntimeError& e )
    {
        m_Result.SetError( ResponseErrorCode::Unknown );
        m_Result.m_Error.m_ErrorString = e.what( );
        DebugLog( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }
    catch( curlpp::LogicError& e )
    {
        m_Result.SetError( ResponseErrorCode::Unknown );
        m_Result.m_Error.m_ErrorString = e.what( );
        DebugLog( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }

    DebugLog( "[%s] DownloadRequest complete!", __FUNCTION__ );
    m_Result.m_Success = true;
    return m_Result;
}

size_t ImageScraper::DownloadRequest::WriteCallback( char* contents, size_t size, size_t nmemb )
{
    if( !m_BufferPtr )
    {
        ErrorLog( "[%s] DownloadRequest failed, buffer invalid!", __FUNCTION__ );
        m_Result.SetError( ResponseErrorCode::InvalidOptions );
        return 0;
    }

    size_t realsize = size * nmemb;
    m_BufferPtr->insert( m_BufferPtr->begin( ) + m_BytesWritten, contents, contents + realsize );
    m_BytesWritten += realsize;

    DebugLog( "[%s] DownloadRequest %i bytes written...", __FUNCTION__, static_cast< int >( m_BytesWritten ) );
    return realsize;
}
