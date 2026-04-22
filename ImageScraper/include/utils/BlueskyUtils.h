#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ImageScraper::BlueskyUtils
{
    using Json = nlohmann::json;

    enum class MediaKind : uint8_t
    {
        Image = 0,
        Video = 1
    };

    struct MediaItem
    {
        MediaKind m_Kind{ MediaKind::Image };
        std::string m_PostUri{ };
        std::string m_DownloadUrl{ };
        std::string m_ThumbnailUrl{ };
        std::string m_AltText{ };
        std::string m_Cid{ };
        std::string m_MimeType{ };
        std::string m_ActorDid{ };
        std::string m_ActorHandle{ };
    };

    struct MediaItemsPage
    {
        std::vector<MediaItem> m_Items{ };
        std::string m_Cursor{ };
    };

    struct ImageDownload
    {
        std::string m_SourceUrl{ };
        std::string m_FileName{ };
        std::string m_ActorDirectory{ };
    };

    inline bool IsDid( const std::string& actor )
    {
        return actor.rfind( "did:", 0 ) == 0;
    }

    inline std::optional<std::string> ParseDidFromResolveHandleResponse( const Json& response )
    {
        if( !response.contains( "did" ) )
        {
            LogError( "[%s] Bluesky handle resolution response missing did.", __FUNCTION__ );
            return std::nullopt;
        }

        try
        {
            return response[ "did" ].get<std::string>( );
        }
        catch( const Json::exception& ex )
        {
            LogError( "[%s] Could not parse Bluesky did, error: %s", __FUNCTION__, ex.what( ) );
            return std::nullopt;
        }
    }

    inline std::string GetStringOrEmpty( const Json& value, const char* key )
    {
        if( !value.is_object( ) || !value.contains( key ) || !value[ key ].is_string( ) )
        {
            return { };
        }

        return value[ key ].get<std::string>( );
    }

    inline std::string GetBlobCid( const Json& blob )
    {
        if( blob.is_object( ) )
        {
            if( blob.contains( "cid" ) && blob[ "cid" ].is_string( ) )
            {
                return blob[ "cid" ].get<std::string>( );
            }

            if( blob.contains( "ref" ) &&
                blob[ "ref" ].is_object( ) &&
                blob[ "ref" ].contains( "$link" ) &&
                blob[ "ref" ][ "$link" ].is_string( ) )
            {
                return blob[ "ref" ][ "$link" ].get<std::string>( );
            }
        }

        return { };
    }

    inline std::string GetBlobMimeType( const Json& blob )
    {
        return GetStringOrEmpty( blob, "mimeType" );
    }

    inline std::string StripUrlQueryAndFragment( const std::string& url )
    {
        const std::size_t queryPos = url.find( '?' );
        const std::size_t fragmentPos = url.find( '#' );

        std::size_t endPos = std::string::npos;
        if( queryPos != std::string::npos )
        {
            endPos = queryPos;
        }

        if( fragmentPos != std::string::npos )
        {
            endPos = ( endPos == std::string::npos ) ? fragmentPos : ( std::min )( endPos, fragmentPos );
        }

        return endPos == std::string::npos ? url : url.substr( 0, endPos );
    }

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

        if( sanitized.empty( ) )
        {
            return fallback;
        }

        return sanitized;
    }

    inline std::string GetActorIdentifier( const MediaItem& item, const std::string& fallbackActor )
    {
        if( !item.m_ActorHandle.empty( ) )
        {
            return item.m_ActorHandle;
        }

        if( !item.m_ActorDid.empty( ) )
        {
            return item.m_ActorDid;
        }

        if( !fallbackActor.empty( ) )
        {
            return fallbackActor;
        }

        return "actor";
    }

    inline std::string GetPostIdFromUri( const std::string& postUri )
    {
        const std::size_t slashPos = postUri.find_last_of( '/' );
        if( slashPos == std::string::npos || slashPos + 1 >= postUri.size( ) )
        {
            return "post";
        }

        return SanitizePathComponent( postUri.substr( slashPos + 1 ), "post" );
    }

    inline std::string CanonicalizeImageExtension( std::string extension )
    {
        std::transform( extension.begin( ), extension.end( ), extension.begin( ), []( unsigned char c )
            {
                return static_cast<char>( std::tolower( c ) );
            } );

        if( extension == "jpeg" || extension == "jpe" || extension == "jfif" )
        {
            return "jpg";
        }

        if( extension == "tif" )
        {
            return "tiff";
        }

        return extension;
    }

    inline std::string GetImageExtensionFromMimeType( const std::string& mimeType )
    {
        if( mimeType.rfind( "image/", 0 ) != 0 )
        {
            return { };
        }

        return CanonicalizeImageExtension( mimeType.substr( 6 ) );
    }

    inline std::string GetImageExtensionFromUrl( const std::string& url )
    {
        const std::string normalizedUrl = StripUrlQueryAndFragment( url );
        const std::size_t slashPos = normalizedUrl.find_last_of( '/' );
        const std::string filename = slashPos == std::string::npos ? normalizedUrl : normalizedUrl.substr( slashPos + 1 );
        if( filename.empty( ) )
        {
            return { };
        }

        const std::size_t atPos = filename.find_last_of( '@' );
        if( atPos != std::string::npos && atPos + 1 < filename.size( ) )
        {
            return CanonicalizeImageExtension( filename.substr( atPos + 1 ) );
        }

        const std::size_t dotPos = filename.find_last_of( '.' );
        if( dotPos == std::string::npos || dotPos + 1 >= filename.size( ) )
        {
            return { };
        }

        return CanonicalizeImageExtension( filename.substr( dotPos + 1 ) );
    }

    inline std::string GetImageExtension( const MediaItem& item )
    {
        const std::string urlExtension = GetImageExtensionFromUrl( item.m_DownloadUrl );
        if( !urlExtension.empty( ) )
        {
            return urlExtension;
        }

        const std::string mimeExtension = GetImageExtensionFromMimeType( item.m_MimeType );
        if( !mimeExtension.empty( ) )
        {
            return mimeExtension;
        }

        return "jpg";
    }

    inline std::string BuildImageFileName( const MediaItem& item, int mediaIndex )
    {
        const std::string postId = GetPostIdFromUri( item.m_PostUri );
        const std::string extension = GetImageExtension( item );
        const std::string index = mediaIndex < 10 ? "0" + std::to_string( mediaIndex ) : std::to_string( mediaIndex );

        return postId + "_" + index + "." + extension;
    }

    inline std::vector<ImageDownload> PrepareImageDownloads( const std::vector<MediaItem>& mediaItems,
                                                             int maxItems,
                                                             const std::string& fallbackActor )
    {
        std::vector<ImageDownload> downloads{ };
        if( maxItems <= 0 )
        {
            return downloads;
        }

        std::unordered_set<std::string> seenUrls{ };
        std::unordered_map<std::string, int> mediaIndexByPost{ };

        for( const MediaItem& item : mediaItems )
        {
            if( static_cast<int>( downloads.size( ) ) >= maxItems )
            {
                break;
            }

            if( item.m_Kind != MediaKind::Image || item.m_DownloadUrl.empty( ) )
            {
                continue;
            }

            const std::string normalizedUrl = StripUrlQueryAndFragment( item.m_DownloadUrl );
            if( normalizedUrl.empty( ) || !seenUrls.insert( normalizedUrl ).second )
            {
                continue;
            }

            const std::string postKey = item.m_PostUri.empty( )
                ? SanitizePathComponent( GetActorIdentifier( item, fallbackActor ), "actor" ) + "|" + GetPostIdFromUri( item.m_PostUri )
                : item.m_PostUri;
            const int mediaIndex = ++mediaIndexByPost[ postKey ];

            ImageDownload download{ };
            download.m_SourceUrl = item.m_DownloadUrl;
            download.m_ActorDirectory = SanitizePathComponent( GetActorIdentifier( item, fallbackActor ), "actor" );
            download.m_FileName = BuildImageFileName( item, mediaIndex );
            downloads.push_back( std::move( download ) );
        }

        return downloads;
    }

    inline void PopulateAuthorFields( const Json& author, MediaItem& item )
    {
        item.m_ActorDid = GetStringOrEmpty( author, "did" );
        item.m_ActorHandle = GetStringOrEmpty( author, "handle" );
    }

    inline void AddImageItemsFromView( const Json& viewEmbed,
                                       const std::string& postUri,
                                       const Json& author,
                                       std::vector<MediaItem>& out,
                                       int maxItems,
                                       std::size_t startIndex = 0 )
    {
        if( !viewEmbed.contains( "images" ) || !viewEmbed[ "images" ].is_array( ) )
        {
            return;
        }

        const auto& viewImages = viewEmbed[ "images" ];
        for( std::size_t i = startIndex; i < viewImages.size( ); ++i )
        {
            if( static_cast<int>( out.size( ) ) >= maxItems )
            {
                return;
            }

            const Json& imageView = viewImages[ i ];
            const std::string downloadUrl = GetStringOrEmpty( imageView, "fullsize" );
            if( downloadUrl.empty( ) )
            {
                continue;
            }

            MediaItem item{ };
            item.m_Kind = MediaKind::Image;
            item.m_PostUri = postUri;
            item.m_DownloadUrl = downloadUrl;
            item.m_ThumbnailUrl = GetStringOrEmpty( imageView, "thumb" );
            item.m_AltText = GetStringOrEmpty( imageView, "alt" );
            PopulateAuthorFields( author, item );
            out.push_back( item );
        }
    }

    inline void AddImageItems( const Json& recordEmbed,
                               const Json& viewEmbed,
                               const std::string& postUri,
                               const Json& author,
                               std::vector<MediaItem>& out,
                               int maxItems )
    {
        if( !recordEmbed.contains( "images" ) || !recordEmbed[ "images" ].is_array( ) )
        {
            AddImageItemsFromView( viewEmbed, postUri, author, out, maxItems );
            return;
        }

        if( !viewEmbed.contains( "images" ) || !viewEmbed[ "images" ].is_array( ) )
        {
            return;
        }

        const auto& recordImages = recordEmbed[ "images" ];
        const auto& viewImages = viewEmbed[ "images" ];
        const std::size_t count = ( std::min )( recordImages.size( ), viewImages.size( ) );

        for( std::size_t i = 0; i < count; ++i )
        {
            if( static_cast<int>( out.size( ) ) >= maxItems )
            {
                return;
            }

            const std::string downloadUrl = GetStringOrEmpty( viewImages[ i ], "fullsize" );
            if( downloadUrl.empty( ) )
            {
                continue;
            }

            MediaItem item{ };
            item.m_Kind = MediaKind::Image;
            item.m_PostUri = postUri;
            item.m_DownloadUrl = downloadUrl;
            item.m_ThumbnailUrl = GetStringOrEmpty( viewImages[ i ], "thumb" );
            item.m_AltText = GetStringOrEmpty( viewImages[ i ], "alt" );
            if( item.m_AltText.empty( ) )
            {
                item.m_AltText = GetStringOrEmpty( recordImages[ i ], "alt" );
            }

            if( recordImages[ i ].contains( "image" ) )
            {
                item.m_Cid = GetBlobCid( recordImages[ i ][ "image" ] );
                item.m_MimeType = GetBlobMimeType( recordImages[ i ][ "image" ] );
            }

            PopulateAuthorFields( author, item );
            out.push_back( item );
        }

        if( count < viewImages.size( ) )
        {
            AddImageItemsFromView( viewEmbed, postUri, author, out, maxItems, count );
        }
    }

    inline void AddVideoItemFromView( const Json& viewEmbed,
                                      const std::string& postUri,
                                      const Json& author,
                                      std::vector<MediaItem>& out,
                                      int maxItems )
    {
        if( static_cast<int>( out.size( ) ) >= maxItems )
        {
            return;
        }

        const std::string playlist = GetStringOrEmpty( viewEmbed, "playlist" );
        if( playlist.empty( ) )
        {
            return;
        }

        MediaItem item{ };
        item.m_Kind = MediaKind::Video;
        item.m_PostUri = postUri;
        item.m_DownloadUrl = playlist;
        item.m_ThumbnailUrl = GetStringOrEmpty( viewEmbed, "thumbnail" );
        item.m_AltText = GetStringOrEmpty( viewEmbed, "alt" );
        item.m_Cid = GetBlobCid( viewEmbed );
        PopulateAuthorFields( author, item );
        out.push_back( item );
    }

    inline void AddVideoItem( const Json& recordEmbed,
                              const Json& viewEmbed,
                              const std::string& postUri,
                              const Json& author,
                              std::vector<MediaItem>& out,
                              int maxItems )
    {
        if( !recordEmbed.contains( "video" ) )
        {
            AddVideoItemFromView( viewEmbed, postUri, author, out, maxItems );
            return;
        }

        if( static_cast<int>( out.size( ) ) >= maxItems )
        {
            return;
        }

        const std::string playlist = GetStringOrEmpty( viewEmbed, "playlist" );
        if( playlist.empty( ) )
        {
            return;
        }

        MediaItem item{ };
        item.m_Kind = MediaKind::Video;
        item.m_PostUri = postUri;
        item.m_DownloadUrl = playlist;
        item.m_ThumbnailUrl = GetStringOrEmpty( viewEmbed, "thumbnail" );
        item.m_AltText = GetStringOrEmpty( viewEmbed, "alt" );
        if( item.m_AltText.empty( ) )
        {
            item.m_AltText = GetStringOrEmpty( recordEmbed, "alt" );
        }

        item.m_Cid = GetBlobCid( recordEmbed[ "video" ] );
        if( item.m_Cid.empty( ) )
        {
            item.m_Cid = GetBlobCid( viewEmbed );
        }
        item.m_MimeType = GetBlobMimeType( recordEmbed[ "video" ] );
        PopulateAuthorFields( author, item );
        out.push_back( item );
    }

    inline void ExtractMediaItems( const Json& recordEmbed,
                                   const Json& viewEmbed,
                                   const std::string& postUri,
                                   const Json& author,
                                   std::vector<MediaItem>& out,
                                   int maxItems )
    {
        if( static_cast<int>( out.size( ) ) >= maxItems || !viewEmbed.is_object( ) )
        {
            return;
        }

        const std::string recordType = GetStringOrEmpty( recordEmbed, "$type" );
        const std::string viewType = GetStringOrEmpty( viewEmbed, "$type" );

        if( viewType == "app.bsky.embed.images#view" )
        {
            AddImageItems( recordEmbed, viewEmbed, postUri, author, out, maxItems );
            return;
        }

        if( viewType == "app.bsky.embed.video#view" )
        {
            AddVideoItem( recordEmbed, viewEmbed, postUri, author, out, maxItems );
            return;
        }

        if( recordType == "app.bsky.embed.recordWithMedia" &&
            viewType == "app.bsky.embed.recordWithMedia#view" &&
            recordEmbed.contains( "media" ) &&
            viewEmbed.contains( "media" ) )
        {
            ExtractMediaItems( recordEmbed[ "media" ], viewEmbed[ "media" ], postUri, author, out, maxItems );
        }
    }

    inline MediaItemsPage GetMediaItemsFromAuthorFeedResponse( const Json& response, int maxItems )
    {
        MediaItemsPage page{ };

        if( response.contains( "cursor" ) && response[ "cursor" ].is_string( ) )
        {
            page.m_Cursor = response[ "cursor" ].get<std::string>( );
        }

        if( !response.contains( "feed" ) || !response[ "feed" ].is_array( ) )
        {
            return page;
        }

        for( const auto& feedItem : response[ "feed" ] )
        {
            if( static_cast<int>( page.m_Items.size( ) ) >= maxItems )
            {
                break;
            }

            if( !feedItem.contains( "post" ) || !feedItem[ "post" ].is_object( ) )
            {
                continue;
            }

            const Json& post = feedItem[ "post" ];
            if( !post.contains( "embed" ) || !post[ "embed" ].is_object( ) )
            {
                continue;
            }

            const Json author = post.contains( "author" ) ? post[ "author" ] : Json{ };
            const std::string postUri = GetStringOrEmpty( post, "uri" );
            const Json recordEmbed = ( post.contains( "record" ) &&
                                       post[ "record" ].is_object( ) &&
                                       post[ "record" ].contains( "embed" ) )
                ? post[ "record" ][ "embed" ]
                : Json{ };

            ExtractMediaItems( recordEmbed, post[ "embed" ], postUri, author, page.m_Items, maxItems );
        }

        return page;
    }
}
