#include "ui/VideoPlayer.h"
#include "log/Logger.h"

#include <algorithm>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace
{
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
}

ImageScraper::VideoPlayer::~VideoPlayer( )
{
    Close( );
}

ImageScraper::VideoPlayer::VideoPlayer( VideoPlayer&& other ) noexcept
{
    *this = std::move( other );
}

ImageScraper::VideoPlayer& ImageScraper::VideoPlayer::operator=( VideoPlayer&& other ) noexcept
{
    if( this == &other )
    {
        return *this;
    }

    Close( );

    m_FormatCtx   = other.m_FormatCtx;
    m_CodecCtx    = other.m_CodecCtx;
    m_SwsCtx      = other.m_SwsCtx;
    m_Frame       = other.m_Frame;
    m_Packet      = other.m_Packet;
    m_VideoStream = other.m_VideoStream;
    m_HasAudio    = other.m_HasAudio;
    m_Width       = other.m_Width;
    m_Height      = other.m_Height;
    m_Fps         = other.m_Fps;
    m_LastFramePtsSeconds = other.m_LastFramePtsSeconds;

    other.m_FormatCtx   = nullptr;
    other.m_CodecCtx    = nullptr;
    other.m_SwsCtx      = nullptr;
    other.m_Frame       = nullptr;
    other.m_Packet      = nullptr;
    other.m_VideoStream = -1;
    other.m_HasAudio    = false;
    other.m_Width       = 0;
    other.m_Height      = 0;
    other.m_Fps         = 30.0;
    other.m_LastFramePtsSeconds = -1.0;

    return *this;
}

