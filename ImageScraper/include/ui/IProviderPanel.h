#pragma once

#include "ui/IUiPanel.h"
#include "io/JsonFile.h"
#include "services/ServiceOptionTypes.h"

#include <memory>

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

        // Load persisted search inputs from appConfig; save back on every change.
        // Default no-op - override in panels that have persistent inputs.
        virtual void LoadPanelState( std::shared_ptr<JsonFile> ) { }

        // Called by DownloadOptionsPanel when a search is successfully submitted.
        // Panels push the current search term to their history list here.
        virtual void OnSearchCommitted( ) { }
    };
}
