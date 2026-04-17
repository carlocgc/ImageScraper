#pragma once

#include "io/JsonFile.h"

#include <string>
#include <vector>
#include <memory>

namespace ImageScraper
{
    // Manages a capped list of recently committed search terms for a single provider.
    // Persists to and restores from AppConfig using a caller-supplied JSON key.
    class SearchHistory
    {
    public:
        // Load history from appConfig under configKey.
        // Sets the most-recent item as the restored value (call GetMostRecent after Load).
        void Load( std::shared_ptr<JsonFile> appConfig, const std::string& configKey );

        // Push value to the front of the history, dedup, trim to k_MaxItems, and save.
        void Push( const std::string& value );

        // Returns the most-recently committed search term, or empty string if no history.
        std::string GetMostRecent( ) const;

        const std::vector<std::string>& GetItems( ) const;
        bool IsEmpty( ) const;

    private:
        void Save( );

        static constexpr int k_MaxItems = 5;

        std::shared_ptr<JsonFile> m_AppConfig{ };
        std::string               m_ConfigKey{ };
        std::vector<std::string>  m_Items{ };
    };
}
