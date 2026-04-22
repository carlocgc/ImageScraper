#pragma once

#include "log/Logger.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
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
