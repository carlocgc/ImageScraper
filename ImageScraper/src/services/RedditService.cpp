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
#include "curlpp/cURLpp.hpp"
#include "requests/reddit/RefreshAccessTokenRequest.h"
#include "requests/reddit/RevokeAccessTokenRequest.h"
#include "requests/reddit/GetCurrentUserRequest.h"

#include <string>
#include <map>
#include <mutex>

#include <Shellapi.h>

using namespace ImageScraper::Reddit;
using Json = nlohmann::json;

const std::string ImageScraper::RedditService::s_RedirectUrl = "http://localhost:8080";
const std::string ImageScraper::RedditService::s_AppDataKey_DeviceId = "reddit_device_id";
const std::string ImageScraper::RedditService::s_AppDataKey_RefreshToken = "reddit_refresh_token";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientId = "reddit_client_id";
const std::string ImageScraper::RedditService::s_UserDataKey_ClientSecret = "reddit_client_secret";

ImageScraper::RedditService::RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : Service( ContentProvider::Reddit, appConfig, userConfig, caBundle, outputDir, sink )
{
    if( !m_AppConfig->GetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId ) )
    {
        m_DeviceId = StringUtils::CreateGuid( 30 );
        m_AppConfig->SetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId );
        m_AppConfig->Serialise( );
        LogDebug( "[%s] Created new Reddit device id: %s", __FUNCTION__, m_DeviceId.c_str( ) );
    }

    {
        std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
        if( m_AppConfig->GetValue<std::string>( s_AppDataKey_RefreshToken, m_RefreshToken ) && !m_RefreshToken.empty( ) )
        {
            LogDebug( "[%s] Refresh token found!, m_RefreshToken: %s", __FUNCTION__, m_RefreshToken.c_str( ) );
        }
        else
        {
            LogDebug( "[%s] No refresh token found!", __FUNCTION__ );
        }
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
    std::string clientId;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );

    std::wstring wurl = L"https://www.reddit.com/api/v1/authorize";
    wurl += L"?client_id=" + StringUtils::Utf8ToWideString( clientId, false );
    wurl += L"&response_type=code";
    wurl += L"&state=" + StringUtils::Utf8ToWideString( m_DeviceId, false );
    wurl += L"&redirect_uri=" + StringUtils::Utf8ToWideString( s_RedirectUrl, false );
    wurl += L"&duration=permanent";
    wurl += L"&scope=identity,read";

    if( ShellExecute( NULL, L"open", wurl.c_str( ), NULL, NULL, SW_SHOWNORMAL ) )
    {
        const std::string url = StringUtils::WideStringToUtf8String( wurl, true );
        InfoLog( "[%s] Opened external auth in browser!", __FUNCTION__ );
        LogDebug( "[%s] External auth url: %s", __FUNCTION__, url.c_str( ) );
        return true;
    }

    LogError( "[%s] Could not open default browser, make sure a default browser is set in OS settings!", __FUNCTION__ );
    return false;
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

    const bool hasError = response.find( "?error=" ) != std::string::npos
                       || response.find( "&error=" ) != std::string::npos;

    if( hasError )
    {
        const std::string error = StringUtils::ExtractQueryParam( response, "error" );
        const std::string desc  = StringUtils::UrlDecode( StringUtils::ExtractQueryParam( response, "error_description" ) );
        WarningLog( "[%s] Reddit OAuth error: %s - %s", __FUNCTION__, error.c_str( ), desc.c_str( ) );
        if( error == "redirect_uri_mismatch" )
        {
            WarningLog( "[%s] Ensure the redirect URI in your Reddit app settings is set to: %s", __FUNCTION__, s_RedirectUrl.c_str( ) );
        }
        m_Sink->OnSignInComplete( m_ContentProvider );
        return false;
    }

    const std::string receivedState = StringUtils::ExtractQueryParam( response, "state" );
    if( receivedState.empty( ) || receivedState != m_DeviceId )
    {
        LogError( "[%s] Reddit OAuth state mismatch - possible CSRF attack. Expected: %s, Received: %s",
                  __FUNCTION__, m_DeviceId.c_str( ), receivedState.c_str( ) );
        m_Sink->OnSignInComplete( m_ContentProvider );
        return false;
    }

    const std::string authCode = StringUtils::ExtractQueryParam( response, "code" );
    if( authCode.empty( ) )
    {
        LogDebug( "[%s] RedditService::HandleExternalAuth failed, could not find auth code!", __FUNCTION__ );
        return false;
    }

    InfoLog( "[%s] Auth code received!", __FUNCTION__ );
    LogDebug( "[%s] Auth code: %s", __FUNCTION__, authCode.c_str( ) );

    FetchAccessToken( authCode );
    return true;
}

