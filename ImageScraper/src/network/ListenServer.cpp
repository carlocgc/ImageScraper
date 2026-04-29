#include "network/ListenServer.h"
#include "async/TaskManager.h"
#include "log/Logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <array>
#include <algorithm>

#include <winsock2.h>
#include <WS2tcpip.h>

namespace
{
    constexpr int s_ReceiveTimeoutMs = 2000;
    constexpr std::size_t s_RequestChunkSize = 4096;
    constexpr std::size_t s_MaxAuthRequestSize = 1024 * 16;

    bool WaitForReadableSocket( SOCKET socket, int timeoutMs )
    {
        fd_set readSet;
        FD_ZERO( &readSet );
        FD_SET( socket, &readSet );

        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = ( timeoutMs % 1000 ) * 1000;

        return select( 0, &readSet, nullptr, nullptr, &timeout ) > 0;
    }

    bool ReceiveHttpRequest( SOCKET clientSocket, std::string& request )
    {
        request.clear( );
        std::array<char, s_RequestChunkSize> buffer{ };

        while( request.size( ) < s_MaxAuthRequestSize )
        {
            if( !WaitForReadableSocket( clientSocket, s_ReceiveTimeoutMs ) )
            {
                return !request.empty( );
            }

            const int remainingBytes = static_cast<int>( s_MaxAuthRequestSize - request.size( ) );
            const int bytesToReceive = static_cast<int>( std::min<std::size_t>( buffer.size( ), remainingBytes ) );
            const int bytesReceived = recv( clientSocket, buffer.data( ), bytesToReceive, 0 );

            if( bytesReceived == SOCKET_ERROR )
            {
                const int error = WSAGetLastError( );
                if( error == WSAEWOULDBLOCK )
                {
                    continue;
                }

                ImageScraper::Logger::Log( ImageScraper::LogLevel::Warning, "[%s] ListenServer failed to receive http request, WSA error: %i", __FUNCTION__, error );
                return false;
            }

            if( bytesReceived == 0 )
            {
                return !request.empty( );
            }

            request.append( buffer.data( ), bytesReceived );

            if( request.find( "\r\n\r\n" ) != std::string::npos )
            {
                return true;
            }
        }

        ImageScraper::Logger::Log( ImageScraper::LogLevel::Warning, "[%s] ListenServer auth request exceeded max size of %zu bytes.", __FUNCTION__, s_MaxAuthRequestSize );
        return false;
    }
}

void ImageScraper::ListenServer::Init( std::vector<std::shared_ptr<Service>> services, int port, const std::string& authHtmlPath, std::shared_ptr<IServiceSink> sink )
{
    m_Services = services;
    m_Port = port;
    m_Sink = sink;
    m_Initialised = true;

    std::stringstream htmlContent{ };
    std::ifstream htmlFile( authHtmlPath );
    if( htmlFile.is_open( ) )
    {
        htmlContent << htmlFile.rdbuf( );
        m_AuthHtmlTemplate = htmlContent.str( );
        htmlFile.close( );
    }
    else
    {
        m_AuthHtmlTemplate = "<html><body><h1>Authorization complete!</h1></body></html>";
    }
}

