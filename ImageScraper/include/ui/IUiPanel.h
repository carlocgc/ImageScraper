#pragma once

#include <cstdint>

namespace ImageScraper
{
    enum class InputState : uint8_t
    {
        Free,
        Blocked
    };

    class IUiPanel
    {
    public:
        virtual ~IUiPanel( ) = default;
        virtual void Update( ) = 0;
    };
}
