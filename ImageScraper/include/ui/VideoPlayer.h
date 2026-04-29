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
    // Wraps FFmpeg decode for a single local file.
    // Open() finds the first video stream and prepares the decoder.
    // DecodeNextFrame() steps one frame at a time; returns false on EOF.
    // SeekToStart() rewinds for looping.
    // All FFmpeg memory is owned internally and released by Close() / destructor.
    class VideoPlayer
    {
    public:
        VideoPlayer( ) = default;
        ~VideoPlayer( );
        VideoPlayer( const VideoPlayer& ) = delete;
        VideoPlayer& operator=( const VideoPlayer& ) = delete;
        VideoPlayer( VideoPlayer&& other ) noexcept;
        VideoPlayer& operator=( VideoPlayer&& other ) noexcept;

        bool Open( const std::string& filepath );

        // Opens a local file, decodes the first frame into RGBA, and closes the decoder.
        // Works for both videos and single-frame image formats supported by FFmpeg.
        static bool DecodeFirstFrameFile(
            const std::string& filepath,
            std::vector<uint8_t>& rgbaOut,
            int& width,
            int& height,
            bool* hasAudio = nullptr );

        // Decodes the next video frame into rgbaOut (width * height * 4 bytes, RGBA).
        // Returns true if a frame was produced, false on EOF or unrecoverable error.
        bool DecodeNextFrame( std::vector<uint8_t>& rgbaOut );

        void SeekToStart( );

        // Seeks to the nearest keyframe at or before targetSeconds. Cheap - no decode
        // is performed. Subsequent DecodeNextFrame() calls return frames starting at
        // that keyframe. Returns true if the seek was issued.
        bool SeekToKeyframe( double targetSeconds );

        // Seeks to the keyframe at-or-before targetSeconds, then decodes forward
        // until a frame with PTS >= targetSeconds is produced (or a small frame budget
        // is exhausted). The final frame is written to rgbaOut. Used on drag-release
        // for exact positioning.
        bool SeekToTimeExact( double targetSeconds, std::vector<uint8_t>& rgbaOut );

        // Last successfully produced frame's PTS in seconds, or -1 if none.
        double GetLastFramePts( ) const { return m_LastFramePtsSeconds; }

        void Close( );

        bool   IsOpen( )      const { return m_FormatCtx != nullptr; }
        bool   HasAudio( )    const { return m_HasAudio; }
        int    GetWidth( )    const { return m_Width; }
        int    GetHeight( )   const { return m_Height; }
        double GetFPS( )      const { return m_Fps; }
        double GetDuration( ) const; // total duration in seconds; 0 if unknown

    private:
        bool ConvertFrame( std::vector<uint8_t>& rgbaOut );

        AVFormatContext* m_FormatCtx  { nullptr };
        AVCodecContext*  m_CodecCtx   { nullptr };
        SwsContext*      m_SwsCtx     { nullptr };
        AVFrame*         m_Frame      { nullptr };
        AVPacket*        m_Packet     { nullptr };
        int              m_VideoStream{ -1 };
        bool             m_HasAudio   { false };
        int              m_Width      { 0 };
        int              m_Height     { 0 };
        double           m_Fps        { 30.0 };
        double           m_LastFramePtsSeconds{ -1.0 };
    };
}
