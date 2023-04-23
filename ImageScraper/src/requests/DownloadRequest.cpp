#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "curlpp/Options.hpp"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "ui/FrontEnd.h"

#include <sstream>
#include <cmath>

ImageScraper::DownloadRequest::DownloadRequest( std::shared_ptr<FrontEnd> frontEnd ) : m_FrontEnd{ frontEnd }
{
}

ImageScraper::RequestResult ImageScraper::DownloadRequest::Perform( const DownloadOptions& options )
{
    DebugLog( "[%s] DownloadRequest started! URL: %s", __FUNCTION__, options.m_Url.c_str( ) );

    if( options.m_BufferPtr == nullptr )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        DebugLog( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }

    m_BufferPtr = options.m_BufferPtr;

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };

        using namespace std::placeholders;

        // Set the write callback
        curlpp::types::WriteFunctionFunctor writeFunc = std::bind( &DownloadRequest::WriteCallback, this, _1, _2, _3 );
        curlpp::options::WriteFunction* writeCallback = new curlpp::options::WriteFunction( writeFunc );
        request.setOpt( writeCallback );

        // Set the progress callback


        curlpp::types::ProgressFunctionFunctor progressFunc = std::bind( &DownloadRequest::ProgressCallback, this, _1, _2, _3, _4 );
        curlpp::options::ProgressFunction* progressCallback = new curlpp::options::ProgressFunction( progressFunc );
        request.setOpt( progressCallback );
        request.setOpt( new curlpp::options::NoProgress( false ) );

        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );
        request.setOpt( new curlpp::options::Url( options.m_Url ) );
        request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );
        request.setOpt( new curlpp::options::FollowLocation( true ) );

        request.perform( );
    }
    catch( curlpp::RuntimeError& e )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        m_Result.m_Error.m_ErrorString = e.what( );
        DebugLog( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }
    catch( curlpp::LogicError& e )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
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
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        return 0;
    }

    size_t realsize = size * nmemb;
    m_BufferPtr->insert( m_BufferPtr->begin( ) + m_BytesWritten, contents, contents + realsize );
    m_BytesWritten += realsize;

    DebugLog( "[%s] %i bytes written.", __FUNCTION__, static_cast< int >( m_BytesWritten ) );
    return realsize;
}

int ImageScraper::DownloadRequest::ProgressCallback( double dltotal, double dlnow, double ultotal, double ulnow )
{
    if( m_FrontEnd )
    {
        const float current = static_cast< float >( dlnow );
        const float total = static_cast< float >( dltotal );

        if( !IsFloatZero( current ) && !IsFloatZero( total ) )
        {
            m_FrontEnd->UpdateCurrentDownloadProgress( current / total );
        }
    }

    return 0;
}

bool ImageScraper::DownloadRequest::IsFloatZero( float value, float epsilon /*= 1e-6 */ )
{
    return std::abs( value ) < epsilon;
}