bool ImageScraper::RedditService::IsSignedIn( ) const
{
    // True only when the user has signed in via OAuth - refresh token is the
    // indicator. App-only auth populates m_AccessToken but not m_RefreshToken,
    // so checking access token alone would flicker the button during downloads.
    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    return !m_RefreshToken.empty( );
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
        LogDebug( "[%s] Reddit autheticated successfully!", __FUNCTION__ );
    };

    auto onFail = [ this, completeCallback = callback ]( )
    {
        completeCallback( m_ContentProvider, false );
        LogDebug( "[%s] Reddit authetication failed", __FUNCTION__ );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, onComplete, onFail ]( )
        {
            if( TryPerformAuthTokenRefresh( ) )
            {
                onComplete( );
            }
            else
            {
                onFail( );
            }
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
    ClearAccessToken( );
    ClearRefreshToken( );
    {
        std::unique_lock<std::mutex> lock( m_UsernameMutex );
        m_Username.clear( );
    }

    InfoLog( "[%s] Reddit signed out.", __FUNCTION__ );
}

void ImageScraper::RedditService::FetchCurrentUser( )
{
    std::string accessToken{ };
    {
        std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
        accessToken = m_AccessToken;
    }

    RequestOptions options{ };
    options.m_CaBundle    = m_CaBundle;
    options.m_UserAgent   = m_UserAgent;
    options.m_AccessToken = accessToken;

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

const bool ImageScraper::RedditService::IsAuthenticated( ) const
{
    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );

    if( m_AccessToken.empty( ) )
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

void ImageScraper::RedditService::FetchAccessToken( const std::string& authCode )
{
    auto onComplete = [ this ]( )
    {
        FetchCurrentUser( );
        m_Sink->OnSignInComplete( ContentProvider::Reddit );
        InfoLog( "[%s] Reddit signed in successfully!", __FUNCTION__ );
    };

    auto onFail = [ this ]( const std::string error )
    {
        m_Sink->OnSignInComplete( ContentProvider::Reddit );
        LogError( "[%s] Reddit sign in failed, error: %s", __FUNCTION__, error.c_str( ) );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, authCode, onComplete, onFail ]( )
        {
            InfoLog( "[%s] Started OAuth process!", __FUNCTION__ );
            LogDebug( "[%s] authCode: %s", __FUNCTION__, authCode.c_str( ) );

            RequestOptions fetchOptions{ };

            std::string clientId;
            std::string clientSecret;
            m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
            m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, clientSecret );

            fetchOptions.m_QueryParams.push_back( { "code", authCode } );
            fetchOptions.m_QueryParams.push_back( { "redirect_uri", curlpp::escape( s_RedirectUrl ) } );
            fetchOptions.m_CaBundle = m_CaBundle;
            fetchOptions.m_UserAgent = m_UserAgent;
            fetchOptions.m_ClientId = clientId;
            fetchOptions.m_ClientSecret = clientSecret;

            FetchAccessTokenRequest fetchRequest{ };
            RequestResult fetchResult = fetchRequest.Perform( fetchOptions );

            if( !fetchResult.m_Success )
            {
                TaskManager::Instance( ).SubmitMain( onFail, fetchResult.m_Error.m_ErrorString );
                return;
            }

            const Json response = Json::parse( fetchResult.m_Response );

            if( !TryParseAccessTokenAndExpiry( response ) )
            {
                TaskManager::Instance( ).SubmitMain( onFail, "Could not parse access token and expiry" );
                return;
            }

            if( !TryParseRefreshToken( response ) )
            {
                TaskManager::Instance( ).SubmitMain( onFail, "Could not parse refresh token" );
                return;
            }

            TaskManager::Instance( ).SubmitMain( onComplete );
        } );
    ( void )task;
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

            if( !IsAuthenticated( ) )
            {
                if( !m_RefreshToken.empty( ) )
                {
                    if( !TryPerformAuthTokenRefresh( ) )
                    {
                        ClearRefreshToken( );
                    }
                }
                else
                {
                    TryPerformAppOnlyAuth( );
                }
            }

            if( !IsAuthenticated( ) )
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
                fetchOptions.m_UrlExt = options.m_SubredditName + '/' + options.m_RedditScope + ".json";
                fetchOptions.m_CaBundle = m_CaBundle;
                fetchOptions.m_UserAgent = m_UserAgent;
                {
                    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
                    fetchOptions.m_AccessToken = m_AccessToken;
                }

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

                if( !newUrl.empty( ) )
                {
                    url = newUrl;
                }

                std::vector<char> buffer{ };

                DownloadOptions options{ };
                options.m_CaBundle = m_CaBundle;
                options.m_Url = url;
                options.m_UserAgent = m_UserAgent;
                options.m_BufferPtr = &buffer;

                DownloadRequest request{ m_Sink };
                RequestResult result = request.Perform( options );
                if( !result.m_Success )
                {
                    LogError( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), url.c_str( ) );
                    continue;
                }

                const std::string filename = DownloadHelpers::ExtractFileNameAndExtFromUrl( options.m_Url );
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
    authOptions.m_CaBundle = m_CaBundle;
    authOptions.m_UserAgent = m_UserAgent;
    authOptions.m_ClientId = clientId;
    authOptions.m_ClientSecret = clientSecret;

    AppOnlyAuthRequest authRequest{ };
    RequestResult authResult = authRequest.Perform( authOptions );

    if( !authResult.m_Success )
    {
        LogError( "[%s] Failed to authenticate with Reddit API, error: %s", __FUNCTION__, authResult.m_Error.m_ErrorString );
        return false;
    }

    const Json authResponse = Json::parse( authResult.m_Response );

    if( !TryParseAccessTokenAndExpiry( authResponse ) )
    {
        LogError( "[%s] Reddit authentication failed!", __FUNCTION__ );
        return false;
    }

    InfoLog( "[%s] Reddit authenticated successfully!", __FUNCTION__ );
    return true;
}

