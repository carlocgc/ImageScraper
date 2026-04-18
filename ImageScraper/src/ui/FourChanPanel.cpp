#include "ui/FourChanPanel.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::FourChanPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "fourchan_board_history" );
    m_FourChanBoard = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = FOURCHAN_MEDIA_DEFAULT;
        if( m_AppConfig->GetValue<int>( "fourchan_max_downloads", saved ) )
        {
            m_FourChanMaxMediaItems = std::clamp( saved, FOURCHAN_MEDIA_MIN, FOURCHAN_MEDIA_MAX );
        }
    }
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
        const int prev = m_FourChanMaxMediaItems;
        ImGui::InputInt( "Max Downloads", &m_FourChanMaxMediaItems );
        m_FourChanMaxMediaItems = std::clamp( m_FourChanMaxMediaItems, FOURCHAN_MEDIA_MIN, FOURCHAN_MEDIA_MAX );
        if( m_FourChanMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "fourchan_max_downloads", m_FourChanMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }

    ImGui::EndChild( );
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
