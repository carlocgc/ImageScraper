#pragma once

#include "imgui/imgui.h"
#include "ui/SearchHistory.h"

#include <string>

namespace ImageScraper
{
    struct DownloadOptionSearchConfig
    {
        const char* m_RowId{ nullptr };
        const char* m_InputId{ nullptr };
        const char* m_HistoryButtonId{ nullptr };
        const char* m_HistoryPopupId{ nullptr };
        const char* m_Label{ nullptr };
        const char* m_Tooltip{ nullptr };
        ImGuiInputTextFlags m_InputFlags{ ImGuiInputTextFlags_None };
    };

    struct DownloadOptionFieldConfig
    {
        const char* m_RowId{ nullptr };
        const char* m_WidgetId{ nullptr };
        const char* m_Label{ nullptr };
        const char* m_Tooltip{ nullptr };
    };

    namespace DownloadOptionControls
    {
        inline constexpr const char* s_MaxMediaDownloadsTooltip = "Maximum media files to download.\nExamples:\n5 - quick sample\n100 - larger batch";

        bool DrawSearchInput( const DownloadOptionSearchConfig& config, std::string& value, const SearchHistory& history );
        bool DrawCombo( const DownloadOptionFieldConfig& config, int& currentIndex, const char* const* items, int itemCount );
        bool DrawClampedInputInt( const DownloadOptionFieldConfig& config, int& value, int minValue, int maxValue );
    }
}
