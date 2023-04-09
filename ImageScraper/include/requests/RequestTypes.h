#pragma once
#include <string>

namespace ImageScraper
{
    struct RequestOptions
    {
        std::string m_Url;
        std::string m_CaBundle;
        std::string m_UserAgent;
    };

    enum class ResponseErrorCode : uint16_t
    {
        None = 0,
        Unknown = 1,
        UrlNotFound = 2,
        TooManyRequests = 3,
        InvalidDirectory = 4,
    };

    struct ResponseError
    {
        ResponseErrorCode m_ErrorCode;
    };

    struct RequestResult
    {
        bool m_Success{ false };
        std::string m_Response;
        ResponseError m_Error{ ResponseErrorCode::None };
    };
}