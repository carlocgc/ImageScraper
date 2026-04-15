#pragma once

#include "ui/IProviderPanel.h"
#include "imgui/imgui.h"

#include <string>

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

    private:
        std::string m_FourChanBoard{ };
        int         m_FourChanMaxThreads{ FOURCHAN_THREAD_MAX };
        int         m_FourChanMaxMediaItems{ FOURCHAN_MEDIA_DEFAULT };
    };
}
