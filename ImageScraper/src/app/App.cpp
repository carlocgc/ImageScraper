#include "app/App.h"

#include "log/Logger.h"
#include "log/FrontEndLogger.h"
#include "log/DevLogger.h"
#include "log/ConsoleLogger.h"
#include "services/RedditService.h"
#include "async/TaskManager.h"
#include "config/Config.h"
#include "ui/FrontEnd.h"
#include "io/JsonFile.h"

#include <string>

#define UI_MAX_LOG_LINES 10000

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
}

int ImageScraper::App::Run( )
{
    if( !m_FrontEnd || !m_FrontEnd->Init( ) )
    {
        return 1;
    }

    // Main loop
    while( !glfwWindowShouldClose( m_FrontEnd->GetWindow( ) ) )
    {
        m_FrontEnd->Update( );

        if( m_FrontEnd->HandleUserInput( m_Services ) )
        {
            m_FrontEnd->SetInputState( InputState::Blocked );
        }

        TaskManager::Instance( ).Update( );

        m_FrontEnd->Render( ); // TOOD dedicated UI thread
    }

    return 0;
}