bool ImageScraper::VideoPlayer::Open( const std::string& filepath )
{
    Close( );

    if( avformat_open_input( &m_FormatCtx, filepath.c_str( ), nullptr, nullptr ) < 0 )
    {
        LogError( "[%s] avformat_open_input failed for: %s", __FUNCTION__, filepath.c_str( ) );
        return false;
    }

    if( avformat_find_stream_info( m_FormatCtx, nullptr ) < 0 )
    {
        LogError( "[%s] avformat_find_stream_info failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Close( );
        return false;
    }

    m_VideoStream = -1;
    m_HasAudio    = false;
    for( unsigned int i = 0; i < m_FormatCtx->nb_streams; ++i )
    {
        const AVMediaType streamType = m_FormatCtx->streams[ i ]->codecpar->codec_type;
        if( streamType == AVMEDIA_TYPE_VIDEO && m_VideoStream < 0 )
        {
            m_VideoStream = static_cast<int>( i );
        }
        else if( streamType == AVMEDIA_TYPE_AUDIO )
        {
            m_HasAudio = true;
        }
    }

    if( m_VideoStream < 0 )
    {
        LogError( "[%s] No video stream found in: %s", __FUNCTION__, filepath.c_str( ) );
        Close( );
        return false;
    }

    AVCodecParameters* codecPar = m_FormatCtx->streams[ m_VideoStream ]->codecpar;
    const AVCodec* codec = avcodec_find_decoder( codecPar->codec_id );
    if( !codec )
    {
        LogError( "[%s] Unsupported codec in: %s", __FUNCTION__, filepath.c_str( ) );
        Close( );
        return false;
    }

    m_CodecCtx = avcodec_alloc_context3( codec );
    if( !m_CodecCtx )
    {
        LogError( "[%s] avcodec_alloc_context3 failed", __FUNCTION__ );
        Close( );
        return false;
    }

    if( avcodec_parameters_to_context( m_CodecCtx, codecPar ) < 0 )
    {
        LogError( "[%s] avcodec_parameters_to_context failed", __FUNCTION__ );
        Close( );
        return false;
    }

    if( avcodec_open2( m_CodecCtx, codec, nullptr ) < 0 )
    {
        LogError( "[%s] avcodec_open2 failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Close( );
        return false;
    }

    m_Width  = m_CodecCtx->width;
    m_Height = m_CodecCtx->height;

    AVRational frameRate = av_guess_frame_rate( m_FormatCtx, m_FormatCtx->streams[ m_VideoStream ], nullptr );
    if( frameRate.num <= 0 || frameRate.den <= 0 )
    {
        frameRate = m_FormatCtx->streams[ m_VideoStream ]->avg_frame_rate;
    }
    if( frameRate.num <= 0 || frameRate.den <= 0 )
    {
        frameRate = m_FormatCtx->streams[ m_VideoStream ]->r_frame_rate;
    }

    m_Fps = 30.0;
    if( frameRate.num > 0 && frameRate.den > 0 )
    {
        const double fps = static_cast<double>( frameRate.num ) / static_cast<double>( frameRate.den );
        if( fps > 0.0 )
        {
            m_Fps = fps;
        }
    }

    const AVPixelFormat swscalePixelFormat = NormaliseSwscalePixelFormat( m_CodecCtx->pix_fmt );
    m_SwsCtx = sws_getContext(
        m_Width, m_Height, swscalePixelFormat,
        m_Width, m_Height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr );

    if( !m_SwsCtx )
    {
        LogError( "[%s] sws_getContext failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Close( );
        return false;
    }

    const int* swscaleCoefficients = sws_getCoefficients( SWS_CS_DEFAULT );
    const int sourceRange = GetSwscaleRangeFlag( m_CodecCtx->pix_fmt, m_CodecCtx->color_range );
    if( sws_setColorspaceDetails(
            m_SwsCtx,
            swscaleCoefficients, sourceRange,
            swscaleCoefficients, 1,
            0, 1 << 16, 1 << 16 ) < 0 )
    {
        WarningLog( "[%s] sws_setColorspaceDetails failed for: %s", __FUNCTION__, filepath.c_str( ) );
    }

    m_Frame  = av_frame_alloc( );
    m_Packet = av_packet_alloc( );

    if( !m_Frame || !m_Packet )
    {
        LogError( "[%s] Frame/packet alloc failed", __FUNCTION__ );
        Close( );
        return false;
    }

    return true;
}

bool ImageScraper::VideoPlayer::DecodeFirstFrameFile(
    const std::string& filepath,
    std::vector<uint8_t>& rgbaOut,
    int& width,
    int& height,
    bool* hasAudio )
{
    VideoPlayer player;
    if( !player.Open( filepath ) )
    {
        return false;
    }

    if( !player.DecodeNextFrame( rgbaOut ) )
    {
        return false;
    }

    width  = player.GetWidth( );
    height = player.GetHeight( );
    if( hasAudio )
    {
        *hasAudio = player.HasAudio( );
    }

    const size_t pixelBytes = static_cast<size_t>( width ) * static_cast<size_t>( height ) * 4;
    if( rgbaOut.size( ) > pixelBytes )
    {
        rgbaOut.resize( pixelBytes );
    }

    return true;
}

bool ImageScraper::VideoPlayer::DecodeNextFrame( std::vector<uint8_t>& rgbaOut )
{
    if( !m_FormatCtx )
    {
        return false;
    }

    while( true )
    {
        // Try to receive a decoded frame from the codec first
        const int recvRet = avcodec_receive_frame( m_CodecCtx, m_Frame );
        if( recvRet == 0 )
        {
            const int64_t bestPts = m_Frame->best_effort_timestamp;
            if( bestPts != AV_NOPTS_VALUE && m_VideoStream >= 0 && m_FormatCtx )
            {
                const AVRational tb = m_FormatCtx->streams[ m_VideoStream ]->time_base;
                m_LastFramePtsSeconds = static_cast<double>( bestPts ) * av_q2d( tb );
            }
            return ConvertFrame( rgbaOut );
        }

        if( recvRet != AVERROR( EAGAIN ) )
        {
            // EOF or error - no more frames
            return false;
        }

        // Need more packets - read and send to decoder
        bool sentPacket = false;
        while( !sentPacket )
        {
            const int readRet = av_read_frame( m_FormatCtx, m_Packet );
            if( readRet < 0 )
            {
                // EOF: flush the decoder
                avcodec_send_packet( m_CodecCtx, nullptr );
                sentPacket = true;
                break;
            }

            if( m_Packet->stream_index == m_VideoStream )
            {
                avcodec_send_packet( m_CodecCtx, m_Packet );
                sentPacket = true;
            }

            av_packet_unref( m_Packet );
        }
    }
}

double ImageScraper::VideoPlayer::GetDuration( ) const
{
    if( !m_FormatCtx || m_FormatCtx->duration == AV_NOPTS_VALUE )
    {
        return 0.0;
    }
    return static_cast<double>( m_FormatCtx->duration ) / static_cast<double>( AV_TIME_BASE );
}

void ImageScraper::VideoPlayer::SeekToStart( )
{
    if( !m_FormatCtx )
    {
        return;
    }

    av_seek_frame( m_FormatCtx, m_VideoStream, 0, AVSEEK_FLAG_BACKWARD );
    avcodec_flush_buffers( m_CodecCtx );
    m_LastFramePtsSeconds = -1.0;
}

bool ImageScraper::VideoPlayer::SeekToKeyframe( double targetSeconds )
{
    if( !m_FormatCtx || m_VideoStream < 0 )
    {
        return false;
    }

    const double duration = GetDuration( );
    if( duration > 0.0 )
    {
        targetSeconds = std::clamp( targetSeconds, 0.0, duration );
    }
    else if( targetSeconds < 0.0 )
    {
        targetSeconds = 0.0;
    }

    const AVRational tb = m_FormatCtx->streams[ m_VideoStream ]->time_base;
    const int64_t streamTs = av_rescale_q(
        static_cast<int64_t>( targetSeconds * AV_TIME_BASE ),
        AVRational{ 1, AV_TIME_BASE },
        tb );

    const int ret = av_seek_frame( m_FormatCtx, m_VideoStream, streamTs, AVSEEK_FLAG_BACKWARD );
    if( ret < 0 )
    {
        return false;
    }
    avcodec_flush_buffers( m_CodecCtx );
    m_LastFramePtsSeconds = -1.0;
    return true;
}

bool ImageScraper::VideoPlayer::SeekToTimeExact( double targetSeconds, std::vector<uint8_t>& rgbaOut )
{
    if( !SeekToKeyframe( targetSeconds ) )
    {
        return false;
    }

    // Decode forward until we land on or past the requested PTS.
    constexpr int k_MaxForwardFrames = 240;
    bool produced = false;
    for( int i = 0; i < k_MaxForwardFrames; ++i )
    {
        if( !DecodeNextFrame( rgbaOut ) )
        {
            return produced;
        }
        produced = true;
        if( m_LastFramePtsSeconds < 0.0 || m_LastFramePtsSeconds + 1e-3 >= targetSeconds )
        {
            return true;
        }
    }
    return produced;
}

void ImageScraper::VideoPlayer::Close( )
{
    if( m_Frame )  { av_frame_free( &m_Frame );   m_Frame  = nullptr; }
    if( m_Packet ) { av_packet_free( &m_Packet );  m_Packet = nullptr; }
    if( m_SwsCtx ) { sws_freeContext( m_SwsCtx );  m_SwsCtx = nullptr; }
    if( m_CodecCtx ) { avcodec_free_context( &m_CodecCtx ); m_CodecCtx = nullptr; }
    if( m_FormatCtx ) { avformat_close_input( &m_FormatCtx ); m_FormatCtx = nullptr; }

    m_VideoStream = -1;
    m_HasAudio    = false;
    m_Width       = 0;
    m_Height      = 0;
    m_Fps         = 30.0;
    m_LastFramePtsSeconds = -1.0;
}

bool ImageScraper::VideoPlayer::ConvertFrame( std::vector<uint8_t>& rgbaOut )
{
    const int bufSize    = m_Width * m_Height * 4;
    // sws_scale uses SIMD stores that can write up to 64 bytes past the last row.
    // Allocate a padded buffer so those stores land in owned memory.
    const int paddedSize = bufSize + 64;
    if( static_cast<int>( rgbaOut.size( ) ) < paddedSize )
    {
        rgbaOut.resize( static_cast<size_t>( paddedSize ) );
    }

    uint8_t*  dstData[ 4 ]     = { rgbaOut.data( ), nullptr, nullptr, nullptr };
    int       dstLinesize[ 4 ] = { m_Width * 4, 0, 0, 0 };

    sws_scale( m_SwsCtx,
               m_Frame->data, m_Frame->linesize,
               0, m_Height,
               dstData, dstLinesize );

    return true;
}
