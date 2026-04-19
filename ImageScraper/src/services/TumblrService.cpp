#include "services/TumblrService.h"
#include "services/OAuthServiceHelpers.h"
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

using namespace ImageScraper::Tumblr;
using Json = nlohmann::json;

const std::string ImageScraper::TumblrService::s_RedirectUrl              = "http://localhost:8080";
const std::string ImageScraper::TumblrService::s_UserDataKey_ConsumerKey     = "tumblr_consumer_key";
const std::string ImageScraper::TumblrService::s_UserDataKey_ConsumerSecret = "tumblr_consumer_secret";
const std::string ImageScraper::TumblrService::s_AppDataKey_RefreshToken  = "tumblr_refresh_token";
const std::string ImageScraper::TumblrService::s_AppDataKey_StateId       = "tumblr_state_id";

ImageScraper::TumblrService::TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : Service( ContentProvider::Tumblr, appConfig, userConfig, caBundle, outputDir, sink )
{
    OAuthConfig oauthConfig{
        "https://www.tumblr.com/oauth2/authorize",
        "basic%20offline_access",
        s_UserDataKey_ConsumerKey,
        s_UserDataKey_ConsumerSecret,
        s_AppDataKey_StateId,
        s_AppDataKey_RefreshToken,
        // redirect_uri intentionally omitted - Tumblr's OAuth2 server mismatches
        // the parameter even when it matches the registered URL exactly. Omitting it
        // causes Tumblr to redirect to the sole registered callback URL automatically,
        // which is valid per RFC 6749 s4.1.1 when only one URI is registered.
        "",
        {}
    };

    auto fetchFn = [ ]( const RequestOptions& opts )
        {
            return TumblrFetchAccessTokenRequest{ }.Perform( opts );
        };

    auto refreshFn = [ ]( const RequestOptions& opts )
        {
            return TumblrRefreshAccessTokenRequest{ }.Perform( opts );
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

bool ImageScraper::TumblrService::HasRequiredCredentials( ) const
{
    // Consumer Key (api_key) is sufficient for anonymous downloads.
    // Consumer Secret is only needed for OAuth sign-in.
    std::string clientId;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ConsumerKey, clientId );
    return !clientId.empty( );
}

bool ImageScraper::TumblrService::HasSignInCredentials( ) const
{
    // Sign-in requires both Consumer Key and Consumer Secret.
    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ConsumerKey, clientId );
    m_UserConfig->GetValue<std::string>( s_UserDataKey_ConsumerSecret, clientSecret );
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
    return OAuthServiceHelpers::OpenExternalAuth( *m_OAuthClient );
}

bool ImageScraper::TumblrService::HandleExternalAuth( const std::string& response )
{
    return OAuthServiceHelpers::HandleExternalAuth( m_ContentProvider, GetProviderDisplayName( ), m_Sink, *m_OAuthClient, response, [ this ]( )
        {
            FetchCurrentUser( );
        } );
}

bool ImageScraper::TumblrService::IsSignedIn( ) const
{
    return m_OAuthClient->IsSignedIn( );
}

std::string ImageScraper::TumblrService::GetSignedInUser( ) const
{
    std::unique_lock<std::mutex> lock( m_UsernameMutex );
    return m_Username;
}

void ImageScraper::TumblrService::Authenticate( AuthenticateCallback callback )
{
    OAuthServiceHelpers::Authenticate( m_ContentProvider, GetProviderDisplayName( ), *m_OAuthClient, callback, [ this ]( )
        {
            FetchCurrentUser( );
        } );
}

bool ImageScraper::TumblrService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

void ImageScraper::TumblrService::SignOut( )
{
    OAuthServiceHelpers::SignOut( GetProviderDisplayName( ), *m_OAuthClient, [ this ]( )
        {
            std::unique_lock<std::mutex> lock( m_UsernameMutex );
            m_Username.clear( );
        } );
}

void ImageScraper::TumblrService::FetchCurrentUser( )
{
    RequestOptions options{ };
    options.m_CaBundle    = m_CaBundle;
    options.m_UserAgent   = m_UserAgent;
    options.m_AccessToken = m_OAuthClient->GetAccessToken( );

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

            if( m_OAuthClient->IsAuthenticated( ) )
            {
                retrievePostsOptions.m_AccessToken = m_OAuthClient->GetAccessToken( );
            }
            else if( m_OAuthClient->IsSignedIn( ) )
            {
                if( m_OAuthClient->TryRefreshToken( ) )
                {
                    retrievePostsOptions.m_AccessToken = m_OAuthClient->GetAccessToken( );
                }
                else
                {
                    // Refresh failed - fall through to api_key auth
                    std::string apiKey;
                    m_UserConfig->GetValue<std::string>( s_UserDataKey_ConsumerKey, apiKey );
                    retrievePostsOptions.m_QueryParams.push_back( { "api_key", apiKey } );
                }
            }
            else
            {
                std::string apiKey;
                m_UserConfig->GetValue<std::string>( s_UserDataKey_ConsumerKey, apiKey );
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
                    if( !IsCancelled( ) )
                    {
                        LogError( "[%s] Download failed, error: %s, url: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ), url.c_str( ) );
                    }
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
