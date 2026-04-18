#include "services/RedditService.h"
#include "requests/RequestTypes.h"
#include "requests/reddit/FetchSubredditPostsRequest.h"
#include "requests/reddit/AppOnlyAuthRequest.h"
#include "requests/reddit/FetchAccessTokenRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "utils/RedditUtils.h"
#include "log/Logger.h"
#include "async/TaskManager.h"
#include "io/JsonFile.h"
#include "utils/StringUtils.h"
#include "requests/reddit/RefreshAccessTokenRequest.h"
#include "requests/reddit/RevokeAccessTokenRequest.h"
#include "requests/reddit/GetCurrentUserRequest.h"

#include <string>
#include <map>
#include <mutex>

#include <Shellapi.h>

using namespace ImageScraper::Reddit;
using Json = nlohmann::json;

const std::string ImageScraper::RedditService::s_RedirectUrl              = "http://localhost:8080";
const std::string ImageScraper::RedditService::s_AppDataKey_DeviceId      = "reddit_device_id";
const std::string ImageScraper::RedditService::s_AppDataKey_RefreshToken  = "reddit_refresh_token";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientId     = "reddit_client_id";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientSecret = "reddit_client_secret";

ImageScraper::RedditService::RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : Service( ContentProvider::Reddit, appConfig, userConfig, caBundle, outputDir, sink )
{
    OAuthConfig oauthConfig{
        "https://www.reddit.com/api/v1/authorize",
        "identity,read",
        s_UserDataKey_ClientId,
        s_UserDataKey_ClientSecret,
        s_AppDataKey_DeviceId,
        s_AppDataKey_RefreshToken,
        s_RedirectUrl,
        { { "duration", "permanent" } }
    };

    auto fetchFn = [ ]( const RequestOptions& opts )
        {
            return FetchAccessTokenRequest{ }.Perform( opts );
        };

    auto refreshFn = [ ]( const RequestOptions& opts )
        {
            return RefreshAccessTokenRequest{ }.Perform( opts );
        };

    m_OAuthClient = std::make_unique<OAuthClient>(
        std::move( oauthConfig ),
        m_AppConfig,
        m_UserConfig,
        m_CaBundle,
        m_UserAgent,
        fetchFn,
        refreshFn );
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

bool ImageScraper::RedditService::HasRequiredCredentials( ) const
{
    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, clientSecret );
    return !clientId.empty( ) && !clientSecret.empty( );
}

bool ImageScraper::RedditService::OpenExternalAuth( )
{
    return m_OAuthClient->OpenAuth( );
}

bool ImageScraper::RedditService::HandleExternalAuth( const std::string& response )
{
    const int signingInProvider = m_Sink->GetSigningInProvider( );
    if( signingInProvider == INVALID_CONTENT_PROVIDER )
    {
        LogError( "[%s] RedditService::HandleExternalAuth skipped, No signing in provider!", __FUNCTION__ );
        return false;
    }

    if( static_cast< ContentProvider >( signingInProvider ) != m_ContentProvider )
    {
        LogDebug( "[%s] RedditService::HandleExternalAuth skipped, incorrect provider!", __FUNCTION__ );
        return false;
    }

    if( response.find( "favicon" ) != std::string::npos )
    {
        LogDebug( "[%s] RedditService::HandleExternalAuth skipped, invalid message!", __FUNCTION__ );
        return false;
    }

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, response ]( )
        {
            if( m_OAuthClient->HandleAuth( response, "Reddit" ) )
            {
                FetchCurrentUser( );
                TaskManager::Instance( ).SubmitMain( [ this ]( )
                    {
                        m_Sink->OnSignInComplete( ContentProvider::Reddit );
                        InfoLog( "[%s] Reddit signed in successfully!", __FUNCTION__ );
                    } );
            }
            else
            {
                TaskManager::Instance( ).SubmitMain( [ this ]( )
                    {
                        m_Sink->OnSignInComplete( ContentProvider::Reddit );
                        LogError( "[%s] Reddit sign in failed.", __FUNCTION__ );
                    } );
            }
        } );

    ( void )task;
    return true;
}

