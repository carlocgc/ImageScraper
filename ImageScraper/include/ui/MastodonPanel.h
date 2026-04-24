#pragma once

#include "ui/IProviderPanel.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <memory>
#include <string>

namespace ImageScraper
{
    class MastodonPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::Mastodon; }
        bool             CanSignIn( ) const override { return false; }
        bool             IsReadyToRun( ) const override { return !m_MastodonInstance.empty( ) && !m_MastodonAccount.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadPanelState( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory             m_InstanceHistory{ };
        SearchHistory             m_AccountHistory{ };
        std::string               m_MastodonInstance{ };
        std::string               m_MastodonAccount{ };
        int                       m_MastodonMaxMediaItems{ MASTODON_LIMIT_DEFAULT };
        std::shared_ptr<JsonFile> m_AppConfig{ };
    };
}
