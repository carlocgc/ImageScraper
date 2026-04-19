#include "ui/VideoPlayer.h"
#include "log/Logger.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

ImageScraper::VideoPlayer::~VideoPlayer( )
{
    Close( );
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

    // Find the first video stream
    m_VideoStream = -1;
    for( unsigned int i = 0; i < m_FormatCtx->nb_streams; ++i )
    {
        if( m_FormatCtx->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO )
        {
            m_VideoStream = static_cast<int>( i );
            break;
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

    const AVRational tb = m_FormatCtx->streams[ m_VideoStream ]->avg_frame_rate;
    m_Fps = ( tb.den > 0 ) ? static_cast<double>( tb.num ) / tb.den : 30.0;

    m_SwsCtx = sws_getContext(
        m_Width, m_Height, m_CodecCtx->pix_fmt,
        m_Width, m_Height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr );

    if( !m_SwsCtx )
    {
        LogError( "[%s] sws_getContext failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Close( );
        return false;
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
}

void ImageScraper::VideoPlayer::Close( )
{
    if( m_Frame )  { av_frame_free( &m_Frame );   m_Frame  = nullptr; }
    if( m_Packet ) { av_packet_free( &m_Packet );  m_Packet = nullptr; }
    if( m_SwsCtx ) { sws_freeContext( m_SwsCtx );  m_SwsCtx = nullptr; }
    if( m_CodecCtx ) { avcodec_free_context( &m_CodecCtx ); m_CodecCtx = nullptr; }
    if( m_FormatCtx ) { avformat_close_input( &m_FormatCtx ); m_FormatCtx = nullptr; }

    m_VideoStream = -1;
    m_Width       = 0;
    m_Height      = 0;
    m_Fps         = 30.0;
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