void ImageScraper::ListenServer::Start( )
{
    if( !m_Initialised )
    {
        LogError( "[%s] ListenServer not initialised, call Init() before calling Start()!", __FUNCTION__ );
        return;
    }

    auto OnMessageReceived = [ this ]( const std::string message )
        {
            LogDebug( "[%s] ListenServer message received, bytes: %zu", __FUNCTION__, message.size( ) );

            for( const auto& service : m_Services )
            {
                if( service->HandleExternalAuth( message ) )
                {
                    LogDebug( "[%s] ListenServer auth response handled!", __FUNCTION__ );
                    return;
                }
            }

            LogDebug( "[%s] ListenServer auth response not handled!", __FUNCTION__ );
        };

    auto OnError = [ this ]( const std::string error )
        {
            LogDebug( "[%s] ListenServer failed, error: %s", __FUNCTION__, error.c_str( ) );

            if( m_CurrentRetries >= m_MaxRetries )
            {
                LogDebug( "[%s] ListenServer max retries reached!", __FUNCTION__ );
                return;
            }

            LogDebug( "[%s] ListenServer retrying startup: %i/%i !", __FUNCTION__, m_CurrentRetries, m_MaxRetries );
            ++m_CurrentRetries;
            Start( );
        };

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ListenServer, [ this, OnMessageReceived, OnError ]( )
        {
            m_Running.store( true );

            WSADATA wsData;
            if( WSAStartup( MAKEWORD( 2, 2 ), &wsData ) != 0 )
            {
                WSACleanup( );
                std::string errorString{ };
                auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "WSAStartup failed" );
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

            if( getaddrinfo( NULL, port.c_str( ), &hints, &serverInfo ) != 0 )
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

            // Set the socket to non-blocking mode
            u_long nonBlocking = 1;
            if( ioctlsocket( listenSocket, FIONBIO, &nonBlocking ) == SOCKET_ERROR )
            {
                closesocket( listenSocket );
                WSACleanup( );
                auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "error setting non-blocking mode." );
                ( void )errorTask;
                return;
            }

            if( listen( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
            {
                closesocket( listenSocket );
                WSACleanup( );
                auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "listen failed" );
                ( void )errorTask;
                return;
            }

            SuccessLog( "[%s] ListenServer started successfully, waiting for connections!", __FUNCTION__ );

            while( m_Running.load( ) )
            {
                fd_set readSet;
                FD_ZERO( &readSet );
                FD_SET( listenSocket, &readSet );

                timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000; // 100ms

                int result = select( 0, &readSet, nullptr, nullptr, &timeout );
                if( result == SOCKET_ERROR )
                {
                    closesocket( listenSocket );
                    WSACleanup( );
                    auto errorTask = TaskManager::Instance( ).SubmitMain( OnError, "select failed" );
                    ( void )errorTask;
                    return;
                }

                if( result <= 0 )
                {
                    // No incoming connections
                    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
                    continue;
                }

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

                LogDebug( "[%s] ListenServer connection established!", __FUNCTION__ );

                // Now receive and process the HTTP request from the browser.
                std::string response;
                if( !ReceiveHttpRequest( clientSocket, response ) )
                {
                    WarningLog( "[%s] ListenServer received an invalid or empty auth request.", __FUNCTION__ );
                    closesocket( clientSocket );
                    continue;
                }

                LogDebug( "[%s] ListenServer bytes received!, bytes: %zu", __FUNCTION__, response.size( ) );

                // Build the dynamic auth page - substitute brand colour and service name
                // based on whichever provider is currently signing in.
                std::string brandColor = "#888888";
                std::string serviceName = "Service";

                if( auto sink = m_Sink.lock( ) )
                {
                    const int signingInProvider = sink->GetSigningInProvider( );
                    if( signingInProvider != INVALID_CONTENT_PROVIDER )
                    {
                        for( const auto& svc : m_Services )
                        {
                            if( static_cast<int>( svc->GetContentProvider( ) ) == signingInProvider )
                            {
                                brandColor  = svc->GetBrandColor( );
                                serviceName = svc->GetProviderDisplayName( );
                                break;
                            }
                        }
                    }
                }

                auto ReplaceAll = []( std::string& str, const std::string& from, const std::string& to )
                    {
                        std::size_t pos = 0;
                        while( ( pos = str.find( from, pos ) ) != std::string::npos )
                        {
                            str.replace( pos, from.length( ), to );
                            pos += to.length( );
                        }
                    };

                std::string authHtml = m_AuthHtmlTemplate;
                ReplaceAll( authHtml, "{{BRAND_COLOR}}",  brandColor );
                ReplaceAll( authHtml, "{{SERVICE_NAME}}", serviceName );

                const int contentLength = static_cast< int >( authHtml.length( ) );

                std::string httpResponse = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: " + std::to_string( contentLength ) + "\r\n"
                    "\r\n";

                httpResponse += authHtml;

                int bytesSent = send( clientSocket, httpResponse.c_str( ), static_cast< int >( httpResponse.length( ) ), 0 );
                if( bytesSent == SOCKET_ERROR )
                {
                    WarningLog( "[%s] ListenServer failed to send http response.", __FUNCTION__ );                    
                }
                else
                {
                    LogDebug( "[%s] ListenServer sent http response successfully!", __FUNCTION__ );
                }                

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
    if( m_Running.load( ) )
    {
        return;
    }

    m_CurrentRetries = 0;
}
