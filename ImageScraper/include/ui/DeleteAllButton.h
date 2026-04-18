#pragma once

#include "imgui/imgui.h"

#include <filesystem>
#include <functional>
#include <string>

namespace ImageScraper
{
    // Self-contained "Delete All" button + confirmation modal for a single provider.
    // Owns the disabled check (filesystem scan) and the orange-tinted modal.
    // Call Update() once per frame from the owning panel's Update().
    class DeleteAllButton
    {
    public:
        // providerSubDir : subfolder name under Downloads/ on disk (e.g. "Reddit")
        // displayName    : shown in the confirmation message (e.g. "Reddit")
        DeleteAllButton( std::string providerSubDir, std::string displayName );

        // Renders the button and, when triggered, the confirmation modal.
        // outputDir     : root output directory from the owning panel's m_OutputDir
        // onDeleteAll   : callback invoked with the deleted directory path on confirmation
        // extraDisabled : additional disable condition (e.g. sign-in in progress)
        void Update( const std::string& outputDir,
                     const std::function<void( const std::string& )>& onDeleteAll,
                     bool extraDisabled = false );

    private:
        static bool HasAnyFile( const std::filesystem::path& dir );

        std::string m_ProviderSubDir{ };
        std::string m_DisplayName{ };
        std::string m_ButtonId{ };
        std::string m_PopupId{ };
    };
}
