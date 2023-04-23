#include "network/ListenServer.h"
#include "async/TaskManager.h"
#include "log/Logger.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <winsock2.h>
#include <WS2tcpip.h>

void ImageScraper::ListenServer::Init( std::vector<std::shared_ptr<Service>> services, int port )
{
    m_Services = services;
    m_Port = port;
    m_Initialised = true;

    std::stringstream htmlContent{ };
    std::ifstream htmlFile( "auth.html" );
    if( htmlFile.is_open( ) )
    {
        htmlContent << htmlFile.rdbuf( );
        m_AuthHtml = htmlContent.str( );
        htmlFile.close( );
    }
    else
    {
        m_AuthHtml = "<html><body><h1>Authorization complete!</h1></body></html>";
    }
}

void ImageScraper::ListenServer::Start(  )
{
    if( !m_Initialised )
    {
        ErrorLog( "[%s] ListenServer not initialised, call Init() before calling Start()!", __FUNCTION__ );
        return;
    }

    auto OnMessageReceived = [ & ]( const std::string message )
    {
        DebugLog( "[%s] ListenServer message received, message: %s", __FUNCTION__, message.c_str( ) );

        for (const auto& service : m_Services)
        {
            if( service->HandleExternalAuth( message ) )
            {
                DebugLog( "[%s] ListenServer auth response handled!", __FUNCTION__ );
                return;
            }
        }

        DebugLog( "[%s] ListenServer auth response not handled!", __FUNCTION__ );
    };

    auto OnError = [ & ]( const std::string error )
    {
        DebugLog( "[%s] ListenServer failed, error: %s", __FUNCTION__, error.c_str( ) );

        if( m_CurrentRetries >= m_MaxRetries )
        {
            DebugLog( "[%s] ListenServer max retries reached!", __FUNCTION__ );
            return;
        }

        DebugLog( "[%s] ListenServer retrying startup: %i/%i !", __FUNCTION__, m_CurrentRetries, m_MaxRetries );
        ++m_CurrentRetries;
        Start( );
    };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ListenServer, [ &, OnMessageReceived, OnError ]( )
        {
            m_Running.store( true );

            WSADATA wsData;
            if( WSAStartup( MAKEWORD( 2, 2 ), &wsData ) != 0 )
            {
                WSACleanup( );
                std::string errorString{ };
                auto errorTask = TaskManager::Instance().SubmitMain( OnError, "WSAStartup failed" );
                ( void )errorTask;
                return;
            }

            const std::string port = std::to_string( m_Port );

            addrinfo hints = { 0 };
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags = AI_PASSIVE;

            addrinfo* serverInfo;

            if( getaddrinfo( NULL, port.c_str(), &hints, &serverInfo ) != 0 )
            {
                WSACleanup( );
                auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "getaddrinfo failed" );
                ( void )errorTask;
                return;
            }

            SOCKET listenSocket = socket( serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol );
            if( listenSocket == INVALID_SOCKET )
            {
                freeaddrinfo( serverInfo );
                WSACleanup( );
                auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "freeaddrinfo failed" );
                ( void )errorTask;
                return;
            }

            if( bind( listenSocket, serverInfo->ai_addr, ( int )serverInfo->ai_addrlen ) == SOCKET_ERROR )
            {
                closesocket( listenSocket );
                freeaddrinfo( serverInfo );
                WSACleanup( );
                auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "bind failed" );
                ( void )errorTask;
                return;
            }

            freeaddrinfo( serverInfo );

            if( listen( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
            {
                closesocket( listenSocket );
                WSACleanup( );
                auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "listen failed" );
                ( void )errorTask;
                return;
            }

            InfoLog( "[%s] ListenServer started successfully, waiting for connections!", __FUNCTION__ );

            while( m_Running )
            {
                sockaddr_storage clientAddr;
                int addrSize = sizeof( clientAddr );
                SOCKET clientSocket = accept( listenSocket, ( sockaddr* )&clientAddr, &addrSize );
                if( clientSocket == INVALID_SOCKET )
                {
                    closesocket( listenSocket );
                    WSACleanup( );
                    auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "accept failed" );
                    ( void )errorTask;
                    return;
                }

                DebugLog( "[%s] ListenServer connection establised!", __FUNCTION__ );

                // Now receive and process the HTTP response from the browser
                char buffer[ 4096 ];
                int bytesReceived = recv( clientSocket, buffer, sizeof( buffer ), 0 );
                std::string response( buffer, bytesReceived );

                DebugLog( "[%s] ListenServer bytes received!, bytes: %i, data: %s, ", __FUNCTION__, bytesReceived, response.c_str() );

                const int contentLength = static_cast< int >( m_AuthHtml.length( ) );

                std::string httpResponse = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: " + std::to_string( contentLength ) + "\r\n"
                    "\r\n";

                httpResponse += m_AuthHtml;

                int bytesSent = send( clientSocket, httpResponse.c_str( ), static_cast<int>( httpResponse.length( ) ), 0 );
                if( bytesSent == SOCKET_ERROR )
                {
                    DebugLog( "[%s] ListenServer failed to send http response.", __FUNCTION__ );
                    DebugLog( "[%s] ListenServer Response: ", __FUNCTION__, response.c_str( ) );
                }

                DebugLog( "[%s] ListenServer sent http response successfully!", __FUNCTION__ );

                DebugLog( "[%s] ListenServer Response: ", __FUNCTION__, response.c_str( ) );

                auto messageTask = TaskManager::Instance( ).SubmitMain( OnMessageReceived, response );
                ( void )messageTask;

                closesocket( clientSocket );
            }

            closesocket( listenSocket );
            WSACleanup( );
        } );
    ( void )task;
}

void ImageScraper::ListenServer::Stop( )
{
    m_Running.store( false );
    Reset( );
}

void ImageScraper::ListenServer::Reset( )
{
    if ( m_Running.load() )
    {
        return;
    }

    m_CurrentRetries = 0;
}
