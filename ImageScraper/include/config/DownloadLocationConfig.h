#pragma once

#include "io/JsonFile.h"

#include <filesystem>
#include <memory>
#include <string>

namespace ImageScraper::DownloadLocationConfig
{
    inline constexpr const char* s_DownloadLocationConfigKey = "download_location";

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

    inline std::filesystem::path GetDefaultDownloadRoot( const std::filesystem::path& exeDir )
    {
        return NormalisePath( exeDir / "Downloads" );
    }

    inline std::filesystem::path LoadDownloadRoot(
        const std::shared_ptr<JsonFile>& appConfig,
        const std::filesystem::path& defaultDownloadRoot )
    {
        std::string savedLocation;
        if( appConfig && appConfig->GetValue<std::string>( s_DownloadLocationConfigKey, savedLocation ) && !savedLocation.empty( ) )
        {
            return NormalisePath( savedLocation );
        }

        return NormalisePath( defaultDownloadRoot );
    }

    inline bool HasCustomDownloadRoot( const std::shared_ptr<JsonFile>& appConfig )
    {
        std::string savedLocation;
        return appConfig
            && appConfig->GetValue<std::string>( s_DownloadLocationConfigKey, savedLocation )
            && !savedLocation.empty( );
    }

    inline bool IsSamePath( const std::filesystem::path& lhs, const std::filesystem::path& rhs )
    {
        return NormalisePath( lhs ) == NormalisePath( rhs );
    }

    inline bool IsPathWithinRoot( const std::filesystem::path& path, const std::filesystem::path& root )
    {
        const std::filesystem::path normalisedPath = NormalisePath( path );
        const std::filesystem::path normalisedRoot = NormalisePath( root );
        if( normalisedPath.empty( ) || normalisedRoot.empty( ) )
        {
            return false;
        }

        if( normalisedPath == normalisedRoot )
        {
            return true;
        }

        const std::wstring pathString = normalisedPath.wstring( );
        const std::wstring rootString = normalisedRoot.wstring( );
        if( pathString.size( ) <= rootString.size( ) || pathString.compare( 0, rootString.size( ), rootString ) != 0 )
        {
            return false;
        }

        return pathString[ rootString.size( ) ] == std::filesystem::path::preferred_separator;
    }

    inline bool IsRootDriveAvailable( const std::filesystem::path& path )
    {
        if( path.empty( ) )
        {
            return true;
        }

        const std::filesystem::path rootPath = path.root_path( );
        if( rootPath.empty( ) )
        {
            return true;
        }

        const std::wstring rootName = path.root_name( ).wstring( );
        if( rootName.rfind( L"\\\\", 0 ) == 0 )
        {
            return true;
        }

        std::error_code ec;
        return std::filesystem::exists( rootPath, ec ) && !ec;
    }
}
