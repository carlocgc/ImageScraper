#include "services/RedditService.h"
#include "services/OAuthServiceHelpers.h"
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

using namespace ImageScraper::Reddit;
using Json = nlohmann::json;

namespace
{
    const char* GetRedditTargetTypeLabel( const ImageScraper::RedditTargetType targetType )
    {
        switch( targetType )
        {
            case ImageScraper::RedditTargetType::Subreddit:
                return "Subreddit";
            case ImageScraper::RedditTargetType::User:
                return "User";
            default:
                return "Unknown";
        }
    }

    std::string BuildRedditListingPath( const ImageScraper::UserInputOptions& options )
    {
        if( options.m_RedditTargetType == ImageScraper::RedditTargetType::User )
        {
            return "user/" + options.m_RedditTargetName + "/submitted.json";
        }

        return "r/" + options.m_RedditTargetName + '/' + options.m_RedditScope + ".json";
    }

    std::filesystem::path BuildRedditDownloadDirectory( const std::string& outputDir, const ImageScraper::UserInputOptions& options )
    {
        const char* targetTypeDirectory = options.m_RedditTargetType == ImageScraper::RedditTargetType::User
            ? "User"
            : "Subreddit";

        return std::filesystem::path( outputDir ) / "Downloads" / "Reddit" / targetTypeDirectory / options.m_RedditTargetName;
    }
}

const std::string ImageScraper::RedditService::s_RedirectUrl              = "http://localhost:8080";
const std::string ImageScraper::RedditService::s_AppDataKey_DeviceId      = "reddit_device_id";
const std::string ImageScraper::RedditService::s_AppDataKey_RefreshToken  = "reddit_refresh_token";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientId     = "reddit_client_id";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientSecret = "reddit_client_secret";

ImageScraper::RedditService::RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink, std::shared_ptr<IUrlResolver> urlResolver )
    : Service( ContentProvider::Reddit, appConfig, userConfig, caBundle, outputDir, sink, std::move( urlResolver ) )
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

    auto fetchFn = [ httpClient = m_HttpClient ]( const RequestOptions& opts )
        {
            return FetchAccessTokenRequest{ httpClient }.Perform( opts );
        };

    auto refreshFn = [ httpClient = m_HttpClient ]( const RequestOptions& opts )
        {
            return RefreshAccessTokenRequest{ httpClient }.Perform( opts );
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
    return OAuthServiceHelpers::OpenExternalAuth( *m_OAuthClient );
}

bool ImageScraper::RedditService::HandleExternalAuth( const std::string& response )
{
    return OAuthServiceHelpers::HandleExternalAuth( m_ContentProvider, GetProviderDisplayName( ), m_Sink, *m_OAuthClient, response, [ this ]( )
        {
            FetchCurrentUser( );
        } );
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
    OAuthServiceHelpers::Authenticate( m_ContentProvider, GetProviderDisplayName( ), *m_OAuthClient, callback, [ this ]( )
        {
            FetchCurrentUser( );
        } );
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
    OAuthServiceHelpers::SignOut( GetProviderDisplayName( ), *m_OAuthClient, [ this ]( )
        {
            std::unique_lock<std::mutex> lock( m_UsernameMutex );
            m_Username.clear( );
        } );
}

void ImageScraper::RedditService::FetchCurrentUser( )
{
    RequestOptions options{ };
    options.m_CaBundle    = m_CaBundle;
    options.m_UserAgent   = m_UserAgent;
    options.m_AccessToken = m_OAuthClient->GetAccessToken( );

    GetCurrentUserRequest request{ m_HttpClient };
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
            LogDebug( "[%s] Target Type: %s", __FUNCTION__, GetRedditTargetTypeLabel( options.m_RedditTargetType ) );
            LogDebug( "[%s] Target: %s", __FUNCTION__, options.m_RedditTargetName.c_str( ) );
            if( options.m_RedditTargetType == RedditTargetType::Subreddit )
            {
                LogDebug( "[%s] Scope: %s", __FUNCTION__, options.m_RedditScope.c_str( ) );
            }
            LogDebug( "[%s] Media Item Limit: %i", __FUNCTION__, options.m_RedditMaxMediaItems );

            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            const bool useAuthenticatedListing = options.m_RedditTargetType == RedditTargetType::Subreddit;

            if( useAuthenticatedListing && !m_OAuthClient->IsAuthenticated( ) )
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

            if( useAuthenticatedListing && !m_OAuthClient->IsAuthenticated( ) )
            {
                LogError( "[%s] Could not authenticate with reddit api", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            m_AfterParam.clear( );

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

                if( options.m_RedditTargetType == RedditTargetType::Subreddit
                    && !options.m_RedditScopeTimeFrame.empty( ) )
                {
                    fetchOptions.m_QueryParams.push_back( { "t", options.m_RedditScopeTimeFrame } );
                }

                if( !m_AfterParam.empty( ) )
                {
                    fetchOptions.m_QueryParams.push_back( { "after", m_AfterParam } );
                }

                fetchOptions.m_QueryParams.push_back( { "limit", "100" } );
                fetchOptions.m_UrlExt = BuildRedditListingPath( options );
                fetchOptions.m_CaBundle = m_CaBundle;
                fetchOptions.m_UserAgent = m_UserAgent;
                if( options.m_RedditTargetType == RedditTargetType::Subreddit )
                {
                    fetchOptions.m_AccessToken = m_OAuthClient->GetAccessToken( );
                }

                FetchSubredditPostsRequest fetchRequest{ m_HttpClient };
                RequestResult fetchResult = fetchRequest.Perform( fetchOptions );

                if( !fetchResult.m_Success )
                {
                    LogError( "[%s] Failed to fetch Reddit listing (page %i), error: %s", __FUNCTION__, pageNum, fetchResult.m_Error.m_ErrorString.c_str( ) );
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

                InfoLog( "[%s] Reddit listing (page %i) fetched successfully. Media urls queued: %i/%i", __FUNCTION__, pageNum, static_cast< int >( mediaUrls.size( ) ), options.m_RedditMaxMediaItems );
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

            InfoLog( "[%s] All Reddit listing data fetched successfully.", __FUNCTION__ );

            // Create download directory
            const std::filesystem::path dir = BuildRedditDownloadDirectory( m_OutputDir, options );
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

    AppOnlyAuthRequest authRequest{ m_HttpClient };
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
