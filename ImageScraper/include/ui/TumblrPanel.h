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

    private:
        std::string m_TumblrUser{ };
    };
}
