#include "app/App.h"

#include "log/Logger.h"
#include "log/FrontEndLogger.h"
#include "log/DevLogger.h"
#include "log/ConsoleLogger.h"
#include "log/FileLogger.h"
#include "services/RedditService.h"
#include "services/TumblrService.h"
#include "services/FourChanService.h"
#include "services/BlueskyService.h"
#include "services/MastodonService.h"
#include "services/RedgifsService.h"
#include "async/TaskManager.h"
#include "config/Config.h"
#include "ui/FrontEnd.h"
#include "io/JsonFile.h"
#include "network/CurlHttpClient.h"
#include "network/ListenServer.h"
#include "network/RateLimitedHttpClient.h"
#include "network/RateLimitTypes.h"
#include "network/RedgifsUrlResolver.h"
#include "network/RetryHttpClient.h"

#include <string>
#include <chrono>
#include <windows.h>

#define UI_MAX_LOG_LINES 10000
#define LISTEN_SERVER_PORT 8080
#define MIN_FRAME_TIME_MS 16
#define THREAD_POOL_MAX_THREADS 2

namespace
{
    // Redgifs has no published rate-limit policy; conservative defaults to avoid being flagged.
    // This table is owned at App scope (not RedgifsService) because the same RateLimitedHttpClient
    // instance is shared between RedgifsService and RedgifsUrlResolver - both hit the redgifs API
    // and a single shared bucket prevents combined throughput from exceeding what either path could
    // safely sustain alone.
    const ImageScraper::RateLimitTable s_RedgifsLimits =
    {
        { "search_user_gifs",                  { { 60, 60 } } },
        { "get_temp_auth",                     { { 30, 60 } } },
        { "get_gif",                           { { 60, 60 } } },
        { ImageScraper::s_DefaultRateLimitKey, { { 60, 60 } } },
    };
}

const std::string ImageScraper::App::s_UserConfigFile = "config.json";
const std::string ImageScraper::App::s_AppConfigFile = "ImageScraper/config.json";
const std::string ImageScraper::App::s_CaBundleFile = "curl-ca-bundle.crt";
const std::string ImageScraper::App::s_AuthHtmlFile = "auth.html";

ImageScraper::App::App( )
{
    Logger::AddLogger( std::make_shared<DevLogger>( ) );
    Logger::AddLogger( std::make_shared<ConsoleLogger>( ) );

    char exePath[ MAX_PATH ];
    GetModuleFileNameA( nullptr, exePath, MAX_PATH );
    const std::filesystem::path exeDir = std::filesystem::path( exePath ).parent_path( );

    auto fileLogger = std::make_shared<FileLogger>( exeDir / "logs" );
    Logger::AddLogger( fileLogger );
    m_LogFilePath = fileLogger->GetLogFilePath( ).string( );

    const std::string appConfigPath = ( std::filesystem::temp_directory_path( ) / s_AppConfigFile ).generic_string( );
    m_AppConfig = std::make_shared<JsonFile>( appConfigPath );

    const std::string userConfigPath = ( exeDir / s_UserConfigFile ).generic_string( );
    m_UserConfig = std::make_shared<JsonFile>( userConfigPath );

    m_AuthHtmlPath = ( exeDir / s_AuthHtmlFile ).generic_string( );

    m_FrontEnd = std::make_shared<FrontEnd>( UI_MAX_LOG_LINES );
    m_FrontEnd->SetLogFilePath( m_LogFilePath );

    Logger::AddLogger( std::make_shared<FrontEndLogger>( m_FrontEnd ) );

    if( m_AppConfig->Deserialise( ) )
    {
        SuccessLog( "[%s] App Config Loaded!", __FUNCTION__ );
    }

    if( m_UserConfig->Deserialise( ) )
    {
        SuccessLog( "[%s] User Config Loaded!", __FUNCTION__ );
    }

    const std::string caBundlePath = ( exeDir / s_CaBundleFile ).generic_string( );
    m_OutputDirPath = exeDir.generic_string( );

    // One rate-limited HTTP client for all redgifs traffic, shared between RedgifsUrlResolver
    // (used by every service that may encounter redgifs URLs) and RedgifsService itself.
    // Without this sharing, the resolver and the service would each get their own independent
    // bucket and combined throughput could exceed the documented limits.
    auto redgifsHttpClient = std::make_shared<RateLimitedHttpClient>(
        std::make_shared<RetryHttpClient>( std::make_shared<CurlHttpClient>( ) ),
        s_RedgifsLimits,
        [ sink = m_FrontEnd ]( ) { return sink ? sink->IsCancelled( ) : false; },
        m_FrontEnd,
        "Redgifs" );

    // One URL resolver, shared across every service. Today only translates
    // redgifs watch URLs; new resolvers can be added by composing them here.
    auto urlResolver = std::make_shared<RedgifsUrlResolver>(
        redgifsHttpClient,
        caBundlePath,
        Service::DefaultUserAgent( ) );

    m_Services.push_back( std::make_shared<RedditService>(   m_AppConfig, m_UserConfig, caBundlePath, m_OutputDirPath, m_FrontEnd, urlResolver ) );
    m_Services.push_back( std::make_shared<TumblrService>(   m_AppConfig, m_UserConfig, caBundlePath, m_OutputDirPath, m_FrontEnd, urlResolver ) );
    m_Services.push_back( std::make_shared<FourChanService>( m_AppConfig, m_UserConfig, caBundlePath, m_OutputDirPath, m_FrontEnd, urlResolver ) );
    m_Services.push_back( std::make_shared<BlueskyService>(  m_AppConfig, m_UserConfig, caBundlePath, m_OutputDirPath, m_FrontEnd, urlResolver ) );
    m_Services.push_back( std::make_shared<MastodonService>( m_AppConfig, m_UserConfig, caBundlePath, m_OutputDirPath, m_FrontEnd, urlResolver ) );
    m_Services.push_back( std::make_shared<RedgifsService>(  m_AppConfig, m_UserConfig, caBundlePath, m_OutputDirPath, m_FrontEnd, redgifsHttpClient, urlResolver ) );

    m_ListenServer = std::make_shared<ListenServer>( );
}

