#include "catch2/catch_amalgamated.hpp"
#include "utils/MastodonUtils.h"

using namespace ImageScraper::MastodonUtils;
using Json = nlohmann::json;

TEST_CASE( "NormalizeInstanceUrl adds https and strips path", "[MastodonUtils]" )
{
    REQUIRE( NormalizeInstanceUrl( "mastodon.social/@Gargron" ) == "https://mastodon.social" );
    REQUIRE( NormalizeInstanceUrl( "https://Mastodon.Social/" ) == "https://mastodon.social" );
}

TEST_CASE( "NormalizeInstanceUrl preserves http localhost", "[MastodonUtils]" )
{
    REQUIRE( NormalizeInstanceUrl( "http://localhost:3000/" ) == "http://localhost:3000" );
}

TEST_CASE( "NormalizeInstanceUrl rejects unsupported scheme", "[MastodonUtils]" )
{
    REQUIRE( NormalizeInstanceUrl( "ftp://mastodon.social" ).empty( ) );
}

TEST_CASE( "NormalizeAccountInput handles handles and URLs", "[MastodonUtils]" )
{
    NormalizedAccount account = NormalizeAccountInput( "@Gargron@mastodon.social" );
    REQUIRE( account.m_SearchQuery == "Gargron@mastodon.social" );
    REQUIRE( account.m_Username == "Gargron" );
    REQUIRE( account.m_Domain == "mastodon.social" );

    account = NormalizeAccountInput( "https://mastodon.social/@Gargron" );
    REQUIRE( account.m_SearchQuery == "Gargron@mastodon.social" );
    REQUIRE( account.m_Username == "Gargron" );
    REQUIRE( account.m_Domain == "mastodon.social" );
}

TEST_CASE( "SelectAccountFromSearchResponse matches remote acct exactly", "[MastodonUtils]" )
{
    const Json response = {
        { "accounts", Json::array( {
            {
                { "id", "1" },
                { "username", "other" },
                { "acct", "other@example.com" },
                { "url", "https://example.com/@other" }
            },
            {
                { "id", "2" },
                { "username", "alice" },
                { "acct", "alice@example.com" },
                { "url", "https://example.com/@alice" }
            }
        } ) }
    };

    const std::optional<Account> account = SelectAccountFromSearchResponse( response, "@alice@example.com", "https://mastodon.social" );
    REQUIRE( account.has_value( ) );
    REQUIRE( account->m_Id == "2" );
}

TEST_CASE( "SelectAccountFromSearchResponse treats instance-local account as exact match", "[MastodonUtils]" )
{
    const Json response = {
        { "accounts", Json::array( {
            {
                { "id", "7" },
                { "username", "Gargron" },
                { "acct", "Gargron" },
                { "url", "https://mastodon.social/@Gargron" }
            }
        } ) }
    };

    const std::optional<Account> account = SelectAccountFromSearchResponse( response, "@Gargron@mastodon.social", "https://mastodon.social" );
    REQUIRE( account.has_value( ) );
    REQUIRE( account->m_Id == "7" );
}

TEST_CASE( "GetMediaItemsFromStatusesResponse extracts supported attachments", "[MastodonUtils]" )
{
    const Json response = Json::array( {
        {
            { "id", "100" },
            { "account", { { "acct", "alice@example.com" } } },
            { "media_attachments", Json::array( {
                {
                    { "id", "att-1" },
                    { "type", "image" },
                    { "url", "https://cdn.example.com/media/photo.jpeg?name=small" },
                    { "preview_url", "https://cdn.example.com/media/photo-preview.jpeg" },
                    { "description", "photo alt" }
                },
                {
                    { "id", "att-2" },
                    { "type", "gifv" },
                    { "url", "https://cdn.example.com/media/loop.mp4" }
                },
                {
                    { "id", "att-3" },
                    { "type", "audio" },
                    { "url", "https://cdn.example.com/media/audio.mp3" }
                }
            } ) }
        },
        {
            { "id", "101" },
            { "account", { { "acct", "alice@example.com" } } },
            { "media_attachments", Json::array( {
                {
                    { "id", "att-4" },
                    { "type", "video" },
                    { "remote_url", "https://remote.example.com/media/video.mov" }
                }
            } ) }
        }
    } );

    const std::vector<MediaItem> items = GetMediaItemsFromStatusesResponse( response, 10 );
    REQUIRE( items.size( ) == 3 );
    REQUIRE( items[ 0 ].m_Kind == MediaKind::Image );
    REQUIRE( items[ 0 ].m_DownloadUrl == "https://cdn.example.com/media/photo.jpeg?name=small" );
    REQUIRE( items[ 0 ].m_PreviewUrl == "https://cdn.example.com/media/photo-preview.jpeg" );
    REQUIRE( items[ 0 ].m_Description == "photo alt" );
    REQUIRE( items[ 0 ].m_AccountAcct == "alice@example.com" );
    REQUIRE( items[ 1 ].m_Kind == MediaKind::Gifv );
    REQUIRE( items[ 2 ].m_Kind == MediaKind::Video );
    REQUIRE( items[ 2 ].m_DownloadUrl == "https://remote.example.com/media/video.mov" );
}

