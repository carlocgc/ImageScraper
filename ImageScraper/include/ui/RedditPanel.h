#pragma once

#include "ui/IProviderPanel.h"
#include "imgui/imgui.h"

#include <string>
#include <vector>

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
        void             LoadSearchHistory( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        void SaveSearchHistory( );
        void PushToHistory( const std::string& value );

        static constexpr int k_MaxHistory = 5;

        std::shared_ptr<JsonFile>  m_AppConfig{ };
        std::string                m_SubredditName{ };
        std::vector<std::string>   m_SearchHistory{ };
        RedditScope                m_RedditScope{ RedditScope::Hot };
        RedditScopeTimeFrame       m_RedditScopeTimeFrame{ RedditScopeTimeFrame::All };
        int                        m_RedditMaxMediaItems{ REDDIT_LIMIT_DEFAULT };
    };
}
