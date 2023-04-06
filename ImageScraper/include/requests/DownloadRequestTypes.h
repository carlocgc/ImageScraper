#pragma once
#include <string>
#include <vector>

struct DownloadOptions
{
    std::string m_Url;
    std::string m_CaBundle;
    std::vector<char>* m_BufferPtr;
};

enum class DownloadErrorCode : uint16_t
{
    None = 0,
    Unknown = 1,
    InvalidOptions = 2,
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