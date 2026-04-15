#pragma once

#include "ui/IUiPanel.h"
#include "services/ServiceOptionTypes.h"

namespace ImageScraper
{
    class IProviderPanel : public IUiPanel
    {
    public:
        virtual ~IProviderPanel( ) = default;
        virtual ContentProvider  GetContentProvider( ) const = 0;
        virtual bool             CanSignIn( ) const = 0;
        virtual bool             IsReadyToRun( ) const = 0;
        virtual UserInputOptions BuildInputOptions( ) const = 0;
    };
}
