#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ImageScraper
{
    namespace GifPlaybackCache
    {
        enum class DecodeStatus
        {
            Ready,
            Cancelled,
            Failed,
        };

        struct Frame
        {
            std::vector<uint8_t> m_RgbaPixels{ };
            int                  m_DelayMs{ 100 };
            int64_t              m_StartTimeMs{ 0 };
            int64_t              m_EndTimeMs{ 0 };
        };

        struct Runtime
        {
            int                m_Width{ 0 };
            int                m_Height{ 0 };
            int64_t            m_TotalDurationMs{ 0 };
            std::vector<Frame> m_Frames{ };

            bool    HasMultipleFrames( ) const;
            int64_t GetTimeMsForNormalized( float normalized ) const;
            size_t  GetFrameIndexForTimeMs( int64_t targetTimeMs ) const;
        };

        struct DecodeResult
        {
            DecodeStatus m_Status{ DecodeStatus::Failed };
            Runtime      m_Runtime{ };
            std::string  m_Error{ };

            bool IsReady( ) const { return m_Status == DecodeStatus::Ready; }
        };

        using CancelCheck = std::function<bool( )>;

        DecodeResult DecodeGifFile(
            const std::string& filepath,
            const CancelCheck& shouldCancel = CancelCheck{ } );
    }
}
