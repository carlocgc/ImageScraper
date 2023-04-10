#include "app/App.h"

#include "log/Logger.h"
#include "log/FrontEndLogger.h"
#include "log/DevLogger.h"
#include "log/ConsoleLogger.h"
#include "services/RedditService.h"
#include "async/TaskManager.h"
#include "config/Config.h"
#include "ui/FrontEnd.h"

#define UI_MAX_LOG_LINES 10000

ImageScraper::App::App( )
{
    Logger::AddLogger( std::make_shared<DevLogger>( ) );
    Logger::AddLogger( std::make_shared<ConsoleLogger>( ) );

    m_Config = std::make_shared<Config>( );
    m_FrontEnd = std::make_shared<FrontEnd>( m_Config, UI_MAX_LOG_LINES );

    Logger::AddLogger( std::make_shared<FrontEndLogger>( m_FrontEnd ) );

    m_Services.push_back( std::make_shared<RedditService>( ) );

    m_Config->ReadFromFile( "config.json" );
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
