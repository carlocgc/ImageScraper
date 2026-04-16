#pragma once

#include "ui/IProviderPanel.h"
#include "imgui/imgui.h"

#include <string>
#include <vector>

namespace ImageScraper
{
    class TumblrPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::Tumblr; }
        bool             CanSignIn( ) const override { return false; }
        bool             IsReadyToRun( ) const override { return !m_TumblrUser.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadSearchHistory( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        void SaveSearchHistory( );
        void PushToHistory( const std::string& value );

        static constexpr int k_MaxHistory = 5;

        std::shared_ptr<JsonFile> m_AppConfig{ };
        std::string               m_TumblrUser{ };
        std::vector<std::string>  m_SearchHistory{ };
        int                       m_TumblrMaxMediaItems{ TUMBLR_LIMIT_DEFAULT };
    };
}