TEST_CASE( "GetMediaItemsFromStatusesResponse respects maxItems", "[MastodonUtils]" )
{
    const Json response = Json::array( {
        {
            { "id", "100" },
            { "media_attachments", Json::array( {
                { { "type", "image" }, { "url", "https://cdn.example.com/one.jpg" } },
                { { "type", "image" }, { "url", "https://cdn.example.com/two.jpg" } }
            } ) }
        }
    } );

    const std::vector<MediaItem> items = GetMediaItemsFromStatusesResponse( response, 1 );
    REQUIRE( items.size( ) == 1 );
    REQUIRE( items[ 0 ].m_DownloadUrl == "https://cdn.example.com/one.jpg" );
}

TEST_CASE( "GetLastStatusIdFromStatusesResponse returns last available status id", "[MastodonUtils]" )
{
    const Json response = Json::array( {
        { { "id", "100" } },
        { { "content", "missing id" } },
        { { "id", "099" } }
    } );

    REQUIRE( GetLastStatusIdFromStatusesResponse( response ) == "099" );
}

TEST_CASE( "GetLastStatusIdFromStatusesResponse handles invalid responses", "[MastodonUtils]" )
{
    REQUIRE( GetLastStatusIdFromStatusesResponse( Json::object( ) ).empty( ) );
    REQUIRE( GetLastStatusIdFromStatusesResponse( Json::array( ) ).empty( ) );
}

TEST_CASE( "PrepareMediaDownloads builds deterministic paths and filenames", "[MastodonUtils]" )
{
    const std::vector<MediaItem> items = {
        {
            MediaKind::Image,
            "100",
            "att-1",
            "https://cdn.example.com/photo.jpeg?name=small",
            "",
            "",
            "alice@example.com"
        },
        {
            MediaKind::Video,
            "100",
            "att-2",
            "https://cdn.example.com/video",
            "",
            "",
            "alice@example.com"
        }
    };

    const std::vector<PreparedDownload> downloads = PrepareMediaDownloads( items, 10, "https://mastodon.social", "@alice@example.com" );
    REQUIRE( downloads.size( ) == 2 );
    REQUIRE( downloads[ 0 ].m_InstanceDirectory == "mastodon.social" );
    REQUIRE( downloads[ 0 ].m_AccountDirectory == "alice_example.com" );
    REQUIRE( downloads[ 0 ].m_FileName == "100_01.jpg" );
    REQUIRE( downloads[ 1 ].m_FileName == "100_02.mp4" );
}

TEST_CASE( "PrepareMediaDownloads dedupes URLs without query strings", "[MastodonUtils]" )
{
    const std::vector<MediaItem> items = {
        {
            MediaKind::Image,
            "100",
            "att-1",
            "https://cdn.example.com/photo.jpg?name=small",
            "",
            "",
            "alice@example.com"
        },
        {
            MediaKind::Image,
            "101",
            "att-2",
            "https://cdn.example.com/photo.jpg?name=large",
            "",
            "",
            "alice@example.com"
        }
    };

    const std::vector<PreparedDownload> downloads = PrepareMediaDownloads( items, 10, "mastodon.social", "alice" );
    REQUIRE( downloads.size( ) == 1 );
    REQUIRE( downloads[ 0 ].m_FileName == "100_01.jpg" );
}
