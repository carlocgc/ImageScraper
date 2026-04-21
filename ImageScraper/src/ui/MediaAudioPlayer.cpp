#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>

#include "ui/MediaAudioPlayer.h"
#include "log/Logger.h"

#include <algorithm>
#include <limits>
#include <utility>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

ImageScraper::MediaAudioPlayer::~MediaAudioPlayer( )
{
    Close( );
}

ImageScraper::MediaAudioPlayer::MediaAudioPlayer( MediaAudioPlayer&& other ) noexcept
{
    *this = std::move( other );
}

ImageScraper::MediaAudioPlayer& ImageScraper::MediaAudioPlayer::operator=( MediaAudioPlayer&& other ) noexcept
{
    if( this == &other )
    {
        return *this;
    }

    Close( );

    m_FilePath            = std::move( other.m_FilePath );
    m_AudioData           = std::move( other.m_AudioData );
    m_Duration            = other.m_Duration;
    m_OutputSampleRate    = other.m_OutputSampleRate;
    m_OutputChannels      = other.m_OutputChannels;
    m_OutputBitsPerSample = other.m_OutputBitsPerSample;
    m_BlockAlign          = other.m_BlockAlign;
    m_XAudio              = other.m_XAudio;
    m_MasteringVoice      = other.m_MasteringVoice;
    m_SourceVoice         = other.m_SourceVoice;

    other.m_Duration            = 0.0;
    other.m_OutputSampleRate    = 48000;
    other.m_OutputChannels      = 2;
    other.m_OutputBitsPerSample = 16;
    other.m_BlockAlign          = 4;
    other.m_XAudio              = nullptr;
    other.m_MasteringVoice      = nullptr;
    other.m_SourceVoice         = nullptr;

    return *this;
}

