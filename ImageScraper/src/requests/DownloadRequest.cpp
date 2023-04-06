#include "requests/DownloadRequest.h"
#include "curlpp/Options.hpp"
#include "utils/DownloadUtils.h"

RequestResult DownloadRequest::Perform( const RequestOptions& options )
{
    RequestResult result{ };

    std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( options.m_Url );
    //std::string folder = DownloadHelpers::UrlToSafeString( options.m_Url );
    std::string folder = "output";
    std::string dir = DownloadHelpers::CreateFolder( folder );
    if( dir.empty( ) )
    {
        ErrorLog( "[%s] Failed to create download directory for url: ", __FUNCTION__, options.m_Url );
        result.m_Error.m_ErrorCode = ResponseErrorCode::InvalidDirectory;
        return result;
    }

    const std::string filepath = dir + "/" + filename;

    // Open file for writing // TODO : Use a raw buffer for this? safer?
    m_File.open( filepath, std::ios::binary );
    if( !m_File.is_open( ) )
    {
        ErrorLog( "[%s] Failed to open buffer file with path: %s", __FUNCTION__, filepath );
        result.m_Error.m_ErrorCode = ResponseErrorCode::InvalidDirectory;
        return result;
    }

    curlpp::Cleanup cleanup{ };
    curlpp::Easy request{ };

    // Set the write callback
    using namespace std::placeholders;
    curlpp::types::WriteFunctionFunctor functor = std::bind( &DownloadRequest::WriteCallback, this, _1, _2, _3 );

    curlpp::options::WriteFunction* writeCallback = new curlpp::options::WriteFunction( functor );
    request.setOpt( writeCallback );

    // Set the cert bundle for TLS/HTTPS
    request.setOpt( new curlpp::options::CaInfo( options.m_CaBundle ) );

    // Set URL for the image
    request.setOpt( new curlpp::options::Url( options.m_Url ) );

    request.perform( );

    m_File.close( );

    result.m_Response = filepath;
    result.m_Success = true;

    return result;
}

size_t DownloadRequest::WriteCallback( char* contents, size_t size, size_t nmemb )
{
    size_t realsize = size * nmemb;
    m_File.write( static_cast< char* >( contents ), realsize );
    return realsize;
}
