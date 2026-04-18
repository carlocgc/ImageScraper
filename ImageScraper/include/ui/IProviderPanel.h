#pragma once

#include "ui/IUiPanel.h"
#include "io/JsonFile.h"
#include "services/ServiceOptionTypes.h"

#include <functional>
#include <memory>
#include <string>

namespace ImageScraper
{
    class IProviderPanel : public IUiPanel
    {
    public:
        using OnDeleteAllFn = std::function<void( const std::string& deletedDir )>;

        virtual ~IProviderPanel( ) = default;
        virtual ContentProvider  GetContentProvider( ) const = 0;
        virtual bool             CanSignIn( ) const = 0;
        virtual bool             IsReadyToRun( ) const = 0;
        virtual UserInputOptions BuildInputOptions( ) const = 0;

        // Load persisted search inputs from appConfig; save back on every change.
        // Default no-op - override in panels that have persistent inputs.
        virtual void LoadSearchHistory( std::shared_ptr<JsonFile> ) { }

        // Called by DownloadOptionsPanel when a search is successfully submitted.
        // Panels push the current search term to their history list here.
        virtual void OnSearchCommitted( ) { }

        // Concrete setters - same implementation for every panel, no polymorphism needed.
        void SetOutputDir(        const std::string& outputDir ) { m_OutputDir   = outputDir;       }
        void SetSigningIn(        bool signing )                 { m_SigningIn   = signing;         }
        void SetDeleteAllCallback( OnDeleteAllFn fn )            { m_OnDeleteAll = std::move( fn ); }

    protected:
        std::string   m_OutputDir{ };
        bool          m_SigningIn{ false };
        OnDeleteAllFn m_OnDeleteAll{ };
    };
}
