#include "requests/VideoDownloadRequest.h"
#include "log/Logger.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <algorithm>

namespace
{
    std::string AvErrorToString( int errorCode )
    {
        char buffer[ AV_ERROR_MAX_STRING_SIZE ]{ };
        av_strerror( errorCode, buffer, sizeof( buffer ) );
        return buffer;
    }
}

ImageScraper::VideoDownloadRequest::VideoDownloadRequest( std::shared_ptr<IServiceSink> sink ) : m_Sink{ sink }
{
}

ImageScraper::RequestResult ImageScraper::VideoDownloadRequest::Perform( const DownloadOptions& options )
{
    LogDebug( "[%s] VideoDownloadRequest started! URL: %s", __FUNCTION__, options.m_Url.c_str( ) );

    if( options.m_OutputFilePath.empty( ) )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        LogError( "[%s] VideoDownloadRequest failed, no output file path specified.", __FUNCTION__ );
        return m_Result;
    }

    if( options.m_Url.empty( ) )
    {
        m_Result.SetError( ResponseErrorCode::BadRequest );
        LogError( "[%s] VideoDownloadRequest failed, no source url specified.", __FUNCTION__ );
        return m_Result;
    }

    m_Result = RequestResult{ };
    m_OutputFilePath = options.m_OutputFilePath;

    const int networkInitResult = avformat_network_init( );
    if( networkInitResult < 0 )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        m_Result.m_Error.m_ErrorString = AvErrorToString( networkInitResult );
        LogError( "[%s] avformat_network_init failed: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
        return m_Result;
    }

    const auto closeRequest = [ this ]( bool cleanupPartialFile )
    {
        Close( );
        avformat_network_deinit( );
        if( cleanupPartialFile )
        {
            CleanupPartialOutputFile( );
        }
    };

    if( !OpenInput( options ) )
    {
        closeRequest( true );
        return m_Result;
    }

    if( !CreateOutput( ) )
    {
        closeRequest( true );
        return m_Result;
    }

    while( true )
    {
        const int readResult = av_read_frame( m_InputFormatCtx, m_Packet );
        if( readResult == AVERROR_EOF )
        {
            break;
        }

        if( readResult == AVERROR_EXIT && IsCancelled( ) )
        {
            LogDebug( "[%s] VideoDownloadRequest aborted by cancellation.", __FUNCTION__ );
            closeRequest( true );
            return m_Result;
        }

        if( readResult < 0 )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            m_Result.m_Error.m_ErrorString = AvErrorToString( readResult );
            LogError( "[%s] av_read_frame failed for %s, error: %s", __FUNCTION__, options.m_Url.c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
            closeRequest( true );
            return m_Result;
        }

        const int inputStreamIndex = m_Packet->stream_index;
        if( inputStreamIndex < 0 ||
            inputStreamIndex >= static_cast<int>( m_OutputStreamByInputStream.size( ) ) ||
            m_OutputStreamByInputStream[ inputStreamIndex ] < 0 )
        {
            av_packet_unref( m_Packet );
            continue;
        }

        UpdateProgress( *m_Packet );

        AVStream* inputStream = m_InputFormatCtx->streams[ inputStreamIndex ];
        AVStream* outputStream = m_OutputFormatCtx->streams[ m_OutputStreamByInputStream[ inputStreamIndex ] ];

        av_packet_rescale_ts( m_Packet, inputStream->time_base, outputStream->time_base );
        m_Packet->stream_index = outputStream->index;
        m_Packet->pos = -1;

        const int writeResult = av_interleaved_write_frame( m_OutputFormatCtx, m_Packet );
        av_packet_unref( m_Packet );

        if( writeResult == AVERROR_EXIT && IsCancelled( ) )
        {
            LogDebug( "[%s] VideoDownloadRequest aborted by cancellation.", __FUNCTION__ );
            closeRequest( true );
            return m_Result;
        }

        if( writeResult < 0 )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            m_Result.m_Error.m_ErrorString = AvErrorToString( writeResult );
            LogError( "[%s] av_interleaved_write_frame failed for %s, error: %s", __FUNCTION__, m_OutputFilePath.string( ).c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
            closeRequest( true );
            return m_Result;
        }
    }

    const int trailerResult = av_write_trailer( m_OutputFormatCtx );
    if( trailerResult < 0 )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        m_Result.m_Error.m_ErrorString = AvErrorToString( trailerResult );
        LogError( "[%s] av_write_trailer failed for %s, error: %s", __FUNCTION__, m_OutputFilePath.string( ).c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
        closeRequest( true );
        return m_Result;
    }

    if( m_Sink )
    {
        m_Sink->OnCurrentDownloadProgress( 1.0f );
    }

    closeRequest( false );

    LogDebug( "[%s] VideoDownloadRequest complete!", __FUNCTION__ );
    m_Result.m_Success = true;
    return m_Result;
}

