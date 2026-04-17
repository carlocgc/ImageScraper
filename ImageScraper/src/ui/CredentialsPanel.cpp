#include "ui/CredentialsPanel.h"
#include "log/Logger.h"

#include "imgui/imgui.h"

#include <filesystem>

static const std::string s_Key_RedditClientId     = "reddit_client_id";
static const std::string s_Key_RedditClientSecret = "reddit_client_secret";
static const std::string s_Key_TumblrApiKey        = "tumblr_api_key";
static const std::string s_Key_TumblrClientSecret = "tumblr_client_secret";
static const std::string s_Key_DiscordClientId     = "discord_client_id";
static const std::string s_Key_DiscordClientSecret = "discord_client_secret";

ImageScraper::CredentialsPanel::CredentialsPanel( std::shared_ptr<JsonFile> userConfig )
    : m_UserConfig{ userConfig }
{
    if( !m_UserConfig )
    {
        return;
    }

    std::string value;

    auto load = [ & ]( const std::string& key, std::array<char, k_BufSize>& buf )
        {
            value.clear( );
            if( m_UserConfig->GetValue<std::string>( key, value ) )
            {
                strncpy_s( buf.data( ), k_BufSize, value.c_str( ), k_BufSize - 1 );
            }
        };

    load( s_Key_RedditClientId,      m_RedditClientId );
    load( s_Key_RedditClientSecret,  m_RedditClientSecret );
    load( s_Key_TumblrApiKey,        m_TumblrApiKey );
    load( s_Key_TumblrClientSecret,  m_TumblrClientSecret );
    load( s_Key_DiscordClientId,     m_DiscordClientId );
    load( s_Key_DiscordClientSecret, m_DiscordClientSecret );
}

