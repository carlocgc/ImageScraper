#include "ui/CredentialsPanel.h"
#include "log/Logger.h"

#include "imgui/imgui.h"

static const std::string s_Key_RedditClientId     = "reddit_client_id";
static const std::string s_Key_RedditClientSecret = "reddit_client_secret";
static const std::string s_Key_TumblrApiKey       = "tumblr_api_key";
static const std::string s_Key_DiscordClientId    = "discord_client_id";
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
    load( s_Key_DiscordClientId,     m_DiscordClientId );
    load( s_Key_DiscordClientSecret, m_DiscordClientSecret );
}

void ImageScraper::CredentialsPanel::Update( )
{
    ImGui::SetNextWindowSize( ImVec2( 500, 320 ), ImGuiCond_FirstUseEver );

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

    // --- Reddit ---
    ImGui::SeparatorText( "Reddit" );
    InputField( "Client ID",     "##reddit_id",     m_RedditClientId,     m_ShowRedditSecret, false, s_Key_RedditClientId,     true );
    InputField( "Client Secret", "##reddit_secret", m_RedditClientSecret, m_ShowRedditSecret, true,  s_Key_RedditClientSecret, true );

    ImGui::Spacing( );

    // --- Tumblr ---
    ImGui::SeparatorText( "Tumblr" );
    InputField( "API Key", "##tumblr_key", m_TumblrApiKey, m_ShowTumblrKey, true, s_Key_TumblrApiKey, true );

    ImGui::Spacing( );

    // --- Discord ---
    ImGui::SeparatorText( "Discord" );
    InputField( "Client ID",     "##discord_id",     m_DiscordClientId,     m_ShowDiscordSecret, false, s_Key_DiscordClientId,     false );
    InputField( "Client Secret", "##discord_secret", m_DiscordClientSecret, m_ShowDiscordSecret, true,  s_Key_DiscordClientSecret, false );

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
    }
    else
    {
        DebugLog( "[%s] Saved credential: %s", __FUNCTION__, key.c_str( ) );
    }
}