int ImageScraper::VideoDownloadRequest::InterruptCallback( void* opaque )
{
    const auto* request = static_cast<VideoDownloadRequest*>( opaque );
    return request && request->IsCancelled( ) ? 1 : 0;
}

bool ImageScraper::VideoDownloadRequest::OpenInput( const DownloadOptions& options )
{
    m_InputFormatCtx = avformat_alloc_context( );
    if( !m_InputFormatCtx )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        LogError( "[%s] avformat_alloc_context failed.", __FUNCTION__ );
        return false;
    }

    m_InputFormatCtx->interrupt_callback.callback = &VideoDownloadRequest::InterruptCallback;
    m_InputFormatCtx->interrupt_callback.opaque = this;

    AVDictionary* openOptions = nullptr;
    if( !options.m_UserAgent.empty( ) )
    {
        av_dict_set( &openOptions, "user_agent", options.m_UserAgent.c_str( ), 0 );
    }

    if( !options.m_CaBundle.empty( ) )
    {
        av_dict_set( &openOptions, "ca_file", options.m_CaBundle.c_str( ), 0 );
    }

    const int openResult = avformat_open_input( &m_InputFormatCtx, options.m_Url.c_str( ), nullptr, &openOptions );
    av_dict_free( &openOptions );

    if( openResult < 0 )
    {
        if( openResult != AVERROR_EXIT || !IsCancelled( ) )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            m_Result.m_Error.m_ErrorString = AvErrorToString( openResult );
            LogError( "[%s] avformat_open_input failed for %s, error: %s", __FUNCTION__, options.m_Url.c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
        }

        return false;
    }

    const int streamInfoResult = avformat_find_stream_info( m_InputFormatCtx, nullptr );
    if( streamInfoResult < 0 )
    {
        if( streamInfoResult != AVERROR_EXIT || !IsCancelled( ) )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            m_Result.m_Error.m_ErrorString = AvErrorToString( streamInfoResult );
            LogError( "[%s] avformat_find_stream_info failed for %s, error: %s", __FUNCTION__, options.m_Url.c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
        }

        return false;
    }

    m_Packet = av_packet_alloc( );
    if( !m_Packet )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        LogError( "[%s] av_packet_alloc failed.", __FUNCTION__ );
        return false;
    }

    m_VideoInputStream = av_find_best_stream( m_InputFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0 );
    if( m_VideoInputStream < 0 )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        m_Result.m_Error.m_ErrorString = "No video stream found.";
        LogError( "[%s] No video stream found in source: %s", __FUNCTION__, options.m_Url.c_str( ) );
        return false;
    }

    m_TotalDurationSeconds = 0.0;
    if( m_InputFormatCtx->duration != AV_NOPTS_VALUE )
    {
        m_TotalDurationSeconds = static_cast<double>( m_InputFormatCtx->duration ) / static_cast<double>( AV_TIME_BASE );
    }

    return true;
}

