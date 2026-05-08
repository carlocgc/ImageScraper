#pragma once
#include <filesystem>
#include <system_error>

namespace ImageScraper::FilesystemUtils
{
    // Canonicalise path with graceful fallbacks.
    // Returns weakly_canonical, then absolute, then lexically_normal.
    // Returns an empty path if the input is empty.
    inline std::filesystem::path NormalisePath( const std::filesystem::path& path )
    {
        if( path.empty( ) )
        {
            return { };
        }

        std::error_code ec;
        std::filesystem::path normalised = std::filesystem::weakly_canonical( path, ec );
        if( ec )
        {
            normalised = std::filesystem::absolute( path, ec );
            if( ec )
            {
                normalised = path.lexically_normal( );
            }
        }

        normalised.make_preferred( );
        return normalised;
    }

    // Returns true if path exists and is a directory, without throwing.
    inline bool DirectoryExists( const std::filesystem::path& path )
    {
        std::error_code ec;
        return std::filesystem::exists( path, ec ) && !ec
            && std::filesystem::is_directory( path, ec ) && !ec;
    }

    // Returns true if the directory exists and contains at least one entry.
    inline bool DirectoryHasEntries( const std::filesystem::path& path )
    {
        if( !DirectoryExists( path ) )
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::directory_iterator it{ path, std::filesystem::directory_options::skip_permission_denied, ec };
        return !ec && it != std::filesystem::directory_iterator{ };
    }
}
