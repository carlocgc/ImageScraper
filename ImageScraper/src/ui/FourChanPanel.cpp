#include "ui/FourChanPanel.h"
#include "log/Logger.h"

#include <algorithm>
#include <filesystem>

static bool HasAnyFile( const std::filesystem::path& dir )
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

void ImageScraper::FourChanPanel::LoadSearchHistory( std::shared_ptr<JsonFile> appConfig )
{
    m_SearchHistory.Load( std::move( appConfig ), "fourchan_board_history" );
    m_FourChanBoard = m_SearchHistory.GetMostRecent( );
}

void ImageScraper::FourChanPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_FourChanBoard );
}

void ImageScraper::FourChanPanel::Update( )
{
    if( ImGui::BeginChild( "FourChanBoard", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_FourChanBoard.c_str( ) );
        const float arrowW  = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        ImGui::SetNextItemWidth( ImGui::CalcItemWidth( ) - arrowW - spacing );
        if( ImGui::InputText( "##fourchan_board", buffer, INPUT_STRING_MAX, ImGuiInputTextFlags_CharsNoBlank ) )
        {
            m_FourChanBoard = buffer;
        }

        const ImVec2 inputMin = ImGui::GetItemRectMin( );
        const ImVec2 inputMax = ImGui::GetItemRectMax( );

        ImGui::SameLine( 0.f, spacing );
        if( ImGui::ArrowButton( "##fourchan_hist_btn", ImGuiDir_Down ) )
        {
            ImGui::OpenPopup( "##fourchan_hist" );
        }

        ImGui::SameLine( 0.f, spacing );
        ImGui::TextUnformatted( "Board (e.g. v, sci )" );

        const float popupW = ( inputMax.x - inputMin.x ) + spacing + arrowW;
        ImGui::SetNextWindowPos( ImVec2( inputMin.x, inputMax.y ), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2( popupW, 0.f ), ImGuiCond_Always );
        if( ImGui::BeginPopup( "##fourchan_hist", ImGuiWindowFlags_NoFocusOnAppearing ) )
        {
            if( m_SearchHistory.IsEmpty( ) )
            {
                ImGui::TextDisabled( "No history yet" );
            }
            else
            {
                for( const auto& item : m_SearchHistory.GetItems( ) )
                {
                    if( ImGui::Selectable( item.c_str( ) ) )
                    {
                        m_FourChanBoard = item;
                    }
                }
            }
            ImGui::EndPopup( );
        }
    }

    ImGui::EndChild( );

    if( ImGui::BeginChild( "FourChanMaxMediaItems", ImVec2( 500, 25 ), false ) )
    {
        ImGui::InputInt( "Max Downloads", &m_FourChanMaxMediaItems );
        m_FourChanMaxMediaItems = std::clamp( m_FourChanMaxMediaItems, FOURCHAN_MEDIA_MIN, FOURCHAN_MEDIA_MAX );
    }

    ImGui::EndChild( );

    const std::filesystem::path fourChanDir =
        std::filesystem::path( m_OutputDir ) / "Downloads" / "4chan";
    const bool hasContent = HasAnyFile( fourChanDir );

    ImGui::BeginDisabled( !hasContent || m_SigningIn );

    if( ImGui::Button( "Delete All##fourchan", ImVec2( 100, 40 ) ) )
    {
        ImGui::OpenPopup( "Confirm Delete All##fourchan" );
    }

    ImGui::EndDisabled( );

    ImGui::SetNextWindowSize( ImVec2( 380, 0 ), ImGuiCond_Always );
    if( ImGui::BeginPopupModal( "Confirm Delete All##fourchan", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::Spacing( );
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.6f, 0.1f, 1.0f ) );
        ImGui::TextWrapped( "This will permanently delete all downloaded 4chan content from disk." );
        ImGui::PopStyleColor( );
        ImGui::Spacing( );

        ImGui::PushStyleColor( ImGuiCol_Button,        ImVec4( 0.7f, 0.3f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.9f, 0.4f, 0.0f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive,  ImVec4( 0.5f, 0.2f, 0.0f, 1.0f ) );

        if( ImGui::Button( "Delete All", ImVec2( 110, 0 ) ) )
        {
            std::error_code ec;
            std::filesystem::remove_all( fourChanDir, ec );
            if( ec )
            {
                LogError( "[%s] Failed to delete 4chan downloads: %s",
                          __FUNCTION__, ec.message( ).c_str( ) );
            }

            if( m_OnDeleteAll )
            {
                m_OnDeleteAll( fourChanDir.string( ) );
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

ImageScraper::UserInputOptions ImageScraper::FourChanPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider              = ContentProvider::FourChan;
    options.m_FourChanBoard         = m_FourChanBoard;
    options.m_FourChanMaxThreads    = m_FourChanMaxThreads;
    options.m_FourChanMaxMediaItems = m_FourChanMaxMediaItems;
    return options;
}
