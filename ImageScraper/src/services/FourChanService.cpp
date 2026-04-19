#include "services/FourChanService.h"
#include "async/TaskManager.h"
#include "log/Logger.h"
#include "requests/RequestTypes.h"
#include "requests/fourchan/GetBoardsRequest.h"
#include "requests/fourchan/GetThreadsRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "utils/FourChanUtils.h"

#include <string>

ImageScraper::FourChanService::FourChanService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : Service( ContentProvider::FourChan, appConfig, userConfig, caBundle, outputDir, sink )
{
}

bool ImageScraper::FourChanService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider == ContentProvider::FourChan )
    {
        DownloadContent( options );
        return true;
    }

    return false;
}

bool ImageScraper::FourChanService::OpenExternalAuth( )
{
    LogError( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
    return false;
}

bool ImageScraper::FourChanService::HandleExternalAuth( const std::string& response )
{
    return false;
}

bool ImageScraper::FourChanService::IsSignedIn( ) const
{
    return false;
}

void ImageScraper::FourChanService::Authenticate( AuthenticateCallback callback )
{
    callback( m_ContentProvider, true );
}

bool ImageScraper::FourChanService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

void ImageScraper::FourChanService::DownloadContent( const UserInputOptions& inputOptions )
{
    InfoLog( "[%s] Starting 4chan media download!", __FUNCTION__ );
    LogDebug( "[%s] Board: %s", __FUNCTION__, inputOptions.m_FourChanBoard.c_str( ) );

    auto onComplete = [ this ]( int filesDownloaded )
    {
        InfoLog( "[%s] Content download complete!, files downloaded: %i", __FUNCTION__, filesDownloaded );
        m_Sink->OnRunComplete( );
    };

    auto onFail = [ this ]( )
    {
        LogError( "[%s] Failed to download media!, See log for details.", __FUNCTION__ );
        m_Sink->OnRunComplete( );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, options = inputOptions, onComplete, onFail ]( )
        {
            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            // Get all board data

            RequestOptions getBoardOptions{ };
            getBoardOptions.m_CaBundle = m_CaBundle;
            getBoardOptions.m_UserAgent = m_UserAgent;

            FourChan::GetBoardsRequest getBoardRequest{ };
            RequestResult getBoardResult = getBoardRequest.Perform( getBoardOptions );

            if( !getBoardResult.m_Success )
            {
                LogError( "[%s] Failed to get 4chan board data, error: %s", __FUNCTION__, getBoardResult.m_Error.m_ErrorString.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            InfoLog( "[%s] 4chan boards retrieved successfully.", __FUNCTION__ );
            LogDebug( "[%s] Response: %s", __FUNCTION__, getBoardResult.m_Response.c_str( ) );

            // Get page count for board

            json getBoardsResponse = json::parse( getBoardResult.m_Response );
            const int pages = GetPageCountForBoard( options.m_FourChanBoard, getBoardsResponse );

            if( pages <= 0 )
            {
                LogDebug( "[%s] Board %s contained no pages, nothing to download...", __FUNCTION__, options.m_FourChanBoard.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            std::vector<std::string> mediaUrls{ };

            for( int i = 1; i <= pages; ++i )
            {
                if( IsCancelled( ) )
                {
                    InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                    TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                    return;
                }

                std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

                RequestOptions getThreadOptions{ };
                getThreadOptions.m_CaBundle = m_CaBundle;
                getThreadOptions.m_UserAgent = m_UserAgent;
                getThreadOptions.m_UrlExt = options.m_FourChanBoard + '/' + std::to_string( i ) + ".json";

                FourChan::GetThreadsRequest getThreadRequest{ };
                RequestResult getThreadResult = getThreadRequest.Perform( getThreadOptions );

                if( !getThreadResult.m_Success )
                {
                    WarningLog( "[%s] Failed to get threads for page %i, error: %s", __FUNCTION__, i, getThreadResult.m_Error.m_ErrorString.c_str( ) );
                    continue;;
                }

                InfoLog( "[%s] Threads on page %i retrieved successfully.", __FUNCTION__, i );
                LogDebug( "[%s] Response: %s", __FUNCTION__, getThreadResult.m_Response.c_str( ) );

                json getThreadResponse = json::parse( getThreadResult.m_Response );
                std::vector<std::string> filenames{ };
                filenames = GetFileNamesFromResponse( getThreadResponse );

                if( filenames.empty( ) )
                {
                    InfoLog( "[%s] Board %s page %i contained no media...", __FUNCTION__, options.m_FourChanBoard.c_str( ), i );
                    continue;
                }

                for( const auto& filename : filenames )
                {
                    mediaUrls.push_back( "https://i.4cdn.org/" + options.m_FourChanBoard + "/" + filename );
                    if( static_cast<int>( mediaUrls.size( ) ) >= options.m_FourChanMaxMediaItems )
                    {
                        break;
                    }
                }

                if( static_cast<int>( mediaUrls.size( ) ) >= options.m_FourChanMaxMediaItems )
                {
                    break;
                }
            }

            if( mediaUrls.empty( ) )
            {
                InfoLog( "[%s] No content to download, nothing was done...", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            // Create download directory

            const std::filesystem::path dir = std::filesystem::path( m_OutputDir ) / "Downloads" / "4chan" / options.m_FourChanBoard;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                LogError( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            InfoLog( "[%s] Started downloading content, urls: %i", __FUNCTION__, mediaUrls.size( ) );

            int filesDownloaded = 0;

            const int totalDownloads = static_cast< int >( mediaUrls.size( ) );

            std::vector<char> buffer{ };

            // Download everything
            for( std::string& url : mediaUrls )
            {
                if( IsCancelled( ) )
                {
                    InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                    TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                    return;
                }

                std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

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

                outfile.write( buffer.data( ), buffer.size( ) );
                outfile.close( );

                m_Sink->OnFileDownloaded( filepath, url );

                buffer.clear( );

                ++filesDownloaded;

                m_Sink->OnTotalDownloadProgress( filesDownloaded, totalDownloads );

                InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, totalDownloads, filepath.c_str( ) );
            }

            TaskManager::Instance( ).SubmitMain( onComplete, filesDownloaded );
        } );

    ( void )task;
}

int ImageScraper::FourChanService::GetPageCountForBoard( const std::string& boardId, const Json& response )
{
    return FourChanUtils::GetPageCountForBoard( boardId, response );
}

std::vector<std::string> ImageScraper::FourChanService::GetFileNamesFromResponse( const Json& response )
{
    return FourChanUtils::GetFileNamesFromResponse( response );
}
