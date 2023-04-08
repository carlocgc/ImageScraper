#include "ui/ConsoleUI.h"
#include <iostream>
#include "log/Logger.h"
#include <thread>
#include <chrono>
#include "async/ThreadPool.h"

ConsoleUI::ConsoleUI( )
{
    Init( );
}

void ConsoleUI::Init( )
{
    auto result = ThreadPool::Instance( ).Submit( ThreadPool::s_UIContext, [ & ]( )
        {
            while( true )
            {
                {
                    std::unique_lock<std::mutex> lock{ ConsoleUI::GetMutex( ) };
                    std::cin.seekg( 0, std::cin.end );
                    int length = static_cast< int >( std::cin.tellg( ) );
                    if( length > 0 )
                    {
                        std::cin.seekg( 0, std::cin.beg );
                        std::getline( std::cin, m_Input );
                        if( m_Input == "exit" )
                        {
                            break;
                        }
                    }
                }

                std::this_thread::sleep_for( std::chrono::milliseconds( 16 ) );
            }
        } );

    ( void )result;
}

bool ConsoleUI::GetUserInput( std::string& out )
{
    if( !m_Input.empty( ) )
    {
        {
            std::unique_lock<std::mutex> lock{ ConsoleUI::GetMutex( ) };
            out = m_Input;
            m_Input.clear( );
        }

        return true;
    }

    return false;
}
