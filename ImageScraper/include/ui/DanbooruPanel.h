#pragma once

#include "ui/IProviderPanel.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <memory>
#include <string>

namespace ImageScraper
{
    class DanbooruPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::Danbooru; }
        bool             CanSignIn( ) const override { return false; }
        bool             IsReadyToRun( ) const override { return !m_DanbooruQuery.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadPanelState( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory m_SearchHistory{ };
        std::string m_DanbooruQuery{ };
        int m_DanbooruMaxMediaItems{ DANBOORU_LIMIT_DEFAULT };
        std::shared_ptr<JsonFile> m_AppConfig{ };
    };
}
