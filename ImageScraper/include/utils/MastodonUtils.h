#pragma once

#include "nlohmann/json.hpp"
#include "utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ImageScraper::MastodonUtils
{
    using Json = nlohmann::json;

    enum class MediaKind : uint8_t
    {
        Image = 0,
        Gifv = 1,
        Video = 2
    };

    struct NormalizedAccount
    {
        std::string m_SearchQuery{ };
        std::string m_Username{ };
        std::string m_Domain{ };
    };

    struct Account
    {
        std::string m_Id{ };
        std::string m_Username{ };
        std::string m_Acct{ };
        std::string m_Url{ };
    };

    struct MediaItem
    {
        MediaKind m_Kind{ MediaKind::Image };
        std::string m_StatusId{ };
        std::string m_AttachmentId{ };
        std::string m_DownloadUrl{ };
        std::string m_PreviewUrl{ };
        std::string m_Description{ };
        std::string m_AccountAcct{ };
    };

    struct PreparedDownload
    {
        MediaKind m_Kind{ MediaKind::Image };
        std::string m_SourceUrl{ };
        std::string m_FileName{ };
        std::string m_InstanceDirectory{ };
        std::string m_AccountDirectory{ };
    };

    inline std::string SanitizePathComponent( const std::string& value, const std::string& fallback = "item" )
    {
        std::string sanitized{ };
        sanitized.reserve( value.size( ) );

        bool lastWasUnderscore = false;
        for( unsigned char c : value )
        {
            const bool isAllowed = std::isalnum( c ) != 0 || c == '.' || c == '-' || c == '_';
            if( isAllowed )
            {
                sanitized.push_back( static_cast<char>( c ) );
                lastWasUnderscore = false;
                continue;
            }

            if( !lastWasUnderscore )
            {
                sanitized.push_back( '_' );
                lastWasUnderscore = true;
            }
        }

        while( !sanitized.empty( ) && ( sanitized.back( ) == '.' || sanitized.back( ) == ' ' || sanitized.back( ) == '_' ) )
        {
            sanitized.pop_back( );
        }

        return sanitized.empty( ) ? fallback : sanitized;
    }

    inline std::string GetHostFromUrl( const std::string& url )
    {
        const std::string trimmed = StringUtils::Trim( StringUtils::StripUrlQueryAndFragment( url ) );
        const std::size_t schemePos = trimmed.find( "://" );
        if( schemePos == std::string::npos )
        {
            const std::size_t slashPos = trimmed.find( '/' );
            return StringUtils::ToLower( slashPos == std::string::npos ? trimmed : trimmed.substr( 0, slashPos ) );
        }

        const std::size_t hostStart = schemePos + 3;
        const std::size_t hostEnd = trimmed.find( '/', hostStart );
        return StringUtils::ToLower( hostEnd == std::string::npos ? trimmed.substr( hostStart ) : trimmed.substr( hostStart, hostEnd - hostStart ) );
    }

    inline std::string NormalizeInstanceUrl( const std::string& instance )
    {
        std::string normalized = StringUtils::Trim( StringUtils::StripUrlQueryAndFragment( instance ) );
        std::replace( normalized.begin( ), normalized.end( ), '\\', '/' );

        if( normalized.empty( ) )
        {
            return { };
        }

        if( normalized.find( "://" ) == std::string::npos )
        {
            normalized = "https://" + normalized;
        }

        const std::size_t schemePos = normalized.find( "://" );
        if( schemePos == std::string::npos )
        {
            return { };
        }

        std::string scheme = StringUtils::ToLower( normalized.substr( 0, schemePos ) );
        if( scheme != "http" && scheme != "https" )
        {
            return { };
        }

        const std::size_t hostStart = schemePos + 3;
        const std::size_t hostEnd = normalized.find( '/', hostStart );
        std::string host = hostEnd == std::string::npos ? normalized.substr( hostStart ) : normalized.substr( hostStart, hostEnd - hostStart );
        host = StringUtils::ToLower( host );

        while( !host.empty( ) && host.back( ) == '/' )
        {
            host.pop_back( );
        }

        if( host.empty( ) )
        {
            return { };
        }

        return scheme + "://" + host;
    }

    inline std::string GetHostFromInstanceUrl( const std::string& instance )
    {
        return GetHostFromUrl( NormalizeInstanceUrl( instance ) );
    }

    inline NormalizedAccount NormalizeAccountInput( const std::string& account )
    {
        NormalizedAccount normalized{ };
        std::string value = StringUtils::Trim( StringUtils::StripUrlQueryAndFragment( account ) );
        std::replace( value.begin( ), value.end( ), '\\', '/' );

        if( value.empty( ) )
        {
            return normalized;
        }

        if( StringUtils::StartsWith( StringUtils::ToLower( value ), "http://" ) || StringUtils::StartsWith( StringUtils::ToLower( value ), "https://" ) )
        {
            normalized.m_Domain = GetHostFromUrl( value );

            const std::size_t schemePos = value.find( "://" );
            const std::size_t pathStart = schemePos == std::string::npos ? value.find( '/' ) : value.find( '/', schemePos + 3 );
            if( pathStart == std::string::npos || pathStart + 1 >= value.size( ) )
            {
                return normalized;
            }

            std::string path = value.substr( pathStart + 1 );
            if( StringUtils::StartsWith( path, "@" ) )
            {
                path.erase( path.begin( ) );
            }
            else if( StringUtils::StartsWith( path, "users/" ) )
            {
                path.erase( 0, 6 );
            }

            const std::size_t slashPos = path.find( '/' );
            normalized.m_Username = slashPos == std::string::npos ? path : path.substr( 0, slashPos );
            normalized.m_SearchQuery = normalized.m_Username;

            if( !normalized.m_Domain.empty( ) && !normalized.m_Username.empty( ) )
            {
                normalized.m_SearchQuery += "@" + normalized.m_Domain;
            }

            return normalized;
        }

        while( !value.empty( ) && value.front( ) == '@' )
        {
            value.erase( value.begin( ) );
        }

        while( !value.empty( ) && value.back( ) == '/' )
        {
            value.pop_back( );
        }

        const std::size_t atPos = value.find( '@' );
        normalized.m_Username = atPos == std::string::npos ? value : value.substr( 0, atPos );
        normalized.m_Domain = atPos == std::string::npos ? std::string{ } : StringUtils::ToLower( value.substr( atPos + 1 ) );
        normalized.m_SearchQuery = normalized.m_Domain.empty( ) ? normalized.m_Username : normalized.m_Username + "@" + normalized.m_Domain;
        return normalized;
    }

    inline std::string GetStringOrEmpty( const Json& value, const char* key )
    {
        if( !value.is_object( ) || !value.contains( key ) || !value[ key ].is_string( ) )
        {
            return { };
        }

        return value[ key ].get<std::string>( );
    }

    inline Account ParseAccount( const Json& value )
    {
        Account account{ };
        account.m_Id = GetStringOrEmpty( value, "id" );
        account.m_Username = GetStringOrEmpty( value, "username" );
        account.m_Acct = GetStringOrEmpty( value, "acct" );
        account.m_Url = GetStringOrEmpty( value, "url" );
        return account;
    }

    inline bool AccountMatches( const Account& account, const NormalizedAccount& target, const std::string& instanceHost )
    {
        if( target.m_Username.empty( ) )
        {
            return false;
        }

        const std::string targetUsername = StringUtils::ToLower( target.m_Username );
        const std::string targetDomain = StringUtils::ToLower( target.m_Domain );
        const std::string accountUsername = StringUtils::ToLower( account.m_Username );
        const std::string accountAcct = StringUtils::ToLower( account.m_Acct );
        const std::string accountUrlHost = GetHostFromUrl( account.m_Url );

        if( targetDomain.empty( ) )
        {
            return accountAcct == targetUsername || accountUsername == targetUsername;
        }

        const std::string targetAcct = targetUsername + "@" + targetDomain;
        if( accountAcct == targetAcct )
        {
            return true;
        }

        if( accountAcct == targetUsername && targetDomain == instanceHost )
        {
            return true;
        }

        return accountUsername == targetUsername && accountUrlHost == targetDomain;
    }

    inline std::optional<Account> SelectAccountFromSearchResponse( const Json& response, const std::string& accountInput, const std::string& instanceUrl )
    {
        if( !response.contains( "accounts" ) || !response[ "accounts" ].is_array( ) )
        {
            return std::nullopt;
        }

        const NormalizedAccount target = NormalizeAccountInput( accountInput );
        const std::string instanceHost = GetHostFromInstanceUrl( instanceUrl );

        for( const auto& accountValue : response[ "accounts" ] )
        {
            const Account account = ParseAccount( accountValue );
            if( AccountMatches( account, target, instanceHost ) )
            {
                return account;
            }
        }

        return std::nullopt;
    }

    inline std::optional<MediaKind> ParseMediaKind( const std::string& type )
    {
        if( type == "image" )
        {
            return MediaKind::Image;
        }

        if( type == "gifv" )
        {
            return MediaKind::Gifv;
        }

        if( type == "video" )
        {
            return MediaKind::Video;
        }

        return std::nullopt;
    }

    inline std::string CanonicalizeExtension( std::string extension )
    {
        extension = StringUtils::ToLower( extension );

        if( extension == "jpeg" || extension == "jpe" || extension == "jfif" )
        {
            return "jpg";
        }

        if( extension == "tif" )
        {
            return "tiff";
        }

        if( extension == "quicktime" )
        {
            return "mov";
        }

        if( extension == "x-matroska" )
        {
            return "mkv";
        }

        if( extension == "x-msvideo" )
        {
            return "avi";
        }

        if( extension.rfind( "x-", 0 ) == 0 && extension.size( ) > 2 )
        {
            return extension.substr( 2 );
        }

        return extension;
    }

    inline std::string GetExtensionFromUrl( const std::string& url )
    {
        const std::string normalizedUrl = StringUtils::StripUrlQueryAndFragment( url );
        const std::size_t slashPos = normalizedUrl.find_last_of( '/' );
        const std::string filename = slashPos == std::string::npos ? normalizedUrl : normalizedUrl.substr( slashPos + 1 );
        if( filename.empty( ) )
        {
            return { };
        }

        const std::size_t dotPos = filename.find_last_of( '.' );
        if( dotPos == std::string::npos || dotPos + 1 >= filename.size( ) )
        {
            return { };
        }

        return CanonicalizeExtension( filename.substr( dotPos + 1 ) );
    }

    inline std::string GetDefaultExtension( MediaKind kind )
    {
        return kind == MediaKind::Image ? "jpg" : "mp4";
    }

    inline std::string GetMediaExtension( const MediaItem& item )
    {
        const std::string extension = GetExtensionFromUrl( item.m_DownloadUrl );
        return extension.empty( ) ? GetDefaultExtension( item.m_Kind ) : extension;
    }

    inline std::string BuildMediaFileName( const MediaItem& item, int mediaIndex )
    {
        const std::string statusId = SanitizePathComponent( item.m_StatusId, "status" );
        const std::string extension = GetMediaExtension( item );
        const std::string index = mediaIndex < 10 ? "0" + std::to_string( mediaIndex ) : std::to_string( mediaIndex );

        return statusId + "_" + index + "." + extension;
    }

    inline std::vector<MediaItem> GetMediaItemsFromStatusesResponse( const Json& response, int maxItems )
    {
        std::vector<MediaItem> items{ };
        if( maxItems <= 0 || !response.is_array( ) )
        {
            return items;
        }

        for( const auto& status : response )
        {
            if( static_cast<int>( items.size( ) ) >= maxItems )
            {
                break;
            }

            if( !status.is_object( ) || !status.contains( "media_attachments" ) || !status[ "media_attachments" ].is_array( ) )
            {
                continue;
            }

            const std::string statusId = GetStringOrEmpty( status, "id" );
            const std::string accountAcct = status.contains( "account" ) ? GetStringOrEmpty( status[ "account" ], "acct" ) : std::string{ };

            for( const auto& attachment : status[ "media_attachments" ] )
            {
                if( static_cast<int>( items.size( ) ) >= maxItems )
                {
                    break;
                }

                const std::optional<MediaKind> kind = ParseMediaKind( GetStringOrEmpty( attachment, "type" ) );
                if( !kind.has_value( ) )
                {
                    continue;
                }

                std::string downloadUrl = GetStringOrEmpty( attachment, "url" );
                if( downloadUrl.empty( ) )
                {
                    downloadUrl = GetStringOrEmpty( attachment, "remote_url" );
                }

                if( downloadUrl.empty( ) )
                {
                    continue;
                }

                MediaItem item{ };
                item.m_Kind = *kind;
                item.m_StatusId = statusId;
                item.m_AttachmentId = GetStringOrEmpty( attachment, "id" );
                item.m_DownloadUrl = downloadUrl;
                item.m_PreviewUrl = GetStringOrEmpty( attachment, "preview_url" );
                item.m_Description = GetStringOrEmpty( attachment, "description" );
                item.m_AccountAcct = accountAcct;
                items.push_back( std::move( item ) );
            }
        }

        return items;
    }

    inline std::string GetLastStatusIdFromStatusesResponse( const Json& response )
    {
        if( !response.is_array( ) )
        {
            return { };
        }

        for( auto it = response.rbegin( ); it != response.rend( ); ++it )
        {
            const std::string id = GetStringOrEmpty( *it, "id" );
            if( !id.empty( ) )
            {
                return id;
            }
        }

        return { };
    }

    inline std::vector<PreparedDownload> PrepareMediaDownloads( const std::vector<MediaItem>& mediaItems,
                                                                int maxItems,
                                                                const std::string& instanceUrl,
                                                                const std::string& fallbackAccount )
    {
        std::vector<PreparedDownload> downloads{ };
        if( maxItems <= 0 )
        {
            return downloads;
        }

        std::unordered_set<std::string> seenUrls{ };
        std::unordered_map<std::string, int> mediaIndexByStatus{ };

        const std::string instanceDirectory = SanitizePathComponent( GetHostFromInstanceUrl( instanceUrl ), "instance" );
        const std::string fallbackAccountDirectory = SanitizePathComponent( NormalizeAccountInput( fallbackAccount ).m_SearchQuery, "account" );

        for( const MediaItem& item : mediaItems )
        {
            if( static_cast<int>( downloads.size( ) ) >= maxItems )
            {
                break;
            }

            if( item.m_DownloadUrl.empty( ) )
            {
                continue;
            }

            const std::string normalizedUrl = StringUtils::StripUrlQueryAndFragment( item.m_DownloadUrl );
            if( normalizedUrl.empty( ) || !seenUrls.insert( normalizedUrl ).second )
            {
                continue;
            }

            const std::string statusKey = item.m_StatusId.empty( ) ? normalizedUrl : item.m_StatusId;
            const int mediaIndex = ++mediaIndexByStatus[ statusKey ];
            const std::string accountDirectory = item.m_AccountAcct.empty( )
                ? fallbackAccountDirectory
                : SanitizePathComponent( item.m_AccountAcct, "account" );

            PreparedDownload download{ };
            download.m_Kind = item.m_Kind;
            download.m_SourceUrl = item.m_DownloadUrl;
            download.m_FileName = BuildMediaFileName( item, mediaIndex );
            download.m_InstanceDirectory = instanceDirectory;
            download.m_AccountDirectory = accountDirectory;
            downloads.push_back( std::move( download ) );
        }

        return downloads;
    }
}
