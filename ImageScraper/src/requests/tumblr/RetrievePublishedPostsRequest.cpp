#include "requests/tumblr/RetrievePublishedPostsRequest.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "curlpp/Options.hpp"

const std::string ImageScraper::Tumblr::RetrievePublishedPostsRequest::s_BaseUrl = "https://api.tumblr.com/v2/blog/";

ImageScraper::RequestResult ImageScraper::Tumblr::RetrievePublishedPostsRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] Tumblr::RetrievePublishedPostsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };
        std::ostringstream response;
        const std::string url = s_BaseUrl + options.m_UrlExt + DownloadHelpers::CreateQueryParamString( options.m_QueryParams );

        request.setOpt( new curlpp::options::Url( url ) );
        request.setOpt( new curlpp::options::FollowLocation( true ) );
        request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );
        request.setOpt( new curlpp::options::WriteStream( &response ) );
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );

        DebugLog( "[%s] Tumblr::RetrievePublishedPostsRequest, URL: %s", __FUNCTION__, url.c_str( ) );

        // blocks
        request.perform( );

        result.m_Response = response.str( );
    }
    catch( curlpp::RuntimeError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] Tumblr::RetrievePublishedPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }
    catch( curlpp::LogicError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] Tumblr::RetrievePublishedPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    if( DownloadHelpers::IsResponseError( result ) )
    {
        DebugLog( "[%s] Tumblr::RetrievePublishedPostsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] Tumblr::RetrievePublishedPostsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
