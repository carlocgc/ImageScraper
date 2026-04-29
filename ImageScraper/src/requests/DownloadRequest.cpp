#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "utils/StringUtils.h"
#include "log/Logger.h"
#include "curlpp/Infos.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"

#include <sstream>
#include <cmath>

ImageScraper::DownloadRequest::DownloadRequest( std::shared_ptr<IServiceSink> sink ) : m_Sink{ sink }
{
}

ImageScraper::RequestResult ImageScraper::DownloadRequest::Perform( const DownloadOptions& options )
{
    LogDebug( "[%s] DownloadRequest started! URL: %s", __FUNCTION__, options.m_Url.c_str( ) );

    if( options.m_OutputFilePath.empty( ) )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        LogDebug( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }

    m_Result = RequestResult{ };
    m_OutputFilePath = options.m_OutputFilePath;

    if( !m_OutputFilePath.empty( ) )
    {
        m_OutputFile.open( m_OutputFilePath, std::ios::binary | std::ios::trunc );
        if( !m_OutputFile.is_open( ) )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            LogError( "[%s] DownloadRequest failed, could not open file for write: %s", __FUNCTION__, m_OutputFilePath.string( ).c_str( ) );
            return m_Result;
        }
    }

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

        // Curl returns success for any completed transfer, including 4xx/5xx
        // responses where the server's error body gets written to disk as if it
        // were the requested media. Reject responses that aren't a real 2xx
        // file payload so callers don't end up with HTML/JSON error stubs.
        const long status = curlpp::infos::ResponseCode::get( request );
        std::string contentType{ };
        try
        {
            contentType = curlpp::infos::ContentType::get( request );
        }
        catch( const curlpp::RuntimeError& )
        {
            // Some responses don't expose a Content-Type; treat as empty.
        }

        const std::string contentTypeLower = StringUtils::ToLower( contentType );

        const bool statusOk = ( status >= 200 && status < 300 );
        const bool contentTypeIsError = contentTypeLower.rfind( "text/html", 0 ) == 0
                                        || contentTypeLower.rfind( "application/json", 0 ) == 0
                                        || contentTypeLower.rfind( "text/plain", 0 ) == 0;

        if( !statusOk || contentTypeIsError )
        {
            CleanupPartialOutputFile( );
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            std::ostringstream oss;
            oss << "HTTP " << status;
            if( !contentType.empty( ) )
            {
                oss << " (Content-Type: " << contentType << ")";
            }
            m_Result.m_Error.m_ErrorString = oss.str( );
            LogError( "[%s] DownloadRequest rejected response: %s, url: %s",
                      __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ), options.m_Url.c_str( ) );
            return m_Result;
        }
    }
    catch( curlpp::RuntimeError& e )
    {
        CleanupPartialOutputFile( );
        if( m_Sink && m_Sink->IsCancelled( ) )
        {
            LogDebug( "[%s] DownloadRequest aborted by cancellation.", __FUNCTION__ );
        }
        else
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            m_Result.m_Error.m_ErrorString = e.what( );
            LogDebug( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        }
        return m_Result;
    }
    catch( curlpp::LogicError& e )
    {
        CleanupPartialOutputFile( );
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        m_Result.m_Error.m_ErrorString = e.what( );
        LogDebug( "[%s] DownloadRequest failed! error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }

    if( m_OutputFile.is_open( ) )
    {
        m_OutputFile.close( );
    }

    LogDebug( "[%s] DownloadRequest complete!", __FUNCTION__ );
    m_Result.m_Success = true;
    return m_Result;
}

size_t ImageScraper::DownloadRequest::WriteCallback( char* contents, size_t size, size_t nmemb )
{
    const size_t realsize = size * nmemb;

    if( m_OutputFilePath.empty( ) )
    {
        LogError( "[%s] DownloadRequest failed, no output file path specified!", __FUNCTION__ );
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        return 0;
    }

    if( !m_OutputFile.is_open( ) )
    {
        LogError( "[%s] DownloadRequest failed, output file invalid!", __FUNCTION__ );
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        return 0;
    }

    m_OutputFile.write( contents, static_cast<std::streamsize>( realsize ) );
    if( !m_OutputFile.good( ) )
    {
        LogError( "[%s] DownloadRequest failed while writing file: %s", __FUNCTION__, m_OutputFilePath.string( ).c_str( ) );
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        return 0;
    }

    return realsize;
}

int ImageScraper::DownloadRequest::ProgressCallback( double dltotal, double dlnow, double ultotal, double ulnow )
{
    if( m_Sink )
    {
        if( m_Sink->IsCancelled( ) )
        {
            return 1; // Non-zero aborts the transfer immediately (CURLE_ABORTED_BY_CALLBACK)
        }

        const float current = static_cast< float >( dlnow );
        const float total = static_cast< float >( dltotal );

        if( !IsFloatZero( current ) && !IsFloatZero( total ) )
        {
            m_Sink->OnCurrentDownloadProgress( current / total );
        }
    }

    return 0;
}

bool ImageScraper::DownloadRequest::IsFloatZero( float value, float epsilon /*= 1e-6 */ )
{
    return std::abs( value ) < epsilon;
}

void ImageScraper::DownloadRequest::CleanupPartialOutputFile( )
{
    if( m_OutputFile.is_open( ) )
    {
        m_OutputFile.close( );
    }

    if( m_OutputFilePath.empty( ) )
    {
        return;
    }

    std::error_code ec;
    std::filesystem::remove( m_OutputFilePath, ec );
    if( ec )
    {
        WarningLog( "[%s] Failed to remove partial download file %s: %s", __FUNCTION__, m_OutputFilePath.string( ).c_str( ), ec.message( ).c_str( ) );
    }
}
