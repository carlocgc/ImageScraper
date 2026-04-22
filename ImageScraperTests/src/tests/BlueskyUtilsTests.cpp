#include "catch2/catch_amalgamated.hpp"
#include "utils/BlueskyUtils.h"

using namespace ImageScraper::BlueskyUtils;
using Json = nlohmann::json;

TEST_CASE( "IsDid identifies DID actors", "[BlueskyUtils]" )
{
    REQUIRE( IsDid( "did:plc:alice" ) );
    REQUIRE_FALSE( IsDid( "alice.bsky.social" ) );
}

TEST_CASE( "ParseDidFromResolveHandleResponse returns did when present", "[BlueskyUtils]" )
{
    const Json response = { { "did", "did:plc:alice" } };

    const auto did = ParseDidFromResolveHandleResponse( response );
    REQUIRE( did.has_value( ) );
    REQUIRE( *did == "did:plc:alice" );
}

TEST_CASE( "ParseDidFromResolveHandleResponse returns nullopt when did missing", "[BlueskyUtils]" )
{
    const Json response = { { "handle", "alice.bsky.social" } };

    REQUIRE_FALSE( ParseDidFromResolveHandleResponse( response ).has_value( ) );
}

TEST_CASE( "GetMediaItemsFromAuthorFeedResponse extracts image metadata", "[BlueskyUtils]" )
{
    const Json response = {
        { "cursor", "page-2" },
        { "feed", Json::array( {
            {
                { "post", {
                    { "uri", "at://did:plc:alice/app.bsky.feed.post/1" },
                    { "author", { { "did", "did:plc:alice" }, { "handle", "alice.bsky.social" } } },
                    { "record", {
                        { "embed", {
                            { "$type", "app.bsky.embed.images" },
                            { "images", Json::array( {
                                {
                                    { "alt", "record alt" },
                                    { "image", {
                                        { "ref", { { "$link", "bafy-image" } } },
                                        { "mimeType", "image/jpeg" }
                                    } }
                                }
                            } ) }
                        } }
                    } },
                    { "embed", {
                        { "$type", "app.bsky.embed.images#view" },
                        { "images", Json::array( {
                            {
                                { "fullsize", "https://cdn.bsky.app/img1.jpg" },
                                { "thumb", "https://cdn.bsky.app/thumb1.jpg" },
                                { "alt", "view alt" }
                            }
                        } ) }
                    } }
                } }
            }
        } ) }
    };

    const MediaItemsPage page = GetMediaItemsFromAuthorFeedResponse( response, 5 );
    REQUIRE( page.m_Cursor == "page-2" );
    REQUIRE( page.m_Items.size( ) == 1 );

    const MediaItem& item = page.m_Items.front( );
    REQUIRE( item.m_Kind == MediaKind::Image );
    REQUIRE( item.m_PostUri == "at://did:plc:alice/app.bsky.feed.post/1" );
    REQUIRE( item.m_DownloadUrl == "https://cdn.bsky.app/img1.jpg" );
    REQUIRE( item.m_ThumbnailUrl == "https://cdn.bsky.app/thumb1.jpg" );
    REQUIRE( item.m_AltText == "view alt" );
    REQUIRE( item.m_Cid == "bafy-image" );
    REQUIRE( item.m_MimeType == "image/jpeg" );
    REQUIRE( item.m_ActorDid == "did:plc:alice" );
    REQUIRE( item.m_ActorHandle == "alice.bsky.social" );
}

TEST_CASE( "GetMediaItemsFromAuthorFeedResponse extracts video metadata", "[BlueskyUtils]" )
{
    const Json response = {
        { "feed", Json::array( {
            {
                { "post", {
                    { "uri", "at://did:plc:alice/app.bsky.feed.post/2" },
                    { "author", { { "did", "did:plc:alice" }, { "handle", "alice.bsky.social" } } },
                    { "record", {
                        { "embed", {
                            { "$type", "app.bsky.embed.video" },
                            { "alt", "record video alt" },
                            { "video", {
                                { "ref", { { "$link", "bafy-video" } } },
                                { "mimeType", "video/mp4" }
                            } }
                        } }
                    } },
                    { "embed", {
                        { "$type", "app.bsky.embed.video#view" },
                        { "playlist", "https://video.bsky.app/playlist.m3u8" },
                        { "thumbnail", "https://video.bsky.app/thumb.jpg" },
                        { "alt", "view video alt" }
                    } }
                } }
            }
        } ) }
    };

    const MediaItemsPage page = GetMediaItemsFromAuthorFeedResponse( response, 5 );
    REQUIRE( page.m_Items.size( ) == 1 );

    const MediaItem& item = page.m_Items.front( );
    REQUIRE( item.m_Kind == MediaKind::Video );
    REQUIRE( item.m_DownloadUrl == "https://video.bsky.app/playlist.m3u8" );
    REQUIRE( item.m_ThumbnailUrl == "https://video.bsky.app/thumb.jpg" );
    REQUIRE( item.m_AltText == "view video alt" );
    REQUIRE( item.m_Cid == "bafy-video" );
    REQUIRE( item.m_MimeType == "video/mp4" );
}

