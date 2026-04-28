#pragma once

#include "ui/IProviderPanel.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <memory>
#include <string>

namespace ImageScraper
{
    class RedgifsPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::Redgifs; }
        bool             CanSignIn( ) const override { return false; }
        bool             IsReadyToRun( ) const override { return !m_RedgifsUser.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadPanelState( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory             m_SearchHistory{ };
        std::string               m_RedgifsUser{ };
        int                       m_RedgifsMaxMediaItems{ REDGIFS_LIMIT_DEFAULT };
        std::shared_ptr<JsonFile> m_AppConfig{ };
    };
}
