#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

namespace ImageScraper
{
    class MediaAudioPlayer
    {
    public:
        MediaAudioPlayer( ) = default;
        ~MediaAudioPlayer( );
        MediaAudioPlayer( const MediaAudioPlayer& ) = delete;
        MediaAudioPlayer& operator=( const MediaAudioPlayer& ) = delete;
        MediaAudioPlayer( MediaAudioPlayer&& other ) noexcept;
        MediaAudioPlayer& operator=( MediaAudioPlayer&& other ) noexcept;

        bool Open( const std::string& filepath );
        bool StartFrom( double seconds, bool muted );
        void Stop( );
        void SetMuted( bool muted );
        void Close( );

        bool HasAudio( )  const { return !m_AudioData.empty( ); }
        bool IsPlaying( ) const { return m_SourceVoice != nullptr; }
        double GetDuration( ) const { return m_Duration; }
        const std::string& GetFilePath( ) const { return m_FilePath; }

    private:
        bool InitXAudio( );
        void DestroySourceVoice( );
        void DestroyXAudio( );

        std::string          m_FilePath{ };
        std::vector<uint8_t> m_AudioData{ };
        double               m_Duration{ 0.0 };
        int                  m_OutputSampleRate{ 48000 };
        int                  m_OutputChannels{ 2 };
        int                  m_OutputBitsPerSample{ 16 };
        uint32_t             m_BlockAlign{ 4 };

        IXAudio2*               m_XAudio{ nullptr };
        IXAudio2MasteringVoice* m_MasteringVoice{ nullptr };
        IXAudio2SourceVoice*    m_SourceVoice{ nullptr };
    };
}
