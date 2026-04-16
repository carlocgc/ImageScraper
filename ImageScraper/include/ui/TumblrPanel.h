#pragma once

#include "ui/IProviderPanel.h"
#include "imgui/imgui.h"

#include <string>

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

    private:
        void SaveSearchHistory( );

        std::shared_ptr<JsonFile> m_AppConfig{ };
        std::string m_TumblrUser{ };
        int         m_TumblrMaxMediaItems{ TUMBLR_LIMIT_DEFAULT };
    };
}
