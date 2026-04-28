#include "ui/DownloadOptionControls.h"

#include "services/ServiceOptionTypes.h"

#include <algorithm>
#include <cstring>

namespace
{
    float GetLabelWidth( const char* label )
    {
        if( label == nullptr || label[ 0 ] == '\0' )
        {
            return 0.0f;
        }

        return ImGui::CalcTextSize( label ).x;
    }

    constexpr float k_MaxWidgetWidth = 220.0f;

    float GetWidgetWidth( const char* label, float trailingControlsWidth )
    {
        const ImGuiStyle& style = ImGui::GetStyle( );
        const float labelWidth = GetLabelWidth( label );
        const float spacing = style.ItemInnerSpacing.x;
        const float reservedWidth = trailingControlsWidth + labelWidth + ( labelWidth > 0.0f ? spacing : 0.0f );
        const float availableWidth = ( std::max )( 1.0f, ImGui::GetContentRegionAvail( ).x - reservedWidth );
        const float maxWidth = ( std::max )( 1.0f, k_MaxWidgetWidth - trailingControlsWidth );
        return ( std::min )( availableWidth, maxWidth );
    }

    bool BeginRow( const char* rowId )
    {
        return ImGui::BeginChild( rowId, ImVec2( 0.0f, ImGui::GetFrameHeight( ) ), false );
    }

    void EndRow( )
    {
        ImGui::EndChild( );
    }

    void DrawTooltip( const char* tooltip )
    {
        if( tooltip != nullptr && tooltip[ 0 ] != '\0' && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        {
            ImGui::SetTooltip( "%s", tooltip );
        }
    }

    void DrawEndLabel( const char* label, const char* tooltip )
    {
        if( label == nullptr || label[ 0 ] == '\0' )
        {
            return;
        }

        ImGui::SameLine( 0.0f, ImGui::GetStyle( ).ItemInnerSpacing.x );
        ImGui::TextUnformatted( label );
        DrawTooltip( tooltip );
    }
}

bool ImageScraper::DownloadOptionControls::DrawSearchInput( const DownloadOptionSearchConfig& config, std::string& value, const SearchHistory& history )
{
    bool changed = false;

    if( BeginRow( config.m_RowId ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, value.c_str( ) );

        const float arrowWidth = ImGui::GetFrameHeight( );
        const float spacing = ImGui::GetStyle( ).ItemInnerSpacing.x;
        const float inputWidth = GetWidgetWidth( config.m_Label, arrowWidth + spacing );

        ImGui::SetNextItemWidth( inputWidth );
        if( ImGui::InputText( config.m_InputId, buffer, INPUT_STRING_MAX, config.m_InputFlags ) )
        {
            value = buffer;
            changed = true;
        }
        DrawTooltip( config.m_Tooltip );

        const ImVec2 inputMin = ImGui::GetItemRectMin( );
        const ImVec2 inputMax = ImGui::GetItemRectMax( );

        ImGui::SameLine( 0.0f, spacing );
        if( ImGui::ArrowButton( config.m_HistoryButtonId, ImGuiDir_Down ) )
        {
            ImGui::OpenPopup( config.m_HistoryPopupId );
        }
        DrawTooltip( config.m_Tooltip );

        DrawEndLabel( config.m_Label, config.m_Tooltip );

        const float popupWidth = inputWidth + spacing + arrowWidth;
        ImGui::SetNextWindowPos( ImVec2( inputMin.x, inputMax.y ), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2( popupWidth, 0.0f ), ImGuiCond_Always );
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
                        changed = true;
                    }
                }
            }

            ImGui::EndPopup( );
        }
    }

    EndRow( );
    return changed;
}

bool ImageScraper::DownloadOptionControls::DrawCombo( const DownloadOptionFieldConfig& config, int& currentIndex, const char* const* items, int itemCount )
{
    bool changed = false;

    if( BeginRow( config.m_RowId ) )
    {
        ImGui::SetNextItemWidth( GetWidgetWidth( config.m_Label, 0.0f ) );
        changed = ImGui::Combo( config.m_WidgetId, &currentIndex, items, itemCount );
        DrawTooltip( config.m_Tooltip );
        DrawEndLabel( config.m_Label, config.m_Tooltip );
    }

    EndRow( );
    return changed;
}

bool ImageScraper::DownloadOptionControls::DrawClampedInputInt( const DownloadOptionFieldConfig& config, int& value, int minValue, int maxValue )
{
    const int previousValue = value;

    if( BeginRow( config.m_RowId ) )
    {
        ImGui::SetNextItemWidth( GetWidgetWidth( config.m_Label, 0.0f ) );
        ImGui::InputInt( config.m_WidgetId, &value );
        DrawTooltip( config.m_Tooltip );
        DrawEndLabel( config.m_Label, config.m_Tooltip );
    }

    EndRow( );

    value = std::clamp( value, minValue, maxValue );
    return value != previousValue;
}
