#include "services/OAuthServiceHelpers.h"

#include "async/TaskManager.h"
#include "auth/OAuthClient.h"
#include "log/Logger.h"

#include <utility>

bool ImageScraper::OAuthServiceHelpers::OpenExternalAuth( OAuthClient& client )
{
    return client.OpenAuth( );
}

bool ImageScraper::OAuthServiceHelpers::HandleExternalAuth( ContentProvider provider,
                                                            const std::string& providerName,
                                                            const std::shared_ptr<IServiceSink>& sink,
                                                            OAuthClient& client,
                                                            const std::string& response,
                                                            std::function<void( )> onAuthSuccess )
{
    const int signingInProvider = sink->GetSigningInProvider( );
    if( signingInProvider == INVALID_CONTENT_PROVIDER )
    {
        LogError( "[%s] %sService::HandleExternalAuth skipped, no signing in provider!", __FUNCTION__, providerName.c_str( ) );
        return false;
    }

    if( static_cast< ContentProvider >( signingInProvider ) != provider )
    {
        LogDebug( "[%s] %sService::HandleExternalAuth skipped, incorrect provider!", __FUNCTION__, providerName.c_str( ) );
        return false;
    }

    if( response.find( "favicon" ) != std::string::npos )
    {
        LogDebug( "[%s] %sService::HandleExternalAuth skipped, invalid message!", __FUNCTION__, providerName.c_str( ) );
        return false;
    }

    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ provider, providerName, sinkCopy = sink, &client, response, onAuthSuccess = std::move( onAuthSuccess ) ]( ) mutable
        {
            if( client.HandleAuth( response, providerName ) )
            {
                onAuthSuccess( );
                TaskManager::Instance( ).SubmitMain( [ provider, providerName, sinkCopy ]( )
                    {
                        sinkCopy->OnSignInComplete( provider );
                        InfoLog( "[%s] %s signed in successfully!", __FUNCTION__, providerName.c_str( ) );
                    } );
            }
            else
            {
                TaskManager::Instance( ).SubmitMain( [ provider, providerName, sinkCopy ]( )
                    {
                        sinkCopy->OnSignInComplete( provider );
                        LogError( "[%s] %s sign in failed.", __FUNCTION__, providerName.c_str( ) );
                    } );
            }
        } );

    ( void )task;
    return true;
}

void ImageScraper::OAuthServiceHelpers::Authenticate( ContentProvider provider,
                                                      const std::string& providerName,
                                                      OAuthClient& client,
                                                      Service::AuthenticateCallback callback,
                                                      std::function<void( )> onAuthenticated )
{
    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ provider, providerName, &client, callback = std::move( callback ), onAuthenticated = std::move( onAuthenticated ) ]( ) mutable
        {
            if( client.TryRefreshToken( ) )
            {
                onAuthenticated( );
                callback( provider, true );
                LogDebug( "[%s] %s authenticated successfully!", __FUNCTION__, providerName.c_str( ) );
            }
            else
            {
                callback( provider, false );
                LogDebug( "[%s] %s authentication failed.", __FUNCTION__, providerName.c_str( ) );
            }
        } );

    ( void )task;
}

void ImageScraper::OAuthServiceHelpers::SignOut( const std::string& providerName,
                                                 OAuthClient& client,
                                                 std::function<void( )> onSignedOut )
{
    client.SignOut( );
    onSignedOut( );

    InfoLog( "[%s] %s signed out.", __FUNCTION__, providerName.c_str( ) );
}
