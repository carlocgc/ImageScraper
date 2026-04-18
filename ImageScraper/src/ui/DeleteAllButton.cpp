#include "ui/DeleteAllButton.h"
#include "log/Logger.h"

ImageScraper::DeleteAllButton::DeleteAllButton( std::string providerSubDir, std::string displayName )
    : m_ProviderSubDir{ std::move( providerSubDir ) }
    , m_DisplayName{    std::move( displayName )    }
    , m_ButtonId{ "Delete All##del_" + m_ProviderSubDir }
    , m_PopupId{  "Confirm Delete All##del_" + m_ProviderSubDir }
{
}

void ImageScraper::DeleteAllButton::Update( const std::string& outputDir,
                                            const std::function<void( const std::string& )>& onDeleteAll,
                                            bool extraDisabled )
{
    const std::filesystem::path providerDir =
        std::filesystem::path( outputDir ) / "Downloads" / m_ProviderSubDir;

    const bool hasContent = HasAnyFile( providerDir );

    ImGui::BeginDisabled( !hasContent || extraDisabled );

    if( ImGui::Button( m_ButtonId.c_str( ), ImVec2( 100, 40 ) ) )
    {
        ImGui::OpenPopup( m_PopupId.c_str( ) );
    }

    ImGui::EndDisabled( );

    ImGui::SetNextWindowSize( ImVec2( 380, 0 ), ImGuiCond_Always );
    if( ImGui::BeginPopupModal( m_PopupId.c_str( ), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::Spacing( );
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.6f, 0.1f, 1.0f ) );
        ImGui::TextWrapped( "This will permanently delete all downloaded %s content from disk.",
                            m_DisplayName.c_str( ) );
        ImGui::PopStyleColor( );
        ImGui::Spacing( );

        ImGui::PushStyleColor( ImGuiCol_Button,        ImVec4( 0.7f, 0.3f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.9f, 0.4f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,  ImVec4( 0.5f, 0.2f, 0.0f, 1.0f ) );

        if( ImGui::Button( "Delete All", ImVec2( 110, 0 ) ) )
        {
            std::error_code ec;
            std::filesystem::remove_all( providerDir, ec );
            if( ec )
            {
                LogError( "[%s] Failed to delete %s downloads: %s",
                          __FUNCTION__, m_DisplayName.c_str( ), ec.message( ).c_str( ) );
            }

            if( onDeleteAll )
            {
                onDeleteAll( providerDir.string( ) );
            }

            ImGui::CloseCurrentPopup( );
        }

        ImGui::PopStyleColor( 3 );

        ImGui::SameLine( );
        if( ImGui::Button( "Cancel", ImVec2( 100, 0 ) ) )
        {
            ImGui::CloseCurrentPopup( );
        }

        ImGui::EndPopup( );
    }
}

bool ImageScraper::DeleteAllButton::HasAnyFile( const std::filesystem::path& dir )
{
    if( !std::filesystem::exists( dir ) )
    {
        return false;
    }
    std::error_code ec;
    for( const auto& entry : std::filesystem::recursive_directory_iterator( dir, ec ) )
    {
        if( entry.is_regular_file( ) )
        {
            return true;
        }
    }
    return false;
}
