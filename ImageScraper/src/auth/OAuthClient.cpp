#include "auth/OAuthClient.h"
#include "io/JsonFile.h"
#include "log/Logger.h"
#include "utils/StringUtils.h"
#include "nlohmann/json.hpp"

#include <Shellapi.h>

using Json = nlohmann::json;

namespace ImageScraper
{

OAuthClient::OAuthClient( OAuthConfig config,
                          std::shared_ptr<JsonFile> appConfig,
                          std::shared_ptr<JsonFile> userConfig,
                          std::string caBundle,
                          std::string userAgent,
                          TokenRequestFn fetchToken,
                          TokenRequestFn refreshToken )
    : m_Config{ std::move( config ) }
    , m_AppConfig{ appConfig }
    , m_UserConfig{ userConfig }
    , m_CaBundle{ std::move( caBundle ) }
    , m_UserAgent{ std::move( userAgent ) }
    , m_FetchTokenFn{ std::move( fetchToken ) }
    , m_RefreshTokenFn{ std::move( refreshToken ) }
{
    LoadOrGenerateStateId( );

    {
        std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
        if( m_AppConfig->GetValue<std::string>( m_Config.m_RefreshTokenAppKey, m_RefreshToken ) && !m_RefreshToken.empty( ) )
        {
            LogDebug( "[%s] OAuth refresh token found.", __FUNCTION__ );
        }
        else
        {
            LogDebug( "[%s] No OAuth refresh token found.", __FUNCTION__ );
        }
    }
}

void OAuthClient::LoadOrGenerateStateId( )
{
    if( !m_AppConfig->GetValue<std::string>( m_Config.m_StateIdAppKey, m_StateId ) || m_StateId.empty( ) )
    {
        m_StateId = StringUtils::CreateGuid( 30 );
        m_AppConfig->SetValue<std::string>( m_Config.m_StateIdAppKey, m_StateId );
        m_AppConfig->Serialise( );
        LogDebug( "[%s] Created new OAuth state id: %s", __FUNCTION__, m_StateId.c_str( ) );
    }
}

std::string OAuthClient::BuildAuthUrl( const std::string& clientId ) const
{
    std::wstring wurl = StringUtils::Utf8ToWideString( m_Config.m_AuthUrl, false );
    wurl += L"?client_id=" + StringUtils::Utf8ToWideString( clientId, false );
    wurl += L"&response_type=code";
    wurl += L"&state=" + StringUtils::Utf8ToWideString( m_StateId, false );
    wurl += L"&scope=" + StringUtils::Utf8ToWideString( m_Config.m_Scopes, false );

    if( !m_Config.m_RedirectUri.empty( ) )
    {
        wurl += L"&redirect_uri=" + StringUtils::Utf8ToWideString( m_Config.m_RedirectUri, false );
    }

    for( const auto& param : m_Config.m_ExtraAuthParams )
    {
        wurl += L"&" + StringUtils::Utf8ToWideString( param.m_Key, false )
              + L"=" + StringUtils::Utf8ToWideString( param.m_Value, false );
    }

    return StringUtils::WideStringToUtf8String( wurl, false );
}

bool OAuthClient::OpenAuth( )
{
    std::string clientId;
    m_UserConfig->GetValue<std::string>( m_Config.m_ClientIdKey, clientId );

    const std::string url  = BuildAuthUrl( clientId );
    const std::wstring wurl = StringUtils::Utf8ToWideString( url, false );

    if( ShellExecute( NULL, L"open", wurl.c_str( ), NULL, NULL, SW_SHOWNORMAL ) )
    {
        InfoLog( "[%s] Opened OAuth auth in browser.", __FUNCTION__ );
        LogDebug( "[%s] Auth URL: %s", __FUNCTION__, url.c_str( ) );
        return true;
    }

    LogError( "[%s] Could not open default browser - make sure a default browser is set in OS settings!", __FUNCTION__ );
    return false;
}

bool OAuthClient::HandleAuth( const std::string& response, const std::string& providerName )
{
    if( response.find( "favicon" ) != std::string::npos )
    {
        LogDebug( "[%s] %s HandleAuth skipped - favicon request.", __FUNCTION__, providerName.c_str( ) );
        return false;
    }

    const bool hasError = response.find( "?error=" ) != std::string::npos
                       || response.find( "&error=" ) != std::string::npos;

    if( hasError )
    {
        const std::string error = StringUtils::ExtractQueryParam( response, "error" );
        const std::string desc  = StringUtils::UrlDecode( StringUtils::ExtractQueryParam( response, "error_description" ) );
        WarningLog( "[%s] %s OAuth error: %s - %s", __FUNCTION__, providerName.c_str( ), error.c_str( ), desc.c_str( ) );
        if( error == "redirect_uri_mismatch" && !m_Config.m_RedirectUri.empty( ) )
        {
            WarningLog( "[%s] Ensure the redirect URI in your %s app settings is set to: %s",
                        __FUNCTION__, providerName.c_str( ), m_Config.m_RedirectUri.c_str( ) );
        }
        return false;
    }

    const std::string receivedState = StringUtils::ExtractQueryParam( response, "state" );
    if( receivedState.empty( ) || receivedState != m_StateId )
    {
        LogError( "[%s] %s OAuth state mismatch - possible CSRF attack. Expected: %s, Received: %s",
                  __FUNCTION__, providerName.c_str( ), m_StateId.c_str( ), receivedState.c_str( ) );
        return false;
    }

    const std::string authCode = StringUtils::ExtractQueryParam( response, "code" );
    if( authCode.empty( ) )
    {
        LogDebug( "[%s] %s HandleAuth failed - auth code not found in response.", __FUNCTION__, providerName.c_str( ) );
        return false;
    }

    InfoLog( "[%s] %s auth code received.", __FUNCTION__, providerName.c_str( ) );
    return FetchAccessToken( authCode );
}

bool OAuthClient::FetchAccessToken( const std::string& code )
{
    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( m_Config.m_ClientIdKey, clientId );
    m_UserConfig->GetValue<std::string>( m_Config.m_ClientSecretKey, clientSecret );

    RequestOptions options{ };
    options.m_CaBundle     = m_CaBundle;
    options.m_UserAgent    = m_UserAgent;
    options.m_ClientId     = clientId;
    options.m_ClientSecret = clientSecret;
    options.m_QueryParams.push_back( { "code", code } );

    if( !m_Config.m_RedirectUri.empty( ) )
    {
        options.m_QueryParams.push_back( { "redirect_uri", StringUtils::UrlEncode( m_Config.m_RedirectUri ) } );
    }

    const RequestResult result = m_FetchTokenFn( options );

    if( !result.m_Success )
    {
        LogError( "[%s] OAuth token fetch failed: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        return false;
    }

    return ParseAndStoreTokenResponse( result.m_Response );
}

bool OAuthClient::ParseAndStoreTokenResponse( const std::string& rawResponse )
{
    try
    {
        const Json response = Json::parse( rawResponse );

        if( !response.contains( "access_token" ) || !response.contains( "expires_in" ) )
        {
            LogError( "[%s] Token response missing access_token or expires_in.", __FUNCTION__ );
            return false;
        }

        if( !response.contains( "refresh_token" ) )
        {
            LogError( "[%s] Token response missing refresh_token.", __FUNCTION__ );
            return false;
        }

        const std::string accessToken   = response[ "access_token" ].get<std::string>( );
        const int         expireSeconds  = response[ "expires_in" ].get<int>( );
        const std::string refreshToken  = response[ "refresh_token" ].get<std::string>( );

        {
            std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
            m_AccessToken = accessToken;
            constexpr int expireDelta = 180; // 3-minute safety margin
            m_AuthExpireSeconds = std::chrono::seconds{ expireSeconds - expireDelta };
            m_TokenReceived = std::chrono::system_clock::now( );
        }

        {
            std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
            m_RefreshToken = refreshToken;
            m_AppConfig->SetValue( m_Config.m_RefreshTokenAppKey, m_RefreshToken );
            m_AppConfig->Serialise( );
        }

        LogDebug( "[%s] OAuth tokens stored successfully.", __FUNCTION__ );
        return true;
    }
    catch( const Json::exception& ex )
    {
        LogError( "[%s] Failed to parse token response: %s", __FUNCTION__, ex.what( ) );
        return false;
    }
}

bool OAuthClient::TryRefreshToken( )
{
    std::string refreshToken;
    {
        std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
        refreshToken = m_RefreshToken;
    }

    if( refreshToken.empty( ) )
    {
        LogDebug( "[%s] No refresh token available.", __FUNCTION__ );
        return false;
    }

    std::string clientId;
    std::string clientSecret;
    m_UserConfig->GetValue<std::string>( m_Config.m_ClientIdKey, clientId );
    m_UserConfig->GetValue<std::string>( m_Config.m_ClientSecretKey, clientSecret );

    RequestOptions options{ };
    options.m_CaBundle     = m_CaBundle;
    options.m_UserAgent    = m_UserAgent;
    options.m_ClientId     = clientId;
    options.m_ClientSecret = clientSecret;
    options.m_QueryParams.push_back( { "refresh_token", refreshToken } );

    const RequestResult result = m_RefreshTokenFn( options );

    if( !result.m_Success )
    {
        LogError( "[%s] Token refresh failed: %s", __FUNCTION__, result.m_Error.m_ErrorString.c_str( ) );
        ClearRefreshToken( );
        return false;
    }

    return ParseAndStoreTokenResponse( result.m_Response );
}

bool OAuthClient::IsSignedIn( ) const
{
    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    return !m_RefreshToken.empty( );
}

bool OAuthClient::IsAuthenticated( ) const
{
    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    if( m_AccessToken.empty( ) )
    {
        return false;
    }
    const auto now = std::chrono::system_clock::now( );
    return m_TokenReceived + m_AuthExpireSeconds > now;
}

std::string OAuthClient::GetAccessToken( ) const
{
    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    return m_AccessToken;
}

void OAuthClient::StoreAccessToken( const std::string& token, int expireSeconds )
{
    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    m_AccessToken = token;
    constexpr int expireDelta = 180;
    m_AuthExpireSeconds = std::chrono::seconds{ expireSeconds - expireDelta };
    m_TokenReceived = std::chrono::system_clock::now( );
}

void OAuthClient::SignOut( )
{
    ClearAccessToken( );
    ClearRefreshToken( );
}

void OAuthClient::ClearAccessToken( )
{
    std::unique_lock<std::mutex> lock( m_AccessTokenMutex );
    m_AccessToken.clear( );
    m_AuthExpireSeconds = std::chrono::seconds{ 0 };
}

void OAuthClient::ClearRefreshToken( )
{
    std::unique_lock<std::mutex> lock( m_RefreshTokenMutex );
    m_RefreshToken.clear( );
    m_AppConfig->SetValue( m_Config.m_RefreshTokenAppKey, m_RefreshToken );
    m_AppConfig->Serialise( );
}

} // namespace ImageScraper
