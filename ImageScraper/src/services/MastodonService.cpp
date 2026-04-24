#include "services/MastodonService.h"

#include "async/TaskManager.h"
#include "log/Logger.h"

ImageScraper::MastodonService::MastodonService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, const std::string& outputDir, std::shared_ptr<IServiceSink> sink )
    : Service( ContentProvider::Mastodon, appConfig, userConfig, caBundle, outputDir, sink )
{
}

bool ImageScraper::MastodonService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Mastodon )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

bool ImageScraper::MastodonService::OpenExternalAuth( )
{
    LogError( "[%s] Sign in not implemented for this provider!", __FUNCTION__ );
    return false;
}

bool ImageScraper::MastodonService::HandleExternalAuth( const std::string& response )
{
    ( void )response;
    return false;
}

bool ImageScraper::MastodonService::IsSignedIn( ) const
{
    return false;
}

void ImageScraper::MastodonService::Authenticate( AuthenticateCallback callback )
{
    callback( m_ContentProvider, true );
}

bool ImageScraper::MastodonService::IsCancelled( )
{
    return m_Sink->IsCancelled( );
}

void ImageScraper::MastodonService::DownloadContent( const UserInputOptions& inputOptions )
{
    auto task = TaskManager::Instance( ).Submit( TaskManager::s_ServiceContext, [ this, inputOptions ]( )
        {
            InfoLog( "[%s] Mastodon provider shell selected for instance: %s, account: %s, max downloads: %i", __FUNCTION__, inputOptions.m_MastodonInstance.c_str( ), inputOptions.m_MastodonAccount.c_str( ), inputOptions.m_MastodonMaxMediaItems );
            WarningLog( "[%s] Mastodon media scraping is not implemented yet.", __FUNCTION__ );

            TaskManager::Instance( ).SubmitMain( [ this ]( )
                {
                    m_Sink->OnRunComplete( );
                } );
        } );

    ( void )task;
}
