#include "requests/fourchan/GetThreadsRequest.h"
#include "log/Logger.h"
#include "curlpp/Options.hpp"
#include "utils/DownloadUtils.h"

const std::string ImageScraper::FourChan::GetThreadsRequest::s_BaseUrl = "https://a.4cdn.org/";

ImageScraper::RequestResult ImageScraper::FourChan::GetThreadsRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] FourChan::GetThreadsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };
        std::ostringstream response;
        const std::string url = s_BaseUrl + options.m_UrlExt;

        request.setOpt( new curlpp::options::Url( url ) );
        request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );
        request.setOpt( new curlpp::options::WriteStream( &response ) );
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );

        DebugLog( "[%s] FourChan::GetThreadsRequest, URL: %s", __FUNCTION__, url.c_str( ) );

        // blocks
        request.perform( );

        result.m_Response = response.str( );
    }
    catch( curlpp::RuntimeError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] FourChan::GetThreadsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }
    catch( curlpp::LogicError& error )
    {
        result.SetError( ResponseErrorCode::Unknown );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] FourChan::GetThreadsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    if( DownloadHelpers::IsResponseError( result ) )
    {
        DebugLog( "[%s] FourChan::GetThreadsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] FourChan::GetThreadsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