bool ImageScraper::RedditService::IsSignedIn( ) const
{
    // True only when the user has signed in via OAuth - refresh token is the
    // indicator. App-only auth populates the access token but not the refresh token,
    // so checking the access token alone would flicker the button during downloads.
    return m_OAuthClient->IsSignedIn( );
}

std::string ImageScraper::RedditService::GetSignedInUser( ) const
{
    std::unique_lock<std::mutex> lock( m_UsernameMutex );
    return m_Username;
}

void ImageScraper::RedditService::Authenticate( AuthenticateCallback callback )
{
    auto onComplete = [ this, completeCallback = callback ]( )
    {
        completeCallback( m_ContentProvider, true );
        LogDebug( "[%s] Reddit authenticated successfully!", __FUNCTION__ );
    };

    auto onFail = [ this, completeCallback = callback ]( )
    {
        completeCallback( m_ContentProvider, false );
        LogDebug( "[%s] Reddit authentication failed.", __FUNCTION__ );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, onComplete, onFail ]( )
        {
            if( m_OAuthClient->TryRefreshToken( ) )
            {
                FetchCurrentUser( );
                onComplete( );
            }
            else
            {
                onFail( );
            }
        } );

    ( void )task;
}

bool ImageScraper::RedditService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

void ImageScraper::RedditService::SignOut( )
{
    // NOTE: RevokeAccessTokenRequest exists but is intentionally not called here.
    // Reddit imposes a server-side propagation delay of several minutes after token
    // revocation, during which re-authentication hangs (OAuth page never redirects).
    // For a desktop app the security trade-off is acceptable - tokens live only on
    // the user's machine and the refresh token is deleted locally, so it cannot be
    // reused. Local-only clear gives instant, clean sign-out with no UX penalty.
    m_OAuthClient->SignOut( );
    {
        std::unique_lock<std::mutex> lock( m_UsernameMutex );
        m_Username.clear( );
    }

    InfoLog( "[%s] Reddit signed out.", __FUNCTION__ );
}

