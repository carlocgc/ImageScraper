#include "services/DiscordService.h"
#include "services/Service.h"
#include "io/JsonFile.h"
#include "log/Logger.h"

using Json = nlohmann::json;

const std::string ImageScraper::DiscordService::s_UserDataKey_ClientId = "discord_client_id";
const std::string ImageScraper::DiscordService::s_UserDataKey_ClientSecret = "discord_client_secret";

ImageScraper::DiscordService::DiscordService( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle, std::shared_ptr<FrontEnd> frontEnd )
    : Service( ContentProvider::Discord, appConfig, userConfig, caBundle, frontEnd )
{
    if( !m_UserConfig->GetValue( s_UserDataKey_ClientId, m_ClientId ) )
    {
        WarningLog( "[%s] Could not find discord client id, add client id to %s to be able to authenticate with the discord api!", __FUNCTION__, m_UserConfig->GetFilePath( ).c_str( ) );
    }

    if( !m_UserConfig->GetValue( s_UserDataKey_ClientSecret, m_ClientSecret ) )
    {
        WarningLog( "[%s] Could not find discord client secret, add client secret to %s to be able to authenticate with the discord api!", __FUNCTION__, m_UserConfig->GetFilePath( ).c_str( ) );
    }
}

bool ImageScraper::DiscordService::IsCancelled( )
{
    throw std::logic_error( "The method or operation is not implemented." );
}

bool ImageScraper::DiscordService::HandleUserInput( const UserInputOptions& options )
{
    throw std::logic_error( "The method or operation is not implemented." );
}

bool ImageScraper::DiscordService::OpenExternalAuth( )
{
    throw std::logic_error( "The method or operation is not implemented." );
}

bool ImageScraper::DiscordService::HandleExternalAuth( const std::string& response )
{
    throw std::logic_error( "The method or operation is not implemented." );
}

bool ImageScraper::DiscordService::IsSignedIn( ) const
{
    throw std::logic_error( "The method or operation is not implemented." );
}

void ImageScraper::DiscordService::Authenticate( AuthenticateCallback callback )
{
    throw std::logic_error( "The method or operation is not implemented." );
}
