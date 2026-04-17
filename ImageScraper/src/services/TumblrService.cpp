#include "services/TumblrService.h"
#include "io/JsonFile.h"
#include "log/Logger.h"
#include "async/TaskManager.h"
#include "requests/RequestTypes.h"
#include "requests/tumblr/RetrievePublishedPostsRequest.h"
#include "requests/tumblr/TumblrFetchAccessTokenRequest.h"
#include "requests/tumblr/TumblrRefreshAccessTokenRequest.h"
#include "requests/tumblr/TumblrGetCurrentUserRequest.h"
#include "utils/DownloadUtils.h"
#include "utils/TumblrUtils.h"
#include "utils/StringUtils.h"
#include "requests/DownloadRequestTypes.h"
#include "requests/DownloadRequest.h"

#include <string>
#include <mutex>

#include <Shellapi.h>

using namespace ImageScraper::Tumblr;
using Json = nlohmann::json;

const std::string ImageScraper::TumblrService::s_RedirectUrl            = "http://localhost:8080";
const std::string ImageScraper::TumblrService::s_UserDataKey_ClientId   = "tumblr_api_key";
const std::string ImageScraper::TumblrService::s_UserDataKey_ClientSecret = "tumblr_client_secret";
const std::string ImageScraper::TumblrService::s_AppDataKey_RefreshToken = "tumblr_refresh_token";
const std::string ImageScraper::TumblrService::s_AppDataKey_StateId     = "tumblr_state_id";
const std::string ImageScraper::TumblrService::s_UserDataKey_ApiKey     = "tumblr_api_key";

ImageScraper::TumblrService::TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : Service( ContentProvider::Tumblr, appConfig, userConfig, caBundle, outputDir, sink )
{
    if( !m_AppConfig->GetValue<std::string>( s_AppDataKey_StateId, m_StateId ) || m_StateId.empty( ) )
    {
        m_StateId = StringUtils::CreateGuid( 30 );
        m_AppConfig->SetValue<std::string>( s_AppDataKey_StateId, m_StateId );
        m_AppConfig->Serialise( );
        LogDebug( "[%s] Created new Tumblr state id: %s", __FUNCTION__, m_StateId.c_str( ) );
    }

    {
        std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
        if( m_AppConfig->GetValue<std::string>( s_AppDataKey_RefreshToken, m_RefreshToken ) && !m_RefreshToken.empty( ) )
        {
            LogDebug( "[%s] Tumblr refresh token found!", __FUNCTION__ );
        }
        else
        {
            LogDebug( "[%s] No Tumblr refresh token found.", __FUNCTION__ );
        }
    }
}

bool ImageScraper::TumblrService::HasRequiredCredentials( ) const
{
    // Consumer Key (api_key) is sufficient for anonymous downloads.
    // Consumer Secret is only needed for OAuth sign-in.
    std::string clientId;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
    return !clientId.empty( );
}

bool ImageScraper::TumblrService::HasSignInCredentials( ) const
{
    // Sign-in requires both Consumer Key and Consumer Secret.
    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, clientSecret );
    return !clientId.empty( ) && !clientSecret.empty( );
}

bool ImageScraper::TumblrService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Tumblr )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::TumblrService::OpenExternalAuth( )
{
    std::string clientId;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );

    std::wstring wurl = L"https://www.tumblr.com/oauth2/authorize";
    wurl += L"?client_id=" + StringUtils::Utf8ToWideString( clientId, false );
    wurl += L"&response_type=code";
    wurl += L"&scope=basic";
    wurl += L"&redirect_uri=" + StringUtils::Utf8ToWideString( s_RedirectUrl, false );
    wurl += L"&state=" + StringUtils::Utf8ToWideString( m_StateId, false );

    if( ShellExecute( NULL, L"open", wurl.c_str( ), NULL, NULL, SW_SHOWNORMAL ) )
    {
        const std::string url = StringUtils::WideStringToUtf8String( wurl, true );
        InfoLog( "[%s] Opened Tumblr external auth in browser!", __FUNCTION__ );
        LogDebug( "[%s] External auth url: %s", __FUNCTION__, url.c_str( ) );
        return true;
    }

    LogError( "[%s] Could not open default browser, make sure a default browser is set in OS settings!", __FUNCTION__ );
    return false;
}

