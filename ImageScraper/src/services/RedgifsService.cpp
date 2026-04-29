#include "services/RedgifsService.h"

#include "async/TaskManager.h"
#include "log/Logger.h"
#include "network/RedgifsTokenCache.h"
#include "requests/RequestTypes.h"
#include "requests/redgifs/SearchUserGifsRequest.h"
#include "utils/DownloadUtils.h"
#include "utils/RedgifsUtils.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace
{
    constexpr int s_RedgifsUserSearchPageSize = 100;
}

ImageScraper::RedgifsService::RedgifsService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver )
    : Service( ContentProvider::Redgifs, appConfig, userConfig, caBundle, outputDir, sink, std::move( urlResolver ) )
{
}

bool ImageScraper::RedgifsService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Redgifs )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::RedgifsService::OpenExternalAuth( )
{
    LogError( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
    return false;
}

bool ImageScraper::RedgifsService::HandleExternalAuth( const std::string& response )
{
    ( void )response;
    return false;
}

bool ImageScraper::RedgifsService::IsSignedIn( ) const
{
    return false;
}

void ImageScraper::RedgifsService::Authenticate( AuthenticateCallback callback )
{
    callback( m_ContentProvider, true );
}

bool ImageScraper::RedgifsService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

void ImageScraper::RedgifsService::DownloadContent( const UserInputOptions& inputOptions )
{
    auto onComplete = [ this ]( int filesDownloaded )
    {
        SuccessLog( "[%s] Content download complete!, files downloaded: %i", __FUNCTION__, filesDownloaded );
        m_Sink->OnRunComplete( );
    };

    auto onFail = [ this ]( )
    {
        LogError( "[%s] Failed to download Redgifs media!, See log for details.", __FUNCTION__ );
        m_Sink->OnRunComplete( );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, user = inputOptions.m_RedgifsUser, maxItems = inputOptions.m_RedgifsMaxMediaItems, onComplete, onFail ]( )
        {
            InfoLog( "[%s] Starting Redgifs media download for user: %s", __FUNCTION__, user.c_str( ) );

            if( user.empty( ) )
            {
                LogError( "[%s] Redgifs user input was empty.", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            RedgifsTokenCache tokenCache{ m_HttpClient, m_CaBundle, m_UserAgent };
            if( !tokenCache.EnsureToken( ) )
            {
                LogError( "[%s] Could not acquire redgifs token, aborting.", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            const std::string lowerUser = StringUtils::ToLower( user );
            std::vector<std::string> mediaUrls{ };
            int page = 1;
            int totalPages = 1;

            while( static_cast<int>( mediaUrls.size( ) ) < maxItems && page <= totalPages )
            {
                if( IsCancelled( ) )
                {
                    InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                    TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                    return;
                }

                std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

                RequestOptions options{ };
                options.m_CaBundle = m_CaBundle;
                options.m_UserAgent = m_UserAgent;
                options.m_AccessToken = tokenCache.Token( );
                options.m_UrlExt = lowerUser;
                options.m_QueryParams.push_back( { "count", std::to_string( s_RedgifsUserSearchPageSize ) } );
                options.m_QueryParams.push_back( { "page", std::to_string( page ) } );

                Redgifs::SearchUserGifsRequest request{ m_HttpClient };
                const RequestResult result = request.Perform( options );
                if( !result.m_Success )
                {
                    LogError( "[%s] Redgifs user search failed for %s page %i: %s", __FUNCTION__, user.c_str( ), page, result.m_Error.m_ErrorString.c_str( ) );
                    break;
                }

                int responsePages = 0;
                std::vector<std::string> pageUrls = RedgifsUtils::ExtractMediaUrlsFromUserSearchResponse( result.m_Response, responsePages );
                if( responsePages > 0 )
                {
                    totalPages = responsePages;
                }

                if( pageUrls.empty( ) )
                {
                    InfoLog( "[%s] Redgifs user search page %i for %s returned no usable media.", __FUNCTION__, page, user.c_str( ) );
                    break;
                }

                for( std::string& url : pageUrls )
                {
                    if( static_cast<int>( mediaUrls.size( ) ) >= maxItems )
                    {
                        break;
                    }
                    mediaUrls.push_back( std::move( url ) );
                }

                InfoLog( "[%s] Redgifs user %s page %i: %i candidates returned, kept %i / %i.", __FUNCTION__, user.c_str( ), page, static_cast<int>( pageUrls.size( ) ), static_cast<int>( mediaUrls.size( ) ), maxItems );

                ++page;
            }

            if( mediaUrls.empty( ) )
            {
                WarningLog( "[%s] No Redgifs media items were found for user: %s", __FUNCTION__, user.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const std::filesystem::path dir = std::filesystem::path( m_OutputDir ) / "Downloads" / "Redgifs" / lowerUser;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                LogError( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            const std::optional<int> filesDownloaded = DownloadMediaUrls( mediaUrls, dir );
            if( !filesDownloaded.has_value( ) )
            {
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            TaskManager::Instance( ).SubmitMain( onComplete, *filesDownloaded );
        } );

    ( void )task;
}
