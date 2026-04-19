#include "ui/ProviderSearchInput.h"

#include "services/ServiceOptionTypes.h"

void ImageScraper::ProviderSearchInput::Draw( const ProviderSearchInputConfig& config, std::string& value, const SearchHistory& history )
{
    if( ImGui::BeginChild( config.m_ChildId, config.m_Size, false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, value.c_str( ) );

        const float arrowWidth = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        ImGui::SetNextItemWidth( ImGui::CalcItemWidth( ) - arrowWidth - spacing );
        if( ImGui::InputText( config.m_InputId, buffer, INPUT_STRING_MAX, config.m_InputFlags ) )
        {
            value = buffer;
        }

        const ImVec2 inputMin = ImGui::GetItemRectMin( );
        const ImVec2 inputMax = ImGui::GetItemRectMax( );

        ImGui::SameLine( 0.f, spacing );
        if( ImGui::ArrowButton( config.m_HistoryButtonId, ImGuiDir_Down ) )
        {
            ImGui::OpenPopup( config.m_HistoryPopupId );
        }

        ImGui::SameLine( 0.f, spacing );
        ImGui::TextUnformatted( config.m_Label );

        const float popupWidth = ( inputMax.x - inputMin.x ) + spacing + arrowWidth;
        ImGui::SetNextWindowPos( ImVec2( inputMin.x, inputMax.y ), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2( popupWidth, 0.f ), ImGuiCond_Always );
        if( ImGui::BeginPopup( config.m_HistoryPopupId, ImGuiWindowFlags_NoFocusOnAppearing ) )
        {
            if( history.IsEmpty( ) )
            {
                ImGui::TextDisabled( "No history yet" );
            }
            else
            {
                for( const auto& item : history.GetItems( ) )
                {
                    if( ImGui::Selectable( item.c_str( ) ) )
                    {
                        value = item;
                    }
                }
            }

            ImGui::EndPopup( );
        }
    }

    ImGui::EndChild( );
}