bool ImageScraper::MediaAudioPlayer::Open( const std::string& filepath )
{
    Close( );

    AVFormatContext* formatCtx = nullptr;
    AVCodecContext*  codecCtx  = nullptr;
    SwrContext*      swrCtx    = nullptr;
    AVFrame*         frame     = nullptr;
    AVPacket*        packet    = nullptr;
    AVChannelLayout  outLayout{ };
    bool             outLayoutInitialised = false;
    int              audioStream = -1;
    int              inputSampleRate = 48000;

    auto Cleanup = [ & ]( )
    {
        if( packet != nullptr )
        {
            av_packet_free( &packet );
        }
        if( frame != nullptr )
        {
            av_frame_free( &frame );
        }
        if( swrCtx != nullptr )
        {
            swr_free( &swrCtx );
        }
        if( codecCtx != nullptr )
        {
            avcodec_free_context( &codecCtx );
        }
        if( formatCtx != nullptr )
        {
            avformat_close_input( &formatCtx );
        }
        if( outLayoutInitialised )
        {
            av_channel_layout_uninit( &outLayout );
        }
    };

    auto AppendFrame = [ & ]( ) -> bool
    {
        const int64_t outputSamples64 = av_rescale_rnd(
            swr_get_delay( swrCtx, inputSampleRate ) + frame->nb_samples,
            m_OutputSampleRate,
            inputSampleRate,
            AV_ROUND_UP );
        if( outputSamples64 <= 0 || outputSamples64 > std::numeric_limits<int>::max( ) )
        {
            LogError( "[%s] Invalid resampled audio frame size for: %s", __FUNCTION__, filepath.c_str( ) );
            return false;
        }

        const int outputSamples = static_cast<int>( outputSamples64 );

        const int outputBytes = av_samples_get_buffer_size(
            nullptr,
            m_OutputChannels,
            outputSamples,
            AV_SAMPLE_FMT_S16,
            1 );

        if( outputBytes <= 0 )
        {
            LogError( "[%s] av_samples_get_buffer_size failed for: %s", __FUNCTION__, filepath.c_str( ) );
            return false;
        }

        std::vector<uint8_t> converted( static_cast<size_t>( outputBytes ) );
        uint8_t* outputData[ 1 ] = { converted.data( ) };

        const int samplesWritten = swr_convert(
            swrCtx,
            outputData,
            outputSamples,
            const_cast<const uint8_t**>( frame->extended_data ),
            frame->nb_samples );

        if( samplesWritten < 0 )
        {
            LogError( "[%s] swr_convert failed for: %s", __FUNCTION__, filepath.c_str( ) );
            return false;
        }

        const int bytesWritten = samplesWritten * static_cast<int>( m_BlockAlign );
        if( bytesWritten <= 0 )
        {
            return true;
        }

        converted.resize( static_cast<size_t>( bytesWritten ) );
        m_AudioData.insert( m_AudioData.end( ), converted.begin( ), converted.end( ) );
        return true;
    };

    if( avformat_open_input( &formatCtx, filepath.c_str( ), nullptr, nullptr ) < 0 )
    {
        LogError( "[%s] avformat_open_input failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Cleanup( );
        return false;
    }

    if( avformat_find_stream_info( formatCtx, nullptr ) < 0 )
    {
        LogError( "[%s] avformat_find_stream_info failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Cleanup( );
        return false;
    }

    for( unsigned int i = 0; i < formatCtx->nb_streams; ++i )
    {
        if( formatCtx->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO )
        {
            audioStream = static_cast<int>( i );
            break;
        }
    }

    if( audioStream < 0 )
    {
        Cleanup( );
        return false;
    }

    AVCodecParameters* codecPar = formatCtx->streams[ audioStream ]->codecpar;
    const AVCodec* codec = avcodec_find_decoder( codecPar->codec_id );
    if( codec == nullptr )
    {
        LogError( "[%s] Unsupported audio codec in: %s", __FUNCTION__, filepath.c_str( ) );
        Cleanup( );
        return false;
    }

    codecCtx = avcodec_alloc_context3( codec );
    if( codecCtx == nullptr )
    {
        LogError( "[%s] avcodec_alloc_context3 failed", __FUNCTION__ );
        Cleanup( );
        return false;
    }

    if( avcodec_parameters_to_context( codecCtx, codecPar ) < 0 )
    {
        LogError( "[%s] avcodec_parameters_to_context failed", __FUNCTION__ );
        Cleanup( );
        return false;
    }

    if( avcodec_open2( codecCtx, codec, nullptr ) < 0 )
    {
        LogError( "[%s] avcodec_open2 failed for audio in: %s", __FUNCTION__, filepath.c_str( ) );
        Cleanup( );
        return false;
    }

    m_OutputSampleRate = codecCtx->sample_rate > 0 ? codecCtx->sample_rate : 48000;
    m_OutputChannels   = 2;
    m_BlockAlign       = static_cast<uint32_t>( m_OutputChannels * ( m_OutputBitsPerSample / 8 ) );
    inputSampleRate    = codecCtx->sample_rate > 0 ? codecCtx->sample_rate : m_OutputSampleRate;

    av_channel_layout_default( &outLayout, m_OutputChannels );
    outLayoutInitialised = true;

    if( swr_alloc_set_opts2(
            &swrCtx,
            &outLayout,
            AV_SAMPLE_FMT_S16,
            m_OutputSampleRate,
            &codecCtx->ch_layout,
            codecCtx->sample_fmt,
            inputSampleRate,
            0,
            nullptr ) < 0 )
    {
        LogError( "[%s] swr_alloc_set_opts2 failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Cleanup( );
        return false;
    }

    if( swr_init( swrCtx ) < 0 )
    {
        LogError( "[%s] swr_init failed for: %s", __FUNCTION__, filepath.c_str( ) );
        Cleanup( );
        return false;
    }

    frame  = av_frame_alloc( );
    packet = av_packet_alloc( );
    if( frame == nullptr || packet == nullptr )
    {
        LogError( "[%s] Frame/packet alloc failed", __FUNCTION__ );
        Cleanup( );
        return false;
    }

    while( av_read_frame( formatCtx, packet ) >= 0 )
    {
        if( packet->stream_index != audioStream )
        {
            av_packet_unref( packet );
            continue;
        }

        const int sendRet = avcodec_send_packet( codecCtx, packet );
        av_packet_unref( packet );

        if( sendRet < 0 )
        {
            LogError( "[%s] avcodec_send_packet failed for audio in: %s", __FUNCTION__, filepath.c_str( ) );
            Cleanup( );
            return false;
        }

        while( true )
        {
            const int receiveRet = avcodec_receive_frame( codecCtx, frame );
            if( receiveRet == AVERROR( EAGAIN ) || receiveRet == AVERROR_EOF )
            {
                break;
            }
            if( receiveRet < 0 )
            {
                LogError( "[%s] avcodec_receive_frame failed for audio in: %s", __FUNCTION__, filepath.c_str( ) );
                Cleanup( );
                return false;
            }

            if( !AppendFrame( ) )
            {
                Cleanup( );
                return false;
            }
        }
    }

    avcodec_send_packet( codecCtx, nullptr );
    while( true )
    {
        const int receiveRet = avcodec_receive_frame( codecCtx, frame );
        if( receiveRet == AVERROR( EAGAIN ) || receiveRet == AVERROR_EOF )
        {
            break;
        }
        if( receiveRet < 0 )
        {
            LogError( "[%s] Final audio flush failed for: %s", __FUNCTION__, filepath.c_str( ) );
            Cleanup( );
            return false;
        }

        if( !AppendFrame( ) )
        {
            Cleanup( );
            return false;
        }
    }

    Cleanup( );

    if( m_AudioData.empty( ) )
    {
        Close( );
        return false;
    }

    m_FilePath = filepath;
    m_Duration = static_cast<double>( m_AudioData.size( ) ) /
                 static_cast<double>( m_BlockAlign * static_cast<uint32_t>( m_OutputSampleRate ) );
    return true;
}

bool ImageScraper::MediaAudioPlayer::StartFrom( double seconds, bool muted )
{
    if( m_AudioData.empty( ) )
    {
        return false;
    }

    if( !InitXAudio( ) )
    {
        return false;
    }

    DestroySourceVoice( );

    WAVEFORMATEX waveFormat{ };
    waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
    waveFormat.nChannels       = static_cast<WORD>( m_OutputChannels );
    waveFormat.nSamplesPerSec  = static_cast<DWORD>( m_OutputSampleRate );
    waveFormat.wBitsPerSample  = static_cast<WORD>( m_OutputBitsPerSample );
    waveFormat.nBlockAlign     = static_cast<WORD>( m_BlockAlign );
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize          = 0;

    HRESULT hr = m_XAudio->CreateSourceVoice( &m_SourceVoice, &waveFormat );
    if( FAILED( hr ) )
    {
        LogError( "[%s] CreateSourceVoice failed: 0x%08X", __FUNCTION__, static_cast<unsigned int>( hr ) );
        m_SourceVoice = nullptr;
        return false;
    }

    const double clampedSeconds = std::clamp( seconds, 0.0, m_Duration );
    uint64_t startByte = static_cast<uint64_t>( clampedSeconds * static_cast<double>( m_OutputSampleRate ) ) *
                         static_cast<uint64_t>( m_BlockAlign );
    if( startByte >= m_AudioData.size( ) )
    {
        DestroySourceVoice( );
        return false;
    }

    startByte -= startByte % m_BlockAlign;

    const size_t remainingBytes = m_AudioData.size( ) - static_cast<size_t>( startByte );
    if( remainingBytes > std::numeric_limits<UINT32>::max( ) )
    {
        LogError( "[%s] Audio buffer too large for XAudio2 submission: %s", __FUNCTION__, m_FilePath.c_str( ) );
        DestroySourceVoice( );
        return false;
    }

    XAUDIO2_BUFFER buffer{ };
    buffer.pAudioData = reinterpret_cast<const BYTE*>( m_AudioData.data( ) + startByte );
    buffer.AudioBytes = static_cast<UINT32>( remainingBytes );
    buffer.Flags      = XAUDIO2_END_OF_STREAM;

    hr = m_SourceVoice->SubmitSourceBuffer( &buffer );
    if( FAILED( hr ) )
    {
        LogError( "[%s] SubmitSourceBuffer failed: 0x%08X", __FUNCTION__, static_cast<unsigned int>( hr ) );
        DestroySourceVoice( );
        return false;
    }

    m_SourceVoice->SetVolume( muted ? 0.0f : 1.0f );
    hr = m_SourceVoice->Start( 0 );
    if( FAILED( hr ) )
    {
        LogError( "[%s] SourceVoice Start failed: 0x%08X", __FUNCTION__, static_cast<unsigned int>( hr ) );
        DestroySourceVoice( );
        return false;
    }

    return true;
}

void ImageScraper::MediaAudioPlayer::Stop( )
{
    DestroySourceVoice( );
}

void ImageScraper::MediaAudioPlayer::SetMuted( bool muted )
{
    if( m_SourceVoice == nullptr )
    {
        return;
    }

    m_SourceVoice->SetVolume( muted ? 0.0f : 1.0f );
}

void ImageScraper::MediaAudioPlayer::Close( )
{
    DestroyXAudio( );
    m_AudioData.clear( );
    m_FilePath.clear( );
    m_Duration            = 0.0;
    m_OutputSampleRate    = 48000;
    m_OutputChannels      = 2;
    m_OutputBitsPerSample = 16;
    m_BlockAlign          = 4;
}

bool ImageScraper::MediaAudioPlayer::InitXAudio( )
{
    if( m_XAudio != nullptr && m_MasteringVoice != nullptr )
    {
        return true;
    }

    HRESULT hr = XAudio2Create( &m_XAudio, 0, XAUDIO2_DEFAULT_PROCESSOR );
    if( FAILED( hr ) )
    {
        LogError( "[%s] XAudio2Create failed: 0x%08X", __FUNCTION__, static_cast<unsigned int>( hr ) );
        m_XAudio = nullptr;
        return false;
    }

    hr = m_XAudio->CreateMasteringVoice( &m_MasteringVoice );
    if( FAILED( hr ) )
    {
        LogError( "[%s] CreateMasteringVoice failed: 0x%08X", __FUNCTION__, static_cast<unsigned int>( hr ) );
        DestroyXAudio( );
        return false;
    }

    return true;
}

void ImageScraper::MediaAudioPlayer::DestroySourceVoice( )
{
    if( m_SourceVoice != nullptr )
    {
        m_SourceVoice->Stop( 0 );
        m_SourceVoice->FlushSourceBuffers( );
        m_SourceVoice->DestroyVoice( );
        m_SourceVoice = nullptr;
    }
}

void ImageScraper::MediaAudioPlayer::DestroyXAudio( )
{
    DestroySourceVoice( );

    if( m_MasteringVoice != nullptr )
    {
        m_MasteringVoice->DestroyVoice( );
        m_MasteringVoice = nullptr;
    }

    if( m_XAudio != nullptr )
    {
        m_XAudio->Release( );
        m_XAudio = nullptr;
    }
}