bool ImageScraper::TumblrService::HandleExternalAuth( const std::string& response )
{
    const int signingInProvider = m_Sink->GetSigningInProvider( );
    if( signingInProvider == INVALID_CONTENT_PROVIDER )
    {
        LogError( "[%s] TumblrService::HandleExternalAuth skipped, no signing in provider!", __FUNCTION__ );
        return false;
    }

    if( static_cast< ContentProvider >( signingInProvider ) != m_ContentProvider )
    {
        LogDebug( "[%s] TumblrService::HandleExternalAuth skipped, incorrect provider!", __FUNCTION__ );
        return false;
    }

    const std::string errorKey = "error";
    std::size_t errorStart = response.find( errorKey );
    if( errorStart != std::string::npos )
    {
        LogDebug( "[%s] TumblrService::HandleExternalAuth failed, response contained error!", __FUNCTION__ );
        return false;
    }

    if( response.find( "favicon" ) != std::string::npos )
    {
        LogDebug( "[%s] TumblrService::HandleExternalAuth skipped, invalid message!", __FUNCTION__ );
        return false;
    }

    const std::string codeKey = "code=";
    const int codeKeyLength = static_cast< int >( codeKey.length( ) );
    std::size_t codeStart = response.find( codeKey );
    if( codeStart == std::string::npos )
    {
        LogDebug( "[%s] TumblrService::HandleExternalAuth failed, could not find auth code start!", __FUNCTION__ );
        return false;
    }

    codeStart += codeKeyLength;

    // Use both '&' and ' ' as terminators - take the minimum valid position
    const std::size_t ampPos   = response.find( "&", codeStart );
    const std::size_t spacePos = response.find( " ", codeStart );
    const std::size_t codeEnd  = (std::min)( ampPos, spacePos );

    if( codeEnd == std::string::npos )
    {
        LogDebug( "[%s] TumblrService::HandleExternalAuth failed, could not find auth code end!", __FUNCTION__ );
        return false;
    }

    const std::string authCode = response.substr( codeStart, codeEnd - codeStart );
    InfoLog( "[%s] Tumblr auth code received!", __FUNCTION__ );
    LogDebug( "[%s] Auth code: %s", __FUNCTION__, authCode.c_str( ) );

    FetchAccessToken( authCode );
    return true;
}

bool ImageScraper::TumblrService::IsSignedIn( ) const
{
    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    return !m_RefreshToken.empty( );
}

std::string ImageScraper::TumblrService::GetSignedInUser( ) const
{
    std::unique_lock<std::mutex> lock( m_UsernameMutex );
    return m_Username;
}

void ImageScraper::TumblrService::Authenticate( AuthenticateCallback callback )
{
    auto onComplete = [ this, completeCallback = callback ]( )
    {
        completeCallback( m_ContentProvider, true );
        LogDebug( "[%s] Tumblr authenticated successfully!", __FUNCTION__ );
    };

    auto onFail = [ this, completeCallback = callback ]( )
    {
        completeCallback( m_ContentProvider, false );
        LogDebug( "[%s] Tumblr authentication failed.", __FUNCTION__ );
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

    ( void )task;
}

bool ImageScraper::TumblrService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

void ImageScraper::TumblrService::SignOut( )
{
    ClearAccessToken( );
    ClearRefreshToken( );
    {
        std::unique_lock<std::mutex> lock( m_UsernameMutex );
        m_Username.clear( );
    }

    InfoLog( "[%s] Tumblr signed out.", __FUNCTION__ );
}

void ImageScraper::TumblrService::FetchCurrentUser( )
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

    TumblrGetCurrentUserRequest request{ };
    RequestResult result = request.Perform( options );

    if( !result.m_Success )
    {
        WarningLog( "[%s] Failed to fetch Tumblr username: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return;
    }

    try
    {
        const Json response = Json::parse( result.m_Response );
        if( !response.contains( "response" ) ||
            !response[ "response" ].contains( "user" ) ||
            !response[ "response" ][ "user" ].contains( "name" ) )
        {
            WarningLog( "[%s] Tumblr user/info response missing expected fields.", __FUNCTION__ );
            return;
        }

        {
            std::unique_lock<std::mutex> lock( m_UsernameMutex );
            m_Username = response[ "response" ][ "user" ][ "name" ].get<std::string>( );
        }

        InfoLog( "[%s] Tumblr signed in as: %s", __FUNCTION__, m_Username.c_str( ) );
    }
    catch( const Json::exception& ex )
    {
        WarningLog( "[%s] Failed to parse Tumblr user/info response: %s", __FUNCTION__, ex.what( ) );
    }
}

bool ImageScraper::TumblrService::IsAuthenticated( ) const
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

void ImageScraper::TumblrService::FetchAccessToken( const std::string& authCode )
{
    auto onComplete = [ this ]( )
    {
        FetchCurrentUser( );
        m_Sink->OnSignInComplete( ContentProvider::Tumblr );
        InfoLog( "[%s] Tumblr signed in successfully!", __FUNCTION__ );
    };

    auto onFail = [ this ]( const std::string error )
    {
        m_Sink->OnSignInComplete( ContentProvider::Tumblr );
        LogError( "[%s] Tumblr sign in failed, error: %s", __FUNCTION__, error.c_str( ) );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, authCode, onComplete, onFail ]( )
        {
            InfoLog( "[%s] Started Tumblr OAuth process!", __FUNCTION__ );
            LogDebug( "[%s] authCode: %s", __FUNCTION__, authCode.c_str( ) );

            std::string clientId;
            std::string clientSecret;
            m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
            m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, clientSecret );

            RequestOptions fetchOptions{ };
            fetchOptions.m_ClientId     = clientId;
            fetchOptions.m_ClientSecret = clientSecret;
            fetchOptions.m_CaBundle     = m_CaBundle;
            fetchOptions.m_UserAgent    = m_UserAgent;
            fetchOptions.m_QueryParams.push_back( { "code", authCode } );
            fetchOptions.m_QueryParams.push_back( { "redirect_uri", s_RedirectUrl } );

            TumblrFetchAccessTokenRequest fetchRequest{ };
            RequestResult fetchResult = fetchRequest.Perform( fetchOptions );

            if( !fetchResult.m_Success )
            {
                TaskManager::Instance( ).SubmitMain( onFail, fetchResult.m_Error.m_ErrorString );
                return;
            }

            const Json response = Json::parse( fetchResult.m_Response );

            if( !TryParseAccessTokenAndExpiry( response ) )
            {
                TaskManager::Instance( ).SubmitMain( onFail, std::string( "Could not parse access token and expiry" ) );
                return;
            }

            if( !TryParseRefreshToken( response ) )
            {
                TaskManager::Instance( ).SubmitMain( onFail, std::string( "Could not parse refresh token" ) );
                return;
            }

            TaskManager::Instance( ).SubmitMain( onComplete );
        } );

    ( void )task;
}

