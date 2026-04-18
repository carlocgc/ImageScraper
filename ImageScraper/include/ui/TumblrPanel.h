#pragma once

#include "ui/IProviderPanel.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <string>
#include <memory>

namespace ImageScraper
{
    class TumblrPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::Tumblr; }
        bool             CanSignIn( ) const override { return true; }
        bool             IsReadyToRun( ) const override { return !m_TumblrUser.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadPanelState( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory   m_SearchHistory{ };
        std::string     m_TumblrUser{ };
        int             m_TumblrMaxMediaItems{ TUMBLR_LIMIT_DEFAULT };
        std::shared_ptr<JsonFile> m_AppConfig{ };
    };
}