bool ImageScraper::RedditService::TryPerformAuthTokenRefresh( )
{
    RequestOptions authOptions{ };

    {
        std::unique_lock<std::mutex> lock{ m_RefreshTokenMutex };

        if( m_RefreshToken.empty( ) )
        {
            LogDebug( "[%s] Reddit access token not found!", __FUNCTION__ );
            return false;
        }

        authOptions.m_QueryParams.push_back( { "refresh_token", m_RefreshToken } );
    }

    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, clientSecret );

    authOptions.m_CaBundle = m_CaBundle;
    authOptions.m_UserAgent = m_UserAgent;
    authOptions.m_ClientId = clientId;
    authOptions.m_ClientSecret = clientSecret;

    RefreshAccessTokenRequest authRequest{ };
    RequestResult authResult = authRequest.Perform( authOptions );

    if( !authResult.m_Success )
    {
        LogError( "[%s] Failed to refresh access token, error: %s", __FUNCTION__, authResult.m_Error.m_ErrorString.c_str( ) );
        ClearRefreshToken( );
        return false;
    }

    const Json authResponse = Json::parse( authResult.m_Response );

    if( !TryParseAccessTokenAndExpiry( authResponse ) )
    {
        LogError( "[%s] Could not parse access token or expiry!", __FUNCTION__ );
        return false;
    }

    if( !TryParseRefreshToken( authResponse ) )
    {
        LogError( "[%s] Could not parse refresh token!", __FUNCTION__ );
        return false;
    }

    InfoLog( "[%s] Reddit access token refreshed successfully!", __FUNCTION__ );
    FetchCurrentUser( );
    return true;
}

std::vector<std::string> ImageScraper::RedditService::GetMediaUrls( const Json& response )
{
    RedditUtils::MediaUrlsData result = RedditUtils::GetMediaUrls( response );
    m_AfterParam = result.m_AfterParam;
    return result.m_Urls;
}

bool ImageScraper::RedditService::TryParseAccessTokenAndExpiry( const Json& response )
{
    auto result = RedditUtils::ParseAccessToken( response );
    if( !result.has_value( ) )
    {
        return false;
    }

    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    m_AccessToken = result->m_Token;
    const int expireDelta = 180; // 3 minutes
    m_AuthExpireSeconds = std::chrono::seconds{ result->m_ExpireSeconds - expireDelta };
    m_TokenReceived = std::chrono::system_clock::now( );

    LogDebug( "[%s] Reddit access token: %s", __FUNCTION__, m_AccessToken.c_str( ) );
    return true;
}

bool ImageScraper::RedditService::TryParseRefreshToken( const Json& response )
{
    auto result = RedditUtils::ParseRefreshToken( response );
    if( !result.has_value( ) )
    {
        return false;
    }

    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    m_RefreshToken = result.value( );
    m_AppConfig->SetValue( s_AppDataKey_RefreshToken, m_RefreshToken );
    m_AppConfig->Serialise( );

    LogDebug( "[%s] Reddit refresh token: %s", __FUNCTION__, m_RefreshToken.c_str( ) );
    return true;
}

void ImageScraper::RedditService::ClearRefreshToken( )
{
    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    m_RefreshToken.clear( );
    m_AppConfig->SetValue( s_AppDataKey_RefreshToken, m_RefreshToken );
    m_AppConfig->Serialise( );
}

void ImageScraper::RedditService::ClearAccessToken( )
{
    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    m_AccessToken.clear( );
    m_AuthExpireSeconds = std::chrono::seconds{ 0 };
}
