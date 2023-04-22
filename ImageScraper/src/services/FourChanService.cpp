#include "services/FourChanService.h"
#include "async/TaskManager.h"
#include "ui/FrontEnd.h"
#include "log/Logger.h"
#include "requests/RequestTypes.h"
#include "requests/fourchan/GetBoardsRequest.h"
#include "requests/fourchan/GetThreadsRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"

#include <string>

ImageScraper::FourChanService::FourChanService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
    : Service( ContentProvider::FourChan, appConfig, userConfig, caBundle, frontEnd )
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

void ImageScraper::FourChanService::OpenSignInWindow( )
{
    ErrorLog( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
}

bool ImageScraper::FourChanService::IsCancelled( )
{
    return m_FrontEnd->IsCancelled( );
}

void ImageScraper::FourChanService::DownloadContent( const UserInputOptions& inputOptions )
{
    InfoLog( "[%s] Starting 4chan media download!", __FUNCTION__ );
    DebugLog( "[%s] User: %s", __FUNCTION__, inputOptions.m_TumblrUser.c_str( ) );

    auto onComplete = [ & ]( int filesDownloaded )
    {
        InfoLog( "[%s] Content download complete!, files downloaded: %i", __FUNCTION__, filesDownloaded );
        m_FrontEnd->SetInputState( InputState::Free );
    };

    auto onFail = [ & ]( )
    {
        ErrorLog( "[%s] Failed to download media!, See log for details.", __FUNCTION__ );
        m_FrontEnd->SetInputState( InputState::Free );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_NetworkContext, [ &, options = inputOptions, onComplete, onFail ]( )
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
                ErrorLog( "[%s] Failed to get 4chan board data, error: %s", __FUNCTION__, getBoardResult.m_Error.m_ErrorString.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            InfoLog( "[%s] 4chan boards retrieved successfully.", __FUNCTION__ );
            DebugLog( "[%s] Response: %s", __FUNCTION__, getBoardResult.m_Response.c_str( ) );

            // Get page count for board

            json getBoardsResponse = json::parse( getBoardResult.m_Response );
            const int pages = GetPageCountForBoard( options.m_FourChanBoard, getBoardsResponse );

            if( pages <= 0 )
            {
                DebugLog( "[%s] Board %s contained no pages, nothing to download...", __FUNCTION__, options.m_FourChanBoard.c_str( ) );
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
                DebugLog( "[%s] Response: %s", __FUNCTION__, getThreadResult.m_Response.c_str( ) );

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
                }
            }

            if( mediaUrls.empty( ) )
            {
                InfoLog( "[%s] No content to download, nothing was done...", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            // Create download directory

            const std::filesystem::path dir = std::filesystem::current_path( ) / "Downloads" / "4chan" / options.m_FourChanBoard;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                ErrorLog( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
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

                DownloadRequest request{ m_FrontEnd };
                RequestResult result = request.Perform( downloadOptions );
                if( !result.m_Success )
                {
                    ErrorLog( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), url.c_str( ) );
                    continue;
                }

                const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( downloadOptions.m_Url );
                const std::string filepath = dirStr + "/" + filename;

                std::ofstream outfile{ filepath, std::ios::binary };
                if( !outfile.is_open( ) )
                {
                    ErrorLog( "[%s] Download failed, could not open file for write: %s", __FUNCTION__, filepath.c_str( ) );
                    continue;
                }

                outfile.write( buffer.data( ), buffer.size( ) );
                outfile.close( );

                buffer.clear( );

                ++filesDownloaded;

                m_FrontEnd->UpdateTotalDownloadsProgress( filesDownloaded, totalDownloads );

                InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, totalDownloads, filepath.c_str( ) );
            }

            TaskManager::Instance( ).SubmitMain( onComplete, filesDownloaded );
        } );

    ( void )task;
}

int ImageScraper::FourChanService::GetPageCountForBoard( const std::string& boardId, const Json& response )
{
    int pages = 0;

    if( !response.contains( "boards" ) )
    {
        return pages;
    }

    try
    {
        for( const auto& board : response[ "boards" ] )
        {
            if( !board.contains( "board" ) || board[ "board" ] != boardId )
            {
                continue;
            }

            if( board.contains( "pages" ) )
            {
                pages = board[ "pages" ];
                break;
            }
        }
    }
    catch( const Json::exception& e )
    {
        const std::string error = e.what( );
        ErrorLog( "[%s] GetPageCountForBoard Error parsing response!, error: %s", __FUNCTION__, error.c_str( ) );
    }

    return pages;
}

std::vector<std::string> ImageScraper::FourChanService::GetFileNamesFromResponse( const Json& response )
{
    std::vector<std::string> filenames{ };

    if( !response.contains( "threads" ) )
    {
        return filenames;
    }

    try
    {
        for( const auto& thread : response[ "threads" ] )
        {
            if( !thread.contains( "posts" ) )
            {
                continue;
            }

            for( const auto& post : thread[ "posts" ] )
            {
                if( !post.contains( "tim" ) || !post.contains( "ext" ) )
                {
                    continue;
                }

                const uint64_t time = post[ "tim" ].get<uint64_t>( );
                const std::string ext = post[ "ext" ].get<std::string>( );

                filenames.push_back( std::to_string( time ) + ext );
            }
        }
    }
    catch( const Json::exception& e )
    {
        const std::string error = e.what( );
        ErrorLog( "[%s] GetFileNamesFromResponse Error parsing response!, error: %s", __FUNCTION__, error.c_str() );
    }

    return filenames;
}
