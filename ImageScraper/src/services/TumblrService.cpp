#include "services/TumblrService.h"
#include "io/JsonFile.h"
#include "log/Logger.h"

const std::string ImageScraper::TumblrService::s_UserDataKey_ApiKey = "tumblr_api_key";

ImageScraper::TumblrService::TumblrService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
    : Service( appConfig, userConfig, caBundle, frontEnd )
{
    if( !m_UserConfig->GetValue<std::string>( s_UserDataKey_ApiKey, m_ApiKey ) )
    {
        WarningLog( "[%s] Could not find tumblr api key, add token to %s to be able to download tumblr content!", __FUNCTION__, m_UserConfig->GetFilePath( ).c_str( ) );
    }
}

bool ImageScraper::TumblrService::HandleUserInput( const UserInputOptions& options )
{
    if( options.m_Provider != ContentProvider::Tumblr )
    {
        return false;
    }

    DownloadContent( options );
    return true;
}

void ImageScraper::TumblrService::DownloadContent( const UserInputOptions& inputOptions )
{

}
