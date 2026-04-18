#pragma once

#include "ui/IProviderPanel.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <string>
#include <memory>

namespace ImageScraper
{
    class RedditPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::Reddit; }
        bool             CanSignIn( ) const override { return true; }
        bool             IsReadyToRun( ) const override { return !m_SubredditName.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadPanelState( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory        m_SearchHistory{ };
        std::string          m_SubredditName{ };
        RedditScope          m_RedditScope{ RedditScope::Hot };
        RedditScopeTimeFrame m_RedditScopeTimeFrame{ RedditScopeTimeFrame::All };
        int                  m_RedditMaxMediaItems{ REDDIT_LIMIT_DEFAULT };
        std::shared_ptr<JsonFile> m_AppConfig{ };
    };
}
