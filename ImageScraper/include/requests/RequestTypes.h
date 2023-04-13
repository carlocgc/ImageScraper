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
        None,
        Unknown,
        UrlNotFound,
        TooManyRequests,
        InvalidDirectory,
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