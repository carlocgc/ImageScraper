#include "requests/fourchan/GetBoardsRequest.h"
#include "log/Logger.h"
#include "curlpp/Options.hpp"

#include <iostream>
#include "utils/DownloadUtils.h"

const std::string ImageScraper::FourChan::GetBoardsRequest::s_BaseUrl = "https://a.4cdn.org/boards.json";

ImageScraper::RequestResult ImageScraper::FourChan::GetBoardsRequest::Perform( const RequestOptions& options )
{
    DebugLog( "[%s] FourChan::GetBoardsRequest started!", __FUNCTION__ );

    RequestResult result{ };

    try
    {
        curlpp::Cleanup cleanup{ };
        curlpp::Easy request{ };
        std::ostringstream response;

        request.setOpt( new curlpp::options::Url( s_BaseUrl ) );
        request.setOpt( new curlpp::options::UserAgent( options.m_UserAgent ) );
        request.setOpt( new curlpp::options::WriteStream( &response ) );
        request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );

        DebugLog( "[%s] FourChan::GetBoardsRequest, URL: %s", __FUNCTION__, s_BaseUrl.c_str( ) );

        // blocks
        request.perform( );

        result.m_Response = response.str( );
    }
    catch( curlpp::RuntimeError& error )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] FourChan::GetBoardsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }
    catch( curlpp::LogicError& error )
    {
        result.SetError( ResponseErrorCode::InternalServerError );
        result.m_Error.m_ErrorString = error.what( );
        DebugLog( "[%s] FourChan::GetBoardsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    if( DownloadHelpers::IsFourChanResponseError( result ) )
    {
        DebugLog( "[%s] FourChan::GetBoardsRequest failed! %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return result;
    }

    DebugLog( "[%s] FourChan::GetBoardsRequest complete!", __FUNCTION__ );
    result.m_Success = true;
    return result;
}
