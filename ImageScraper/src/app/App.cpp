#include "app/App.h"

#include "services/RedditService.h"
#include "log/Logger.h"

ImageScraper::App::App( )
{
    m_Config = std::make_shared<Config>( );
    m_FrontEnd = std::make_shared<FrontEnd>( m_Config );
    m_Services.push_back( std::make_shared<RedditService>( ) );
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

        if( m_FrontEnd->HandleUserInput( m_Services ) )
        {
            m_FrontEnd->SetInputState( InputState::Blocked );
        }

        // Check main thread queue for callbacks

        m_FrontEnd->Render( );
    }

    return 0;
}
