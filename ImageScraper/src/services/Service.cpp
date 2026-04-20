#include "services/Service.h"
#include "requests/DownloadRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

#include <chrono>
#include <thread>

std::optional<int> ImageScraper::Service::DownloadMediaUrls( std::vector<std::string>& mediaUrls, const std::filesystem::path& dir )
{
    const std::string providerName = GetProviderDisplayName( );

    InfoLog( "[%s] Started downloading %s content, urls: %i", __FUNCTION__, providerName.c_str( ), static_cast<int>( mediaUrls.size( ) ) );

    int filesDownloaded = 0;
    const int totalDownloads = static_cast<int>( mediaUrls.size( ) );

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

        const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( url );
        const std::filesystem::path filepath = dir / filename;
        const std::string filepathStr = filepath.generic_string( );

        DownloadOptions downloadOptions{ };
        downloadOptions.m_CaBundle = m_CaBundle;
        downloadOptions.m_Url = url;
        downloadOptions.m_UserAgent = m_UserAgent;
        downloadOptions.m_OutputFilePath = filepath;

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

        m_Sink->OnFileDownloaded( filepathStr, url );

        ++filesDownloaded;

        m_Sink->OnTotalDownloadProgress( filesDownloaded, totalDownloads );

        InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, totalDownloads, filepathStr.c_str( ) );
    }

    return filesDownloaded;
}
