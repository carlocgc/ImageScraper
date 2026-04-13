#pragma once

#include "ui/IProviderPanel.h"
#include "imgui/imgui.h"

#include <string>

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

    private:
        std::string          m_SubredditName{ };
        RedditScope          m_RedditScope{ RedditScope::Hot };
        RedditScopeTimeFrame m_RedditScopeTimeFrame{ RedditScopeTimeFrame::All };
        int                  m_RedditMaxMediaItems{ REDDIT_LIMIT_DEFAULT };
    };
}