TEST_CASE( "GetMediaItemsFromAuthorFeedResponse extracts nested recordWithMedia items", "[BlueskyUtils]" )
{
    const Json response = {
        { "feed", Json::array( {
            {
                { "post", {
                    { "uri", "at://did:plc:alice/app.bsky.feed.post/3" },
                    { "author", { { "did", "did:plc:alice" }, { "handle", "alice.bsky.social" } } },
                    { "record", {
                        { "embed", {
                            { "$type", "app.bsky.embed.recordWithMedia" },
                            { "media", {
                                { "$type", "app.bsky.embed.images" },
                                { "images", Json::array( {
                                    {
                                        { "image", {
                                            { "ref", { { "$link", "bafy-nested" } } },
                                            { "mimeType", "image/png" }
                                        } }
                                    }
                                } ) }
                            } }
                        } }
                    } },
                    { "embed", {
                        { "$type", "app.bsky.embed.recordWithMedia#view" },
                        { "media", {
                            { "$type", "app.bsky.embed.images#view" },
                            { "images", Json::array( {
                                {
                                    { "fullsize", "https://cdn.bsky.app/nested.png" },
                                    { "thumb", "https://cdn.bsky.app/nested-thumb.png" }
                                }
                            } ) }
                        } }
                    } }
                } }
            }
        } ) }
    };

    const MediaItemsPage page = GetMediaItemsFromAuthorFeedResponse( response, 5 );
    REQUIRE( page.m_Items.size( ) == 1 );
    REQUIRE( page.m_Items.front( ).m_DownloadUrl == "https://cdn.bsky.app/nested.png" );
    REQUIRE( page.m_Items.front( ).m_Cid == "bafy-nested" );
    REQUIRE( page.m_Items.front( ).m_MimeType == "image/png" );
}

TEST_CASE( "GetMediaItemsFromAuthorFeedResponse keeps unmatched view images without duplicates", "[BlueskyUtils]" )
{
    const Json response = {
        { "feed", Json::array( {
            {
                { "post", {
                    { "uri", "at://did:plc:alice/app.bsky.feed.post/4" },
                    { "author", { { "did", "did:plc:alice" }, { "handle", "alice.bsky.social" } } },
                    { "record", {
                        { "embed", {
                            { "$type", "app.bsky.embed.images" },
                            { "images", Json::array( {
                                {
                                    { "image", {
                                        { "ref", { { "$link", "bafy-first" } } },
                                        { "mimeType", "image/jpeg" }
                                    } }
                                }
                            } ) }
                        } }
                    } },
                    { "embed", {
                        { "$type", "app.bsky.embed.images#view" },
                        { "images", Json::array( {
                            {
                                { "fullsize", "https://cdn.bsky.app/first.jpg" },
                                { "thumb", "https://cdn.bsky.app/first-thumb.jpg" }
                            },
                            {
                                { "fullsize", "https://cdn.bsky.app/second.jpg" },
                                { "thumb", "https://cdn.bsky.app/second-thumb.jpg" }
                            }
                        } ) }
                    } }
                } }
            }
        } ) }
    };

    const MediaItemsPage page = GetMediaItemsFromAuthorFeedResponse( response, 5 );
    REQUIRE( page.m_Items.size( ) == 2 );
    REQUIRE( page.m_Items[ 0 ].m_DownloadUrl == "https://cdn.bsky.app/first.jpg" );
    REQUIRE( page.m_Items[ 0 ].m_Cid == "bafy-first" );
    REQUIRE( page.m_Items[ 1 ].m_DownloadUrl == "https://cdn.bsky.app/second.jpg" );
    REQUIRE( page.m_Items[ 1 ].m_Cid.empty( ) );
}

TEST_CASE( "GetMediaItemsFromAuthorFeedResponse respects maxItems", "[BlueskyUtils]" )
{
    const Json response = {
        { "feed", Json::array( {
            {
                { "post", {
                    { "uri", "at://did:plc:alice/app.bsky.feed.post/5" },
                    { "author", { { "did", "did:plc:alice" }, { "handle", "alice.bsky.social" } } },
                    { "record", {
                        { "embed", {
                            { "$type", "app.bsky.embed.images" },
                            { "images", Json::array( {
                                {
                                    { "image", {
                                        { "ref", { { "$link", "bafy-first" } } },
                                        { "mimeType", "image/jpeg" }
                                    } }
                                },
                                {
                                    { "image", {
                                        { "ref", { { "$link", "bafy-second" } } },
                                        { "mimeType", "image/jpeg" }
                                    } }
                                }
                            } ) }
                        } }
                    } },
                    { "embed", {
                        { "$type", "app.bsky.embed.images#view" },
                        { "images", Json::array( {
                            { { "fullsize", "https://cdn.bsky.app/one.jpg" } },
                            { { "fullsize", "https://cdn.bsky.app/two.jpg" } }
                        } ) }
                    } }
                } }
            },
            {
                { "post", {
                    { "uri", "at://did:plc:alice/app.bsky.feed.post/6" },
                    { "author", { { "did", "did:plc:alice" }, { "handle", "alice.bsky.social" } } },
                    { "record", {
                        { "embed", {
                            { "$type", "app.bsky.embed.video" },
                            { "video", {
                                { "ref", { { "$link", "bafy-video" } } },
                                { "mimeType", "video/mp4" }
                            } }
                        } }
                    } },
                    { "embed", {
                        { "$type", "app.bsky.embed.video#view" },
                        { "playlist", "https://video.bsky.app/playlist.m3u8" }
                    } }
                } }
            }
        } ) }
    };

    const MediaItemsPage page = GetMediaItemsFromAuthorFeedResponse( response, 2 );
    REQUIRE( page.m_Items.size( ) == 2 );
    REQUIRE( page.m_Items[ 0 ].m_DownloadUrl == "https://cdn.bsky.app/one.jpg" );
    REQUIRE( page.m_Items[ 1 ].m_DownloadUrl == "https://cdn.bsky.app/two.jpg" );
}