bool ImageScraper::TumblrService::TryPerformAuthTokenRefresh( )
{
    RequestOptions authOptions{ };

    {
        std::unique_lock<std::mutex> lock{ m_RefreshTokenMutex };

        if( m_RefreshToken.empty( ) )
        {
            LogDebug( "[%s] Tumblr refresh token not found!", __FUNCTION__ );
            return false;
        }

        authOptions.m_QueryParams.push_back( { "refresh_token", m_RefreshToken } );
    }

    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientId, clientId );
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ClientSecret, clientSecret );

    authOptions.m_CaBundle     = m_CaBundle;
    authOptions.m_UserAgent    = m_UserAgent;
    authOptions.m_ClientId     = clientId;
    authOptions.m_ClientSecret = clientSecret;

    TumblrRefreshAccessTokenRequest authRequest{ };
    RequestResult authResult = authRequest.Perform( authOptions );

    if( !authResult.m_Success )
    {
        LogError( "[%s] Failed to refresh Tumblr access token, error: %s", __FUNCTION__, authResult.m_Error.m_ErrorString.c_str( ) );
        ClearRefreshToken( );
        return false;
    }

    const Json authResponse = Json::parse( authResult.m_Response );

    if( !TryParseAccessTokenAndExpiry( authResponse ) )
    {
        LogError( "[%s] Could not parse Tumblr access token or expiry!", __FUNCTION__ );
        return false;
    }

    if( !TryParseRefreshToken( authResponse ) )
    {
        LogError( "[%s] Could not parse Tumblr refresh token!", __FUNCTION__ );
        return false;
    }

    InfoLog( "[%s] Tumblr access token refreshed successfully!", __FUNCTION__ );
    FetchCurrentUser( );
    return true;
}

bool ImageScraper::TumblrService::TryParseAccessTokenAndExpiry( const Json& response )
{
    if( !response.contains( "access_token" ) || !response.contains( "expires_in" ) )
    {
        return false;
    }

    std::string accessToken = response[ "access_token" ].get<std::string>( );
    int expireSeconds       = response[ "expires_in" ].get<int>( );

    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    m_AccessToken = accessToken;
    constexpr int expireDelta = 180; // 3 minutes safety margin
    m_AuthExpireSeconds = std::chrono::seconds{ expireSeconds - expireDelta };
    m_TokenReceived = std::chrono::system_clock::now( );

    LogDebug( "[%s] Tumblr access token parsed.", __FUNCTION__ );
    return true;
}

