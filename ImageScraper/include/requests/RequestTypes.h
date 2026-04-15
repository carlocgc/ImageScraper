#pragma once
#include <string>
#include <vector>
#include <map>

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
        OK = 200,
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        InternalServerError = 500
    };

    struct ResponseError
    {
        ResponseErrorCode m_ErrorCode{ ResponseErrorCode::OK };
        std::string m_ErrorString{ };
    };

    inline ResponseErrorCode ResponseErrorCodefromInt( int errorCode )
    {
        static std::map<int, ResponseErrorCode> errorMap{
            { 200, ResponseErrorCode::OK },
            { 400, ResponseErrorCode::BadRequest },
            { 401, ResponseErrorCode::Unauthorized },
            { 403, ResponseErrorCode::Forbidden },
            { 404, ResponseErrorCode::NotFound },
            { 500, ResponseErrorCode::InternalServerError }
        };

        auto it = errorMap.find( errorCode );
        if( it != errorMap.end( ) )
        {
            return it->second;
        }

        return ResponseErrorCode::InternalServerError;
    }

    inline std::string ResponseErrorCodeToString( ResponseErrorCode errorCode )
    {
        static std::map<ResponseErrorCode, std::string> errorMap{
            { ResponseErrorCode::OK, "OK" },
            { ResponseErrorCode::BadRequest, "Bad Request" },
            { ResponseErrorCode::Unauthorized, "Unauthorized" },
            { ResponseErrorCode::Forbidden, "Forbidden" },
            { ResponseErrorCode::NotFound, "Not Found" },
            { ResponseErrorCode::InternalServerError, "Internal Server Error" }
        };

        auto it = errorMap.find( errorCode );
        if( it != errorMap.end( ) )
        {
            return it->second;
        }

        return "Unknown error";
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