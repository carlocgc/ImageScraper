#include "app/App.h"
#include "services/RedditService.h"
#include "log/Logger.h"

App::App( )
{
     m_Services.push_back( std::make_shared<RedditService>() );
}

int App::Run( )
{
    for( ;; )
    {
        std::string input;
        if( m_UI.GetUserInput( input ) )
        {
            if( input == "exit" )
            {
                InfoLog( "[%s] Exiting...", __FUNCTION__ );
                return 0;
            }

            InfoLog( "[%s] Processing input: %s", __FUNCTION__, input.c_str());

            for ( auto service : m_Services )
            {
                if( service->HandleUrl( m_Config, input ) )
                {
                    break;
                }
            }
        }

        // Check main thread queue for callbacks
    }

    return 0;
}