void ImageScraper::CredentialsPanel::Update( )
{
    ImGui::SetNextWindowSize( ImVec2( 500, 360 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Credentials" ) )
    {
        ImGui::End( );
        return;
    }

    ImGui::TextDisabled( "Changes are saved automatically." );
    ImGui::Spacing( );

    const ImVec4 red   = ImVec4( 1.0f, 0.3f, 0.3f, 1.0f );
    const ImVec4 grey  = ImVec4( 0.7f, 0.7f, 0.7f, 1.0f );
    constexpr float labelW = 160.0f;

    auto InputField = [ & ]( const char* label, const char* id, std::array<char, k_BufSize>& buf,
                              bool& showToggle, bool hasToggle, const std::string& key, bool required )
        {
            bool empty = ( buf[ 0 ] == '\0' );
            if( required && empty )
            {
                ImGui::TextColored( red, "* " );
            }
            else
            {
                ImGui::TextColored( grey, "  " );
            }
            ImGui::SameLine( );
            ImGui::Text( "%s", label );
            ImGui::SameLine( labelW );

            ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
            if( hasToggle && !showToggle )
            {
                flags = ImGuiInputTextFlags_Password;
            }

            ImGui::SetNextItemWidth( 220.0f );
            if( ImGui::InputText( id, buf.data( ), k_BufSize, flags ) )
            {
                SaveField( key, buf.data( ) );
            }

            if( hasToggle )
            {
                ImGui::SameLine( );
                const char* toggleLabel = showToggle ? "Hide##" : "Show##";
                std::string toggleId = std::string( toggleLabel ) + id;
                if( ImGui::Button( toggleId.c_str( ) ) )
                {
                    showToggle = !showToggle;
                }
            }
        };

    auto TooltipWithCopy = [ ]( const char* tip, const char* url )
        {
            if( ImGui::IsItemHovered( ) )
            {
                ImGui::SetTooltip( "%s", tip );
            }
            if( ImGui::IsItemHovered( ) && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
            {
                ImGui::SetClipboardText( url );
            }
        };

    // --- Reddit ---
    ImGui::SeparatorText( "Reddit" );
    InputField( "Client ID",     "##reddit_id",     m_RedditClientId,     m_ShowRedditSecret, false, s_Key_RedditClientId,     true );
    TooltipWithCopy( "OAuth2 app Client ID.\nRight-click to copy registration URL.", "https://www.reddit.com/prefs/apps" );
    InputField( "Client Secret", "##reddit_secret", m_RedditClientSecret, m_ShowRedditSecret, true,  s_Key_RedditClientSecret, true );
    TooltipWithCopy( "OAuth2 app Client Secret.\nRight-click to copy registration URL.", "https://www.reddit.com/prefs/apps" );

    ImGui::Spacing( );

    // --- Tumblr ---
    ImGui::SeparatorText( "Tumblr" );
    InputField( "Consumer Key",    "##tumblr_key",    m_TumblrApiKey,       m_ShowTumblrKey,    false, s_Key_TumblrApiKey,        true  );
    TooltipWithCopy( "OAuth Consumer Key - required for downloads.\nRight-click to copy registration URL.", "https://www.tumblr.com/oauth/apps" );
    InputField( "Consumer Secret", "##tumblr_secret", m_TumblrClientSecret, m_ShowTumblrSecret, true,  s_Key_TumblrClientSecret,  false );
    TooltipWithCopy( "OAuth Consumer Secret - only needed for Sign In.\nRight-click to copy registration URL.", "https://www.tumblr.com/oauth/apps" );

    ImGui::Spacing( );

    // --- Discord (work in progress) ---
    ImGui::SeparatorText( "Discord" );
    ImGui::TextDisabled( "Work in progress - not yet functional." );
    ImGui::BeginDisabled( );
    InputField( "Client ID",     "##discord_id",     m_DiscordClientId,     m_ShowDiscordSecret, false, s_Key_DiscordClientId,     false );
    InputField( "Client Secret", "##discord_secret", m_DiscordClientSecret, m_ShowDiscordSecret, true,  s_Key_DiscordClientSecret, false );
    ImGui::EndDisabled( );

#ifdef _DEBUG
    ImGui::Spacing( );
    ImGui::Separator( );
    ImGui::Spacing( );
    ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.2f, 1.0f ), "Dev" );
    ImGui::SameLine( );
    ImGui::Checkbox( "Save credentials to source data/", &m_SaveDevCredentials );
    if( ImGui::IsItemHovered( ) )
    {
        ImGui::SetTooltip( "Backs up config.json to ImageScraper/data/ on every save.\nDebug builds only - keeps dev credentials persistent across rebuilds." );
    }
#endif

    ImGui::End( );
}

void ImageScraper::CredentialsPanel::SaveField( const std::string& key, const char* value )
{
    if( !m_UserConfig )
    {
        return;
    }

    m_UserConfig->SetValue<std::string>( key, std::string( value ) );
    if( !m_UserConfig->Serialise( ) )
    {
        WarningLog( "[%s] Failed to save credentials to config file.", __FUNCTION__ );
        return;
    }

    LogDebug( "[%s] Saved credential: %s", __FUNCTION__, key.c_str( ) );

#ifdef _DEBUG
    if( m_SaveDevCredentials )
    {
        // Derive source data directory from the compile-time path of this file:
        // CredentialsPanel.cpp lives at ImageScraper/src/ui/, so three parent_path() calls
        // reach ImageScraper/, then append data/config.json.
        const std::filesystem::path devDataPath = std::filesystem::path( __FILE__ )
            .parent_path( )   // src/ui/
            .parent_path( )   // src/
            .parent_path( )   // ImageScraper/
            / "data" / "config.json";

        std::error_code ec;
        std::filesystem::copy_file(
            std::filesystem::path( m_UserConfig->GetFilePath( ) ),
            devDataPath,
            std::filesystem::copy_options::overwrite_existing,
            ec );

        if( ec )
        {
            WarningLog( "[%s] Failed to back up credentials to source: %s", __FUNCTION__, ec.message( ).c_str( ) );
        }
        else
        {
            LogDebug( "[%s] Credentials backed up to %s", __FUNCTION__, devDataPath.string( ).c_str( ) );
        }
    }
#endif
}
