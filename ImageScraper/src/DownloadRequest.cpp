#include "DownloadRequest.h"
#include "curlpp/Options.hpp"
#include "DownloadUtils.h"

DownloadRequest::DownloadRequest( const Config& config, const std::string& endpoint )
    : Request( config, endpoint )
{
    Configure( config );
}

bool DownloadRequest::Perform( )
{
    if( !m_File.is_open( ) )
    {
        ErrorLog( "[%s] Failed to perform download, m_File not open, call Configure() first!", __FUNCTION__ );
        return false;
    }

    m_Easy.perform( );
    m_File.close( );

    return true;
}

size_t DownloadRequest::WriteCallback( char* contents, size_t size, size_t nmemb )
{
    size_t realsize = size * nmemb;
    m_File.write( static_cast< char* >( contents ), realsize );
    return realsize;
}

bool DownloadRequest::Configure( const Config& config )
{
    std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( m_EndPoint );
    std::string folder = DownloadHelpers::UrlToSafeString( m_EndPoint );
    std::string dir = DownloadHelpers::CreateFolder( folder );
    if( dir.empty( ) )
    {
        ErrorLog( "[%s] Failed to create download directory for url: ", __FUNCTION__, m_EndPoint );
        return false;
    }

    m_FilePath = dir + "/" + filename;

    // Open file for writing // TODO : Use a raw buffer for this? safer?
    m_File.open( m_FilePath, std::ios::binary );
    if( !m_File.is_open( ) )
    {
        ErrorLog( "[%s] Failed to open buffer file with path: %s", __FUNCTION__, m_FilePath );
        return false;
    }

    // Set the write callback
    using namespace std::placeholders;
    curlpp::types::WriteFunctionFunctor functor = std::bind( &DownloadRequest::WriteCallback, this, _1, _2, _3 );

    curlpp::options::WriteFunction* writeCallback = new curlpp::options::WriteFunction( functor );
    m_Easy.setOpt( writeCallback );

    // Set the cert bundle for TLS/HTTPS
    m_Easy.setOpt( new curlpp::options::CaInfo( m_CaBundle ) );

    // Set URL for the image
    m_Easy.setOpt( new curlpp::options::Url( m_EndPoint ) );

}
