#include "services/Service.h"
#include "requests/DownloadRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

#include <chrono>
#include <fstream>
#include <thread>

std::optional<int> ImageScraper::Service::DownloadMediaUrls( std::vector<std::string>& mediaUrls, const std::filesystem::path& dir )
{
    const std::string providerName = GetProviderDisplayName( );
    const std::string dirStr = dir.generic_string( );

    InfoLog( "[%s] Started downloading %s content, urls: %i", __FUNCTION__, providerName.c_str( ), static_cast<int>( mediaUrls.size( ) ) );

    int filesDownloaded = 0;
    const int totalDownloads = static_cast<int>( mediaUrls.size( ) );
    std::vector<char> buffer{ };

    for( std::string& url : mediaUrls )
    {
        if( IsCancelled( ) )
        {
            InfoLog( "[%s] User cancelled %s download operation!", __FUNCTION__, providerName.c_str( ) );
            return std::nullopt;
        }

        std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

        const std::string newUrl = DownloadHelpers::RedirectToPreferredFileTypeUrl( url );
        if( !newUrl.empty( ) )
        {
            url = newUrl;
        }

        buffer.clear( );

        DownloadOptions downloadOptions{ };
        downloadOptions.m_CaBundle = m_CaBundle;
        downloadOptions.m_Url = url;
        downloadOptions.m_UserAgent = m_UserAgent;
        downloadOptions.m_BufferPtr = &buffer;

        DownloadRequest request{ m_Sink };
        RequestResult result = request.Perform( downloadOptions );
        if( !result.m_Success )
        {
            if( !IsCancelled( ) )
            {
                LogError( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), url.c_str( ) );
            }
            continue;
        }

        const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( downloadOptions.m_Url );
        const std::string filepath = dirStr + "/" + filename;

        std::ofstream outfile{ filepath, std::ios::binary };
        if( !outfile.is_open( ) )
        {
            LogError( "[%s] Download failed, could not open file for write: %s", __FUNCTION__, filepath.c_str( ) );
            continue;
        }

        outfile.write( buffer.data( ), static_cast<std::streamsize>( buffer.size( ) ) );
        outfile.close( );

        m_Sink->OnFileDownloaded( filepath, url );

        ++filesDownloaded;

        m_Sink->OnTotalDownloadProgress( filesDownloaded, totalDownloads );

        InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, totalDownloads, filepath.c_str( ) );
    }

    return filesDownloaded;
}
