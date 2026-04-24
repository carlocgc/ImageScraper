#include "requests/mastodon/MastodonRequestHelpers.h"

#include "utils/DownloadUtils.h"
#include "utils/StringUtils.h"

#include <algorithm>

namespace ImageScraper::Mastodon::RequestHelpers
{
    bool HasQueryParam( const std::vector<QueryParam>& params, const std::string& key )
    {
        return std::any_of( params.begin( ), params.end( ), [ &key ]( const QueryParam& param )
            {
                return param.m_Key == key;
            } );
    }

    std::string CreateEncodedQueryParamString( const std::vector<QueryParam>& params )
    {
        std::string paramString{ };
        if( !params.empty( ) )
        {
            paramString += '?';
        }

        int paramCount = 0;
        for( const QueryParam& param : params )
        {
            if( paramCount++ > 0 )
            {
                paramString += '&';
            }

            paramString += StringUtils::UrlEncode( param.m_Key );
            paramString += '=';
            paramString += StringUtils::UrlEncode( param.m_Value );
        }

        return paramString;
    }

    RequestResult MakeBadRequest( const std::string& message )
    {
        RequestResult result{ };
        result.m_Error.m_ErrorCode = ResponseErrorCode::BadRequest;
        result.m_Error.m_ErrorString = message;
        return result;
    }

    RequestResult CreateResultFromResponse( const HttpResponse& response )
    {
        RequestResult result{ };
        if( !response.m_Success )
        {
            result.m_Error.m_ErrorCode = ResponseErrorCodefromInt( response.m_StatusCode );
            result.m_Error.m_ErrorString = response.m_Error;
            return result;
        }

        result.m_Response = response.m_Body;
        if( DownloadHelpers::IsMastodonResponseError( result ) )
        {
            return result;
        }

        result.m_Success = true;
        return result;
    }

    void ApplyBearerAuthHeader( HttpRequest& request, const std::string& accessToken )
    {
        if( !accessToken.empty( ) )
        {
            request.m_Headers = { "Authorization: Bearer " + accessToken };
        }
    }
}