bool ImageScraper::VideoDownloadRequest::CreateOutput( )
{
    const std::string outputPath = m_OutputFilePath.string( );
    const int allocResult = avformat_alloc_output_context2( &m_OutputFormatCtx, nullptr, nullptr, outputPath.c_str( ) );
    if( allocResult < 0 || !m_OutputFormatCtx )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        m_Result.m_Error.m_ErrorString = allocResult < 0 ? AvErrorToString( allocResult ) : "Could not allocate output context.";
        LogError( "[%s] avformat_alloc_output_context2 failed for %s, error: %s", __FUNCTION__, outputPath.c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
        return false;
    }

    m_OutputStreamByInputStream.assign( m_InputFormatCtx->nb_streams, -1 );

    for( unsigned int i = 0; i < m_InputFormatCtx->nb_streams; ++i )
    {
        AVStream* inputStream = m_InputFormatCtx->streams[ i ];
        const AVMediaType mediaType = inputStream->codecpar->codec_type;
        if( mediaType != AVMEDIA_TYPE_VIDEO && mediaType != AVMEDIA_TYPE_AUDIO )
        {
            continue;
        }

        AVStream* outputStream = avformat_new_stream( m_OutputFormatCtx, nullptr );
        if( !outputStream )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            LogError( "[%s] avformat_new_stream failed.", __FUNCTION__ );
            return false;
        }

        const int copyResult = avcodec_parameters_copy( outputStream->codecpar, inputStream->codecpar );
        if( copyResult < 0 )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            m_Result.m_Error.m_ErrorString = AvErrorToString( copyResult );
            LogError( "[%s] avcodec_parameters_copy failed, error: %s", __FUNCTION__, m_Result.m_Error.m_ErrorString.c_str( ) );
            return false;
        }

        outputStream->codecpar->codec_tag = 0;
        outputStream->time_base = inputStream->time_base;
        m_OutputStreamByInputStream[ i ] = outputStream->index;
    }

    if( !( m_OutputFormatCtx->oformat->flags & AVFMT_NOFILE ) )
    {
        const int avioResult = avio_open( &m_OutputFormatCtx->pb, outputPath.c_str( ), AVIO_FLAG_WRITE );
        if( avioResult < 0 )
        {
            m_Result.SetError( ResponseErrorCode::InternalServerError );
            m_Result.m_Error.m_ErrorString = AvErrorToString( avioResult );
            LogError( "[%s] avio_open failed for %s, error: %s", __FUNCTION__, outputPath.c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
            return false;
        }
    }

    const int headerResult = avformat_write_header( m_OutputFormatCtx, nullptr );
    if( headerResult < 0 )
    {
        m_Result.SetError( ResponseErrorCode::InternalServerError );
        m_Result.m_Error.m_ErrorString = AvErrorToString( headerResult );
        LogError( "[%s] avformat_write_header failed for %s, error: %s", __FUNCTION__, outputPath.c_str( ), m_Result.m_Error.m_ErrorString.c_str( ) );
        return false;
    }

    return true;
}

bool ImageScraper::VideoDownloadRequest::IsCancelled( ) const
{
    return m_Sink && m_Sink->IsCancelled( );
}

void ImageScraper::VideoDownloadRequest::UpdateProgress( const AVPacket& packet ) const
{
    if( !m_Sink || m_TotalDurationSeconds <= 0.0 || packet.stream_index != m_VideoInputStream )
    {
        return;
    }

    const int64_t timestamp = packet.pts != AV_NOPTS_VALUE ? packet.pts : packet.dts;
    if( timestamp == AV_NOPTS_VALUE )
    {
        return;
    }

    const AVStream* videoStream = m_InputFormatCtx->streams[ m_VideoInputStream ];
    const double currentSeconds = static_cast<double>( timestamp ) * av_q2d( videoStream->time_base );
    if( currentSeconds < 0.0 )
    {
        return;
    }

    const float progress = static_cast<float>( std::clamp( currentSeconds / m_TotalDurationSeconds, 0.0, 1.0 ) );
    m_Sink->OnCurrentDownloadProgress( progress );
}

void ImageScraper::VideoDownloadRequest::CleanupPartialOutputFile( )
{
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

void ImageScraper::VideoDownloadRequest::Close( )
{
    if( m_Packet )
    {
        av_packet_free( &m_Packet );
        m_Packet = nullptr;
    }

    if( m_OutputFormatCtx )
    {
        if( !( m_OutputFormatCtx->oformat->flags & AVFMT_NOFILE ) && m_OutputFormatCtx->pb )
        {
            avio_closep( &m_OutputFormatCtx->pb );
        }

        avformat_free_context( m_OutputFormatCtx );
        m_OutputFormatCtx = nullptr;
    }

    if( m_InputFormatCtx )
    {
        avformat_close_input( &m_InputFormatCtx );
        m_InputFormatCtx = nullptr;
    }

    m_OutputStreamByInputStream.clear( );
    m_VideoInputStream = -1;
    m_TotalDurationSeconds = 0.0;
}