bool ImageScraper::TumblrService::TryParseRefreshToken( const Json& response )
{
    if( !response.contains( "refresh_token" ) )
    {
        return false;
    }

    std::string refreshToken = response[ "refresh_token" ].get<std::string>( );

    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    m_RefreshToken = refreshToken;
    m_AppConfig->SetValue( s_AppDataKey_RefreshToken, m_RefreshToken );
    m_AppConfig->Serialise( );

    LogDebug( "[%s] Tumblr refresh token parsed and persisted.", __FUNCTION__ );
    return true;
}

void ImageScraper::TumblrService::ClearRefreshToken( )
{
    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    m_RefreshToken.clear( );
    m_AppConfig->SetValue( s_AppDataKey_RefreshToken, m_RefreshToken );
    m_AppConfig->Serialise( );
}

void ImageScraper::TumblrService::ClearAccessToken( )
{
    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    m_AccessToken.clear( );
    m_AuthExpireSeconds = std::chrono::seconds{ 0 };
}

void ImageScraper::TumblrService::DownloadContent( const UserInputOptions& inputOptions )
{
    InfoLog( "[%s] Starting Tumblr media download!", __FUNCTION__ );
    LogDebug( "[%s] User: %s", __FUNCTION__, inputOptions.m_TumblrUser.c_str( ) );

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

            RequestOptions retrievePostsOptions{ };
            retrievePostsOptions.m_UrlExt    = options.m_TumblrUser + ".tumblr.com/posts";
            retrievePostsOptions.m_CaBundle  = m_CaBundle;
            retrievePostsOptions.m_UserAgent = m_UserAgent;

            if( IsAuthenticated( ) )
            {
                std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
                retrievePostsOptions.m_AccessToken = m_AccessToken;
            }
            else if( !m_RefreshToken.empty( ) )
            {
                if( TryPerformAuthTokenRefresh( ) )
                {
                    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
                    retrievePostsOptions.m_AccessToken = m_AccessToken;
                }
                else
                {
                    // Fall through to api_key auth
                    std::string apiKey;
                    m_UserConfig->GetValue<std::string>( s_UserDataKey_ApiKey, apiKey );
                    retrievePostsOptions.m_QueryParams.push_back( { "api_key", apiKey } );
                }
            }
            else
            {
                std::string apiKey;
                m_UserConfig->GetValue<std::string>( s_UserDataKey_ApiKey, apiKey );
                retrievePostsOptions.m_QueryParams.push_back( { "api_key", apiKey } );
            }

            Tumblr::RetrievePublishedPostsRequest retrievePostsRequest{ };
            RequestResult fetchResult = retrievePostsRequest.Perform( retrievePostsOptions );

            if( !fetchResult.m_Success )
            {
                WarningLog( "[%s] Failed to get Tumblr blog post data, error: %s", __FUNCTION__, fetchResult.m_Error.m_ErrorString.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            InfoLog( "[%s] Tumblr posts retrieved successfully.", __FUNCTION__ );
            LogDebug( "[%s] Response: %s", __FUNCTION__, fetchResult.m_Response.c_str( ) );

            Json response = Json::parse( fetchResult.m_Response );
            std::vector<std::string> mediaUrls = GetMediaUrlsFromResponse( response, options.m_TumblrMaxMediaItems );

            if( mediaUrls.empty( ) )
            {
                WarningLog( "[%s] No content to download, nothing was done...", __FUNCTION__ );
                TaskManager::Instance( ).SubmitMain( onComplete, 0 );
                return;
            }

            // Create download directory
            const std::filesystem::path dir = std::filesystem::path( m_OutputDir ) / "Downloads" / "Tumblr" / options.m_TumblrUser;
            const std::string dirStr = dir.generic_string( );
            if( !DownloadHelpers::CreateDir( dirStr ) )
            {
                LogError( "[%s] Failed to create download directory: %s", __FUNCTION__, dir.c_str( ) );
                TaskManager::Instance( ).SubmitMain( onFail );
                return;
            }

            int filesDownloaded = 0;
            const int totalDownloads = static_cast< int >( mediaUrls.size( ) );

            InfoLog( "[%s] Started downloading content, urls: %i", __FUNCTION__, static_cast< int >( mediaUrls.size( ) ) );

            std::this_thread::sleep_for( std::chrono::seconds{ 1 } );

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

                std::this_thread::sleep_for( std::chrono::seconds{ 1 } );
            }

            TaskManager::Instance( ).SubmitMain( onComplete, filesDownloaded );
        } );

    ( void )task;
}

std::vector<std::string> ImageScraper::TumblrService::GetMediaUrlsFromResponse( const Json& response, int maxItems )
{
    return TumblrUtils::GetMediaUrlsFromResponse( response, maxItems );
}
