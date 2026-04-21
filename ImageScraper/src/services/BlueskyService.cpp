#include "services/BlueskyService.h"

#include "async/TaskManager.h"
#include "log/Logger.h"

ImageScraper::BlueskyService::BlueskyService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : Service( ContentProvider::Bluesky, appConfig, userConfig, caBundle, outputDir, sink )
{
}

bool ImageScraper::BlueskyService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Bluesky )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::BlueskyService::OpenExternalAuth( )
{
    LogError( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
    return false;
}

bool ImageScraper::BlueskyService::HandleExternalAuth( const std::string& response )
{
    ( void )response;
    return false;
}

bool ImageScraper::BlueskyService::IsSignedIn( ) const
{
    return false;
}

void ImageScraper::BlueskyService::Authenticate( AuthenticateCallback callback )
{
    callback( m_ContentProvider, true );
}

bool ImageScraper::BlueskyService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

void ImageScraper::BlueskyService::DownloadContent( const UserInputOptions& inputOptions )
{
    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, actor = inputOptions.m_BlueskyActor ]( )
        {
            InfoLog( "[%s] Bluesky provider shell invoked for actor: %s", __FUNCTION__, actor.c_str( ) );
            WarningLog( "[%s] Bluesky fetching is not implemented yet. This PR only wires the provider shell and UI.", __FUNCTION__ );

            auto completeTask = TaskManager::Instance( ).SubmitMain( [ this ]( )
                {
                    m_Sink->OnRunComplete( );
                } );
            ( void )completeTask;
        } );

    ( void )task;
}
