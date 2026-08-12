#pragma once
#include <filesystem>
#include <string>
#include <system_error>

namespace ImageScraper::FilesystemUtils
{
    // std::filesystem::path stores wchar_t on Windows, and path::string() /
    // path::generic_string() convert it to the process ANSI code page. That
    // conversion throws std::system_error for any character the code page
    // cannot represent, which is every non-Latin filename on a western install.
    // Always route path -> std::string through these helpers so the narrow
    // strings we pass around are UTF-8, which is what ImGui, FFmpeg, curl and
    // nlohmann::json all expect.

    // Returns the path as UTF-8, using the platform preferred separator.
    inline std::string PathToUtf8( const std::filesystem::path& path )
    {
        return path.u8string( );
    }

    // Returns the path as UTF-8, using '/' as the separator.
    inline std::string PathToUtf8Generic( const std::filesystem::path& path )
    {
        return path.generic_u8string( );
    }

    // Builds a path from a UTF-8 string. The std::filesystem::path constructor
    // decodes narrow input using the ANSI code page, so it must not be used
    // for the UTF-8 strings produced by PathToUtf8.
    inline std::filesystem::path PathFromUtf8( const std::string& utf8 )
    {
        // u8path is deprecated in C++20; this project builds as C++17. Swap the
        // body for a std::u8string conversion if the standard is raised.
        return std::filesystem::u8path( utf8 );
    }

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
