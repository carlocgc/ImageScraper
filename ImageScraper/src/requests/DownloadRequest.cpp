#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "Logger.h"
#include "curlpp/Options.hpp"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include <sstream>

DownloadResult DownloadRequest::Perform( const DownloadOptions& options )
{
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

        // Set the cert bundle for TLS/HTTPS
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );

        // Set URL for the image
        request.setOpt( new curlpp::options::Url( options.m_Url ) );

        request.perform( );

        m_Result.m_Success = true;
    }
    catch( curlpp::RuntimeError& e )
    {
        ErrorLog( "[%s] Failed, error: %s", __FUNCTION__, e.what( ) );
        m_Result.m_Error.m_ErrorCode = DownloadErrorCode::Unknown;
    }
    catch( curlpp::LogicError& e )
    {
        ErrorLog( "[%s] Failed, error: %s", __FUNCTION__, e.what( ) );
        m_Result.m_Error.m_ErrorCode = DownloadErrorCode::InvalidOptions;
    }

    return m_Result;
}

size_t DownloadRequest::WriteCallback( char* contents, size_t size, size_t nmemb )
{
    if( !m_BufferPtr )
    {
        ErrorLog( "[%s] Failed, invalid buffer", __FUNCTION__ );
        m_Result.m_Error.m_ErrorCode = DownloadErrorCode::InvalidOptions;
        return 0;
    }

    size_t realsize = size * nmemb;
    m_BufferPtr->insert( m_BufferPtr->begin( ) + m_BytesWritten, contents, contents + realsize );
    m_BytesWritten += realsize;

    InfoLog( "[%s] %i bytes written...", __FUNCTION__, static_cast< int >( m_BytesWritten ) );
    return realsize;
}
