#include "ui/ConsoleInput.h"
#include <iostream>
#include "log/Logger.h"
#include <thread>
#include <chrono>
#include "async/ThreadPool.h"

ConsoleInput::ConsoleInput( )
{
    auto result = ThreadPool::Instance( ).Submit( ThreadPool::s_UIContext, [ & ]( )
        {
            bool running = true;
            while( running )
            {
                {
                    std::unique_lock<std::mutex> lock{ m_InputMutex };
                    std::getline( std::cin, m_Input );

                    if( m_Input == "exit" )
                    {
                        running = false;
                    }
                }

                std::this_thread::sleep_for( std::chrono::milliseconds( 16 ) );
            }
        } );

    ( void )result;
}

bool ConsoleInput::GetUserInput( std::string& out )
{
    if( !m_Input.empty( ) )
    {
        {
            std::unique_lock<std::mutex> lock{ m_InputMutex };
            out = m_Input;
            m_Input.clear( );
        }

        return true;
    }

    return false;
}
