#pragma once

#include "ui/IProviderPanel.h"
#include "ui/DeleteAllButton.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <string>

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
        void             LoadSearchHistory( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory   m_SearchHistory{ };
        std::string     m_TumblrUser{ };
        int             m_TumblrMaxMediaItems{ TUMBLR_LIMIT_DEFAULT };
        DeleteAllButton m_DeleteAllButton{ "Tumblr", "Tumblr" };
    };
}
