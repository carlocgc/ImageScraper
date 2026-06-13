#include "CppUnitTest.h"
#include "utils/BlueskyUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper::BlueskyUtils;
    using Json = nlohmann::json;


    TEST_CLASS(BlueskyUtilsTests)
    {
    public:
    TEST_METHOD(IsDid_Identifies_DID_Actors)
    {
        Assert::IsTrue(  IsDid( "did:plc:alice" ) );
        Assert::IsFalse(  IsDid( "alice.bsky.social" ) );
    }
    
    TEST_METHOD(ParseDidFromResolveHandleResponse_Returns_Did_When_Present)
    {
        const Json response = { { "did", "did:plc:alice" } };
    
        const auto did = ParseDidFromResolveHandleResponse( response );
        Assert::IsTrue(  did.has_value( ) );
        Assert::IsTrue(  *did == "did:plc:alice" );
    }
    
    TEST_METHOD(ParseDidFromResolveHandleResponse_Returns_Nullopt_When_Did_Missing)
    {
        const Json response = { { "handle", "alice.bsky.social" } };
    
        Assert::IsFalse(  ParseDidFromResolveHandleResponse( response ).has_value( ) );
    }
    
    TEST_METHOD(GetMediaItemsFromAuthorFeedResponse_Extracts_Image_Metadata)
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
        Assert::IsTrue(  page.m_Cursor == "page-2" );
        Assert::IsTrue(  page.m_Items.size( ) == 1 );
    
        const MediaItem& item = page.m_Items.front( );
        Assert::IsTrue(  item.m_Kind == MediaKind::Image );
        Assert::IsTrue(  item.m_PostUri == "at://did:plc:alice/app.bsky.feed.post/1" );
        Assert::IsTrue(  item.m_DownloadUrl == "https://cdn.bsky.app/img1.jpg" );
        Assert::IsTrue(  item.m_ThumbnailUrl == "https://cdn.bsky.app/thumb1.jpg" );
        Assert::IsTrue(  item.m_AltText == "view alt" );
        Assert::IsTrue(  item.m_Cid == "bafy-image" );
        Assert::IsTrue(  item.m_MimeType == "image/jpeg" );
        Assert::IsTrue(  item.m_ActorDid == "did:plc:alice" );
        Assert::IsTrue(  item.m_ActorHandle == "alice.bsky.social" );
    }
    
    TEST_METHOD(GetMediaItemsFromAuthorFeedResponse_Extracts_Video_Metadata)
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
        Assert::IsTrue(  page.m_Items.size( ) == 1 );
    
        const MediaItem& item = page.m_Items.front( );
        Assert::IsTrue(  item.m_Kind == MediaKind::Video );
        Assert::IsTrue(  item.m_DownloadUrl == "https://video.bsky.app/playlist.m3u8" );
        Assert::IsTrue(  item.m_ThumbnailUrl == "https://video.bsky.app/thumb.jpg" );
        Assert::IsTrue(  item.m_AltText == "view video alt" );
        Assert::IsTrue(  item.m_Cid == "bafy-video" );
        Assert::IsTrue(  item.m_MimeType == "video/mp4" );
    }
    
    TEST_METHOD(GetMediaItemsFromAuthorFeedResponse_Extracts_Nested_RecordWithMedia_Items)
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
        Assert::IsTrue(  page.m_Items.size( ) == 1 );
        Assert::IsTrue(  page.m_Items.front( ).m_DownloadUrl == "https://cdn.bsky.app/nested.png" );
        Assert::IsTrue(  page.m_Items.front( ).m_Cid == "bafy-nested" );
        Assert::IsTrue(  page.m_Items.front( ).m_MimeType == "image/png" );
    }
    
    TEST_METHOD(GetMediaItemsFromAuthorFeedResponse_Keeps_Unmatched_View_Images_Without_Duplicates)
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
        Assert::IsTrue(  page.m_Items.size( ) == 2 );
        Assert::IsTrue(  page.m_Items[ 0 ].m_DownloadUrl == "https://cdn.bsky.app/first.jpg" );
        Assert::IsTrue(  page.m_Items[ 0 ].m_Cid == "bafy-first" );
        Assert::IsTrue(  page.m_Items[ 1 ].m_DownloadUrl == "https://cdn.bsky.app/second.jpg" );
        Assert::IsTrue(  page.m_Items[ 1 ].m_Cid.empty( ) );
    }
    
    TEST_METHOD(GetMediaItemsFromAuthorFeedResponse_Respects_MaxItems)
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
        Assert::IsTrue(  page.m_Items.size( ) == 2 );
        Assert::IsTrue(  page.m_Items[ 0 ].m_DownloadUrl == "https://cdn.bsky.app/one.jpg" );
        Assert::IsTrue(  page.m_Items[ 1 ].m_DownloadUrl == "https://cdn.bsky.app/two.jpg" );
    }
    
    TEST_METHOD(PrepareImageDownloads_Builds_Deterministic_Filenames_Per_Post)
    {
        const std::vector<MediaItem> mediaItems = {
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-123",
                "https://cdn.bsky.app/img1",
                "",
                "",
                "cid-1",
                "image/jpeg",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-123",
                "https://cdn.bsky.app/img2",
                "",
                "",
                "cid-2",
                "image/png",
                "did:plc:alice",
                "alice.bsky.social"
            }
        };
    
        const std::vector<ImageDownload> downloads = PrepareImageDownloads( mediaItems, 10, "fallback" );
        Assert::IsTrue(  downloads.size( ) == 2 );
        Assert::IsTrue(  downloads[ 0 ].m_ActorDirectory == "alice.bsky.social" );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "post-123_01.jpg" );
        Assert::IsTrue(  downloads[ 1 ].m_FileName == "post-123_02.png" );
    }
    
    TEST_METHOD(PrepareImageDownloads_Dedupes_Repeated_Image_URLs_And_Skips_Videos)
    {
        const std::vector<MediaItem> mediaItems = {
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-1",
                "https://cdn.bsky.app/shared-image@jpeg",
                "",
                "",
                "",
                "image/jpeg",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Video,
                "at://did:plc:alice/app.bsky.feed.post/post-2",
                "https://video.bsky.app/playlist.m3u8",
                "",
                "",
                "",
                "video/mp4",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-3",
                "https://cdn.bsky.app/shared-image@jpeg",
                "",
                "",
                "",
                "image/jpeg",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-3",
                "https://cdn.bsky.app/unique-image@png",
                "",
                "",
                "",
                "image/png",
                "did:plc:alice",
                "alice.bsky.social"
            }
        };
    
        const std::vector<ImageDownload> downloads = PrepareImageDownloads( mediaItems, 10, "fallback" );
        Assert::IsTrue(  downloads.size( ) == 2 );
        Assert::IsTrue(  downloads[ 0 ].m_SourceUrl == "https://cdn.bsky.app/shared-image@jpeg" );
        Assert::IsTrue(  downloads[ 1 ].m_SourceUrl == "https://cdn.bsky.app/unique-image@png" );
        Assert::IsTrue(  downloads[ 1 ].m_FileName == "post-3_01.png" );
    }
    
    TEST_METHOD(PrepareMediaDownloads_Includes_Bluesky_Videos_With_Deterministic_Mp4_Filenames)
    {
        const std::vector<MediaItem> mediaItems = {
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-1",
                "https://cdn.bsky.app/one@jpeg",
                "",
                "",
                "",
                "image/jpeg",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Video,
                "at://did:plc:alice/app.bsky.feed.post/post-2",
                "https://video.bsky.app/watch/did%3Aplc%3Aalice/bafy-video/playlist.m3u8?session_id=abc",
                "https://video.bsky.app/watch/did%3Aplc%3Aalice/bafy-video/thumbnail.jpg",
                "video alt",
                "bafy-video",
                "video/mp4",
                "did:plc:alice",
                "alice.bsky.social"
            }
        };
    
        const std::vector<PreparedDownload> downloads = PrepareMediaDownloads( mediaItems, 10, "fallback" );
        Assert::IsTrue(  downloads.size( ) == 2 );
        Assert::IsTrue(  downloads[ 0 ].m_Kind == MediaKind::Image );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "post-1_01.jpg" );
        Assert::IsTrue(  downloads[ 1 ].m_Kind == MediaKind::Video );
        Assert::IsTrue(  downloads[ 1 ].m_ActorDirectory == "alice.bsky.social" );
        Assert::IsTrue(  downloads[ 1 ].m_SourceUrl == "https://video.bsky.app/watch/did%3Aplc%3Aalice/bafy-video/playlist.m3u8?session_id=abc" );
        Assert::IsTrue(  downloads[ 1 ].m_FileName == "post-2_01.mp4" );
    }
    
    TEST_METHOD(PrepareMediaDownloads_Dedupes_Repeated_Bluesky_Video_Playlists)
    {
        const std::vector<MediaItem> mediaItems = {
            {
                MediaKind::Video,
                "at://did:plc:alice/app.bsky.feed.post/post-2",
                "https://video.bsky.app/watch/did%3Aplc%3Aalice/bafy-video/playlist.m3u8?session_id=abc",
                "",
                "",
                "bafy-video",
                "video/mp4",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Video,
                "at://did:plc:alice/app.bsky.feed.post/post-3",
                "https://video.bsky.app/watch/did%3Aplc%3Aalice/bafy-video/playlist.m3u8?session_id=xyz",
                "",
                "",
                "bafy-video",
                "video/mp4",
                "did:plc:alice",
                "alice.bsky.social"
            }
        };
    
        const std::vector<PreparedDownload> downloads = PrepareMediaDownloads( mediaItems, 10, "fallback" );
        Assert::IsTrue(  downloads.size( ) == 1 );
        Assert::IsTrue(  downloads[ 0 ].m_Kind == MediaKind::Video );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "post-2_01.mp4" );
    }
    
    TEST_METHOD(PrepareImageDownloads_Falls_Back_To_Sanitized_Actor_And_Url_Suffix_Extension)
    {
        const std::vector<MediaItem> mediaItems = {
            {
                MediaKind::Image,
                "at://did:plc:example/app.bsky.feed.post/post-9",
                "https://cdn.bsky.app/img/feed_fullsize/plain/did:plc:example/blob@webp?quality=100",
                "",
                "",
                "",
                "",
                "",
                ""
            }
        };
    
        const std::vector<ImageDownload> downloads = PrepareImageDownloads( mediaItems, 10, "did:plc:example" );
        Assert::IsTrue(  downloads.size( ) == 1 );
        Assert::IsTrue(  downloads[ 0 ].m_ActorDirectory == "did_plc_example" );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "post-9_01.webp" );
    }
    
    TEST_METHOD(PrepareImageDownloads_Prefers_Url_Suffix_Over_Mime_Type_For_Extension)
    {
        const std::vector<MediaItem> mediaItems = {
            {
                MediaKind::Image,
                "at://did:plc:example/app.bsky.feed.post/post-10",
                "https://cdn.bsky.app/img/feed_fullsize/plain/did:plc:example/blob@webp?quality=100",
                "",
                "",
                "",
                "image/jpeg",
                "did:plc:example",
                "example.bsky.social"
            }
        };
    
        const std::vector<ImageDownload> downloads = PrepareImageDownloads( mediaItems, 10, "fallback" );
        Assert::IsTrue(  downloads.size( ) == 1 );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "post-10_01.webp" );
    }
    
    TEST_METHOD(PrepareImageDownloads_Respects_MaxItems)
    {
        const std::vector<MediaItem> mediaItems = {
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-1",
                "https://cdn.bsky.app/one@jpeg",
                "",
                "",
                "",
                "image/jpeg",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-2",
                "https://cdn.bsky.app/two@jpeg",
                "",
                "",
                "",
                "image/jpeg",
                "did:plc:alice",
                "alice.bsky.social"
            },
            {
                MediaKind::Image,
                "at://did:plc:alice/app.bsky.feed.post/post-3",
                "https://cdn.bsky.app/three@jpeg",
                "",
                "",
                "",
                "image/jpeg",
                "did:plc:alice",
                "alice.bsky.social"
            }
        };
    
        const std::vector<ImageDownload> downloads = PrepareImageDownloads( mediaItems, 2, "fallback" );
        Assert::IsTrue(  downloads.size( ) == 2 );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "post-1_01.jpg" );
        Assert::IsTrue(  downloads[ 1 ].m_FileName == "post-2_01.jpg" );
    }
    
    };
}
