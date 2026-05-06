#define NOMINMAX
#include "ui/GifPlaybackCache.h"
#include "log/Logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace
{
    constexpr int k_DefaultGifDelayMs = 100;

    AVPixelFormat NormaliseSwscalePixelFormat( AVPixelFormat pixelFormat )
    {
        switch( pixelFormat )
        {
            case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
            case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
            case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
            case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P;
            case AV_PIX_FMT_YUVJ411P: return AV_PIX_FMT_YUV411P;
            default:                 return pixelFormat;
        }
    }

    int GetSwscaleRangeFlag( AVPixelFormat pixelFormat, AVColorRange colorRange )
    {
        if( colorRange == AVCOL_RANGE_JPEG )
        {
            return 1;
        }

        if( colorRange == AVCOL_RANGE_MPEG )
        {
            return 0;
        }

        switch( pixelFormat )
        {
            case AV_PIX_FMT_YUVJ420P:
            case AV_PIX_FMT_YUVJ422P:
            case AV_PIX_FMT_YUVJ444P:
            case AV_PIX_FMT_YUVJ440P:
            case AV_PIX_FMT_YUVJ411P:
                return 1;
            default:
                return 0;
        }
    }

    bool ShouldCancel( const ImageScraper::GifPlaybackCache::CancelCheck& shouldCancel )
    {
        return shouldCancel && shouldCancel( );
    }

    int NormaliseDelayMs( int64_t durationTicks, AVRational timeBase )
    {
        if( durationTicks <= 0 || timeBase.num <= 0 || timeBase.den <= 0 )
        {
            return k_DefaultGifDelayMs;
        }

        const double durationMs = av_q2d( timeBase ) * static_cast<double>( durationTicks ) * 1000.0;
        const auto roundedMs = static_cast<int64_t>( std::llround( durationMs ) );
        if( roundedMs <= 0 )
        {
            return k_DefaultGifDelayMs;
        }

        return static_cast<int>( std::min<int64_t>( roundedMs, std::numeric_limits<int>::max( ) ) );
    }

    bool ConvertFrameToRgba(
        AVFrame* frame,
        SwsContext* swsContext,
        int width,
        int height,
        std::vector<uint8_t>& rgbaOut )
    {
        const int exactBytes = width * height * 4;
        const int paddedBytes = exactBytes + 64;

        rgbaOut.resize( static_cast<size_t>( paddedBytes ) );

        uint8_t* dstData[ 4 ] = { rgbaOut.data( ), nullptr, nullptr, nullptr };
        int      dstLines[ 4 ] = { width * 4, 0, 0, 0 };

        sws_scale(
            swsContext,
            frame->data,
            frame->linesize,
            0,
            height,
            dstData,
            dstLines );

        rgbaOut.resize( static_cast<size_t>( exactBytes ) );
        return true;
    }

    struct DecodeContext
    {
        AVFormatContext* m_FormatContext{ nullptr };
        AVCodecContext*  m_CodecContext{ nullptr };
        SwsContext*      m_SwsContext{ nullptr };
        AVFrame*         m_Frame{ nullptr };
        AVPacket*        m_Packet{ nullptr };
        int              m_VideoStreamIndex{ -1 };

        ~DecodeContext( )
        {
            if( m_Frame ) { av_frame_free( &m_Frame ); }
            if( m_Packet ) { av_packet_free( &m_Packet ); }
            if( m_SwsContext ) { sws_freeContext( m_SwsContext ); }
            if( m_CodecContext ) { avcodec_free_context( &m_CodecContext ); }
            if( m_FormatContext ) { avformat_close_input( &m_FormatContext ); }
        }
    };
}

bool ImageScraper::GifPlaybackCache::Runtime::HasMultipleFrames( ) const
{
    return m_Frames.size( ) > 1 && m_TotalDurationMs > 0;
}

int64_t ImageScraper::GifPlaybackCache::Runtime::GetTimeMsForNormalized( float normalized ) const
{
    if( m_TotalDurationMs <= 0 )
    {
        return 0;
    }

    normalized = std::clamp( normalized, 0.0f, 1.0f );
    const double scaled = static_cast<double>( normalized ) * static_cast<double>( m_TotalDurationMs - 1 );
    return static_cast<int64_t>( std::llround( scaled ) );
}

