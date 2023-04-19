#include "services/RedditService.h"
#include "requests/RequestTypes.h"
#include "requests/reddit/FetchSubredditPostsRequest.h"
#include "requests/reddit/AuthenticationRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "async/TaskManager.h"
#include "ui/FrontEnd.h"
#include "io/JsonFile.h"
#include "utils/StringUtils.h"

#include <string>
#include <map>

using namespace ImageScraper::Reddit;
using Json = nlohmann::json;

const std::string ImageScraper::RedditService::s_AppDataKey_DeviceId = "reddit_device_id";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientId = "reddit_client_id";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientSecret = "reddit_client_secret";

ImageScraper::RedditService::RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
    : Service( appConfig, userConfig, caBundle, frontEnd )
{
    if( !m_AppConfig->GetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId ) )
    {
        m_DeviceId = StringUtils::CreateGuid( 30 );
        m_AppConfig->SetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId );
        m_AppConfig->Serialise( );
        DebugLog( "[%s] Created new Reddit device id: %s", __FUNCTION__, m_DeviceId.c_str( ) );
    }

    if( !m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, m_ClientId ) )
    {
        WarningLog( "[%s] Could not find reddit client id, add client id to %s to be able to authenticate with the reddit api!", __FUNCTION__, m_UserConfig->GetFilePath( ).c_str( ) );
    }

    if( !m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, m_ClientSecret ) )
    {
        WarningLog( "[%s] Could not find reddit client secret, add client secret to %s to be able to authenticate with the reddit api!", __FUNCTION__, m_UserConfig->GetFilePath( ).c_str( ) );
    }
}

bool ImageScraper::RedditService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Reddit )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::RedditService::IsCancelled( )
{
    return m_FrontEnd->IsCancelled( );
}

const bool ImageScraper::RedditService::IsAuthenticated( ) const
{
    if( m_AuthAccessToken == "" )
    {
        return false;
    }

    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now( );
    if( m_TokenReceived + m_AuthExpireSeconds <= now )
    {
        return false;
    }

    return true;
}

