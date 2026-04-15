#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward-declare FFmpeg structs to keep C headers out of this header
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace ImageScraper
{
    // Wraps FFmpeg video decode for a single local file.
    // Open() finds the first video stream and prepares the decoder.
    // DecodeNextFrame() steps one frame at a time; returns false on EOF.
    // SeekToStart() rewinds for looping.
    // All FFmpeg memory is owned internally and released by Close() / destructor.
    class VideoPlayer
    {
    public:
        ~VideoPlayer( );

        bool Open( const std::string& filepath );

        // Decodes the next video frame into rgbaOut (width * height * 4 bytes, RGBA).
        // Returns true if a frame was produced, false on EOF or unrecoverable error.
        bool DecodeNextFrame( std::vector<uint8_t>& rgbaOut );

        void SeekToStart( );
        void Close( );

        bool   IsOpen( )   const { return m_FormatCtx != nullptr; }
        int    GetWidth( )  const { return m_Width; }
        int    GetHeight( ) const { return m_Height; }
        double GetFPS( )    const { return m_Fps; }

    private:
        bool ConvertFrame( std::vector<uint8_t>& rgbaOut );

        AVFormatContext* m_FormatCtx  { nullptr };
        AVCodecContext*  m_CodecCtx   { nullptr };
        SwsContext*      m_SwsCtx     { nullptr };
        AVFrame*         m_Frame      { nullptr };
        AVPacket*        m_Packet     { nullptr };
        int              m_VideoStream{ -1 };
        int              m_Width      { 0 };
        int              m_Height     { 0 };
        double           m_Fps        { 30.0 };
    };
}