int ImageScraper::App::Run( )
{
    if( !m_FrontEnd || !m_FrontEnd->Init( m_Services, m_UserConfig, m_AppConfig ) )
    {
        LogError( "[%s] Could not start FrontEnd!", __FUNCTION__ );
        return EXIT_FAILURE;
    }

    if( !m_ListenServer )
    {
        LogError( "[%s] Invalid ListenServer!", __FUNCTION__ );
        return EXIT_FAILURE;
    }

    TaskManager::Instance( ).Start( THREAD_POOL_MAX_THREADS );

    m_ListenServer->Init( m_Services, LISTEN_SERVER_PORT, m_AuthHtmlPath, m_FrontEnd );
    m_ListenServer->Start( );

    AuthenticateServices( );

    // Main loop
    while( !glfwWindowShouldClose( m_FrontEnd->GetWindow( ) ) )
    {
        auto startTime = std::chrono::high_resolution_clock::now( );

        m_FrontEnd->Update( );

        TaskManager::Instance( ).Update( );

        m_FrontEnd->Render( ); // TOOD dedicated UI thread

        auto endTime = std::chrono::high_resolution_clock::now( );
        auto nextTime = startTime + std::chrono::milliseconds( MIN_FRAME_TIME_MS );

        std::this_thread::sleep_until( nextTime );
    }

    m_ListenServer->Stop( );

    TaskManager::Instance( ).Stop( );

    return EXIT_SUCCESS;
}

void ImageScraper::App::AuthenticateServices( )
{
    m_AuthenticatingCount = static_cast<int>( m_Services.size( ) );
    if( m_AuthenticatingCount <= 0 )
    {
        m_FrontEnd->SetInputState( InputState::Free );
        return;
    }

    m_FrontEnd->SetInputState( InputState::Blocked );

    auto callback = [ this ]( ContentProvider provider, bool success )
    {
        auto task = TaskManager::Instance( ).SubmitMain( [ this, provider, success ]( )
            {
                OnServiceAuthenticationComplete( provider, success );
            } );
        ( void )task;
    };

    for( const auto& service : m_Services )
    {
        service->Authenticate( callback );
    }
}

void ImageScraper::App::OnServiceAuthenticationComplete( ContentProvider provider, bool success )
{
    if( !success )
    {
        LogDebug( "[%s] %s failed to authenticate!", __FUNCTION__, s_ContentProviderStrings[ static_cast<uint8_t>( provider ) ] );
    }

    if( m_AuthenticatingCount > 0 )
    {
        --m_AuthenticatingCount;
    }

    if( m_AuthenticatingCount <= 0 )
    {
        m_FrontEnd->SetInputState( InputState::Free );
    }
}
