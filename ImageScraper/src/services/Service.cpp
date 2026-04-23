#include "services/Service.h"
#include "requests/DownloadRequest.h"
#include "requests/VideoDownloadRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "network/CurlHttpClient.h"
#include "network/RetryHttpClient.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"

#include <chrono>
#include <thread>

ImageScraper::Service::Service( ContentProvider provider, std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : m_ContentProvider{ provider }
    , m_AppConfig{ appConfig }
    , m_UserConfig{ userConfig }
    , m_CaBundle{ caBundle }
    , m_OutputDir{ outputDir }
    , m_Sink{ sink }
    , m_HttpClient{ std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ) }
{
}

std::optional<int> ImageScraper::Service::DownloadMedia( const std::vector<MediaDownload>& downloads, const std::filesystem::path& dir )
{
    const std::string providerName = GetProviderDisplayName( );

    InfoLog( "[%s] Started downloading %s content, urls: %i", __FUNCTION__, providerName.c_str( ), static_cast<int>( downloads.size( ) ) );

    int filesDownloaded = 0;
    const int totalDownloads = static_cast<int>( downloads.size( ) );

    for( const MediaDownload& download : downloads )
    {
        if( IsCancelled( ) )
        {
            InfoLog( "[%s] User cancelled %s download operation!", __FUNCTION__, providerName.c_str( ) );
            return std::nullopt;
        }

        std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

        const std::filesystem::path filepath = dir / download.m_FileName;
        const std::string filepathStr = filepath.generic_string( );

        DownloadOptions downloadOptions{ };
        downloadOptions.m_CaBundle = m_CaBundle;
        downloadOptions.m_Url = download.m_SourceUrl;
        downloadOptions.m_UserAgent = m_UserAgent;
        downloadOptions.m_OutputFilePath = filepath;

        RequestResult result{ };
        if( download.m_Method == DownloadMethod::HlsVideo )
        {
            VideoDownloadRequest request{ m_Sink };
            result = request.Perform( downloadOptions );
        }
        else
        {
            DownloadRequest request{ m_Sink };
            result = request.Perform( downloadOptions );
        }

        if( !result.m_Success )
        {
            if( !IsCancelled( ) )
            {
                LogError( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), download.m_SourceUrl.c_str( ) );
            }
            continue;
        }

        m_Sink->OnFileDownloaded( filepathStr, download.m_SourceUrl );

        ++filesDownloaded;

        m_Sink->OnTotalDownloadProgress( filesDownloaded, totalDownloads );

        InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, totalDownloads, filepathStr.c_str( ) );
    }

    return filesDownloaded;
}

std::optional<int> ImageScraper::Service::DownloadMediaUrls( std::vector<std::string>& mediaUrls, const std::filesystem::path& dir )
{
    std::vector<MediaDownload> downloads{ };
    downloads.reserve( mediaUrls.size( ) );

    for( std::string& url : mediaUrls )
    {
        const std::string newUrl = DownloadHelpers::RedirectToPreferredFileTypeUrl( url );
        if( !newUrl.empty( ) )
        {
            url = newUrl;
        }

        downloads.push_back( { url, DownloadHelpers::ExtractFileNameAndExtFromUrl( url ), DownloadMethod::DirectFile } );
    }

    return DownloadMedia( downloads, dir );
}