void ImageScraper::RedditService::DownloadContent( const UserInputOptions& inputOptions )
{
    InfoLog( "[%s] Starting Reddit media download!", __FUNCTION__ );
    DebugLog( "[%s] Subreddit: %s", __FUNCTION__, inputOptions.m_SubredditName.c_str( ) );
    DebugLog( "[%s] Scope: %s", __FUNCTION__, inputOptions.m_RedditScope.c_str( ) );
    DebugLog( "[%s] Media Item Limit: %i", __FUNCTION__, inputOptions.m_RedditMaxMediaItems );

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

            if( !IsAuthenticated( ) )
            {
                RequestOptions authOptions{ };
                authOptions.m_CaBundle = m_CaBundle;
                authOptions.m_UserAgent = m_UserAgent;
                authOptions.m_ClientId = m_ClientId;
                authOptions.m_ClientSecret = m_ClientSecret;

                AuthenticationRequest authRequest{ };
                RequestResult authResult = authRequest.Perform( authOptions );

                if( authResult.m_Success )
                {
                    try
                    {
                        const Json authResponse = Json::parse( authResult.m_Response );

                        if( authResponse.contains( "access_token" ) )
                        {
                            m_AuthAccessToken = authResponse[ "access_token" ];

                            if( authResponse.contains( "expires_in" ) )
                            {
                                const int expireSeconds = authResponse[ "expires_in" ];
                                const int expireDelta = 180; // 3 minutes
                                m_AuthExpireSeconds = std::chrono::seconds{ expireSeconds - expireDelta };
                                m_TokenReceived = std::chrono::system_clock::now( );

                                InfoLog( "[%s] Reddit authenticated successfully!", __FUNCTION__ );
                                DebugLog( "[%s] Reddit access token: %s", __FUNCTION__, m_AuthAccessToken.c_str( ) );
                            }
                        }
                        else
                        {
                            ErrorLog( "[%s] Failed to authenticate with Reddit API, Could not retrieve access token. Response %s", __FUNCTION__, authResult.m_Response.c_str( ) );
                        }
                    }
                    catch( const Json::exception& parseError )
                    {
                        ErrorLog( "[%s] Failed to authenticate with Reddit API, error: %s", __FUNCTION__, parseError.what( ) );
                    }
                }
                else
                {
                    ErrorLog( "[%s] Failed to authenticate with Reddit API, error: %s", __FUNCTION__, authResult.m_Error.m_ErrorString );
                }
            }

            std::vector<std::string> mediaUrls{ };
            int pageNum = 1;

            do
            {
                if( IsCancelled( ) )
                {
                    InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                    TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                    return;
                }

                std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

                RequestOptions fetchOptions{ };

                if( !options.m_RedditScopeTimeFrame.empty() )
                {
                    fetchOptions.m_QueryParams.push_back( { "t", options.m_RedditScopeTimeFrame } );
                }

                if( !m_AfterParam.empty( ) )
                {
                    fetchOptions.m_QueryParams.push_back( { "after", m_AfterParam } );
                }

                fetchOptions.m_QueryParams.push_back( { "limit", "100" } );
                fetchOptions.m_UrlExt = options.m_SubredditName + '/' + options.m_RedditScope + ".json";
                fetchOptions.m_CaBundle = m_CaBundle;
                fetchOptions.m_UserAgent = m_UserAgent;
                fetchOptions.m_AccessToken = m_AuthAccessToken;

                FetchSubredditPostsRequest fetchRequest{ };
                RequestResult fetchResult = fetchRequest.Perform( fetchOptions );

                if( !fetchResult.m_Success )
                {
                    WarningLog( "[%s] Failed to fetch subreddit (page %i), error: %s", __FUNCTION__, pageNum, fetchResult.m_Error.m_ErrorString.c_str( ) );
                    continue;;
                }

                Json pageResponse = Json::parse( fetchResult.m_Response );

                std::vector<std::string> pageMediaUrls = GetMediaUrls( pageResponse );
                mediaUrls.insert( mediaUrls.end( ), pageMediaUrls.begin( ), pageMediaUrls.end( ) );

                const std::size_t maxItems = static_cast< std::size_t >( options.m_RedditMaxMediaItems );
                if( mediaUrls.size( ) > maxItems )
                {
                    mediaUrls.erase( mediaUrls.begin( ) + maxItems, mediaUrls.end( ) );
                }

                InfoLog( "[%s] Subreddit (page %i) fetched successfully. Media urls queued: %i/%i", __FUNCTION__, pageNum, static_cast< int >( mediaUrls.size( ) ), options.m_RedditMaxMediaItems );
                DebugLog( "[%s] Response: %s", __FUNCTION__, fetchResult.m_Response.c_str( ) );

                ++pageNum;
            }
            while( !m_AfterParam.empty( ) && static_cast<int>( mediaUrls.size() ) < options.m_RedditMaxMediaItems );

            if( mediaUrls.empty( ) )
            {
                WarningLog( "[%s] No content to download, nothing was done...", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            InfoLog( "[%s] All Subreddit data fetched successfully.", __FUNCTION__ );

            // Create download directory
            const std::filesystem::path dir = std::filesystem::current_path( ) / "Downloads" / "Reddit" / options.m_SubredditName / options.m_RedditScope;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                ErrorLog( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            InfoLog( "[%s] Started downloading content, urls: %i", __FUNCTION__, mediaUrls.size( ) );

            int filesDownloaded = 0;

            const int totalDownloads = static_cast<int>( mediaUrls.size( ) );

            // Download images
            for( std::string& url : mediaUrls )
            {
                if( IsCancelled( ) )
                {
                    InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                    TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                    return;
                }

                std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

                const std::string newUrl = DownloadHelpers::RedirectToPreferredFileTypeUrl( url );

                if( newUrl != "" )
                {
                    url = newUrl;
                }

                std::vector<char> buffer{ };

                DownloadOptions options{ };
                options.m_CaBundle = m_CaBundle;
                options.m_Url = url;
                options.m_UserAgent = m_UserAgent;
                options.m_BufferPtr = &buffer;

                DownloadRequest request{ m_FrontEnd };
                RequestResult result = request.Perform( options );
                if( !result.m_Success )
                {
                    ErrorLog( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), url.c_str( ) );
                    continue;
                }

                const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( options.m_Url );
                const std::string filepath = dirStr + "/" + filename;

                std::ofstream outfile{ filepath, std::ios::binary };
                if( !outfile.is_open( ) )
                {
                    ErrorLog( "[%s] Download failed, could not open file for write: %s", __FUNCTION__, filepath.c_str( ) );
                    continue;
                }

                outfile.write( buffer.data( ), buffer.size( ) );
                outfile.close( );

                ++filesDownloaded;

                m_FrontEnd->UpdateTotalDownloadsProgress( filesDownloaded, totalDownloads );

                InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, totalDownloads, filepath.c_str( ) );
            }

            TaskManager::Instance( ).SubmitMain( onComplete, filesDownloaded );
        } );

    ( void )task;
}

std::vector<std::string> ImageScraper::RedditService::GetMediaUrls( const Json& response )
{
    std::vector<std::string> mediaUrls{ };

    if( !response.contains( "data" ) )
    {
        return mediaUrls;
    }

    try
    {
        const Json& data = response[ "data" ];

        if( !data.contains( "children" ) )
        {
            return mediaUrls;
        }

        const std::vector<Json>& children = data[ "children" ];

        for (const auto& post : children )
        {
            const Json& postData = post[ "data" ];

            std::string contentKey{ };

            if( postData.contains( "url" ) )
            {
                contentKey = "url";
            }
            else if( postData.contains( "url_overridden_by_dest" ) )
            {
                contentKey = "url_overridden_by_dest";
            }
            else
            {
                return mediaUrls;
            }

            const std::vector<std::string> targetExts{ ".jpg", ".jpeg", ".png", ".webm", ".webp", ".gif", ".gifv", ".mp4" };

            const std::string& url = postData[ contentKey ];

            for( const auto& ext : targetExts )
            {
                const auto& pos = url.find( ext );
                if( pos != std::string::npos )
                {
                    mediaUrls.push_back( url );
                }
            }
        }

        if( data.contains( "after" ) && !data[ "after" ].is_null( ) )
        {
            m_AfterParam = data[ "after" ];
        }
        else
        {
            m_AfterParam.clear();
        }
    }
    catch( const Json::exception& e )
    {
        const std::string error = e.what( );
        ErrorLog( "[%s] GetMediaUrls Error parsing data!, error: %s", __FUNCTION__, error.c_str( ) );
    }

    return mediaUrls;
}