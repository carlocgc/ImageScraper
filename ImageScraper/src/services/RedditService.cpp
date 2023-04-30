#include "services/RedditService.h"
#include "requests/RequestTypes.h"
#include "requests/reddit/FetchSubredditPostsRequest.h"
#include "requests/reddit/AppOnlyAuthRequest.h"
#include "requests/reddit/FetchAccessTokenRequest.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"
#include "utils/DownloadUtils.h"
#include "log/Logger.h"
#include "async/TaskManager.h"
#include "ui/FrontEnd.h"
#include "io/JsonFile.h"
#include "utils/StringUtils.h"
#include "curlpp/cURLpp.hpp"
#include "requests/reddit/RefreshAccessTokenRequest.h"

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

ImageScraper::RedditService::RedditService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
    : Service( ContentProvider::Reddit, appConfig, userConfig, caBundle, frontEnd )
{
    if( !m_AppConfig->GetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId ) )
    {
        m_DeviceId = StringUtils::CreateGuid( 30 );
        m_AppConfig->SetValue<std::string>( s_AppDataKey_DeviceId, m_DeviceId );
        m_AppConfig->Serialise( );
        DebugLog( "[%s] Created new Reddit device id: %s", __FUNCTION__, m_DeviceId.c_str( ) );
    }

    {
        std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
        if( m_AppConfig->GetValue<std::string>( s_AppDataKey_RefreshToken, m_RefreshToken ) && m_RefreshToken != "" )
        {
            DebugLog( "[%s] Refresh token found!, m_RefreshToken: %s", __FUNCTION__, m_RefreshToken.c_str( ) );
        }
        else
        {
            DebugLog( "[%s] No refresh token found!", __FUNCTION__ );
        }
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

bool ImageScraper::RedditService::OpenExternalAuth( )
{
    std::wstring wurl = L"https://www.reddit.com/api/v1/authorize";
    wurl += L"?client_id=" + StringUtils::Utf8ToWideString( m_ClientId, false );
    wurl += L"&response_type=code";
    wurl += L"&state=" + StringUtils::Utf8ToWideString( m_DeviceId, false );
    wurl += L"&redirect_uri=" + StringUtils::Utf8ToWideString( s_RedirectUrl, false );
    wurl += L"&duration=permanent";
    wurl += L"&scope=identity,read";

    if( ShellExecute( NULL, L"open", wurl.c_str( ), NULL, NULL, SW_SHOWNORMAL ) )
    {
        const std::string url = StringUtils::WideStringToUtf8String( wurl, true );
        InfoLog( "[%s] Opened external auth in browser!", __FUNCTION__ );
        DebugLog( "[%s] External auth url: %s", __FUNCTION__, url.c_str( ) );
        return true;
    }

    ErrorLog( "[%s] Could not open default browser, make sure a default browser is set in OS settings!", __FUNCTION__ );
    return false;
}

bool ImageScraper::RedditService::HandleExternalAuth( const std::string& response )
{
    const int signingInProvider = m_FrontEnd->GetSigningInProvider( );
    if( signingInProvider == INVALID_CONTENT_PROVIDER )
    {
        ErrorLog( "[%s] RedditService::HandleExternalAuth skipped, No signing in provider!", __FUNCTION__ );
        return false;
    }

    if( static_cast< ContentProvider >( signingInProvider ) != m_ContentProvider )
    {
        DebugLog( "[%s] RedditService::HandleExternalAuth skipped, incorrect provider!", __FUNCTION__ );
        return false;
    }

    const std::string errorKey = "error";
    std::size_t errorStart = response.find( errorKey );
    if( errorStart != std::string::npos )
    {
        DebugLog( "[%s] RedditService::HandleExternalAuth failed, response contained error!", __FUNCTION__ );
        return false;
    }

    if( response.find( "favicon" ) != std::string::npos )
    {
        DebugLog( "[%s] RedditService::HandleExternalAuth failed, invalid message!", __FUNCTION__ );
        return false;
    }

    // TODO check state matches device Id

    const std::string codeKey = "code=";
    const int codeKeyLength = static_cast< int >( codeKey.length( ) );
    std::size_t codeStart = response.find( codeKey );
    if( codeStart == std::string::npos )
    {
        DebugLog( "[%s] RedditService::HandleExternalAuth failed, could not find auth code start!", __FUNCTION__ );
        return false;
    }

    codeStart += codeKeyLength;

    const std::size_t codeEnd = response.find( " ", codeStart );
    if( codeEnd == std::string::npos )
    {
        DebugLog( "[%s] RedditService::HandleExternalAuth failed, could not find auth code end!", __FUNCTION__ );
        return false;
    }

    const std::string authCode = response.substr( codeStart, codeEnd - codeStart );
    InfoLog( "[%s] Auth code received!", __FUNCTION__ );
    DebugLog( "[%s] Auth code: %s", __FUNCTION__, authCode.c_str( ) );

    FetchAccessToken( authCode );
    return true;
}

bool ImageScraper::RedditService::IsSignedIn( )
{
    return m_RefreshToken != "";
}

void ImageScraper::RedditService::Authenticate( AuthenticateCallback callback )
{
    auto onComplete = [ &, completeCallback = callback ]( )
    {
        completeCallback( m_ContentProvider, true );
        DebugLog( "[%s] Reddit autheticated successfully!", __FUNCTION__ );
    };

    auto onFail = [ &, completeCallback = callback ]( )
    {
        completeCallback( m_ContentProvider, false );
        DebugLog( "[%s] Reddit authetication failed", __FUNCTION__ );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_NetworkContext, [ &, onComplete, onFail ]( )
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
    return m_FrontEnd->IsCancelled( );
}

const bool ImageScraper::RedditService::IsAuthenticated( ) const
{
    {
        std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
        if( m_AccessToken == "" )
        {
            return false;
        }
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
    auto onComplete = [ & ]( )
    {
        m_FrontEnd->CompleteSignIn( ContentProvider::Reddit );
        InfoLog( "[%s] Reddit signed in successfully!", __FUNCTION__ );
    };

    auto onFail = [ & ]( const std::string error )
    {
        m_FrontEnd->CompleteSignIn( ContentProvider::Reddit );
        ErrorLog( "[%s] Reddit sign in failed, error: %s", __FUNCTION__, error.c_str( ) );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_NetworkContext, [ &, authCode, onComplete, onFail ]( )
        {
            InfoLog( "[%s] Started OAuth process!", __FUNCTION__ );
            DebugLog( "[%s] authCode: %s", __FUNCTION__, authCode.c_str( ) );

            RequestOptions fetchOptions{ };

            fetchOptions.m_QueryParams.push_back( { "code", authCode } );
            fetchOptions.m_QueryParams.push_back( { "redirect_uri", curlpp::escape( s_RedirectUrl ) } );
            fetchOptions.m_CaBundle = m_CaBundle;
            fetchOptions.m_UserAgent = m_UserAgent;
            fetchOptions.m_ClientId = m_ClientId;
            fetchOptions.m_ClientSecret = m_ClientSecret;

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
            InfoLog( "[%s] Starting Reddit media download!", __FUNCTION__ );
            DebugLog( "[%s] Subreddit: %s", __FUNCTION__, options.m_SubredditName.c_str( ) );
            DebugLog( "[%s] Scope: %s", __FUNCTION__, options.m_RedditScope.c_str( ) );
            DebugLog( "[%s] Media Item Limit: %i", __FUNCTION__, options.m_RedditMaxMediaItems );

            if( IsCancelled( ) )
            {
                InfoLog( "[%s] User cancelled operation!", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            if( !IsAuthenticated( ) )
            {
                if( m_RefreshToken != "" )
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
                ErrorLog( "[%s] Could not authenticate with reddit api", __FUNCTION__ );
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
                    ErrorLog( "[%s] Failed to fetch subreddit (page %i), error: %s", __FUNCTION__, pageNum, fetchResult.m_Error.m_ErrorString.c_str( ) );
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
                DebugLog( "[%s] Response: %s", __FUNCTION__, fetchResult.m_Response.c_str( ) );

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

bool ImageScraper::RedditService::TryPerformAppOnlyAuth( )
{
    RequestOptions authOptions{ };
    authOptions.m_CaBundle = m_CaBundle;
    authOptions.m_UserAgent = m_UserAgent;
    authOptions.m_ClientId = m_ClientId;
    authOptions.m_ClientSecret = m_ClientSecret;

    AppOnlyAuthRequest authRequest{ };
    RequestResult authResult = authRequest.Perform( authOptions );

    if( !authResult.m_Success )
    {
        ErrorLog( "[%s] Failed to authenticate with Reddit API, error: %s", __FUNCTION__, authResult.m_Error.m_ErrorString );
        return false;
    }

    const Json authResponse = Json::parse( authResult.m_Response );

    if( !TryParseAccessTokenAndExpiry( authResponse ) )
    {
        ErrorLog( "[%s] Reddit authentication failed!", __FUNCTION__ );
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
            DebugLog( "[%s] Reddit access token not found!", __FUNCTION__ );
            return false;
        }

        authOptions.m_QueryParams.push_back( { "refresh_token", m_RefreshToken } );
    }

    authOptions.m_CaBundle = m_CaBundle;
    authOptions.m_UserAgent = m_UserAgent;
    authOptions.m_ClientId = m_ClientId;
    authOptions.m_ClientSecret = m_ClientSecret;

    RefreshAccessTokenRequest authRequest{ };
    RequestResult authResult = authRequest.Perform( authOptions );

    if( !authResult.m_Success )
    {
        ErrorLog( "[%s] Failed to refresh access token, error: %s", __FUNCTION__, authResult.m_Error.m_ErrorString.c_str( ) );
        return false;
    }

    const Json authResponse = Json::parse( authResult.m_Response );

    if( !TryParseAccessTokenAndExpiry( authResponse ) )
    {
        ErrorLog( "[%s] Could not parse access token or expiry!", __FUNCTION__ );
        return false;
    }

    if( !TryParseRefreshToken( authResponse ) )
    {
        ErrorLog( "[%s] Could not parse refresh token!", __FUNCTION__ );
        return false;
    }

    InfoLog( "[%s] Reddit access token refreshed successfully!", __FUNCTION__ );
    return true;
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

        for( const auto& post : children )
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
            m_AfterParam.clear( );
        }
    }
    catch( const Json::exception& e )
    {
        const std::string error = e.what( );
        ErrorLog( "[%s] GetMediaUrls Error parsing data!, error: %s", __FUNCTION__, error.c_str( ) );
    }

    return mediaUrls;
}

bool ImageScraper::RedditService::TryParseAccessTokenAndExpiry( const Json& response )
{
    if( !response.contains( "access_token" ) )
    {
        ErrorLog( "[%s] Response did not contain access token!", __FUNCTION__ );
        return false;
    }

    if( !response.contains( "expires_in" ) )
    {
        ErrorLog( "[%s] Response did not contain token expire seconds!", __FUNCTION__ );
        return false;
    }

    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );

    try
    {
        m_AccessToken = response[ "access_token" ];

        const int expireSeconds = response[ "expires_in" ];
        const int expireDelta = 180; // 3 minutes
        m_AuthExpireSeconds = std::chrono::seconds{ expireSeconds - expireDelta };
        m_TokenReceived = std::chrono::system_clock::now( );
    }
    catch( const Json::exception& ex )
    {
        std::string error = ex.what( );
        ErrorLog( "[%s] could not parse access token response, error: %s", __FUNCTION__, error.c_str( ) );
        return false;
    }

    DebugLog( "[%s] Reddit access token: %s", __FUNCTION__, m_AccessToken.c_str( ) );
    DebugLog( "[%s] Reddit access token expires in %i seconds!", __FUNCTION__, m_AuthExpireSeconds );
    return true;
}

bool ImageScraper::RedditService::TryParseRefreshToken( const Json& response )
{
    if( !response.contains( "refresh_token" ) )
    {
        ErrorLog( "[%s] Response did not contain refresh token!", __FUNCTION__ );
        return false;
    }

    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );

    try
    {
        m_RefreshToken = response[ "refresh_token" ];
    }
    catch( const Json::exception& ex )
    {
        std::string error = ex.what( );
        ErrorLog( "[%s] could not parse refresh token response, error: %s", __FUNCTION__, error.c_str( ) );
        return false;
    }

    m_AppConfig->SetValue( s_AppDataKey_RefreshToken, m_RefreshToken );
    m_AppConfig->Serialise( );

    DebugLog( "[%s] Reddit access token: %s", __FUNCTION__, m_RefreshToken.c_str( ) );
    return true;
}

void ImageScraper::RedditService::ClearRefreshToken( )
{
    m_RefreshToken.clear( );
    m_AppConfig->SetValue( s_AppDataKey_RefreshToken, m_RefreshToken );
    m_AppConfig->Serialise( );
}

void ImageScraper::RedditService::ClearAccessToken( )
{
    m_AccessToken.clear( );
    m_AuthExpireSeconds = std::chrono::seconds{ 0 };
}