void ImageScraper::RedditService::FetchCurrentUser( )
{
    RequestOptions options{ };
    options.m_CaBundle    = m_CaBundle;
    options.m_UserAgent   = m_UserAgent;
    options.m_AccessToken = m_OAuthClient->GetAccessToken( );

    GetCurrentUserRequest request{ };
    RequestResult result = request.Perform( options );

    if( !result.m_Success )
    {
        WarningLog( "[%s] Failed to fetch Reddit username: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return;
    }

    const Json response = Json::parse( result.m_Response );
    if( !response.contains( "name" ) )
    {
        WarningLog( "[%s] Reddit /api/v1/me response missing 'name' field.", __FUNCTION__ );
        return;
    }

    {
        std::unique_lock<std::mutex> lock( m_UsernameMutex );
        m_Username = response[ "name" ].get<std::string>( );
    }

    InfoLog( "[%s] Reddit signed in as: %s", __FUNCTION__, m_Username.c_str( ) );
}

void ImageScraper::RedditService::DownloadContent( const UserInputOptions& inputOptions )
{
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
            InfoLog( "[%s] Starting Reddit media download!", __FUNCTION__ );
            LogDebug( "[%s] Subreddit: %s", __FUNCTION__, options.m_SubredditName.c_str( ) );
            LogDebug( "[%s] Scope: %s", __FUNCTION__, options.m_RedditScope.c_str( ) );
            LogDebug( "[%s] Media Item Limit: %i", __FUNCTION__, options.m_RedditMaxMediaItems );

            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            if( !m_OAuthClient->IsAuthenticated( ) )
            {
                if( m_OAuthClient->IsSignedIn( ) )
                {
                    // Refresh token is present but access token has expired - try to refresh.
                    // TryRefreshToken clears the refresh token internally on failure.
                    m_OAuthClient->TryRefreshToken( );
                }
                else
                {
                    TryPerformAppOnlyAuth( );
                }
            }

            if( !m_OAuthClient->IsAuthenticated( ) )
            {
                LogError( "[%s] Could not authenticate with reddit api", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
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

                if( !options.m_RedditScopeTimeFrame.empty( ) )
                {
                    fetchOptions.m_QueryParams.push_back( { "t", options.m_RedditScopeTimeFrame } );
                }

                if( !m_AfterParam.empty( ) )
                {
                    fetchOptions.m_QueryParams.push_back( { "after", m_AfterParam } );
                }

                fetchOptions.m_QueryParams.push_back( { "limit", "100" } );
                fetchOptions.m_UrlExt   = options.m_SubredditName + '/' + options.m_RedditScope + ".json";
                fetchOptions.m_CaBundle = m_CaBundle;
                fetchOptions.m_UserAgent = m_UserAgent;
                fetchOptions.m_AccessToken = m_OAuthClient->GetAccessToken( );

                FetchSubredditPostsRequest fetchRequest{ };
                RequestResult fetchResult = fetchRequest.Perform( fetchOptions );

                if( !fetchResult.m_Success )
                {
                    LogError( "[%s] Failed to fetch subreddit (page %i), error: %s", __FUNCTION__, pageNum, fetchResult.m_Error.m_ErrorString.c_str( ) );
                    continue;
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
                LogDebug( "[%s] Response: %s", __FUNCTION__, fetchResult.m_Response.c_str( ) );

                ++pageNum;
            }
            while( !m_AfterParam.empty( ) && static_cast< int >( mediaUrls.size( ) ) < options.m_RedditMaxMediaItems );

            if( mediaUrls.empty( ) )
            {
                WarningLog( "[%s] No content to download, nothing was done...", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            InfoLog( "[%s] All Subreddit data fetched successfully.", __FUNCTION__ );

            // Create download directory
            const std::filesystem::path dir = std::filesystem::path( m_OutputDir ) / "Downloads" / "Reddit" / options.m_SubredditName;
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
                if( !newUrl.empty( ) )
                {
                    url = newUrl;
                }

                std::vector<char> buffer{ };

                DownloadOptions dlOptions{ };
                dlOptions.m_CaBundle  = m_CaBundle;
                dlOptions.m_Url       = url;
                dlOptions.m_UserAgent = m_UserAgent;
                dlOptions.m_BufferPtr = &buffer;

                DownloadRequest request{ m_Sink };
                RequestResult result = request.Perform( dlOptions );
                if( !result.m_Success )
                {
                    LogError( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), url.c_str( ) );
                    continue;
                }

                const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( dlOptions.m_Url );
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

                ++filesDownloaded;

                m_Sink->OnTotalDownloadProgress( filesDownloaded, totalDownloads );

                InfoLog( "[%s] (%i/%i) Download complete: %s", __FUNCTION__, filesDownloaded, totalDownloads, filepath.c_str( ) );
            }

            TaskManager::Instance( ).SubmitMain( onComplete, filesDownloaded );
        } );

    ( void )task;
}

bool ImageScraper::RedditService::TryPerformAppOnlyAuth( )
{
    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, clientSecret );

    RequestOptions authOptions{ };
    authOptions.m_CaBundle    = m_CaBundle;
    authOptions.m_UserAgent   = m_UserAgent;
    authOptions.m_ClientId    = clientId;
    authOptions.m_ClientSecret = clientSecret;

    AppOnlyAuthRequest authRequest{ };
    RequestResult authResult = authRequest.Perform( authOptions );

    if( !authResult.m_Success )
    {
        LogError( "[%s] Failed to authenticate with Reddit API, error: %s", __FUNCTION__, authResult.m_Error.m_ErrorString );
        return false;
    }

    const Json authResponse = Json::parse( authResult.m_Response );

    auto tokenData = RedditUtils::ParseAccessToken( authResponse );
    if( !tokenData.has_value( ) )
    {
        LogError( "[%s] Reddit app-only authentication failed - could not parse access token!", __FUNCTION__ );
        return false;
    }

    m_OAuthClient->StoreAccessToken( tokenData->m_Token, tokenData->m_ExpireSeconds );

    InfoLog( "[%s] Reddit authenticated successfully (app-only)!", __FUNCTION__ );
    return true;
}

std::vector<std::string> ImageScraper::RedditService::GetMediaUrls( const Json& response )
{
    RedditUtils::MediaUrlsData result = RedditUtils::GetMediaUrls( response );
    m_AfterParam = result.m_AfterParam;
    return result.m_Urls;
}
