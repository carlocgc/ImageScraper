#include "app/App.h"

#include "services/RedditService.h"
#include "log/Logger.h"
#include "async/ThreadPoolSingleton.h"
#include "log/FrontEndLogger.h"
#include "log/DevLogger.h"
#include "log/ConsoleLogger.h"

#define UI_MAX_LOG_LINES 1000

ImageScraper::App::App( )
{
    m_Config = std::make_shared<Config>( );
    m_FrontEnd = std::make_shared<FrontEnd>( m_Config );

    m_Services.push_back( std::make_shared<RedditService>( ) );

    Logger::AddLogger( std::make_shared<DevLogger>( ) );
    Logger::AddLogger( std::make_shared<ConsoleLogger>( ) );
    Logger::AddLogger( std::make_shared<FrontEndLogger>( m_FrontEnd, UI_MAX_LOG_LINES ) );
}

int ImageScraper::App::Run( )
{
    if( !m_FrontEnd || !m_FrontEnd->Init( ) )
    {
        return 1;
    }

    // Main loop
    while( !glfwWindowShouldClose( m_FrontEnd->GetWindow() ) )
    {
        m_FrontEnd->Update( );

        Logger::Update( );

        if( m_FrontEnd->HandleUserInput( m_Services ) )
        {
            m_FrontEnd->SetInputState( InputState::Blocked );
        }

        ThreadPoolSingleton::Instance( ).Update( );

        m_FrontEnd->Render( );
    }

    return 0;
}
