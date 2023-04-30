#include "app/App.h"

#include "log/Logger.h"
#include "log/FrontEndLogger.h"
#include "log/DevLogger.h"
#include "log/ConsoleLogger.h"
#include "services/RedditService.h"
#include "services/TumblrService.h"
#include "services/FourChanService.h"
#include "async/TaskManager.h"
#include "config/Config.h"
#include "ui/FrontEnd.h"
#include "io/JsonFile.h"
#include "network/ListenServer.h"

#include <string>

#define UI_MAX_LOG_LINES 10000
#define LISTEN_SERVER_PORT 8080

const std::string ImageScraper::App::s_UserConfigFile = "config.json";
const std::string ImageScraper::App::s_AppConfigFile = "ImageScraper/config.json";
const std::string ImageScraper::App::s_CaBundleFile = "curl-ca-bundle.crt";

ImageScraper::App::App( )
{
    Logger::AddLogger( std::make_shared<DevLogger>( ) );
    Logger::AddLogger( std::make_shared<ConsoleLogger>( ) );

    const std::string appConfigPath = ( std::filesystem::temp_directory_path( ) / s_AppConfigFile ).generic_string( );
    m_AppConfig = std::make_shared<JsonFile>( appConfigPath );

    const std::string userConfigPath = ( std::filesystem::current_path( ) / s_UserConfigFile ).generic_string( );
    m_UserConfig = std::make_shared<JsonFile>( userConfigPath );

    m_FrontEnd = std::make_shared<FrontEnd>( UI_MAX_LOG_LINES );

    Logger::AddLogger( std::make_shared<FrontEndLogger>( m_FrontEnd ) );

    if( m_AppConfig->Deserialise( ) )
    {
        InfoLog( "[%s] App Config Loaded!", __FUNCTION__ );
    }

    if( m_UserConfig->Deserialise( ) )
    {
        InfoLog( "[%s] User Config Loaded!", __FUNCTION__ );
    }

    const std::string caBundlePath = ( std::filesystem::current_path( ) / s_CaBundleFile ).generic_string( );
    m_Services.push_back( std::make_shared<RedditService>( m_AppConfig, m_UserConfig, caBundlePath, m_FrontEnd ) );
    m_Services.push_back( std::make_shared<TumblrService>( m_AppConfig, m_UserConfig, caBundlePath, m_FrontEnd ) );
    m_Services.push_back( std::make_shared<FourChanService>( m_AppConfig, m_UserConfig, caBundlePath, m_FrontEnd ) );

    m_ListenServer = std::make_shared<ListenServer>( );
}

int ImageScraper::App::Run( )
{
    if( !m_FrontEnd || !m_FrontEnd->Init( m_Services ) )
    {
        ErrorLog( "[%s] Could not start FrontEnd!", __FUNCTION__ );
        return 1;
    }

    if( !m_ListenServer )
    {
        ErrorLog( "[%s] Invalid ListenServer!", __FUNCTION__ );
    }

    m_ListenServer->Init( m_Services, LISTEN_SERVER_PORT );
    m_ListenServer->Start( );

    AuthenticateServices( );

    // Main loop
    while( !glfwWindowShouldClose( m_FrontEnd->GetWindow( ) ) )
    {
        m_FrontEnd->Update( );

        TaskManager::Instance( ).Update( );

        m_FrontEnd->Render( ); // TOOD dedicated UI thread
    }

    m_ListenServer->Stop( );

    return 0;
}

void ImageScraper::App::AuthenticateServices( )
{
    m_FrontEnd->SetInputState( InputState::Blocked );

    auto callback = [ & ]( ContentProvider provider, bool success )
    {
        if( !success )
        {
            DebugLog( "[%s] %s failed to autheticate!", __FUNCTION__, s_ContentProviderStrings[ static_cast<uint8_t>( provider ) ] );
        }

        m_AuthenticatingCount--;
        if( m_AuthenticatingCount <= 0 )
        {
            m_FrontEnd->SetInputState( InputState::Free );
        }
    };

    for( auto service : m_Services )
    {
        ++m_AuthenticatingCount;
        service->Authenticate( callback );
    }
}
