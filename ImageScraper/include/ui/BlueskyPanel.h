#pragma once

#include "ui/IProviderPanel.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <memory>
#include <string>

namespace ImageScraper
{
    class BlueskyPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::Bluesky; }
        bool             CanSignIn( ) const override { return false; }
        bool             IsReadyToRun( ) const override { return !m_BlueskyActor.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadPanelState( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory            m_SearchHistory{ };
        std::string              m_BlueskyActor{ };
        int                      m_BlueskyMaxMediaItems{ BLUESKY_LIMIT_DEFAULT };
        std::shared_ptr<JsonFile> m_AppConfig{ };
    };
}
