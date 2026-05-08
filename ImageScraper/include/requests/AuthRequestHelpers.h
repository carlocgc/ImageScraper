#pragma once
#include "requests/RequestTypes.h"
#include "network/HttpClientTypes.h"
#include <string>

namespace ImageScraper::AuthRequestHelpers
{
    // Returns false and sets result error if ClientId or ClientSecret is empty.
    inline bool ValidateOAuthCredentials( const RequestOptions& options, RequestResult& result )
    {
        if( options.m_ClientId.empty( ) )
        {
            result.SetError( ResponseErrorCode::InternalServerError );
            return false;
        }
        if( options.m_ClientSecret.empty( ) )
        {
            result.SetError( ResponseErrorCode::InternalServerError );
            return false;
        }
        return true;
    }

    // Appends each QueryParam from options to base as "&key=value" pairs.
    inline std::string BuildQueryString( const std::string& base, const RequestOptions& options )
    {
        std::string result = base;
        for( const auto& param : options.m_QueryParams )
        {
            result += "&" + param.m_Key + "=" + param.m_Value;
        }
        return result;
    }

    // Returns true and maps the HTTP error into result if the response was not successful.
    inline bool HandleHttpError( const HttpResponse& response, RequestResult& result )
    {
        if( !response.m_Success )
        {
            result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
            result.m_Error.m_ErrorString = response.m_Error;
            return true;
        }
        return false;
    }
}
