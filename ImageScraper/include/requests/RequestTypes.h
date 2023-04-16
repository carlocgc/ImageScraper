#pragma once
#include <string>
#include <vector>

namespace ImageScraper
{
    struct QueryParam
    {
        std::string m_Key{ };
        std::string m_Value{ };
    };

    struct RequestOptions
    {
        std::string m_UrlExt{ };
        std::vector<QueryParam> m_QueryParams{ };
        std::string m_CaBundle{ };
        std::string m_UserAgent{ };
        std::string m_ClientId{ };
        std::string m_ClientSecret{ };
        std::string m_AccessToken{ };
    };

    enum class ResponseErrorCode : uint16_t
    {
        None,
        InvalidOptions,
        NotFound,
        TooManyRequests,
        Unknown,
    };

    struct ResponseError
    {
        ResponseErrorCode m_ErrorCode{ ResponseErrorCode::None };
        std::string m_ErrorString{ };
    };

    inline std::string ResponseErrorCodeToString( ResponseErrorCode error )
    {
        switch( error )
        {
        case ImageScraper::ResponseErrorCode::None:
            return "None";
            break;
        case ImageScraper::ResponseErrorCode::InvalidOptions:
            return "Invalid Options";
            break;
        case ImageScraper::ResponseErrorCode::NotFound:
            return "Not Found";
            break;
        case ImageScraper::ResponseErrorCode::TooManyRequests:
            return "Too Many Requests";
            break;
        case ImageScraper::ResponseErrorCode::Unknown:
            return "Unknown";
            break;
        default:
            return "";
            break;
        }
    }

    struct RequestResult
    {
        bool m_Success{ false };
        std::string m_Response{ };
        ResponseError m_Error{ };

        void SetError( ResponseErrorCode error )
        {
            m_Error.m_ErrorCode = error;
            m_Error.m_ErrorString = ResponseErrorCodeToString( error );
        }
    };
}