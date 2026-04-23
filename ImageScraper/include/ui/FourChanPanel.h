#pragma once

#include "ui/IProviderPanel.h"
#include "ui/SearchHistory.h"
#include "imgui/imgui.h"

#include <string>
#include <memory>

namespace ImageScraper
{
    class FourChanPanel : public IProviderPanel
    {
    public:
        void             Update( ) override;
        ContentProvider  GetContentProvider( ) const override { return ContentProvider::FourChan; }
        bool             CanSignIn( ) const override { return false; }
        bool             IsReadyToRun( ) const override { return !m_FourChanBoard.empty( ); }
        UserInputOptions BuildInputOptions( ) const override;
        void             LoadPanelState( std::shared_ptr<JsonFile> appConfig ) override;
        void             OnSearchCommitted( ) override;

    private:
        SearchHistory   m_SearchHistory{ };
        std::string     m_FourChanBoard{ };
        int             m_FourChanMaxThreads{ FOURCHAN_THREAD_MAX };
        int             m_FourChanMaxMediaItems{ FOURCHAN_MEDIA_DEFAULT };
        std::shared_ptr<JsonFile> m_AppConfig{ };
    };
}
