#pragma once

#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <string>

namespace ImageScraper
{
    struct ProviderSearchInputConfig
    {
        const char* m_ChildId{ nullptr };
        const char* m_InputId{ nullptr };
        const char* m_HistoryButtonId{ nullptr };
        const char* m_HistoryPopupId{ nullptr };
        const char* m_Label{ nullptr };
        ImGuiInputTextFlags m_InputFlags{ ImGuiInputTextFlags_None };
        ImVec2 m_Size{ 500.f, 25.f };
    };

    namespace ProviderSearchInput
    {
        void Draw( const ProviderSearchInputConfig& config, std::string& value, const SearchHistory& history );
    }
}
