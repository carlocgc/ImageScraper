#pragma once
#include <string>
#include <vector>

namespace ImageScraper
{
    struct DownloadOptions
    {
        std::string m_Url;
        std::string m_CaBundle;
        std::vector<char>* m_BufferPtr;
    };

    enum class DownloadErrorCode : uint16_t
    {
        None,
        Unknown,
        InvalidOptions,
    };

    struct DownloadError
    {
        DownloadErrorCode m_ErrorCode;
    };

    struct DownloadResult
    {
        bool m_Success{ false };
        DownloadError m_Error{ DownloadErrorCode::None };
    };
}
