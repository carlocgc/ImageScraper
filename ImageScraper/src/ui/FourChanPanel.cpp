#include "ui/FourChanPanel.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::FourChanPanel::LoadSearchHistory( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = std::move( appConfig );
    if( !m_AppConfig )
    {
        return;
    }

    Json arr;
    if( m_AppConfig->GetValue<Json>( "fourchan_board_history", arr ) && arr.is_array( ) )
    {
        for( const auto& item : arr )
        {
            if( item.is_string( ) )
            {
                m_SearchHistory.push_back( item.get<std::string>( ) );
            }
        }
        if( static_cast<int>( m_SearchHistory.size( ) ) > k_MaxHistory )
        {
            m_SearchHistory.resize( k_MaxHistory );
        }
    }

    if( !m_SearchHistory.empty( ) )
    {
        m_FourChanBoard = m_SearchHistory.front( );
    }
}

void ImageScraper::FourChanPanel::SaveSearchHistory( )
{
    if( !m_AppConfig )
    {
        return;
    }

    Json arr = Json::array( );
    for( const auto& item : m_SearchHistory )
    {
        arr.push_back( item );
    }

    m_AppConfig->SetValue<Json>( "fourchan_board_history", arr );
    if( !m_AppConfig->Serialise( ) )
    {
        WarningLog( "[%s] Failed to save FourChan search history", __FUNCTION__ );
    }
}

void ImageScraper::FourChanPanel::OnSearchCommitted( )
{
    PushToHistory( m_FourChanBoard );
}

void ImageScraper::FourChanPanel::PushToHistory( const std::string& value )
{
    if( value.empty( ) )
    {
        return;
    }

    auto it = std::find( m_SearchHistory.begin( ), m_SearchHistory.end( ), value );
    if( it != m_SearchHistory.end( ) )
    {
        m_SearchHistory.erase( it );
    }

    m_SearchHistory.insert( m_SearchHistory.begin( ), value );

    if( static_cast<int>( m_SearchHistory.size( ) ) > k_MaxHistory )
    {
        m_SearchHistory.resize( k_MaxHistory );
    }

    SaveSearchHistory( );
}

void ImageScraper::FourChanPanel::Update( )
{
    if( ImGui::BeginChild( "FourChanBoard", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_FourChanBoard.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        const float arrowW  = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        ImGui::SetNextItemWidth( ImGui::CalcItemWidth( ) - arrowW - spacing );
        if( ImGui::InputText( "##fourchan_board", buffer, INPUT_STRING_MAX, flags ) )
        {
            m_FourChanBoard = buffer;
        }

        // Capture screen rect of the input field before adding the button
        const ImVec2 inputMin = ImGui::GetItemRectMin( );
        const ImVec2 inputMax = ImGui::GetItemRectMax( );

        ImGui::SameLine( 0.f, spacing );
        if( ImGui::ArrowButton( "##fourchan_hist_btn", ImGuiDir_Down ) )
        {
            ImGui::OpenPopup( "##fourchan_hist" );
        }

        ImGui::SameLine( 0.f, spacing );
        ImGui::TextUnformatted( "Board (e.g. v, sci )" );

        // Anchor popup directly below the input field, spanning input + button width
        const float popupW = ( inputMax.x - inputMin.x ) + spacing + arrowW;
        ImGui::SetNextWindowPos( ImVec2( inputMin.x, inputMax.y ), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2( popupW, 0.f ), ImGuiCond_Always );
        if( ImGui::BeginPopup( "##fourchan_hist", ImGuiWindowFlags_NoFocusOnAppearing ) )
        {
            if( m_SearchHistory.empty( ) )
            {
                ImGui::TextDisabled( "No history yet" );
            }
            else
            {
                for( const auto& item : m_SearchHistory )
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