size_t ImageScraper::GifPlaybackCache::Runtime::GetFrameIndexForTimeMs( int64_t targetTimeMs ) const
{
    if( m_Frames.empty( ) )
    {
        return 0;
    }

    if( m_TotalDurationMs <= 0 )
    {
        return 0;
    }

    targetTimeMs = std::clamp<int64_t>( targetTimeMs, 0, m_TotalDurationMs - 1 );

    const auto it = std::upper_bound(
        m_Frames.begin( ),
        m_Frames.end( ),
        targetTimeMs,
        []( int64_t timeMs, const Frame& frame )
        {
            return timeMs < frame.m_EndTimeMs;
        } );

    if( it == m_Frames.end( ) )
    {
        return m_Frames.size( ) - 1;
    }

    return static_cast<size_t>( std::distance( m_Frames.begin( ), it ) );
}

ImageScraper::GifPlaybackCache::DecodeResult ImageScraper::GifPlaybackCache::DecodeGifFile(
    const std::string& filepath,
    const CancelCheck& shouldCancel )
{
    DecodeResult result;

    if( ShouldCancel( shouldCancel ) )
    {
        result.m_Status = DecodeStatus::Cancelled;
        return result;
    }

    DecodeContext ctx;

    if( avformat_open_input( &ctx.m_FormatContext, filepath.c_str( ), nullptr, nullptr ) < 0 )
    {
        LogError( "[%s] avformat_open_input failed for: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "avformat_open_input failed";
        return result;
    }

    if( avformat_find_stream_info( ctx.m_FormatContext, nullptr ) < 0 )
    {
        LogError( "[%s] avformat_find_stream_info failed for: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "avformat_find_stream_info failed";
        return result;
    }

    for( unsigned int index = 0; index < ctx.m_FormatContext->nb_streams; ++index )
    {
        if( ctx.m_FormatContext->streams[ index ]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO )
        {
            ctx.m_VideoStreamIndex = static_cast<int>( index );
            break;
        }
    }

    if( ctx.m_VideoStreamIndex < 0 )
    {
        LogError( "[%s] No GIF video stream found in: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "No video stream";
        return result;
    }

    AVStream* stream = ctx.m_FormatContext->streams[ ctx.m_VideoStreamIndex ];
    AVCodecParameters* codecParameters = stream->codecpar;
    const AVCodec* codec = avcodec_find_decoder( codecParameters->codec_id );
    if( !codec )
    {
        LogError( "[%s] Unsupported GIF codec in: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "Unsupported codec";
        return result;
    }

    ctx.m_CodecContext = avcodec_alloc_context3( codec );
    if( !ctx.m_CodecContext )
    {
        LogError( "[%s] avcodec_alloc_context3 failed", __FUNCTION__ );
        result.m_Error = "avcodec_alloc_context3 failed";
        return result;
    }

    if( avcodec_parameters_to_context( ctx.m_CodecContext, codecParameters ) < 0 )
    {
        LogError( "[%s] avcodec_parameters_to_context failed", __FUNCTION__ );
        result.m_Error = "avcodec_parameters_to_context failed";
        return result;
    }

    if( avcodec_open2( ctx.m_CodecContext, codec, nullptr ) < 0 )
    {
        LogError( "[%s] avcodec_open2 failed for: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "avcodec_open2 failed";
        return result;
    }

    const int width = ctx.m_CodecContext->width;
    const int height = ctx.m_CodecContext->height;
    if( width <= 0 || height <= 0 )
    {
        LogError( "[%s] Invalid GIF dimensions for: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "Invalid dimensions";
        return result;
    }

    const AVPixelFormat swscalePixelFormat = NormaliseSwscalePixelFormat( ctx.m_CodecContext->pix_fmt );
    ctx.m_SwsContext = sws_getContext(
        width,
        height,
        swscalePixelFormat,
        width,
        height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr );

    if( !ctx.m_SwsContext )
    {
        LogError( "[%s] sws_getContext failed for: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "sws_getContext failed";
        return result;
    }

    const int* coefficients = sws_getCoefficients( SWS_CS_DEFAULT );
    const int sourceRange = GetSwscaleRangeFlag( ctx.m_CodecContext->pix_fmt, ctx.m_CodecContext->color_range );
    if( sws_setColorspaceDetails(
            ctx.m_SwsContext,
            coefficients,
            sourceRange,
            coefficients,
            1,
            0,
            1 << 16,
            1 << 16 ) < 0 )
    {
        WarningLog( "[%s] sws_setColorspaceDetails failed for: %s", __FUNCTION__, filepath.c_str( ) );
    }

    ctx.m_Frame = av_frame_alloc( );
    ctx.m_Packet = av_packet_alloc( );
    if( !ctx.m_Frame || !ctx.m_Packet )
    {
        LogError( "[%s] GIF frame/packet alloc failed", __FUNCTION__ );
        result.m_Error = "Frame/packet alloc failed";
        return result;
    }

    Runtime runtime;
    runtime.m_Width = width;
    runtime.m_Height = height;

    int64_t cumulativeMs = 0;

    auto appendDecodedFrames = [&]( int64_t packetDurationTicks ) -> bool
    {
        while( true )
        {
            if( ShouldCancel( shouldCancel ) )
            {
                result.m_Status = DecodeStatus::Cancelled;
                return false;
            }

            const int receiveResult = avcodec_receive_frame( ctx.m_CodecContext, ctx.m_Frame );
            if( receiveResult == AVERROR( EAGAIN ) || receiveResult == AVERROR_EOF )
            {
                return true;
            }

            if( receiveResult < 0 )
            {
                LogError( "[%s] avcodec_receive_frame failed for: %s", __FUNCTION__, filepath.c_str( ) );
                result.m_Error = "avcodec_receive_frame failed";
                result.m_Status = DecodeStatus::Failed;
                return false;
            }

            Frame frame;
            if( !ConvertFrameToRgba( ctx.m_Frame, ctx.m_SwsContext, width, height, frame.m_RgbaPixels ) )
            {
                result.m_Error = "ConvertFrameToRgba failed";
                result.m_Status = DecodeStatus::Failed;
                return false;
            }

            int64_t durationTicks = ctx.m_Frame->duration;
            if( durationTicks <= 0 )
            {
                durationTicks = packetDurationTicks;
            }

            frame.m_DelayMs = NormaliseDelayMs( durationTicks, stream->time_base );
            frame.m_StartTimeMs = cumulativeMs;
            cumulativeMs += frame.m_DelayMs;
            frame.m_EndTimeMs = cumulativeMs;

            runtime.m_Frames.push_back( std::move( frame ) );
            av_frame_unref( ctx.m_Frame );
        }
    };

    while( true )
    {
        if( ShouldCancel( shouldCancel ) )
        {
            result.m_Status = DecodeStatus::Cancelled;
            return result;
        }

        const int readResult = av_read_frame( ctx.m_FormatContext, ctx.m_Packet );
        if( readResult < 0 )
        {
            break;
        }

        if( ctx.m_Packet->stream_index != ctx.m_VideoStreamIndex )
        {
            av_packet_unref( ctx.m_Packet );
            continue;
        }

        int sendResult = avcodec_send_packet( ctx.m_CodecContext, ctx.m_Packet );
        while( sendResult == AVERROR( EAGAIN ) )
        {
            if( !appendDecodedFrames( ctx.m_Packet->duration ) )
            {
                av_packet_unref( ctx.m_Packet );
                return result;
            }
            sendResult = avcodec_send_packet( ctx.m_CodecContext, ctx.m_Packet );
        }

        if( sendResult < 0 )
        {
            LogError( "[%s] avcodec_send_packet failed for: %s", __FUNCTION__, filepath.c_str( ) );
            result.m_Error = "avcodec_send_packet failed";
            av_packet_unref( ctx.m_Packet );
            return result;
        }

        if( !appendDecodedFrames( ctx.m_Packet->duration ) )
        {
            av_packet_unref( ctx.m_Packet );
            return result;
        }

        av_packet_unref( ctx.m_Packet );
    }

    if( avcodec_send_packet( ctx.m_CodecContext, nullptr ) < 0 )
    {
        LogError( "[%s] GIF flush failed for: %s", __FUNCTION__, filepath.c_str( ) );
        result.m_Error = "GIF flush failed";
        return result;
    }

    if( !appendDecodedFrames( 0 ) )
    {
        return result;
    }

    if( runtime.m_Frames.empty( ) )
    {
        result.m_Error = "No frames decoded";
        return result;
    }

    runtime.m_TotalDurationMs = cumulativeMs;
    result.m_Status = DecodeStatus::Ready;
    result.m_Runtime = std::move( runtime );
    return result;
}
