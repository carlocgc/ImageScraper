#pragma once

#include "imgui/imgui.h"

#include <string>

namespace ImageScraper
{
    struct ProgressPopupContent
    {
        float       m_ProgressFraction{ 0.0f };
        float       m_ProgressBarWidth{ 360.0f };
        std::string m_Message{ };
        std::string m_PrimaryDetail{ };
        std::string m_SecondaryDetail{ };
        std::string m_CurrentPath{ };
        std::string m_ActionLabel{ };
        ImVec2      m_ActionButtonSize{ 0.0f, 0.0f };
    };

    bool DrawProgressPopupContents( const ProgressPopupContent& content );
}
