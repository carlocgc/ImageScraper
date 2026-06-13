#include "CppUnitTest.h"
#include "utils/MastodonUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper::MastodonUtils;
    using Json = nlohmann::json;


    TEST_CLASS(MastodonUtilsTests)
    {
    public:
    TEST_METHOD(NormalizeInstanceUrl_Adds_Https_And_Strips_Path)
    {
        Assert::IsTrue(  NormalizeInstanceUrl( "mastodon.social/@Gargron" ) == "https://mastodon.social" );
        Assert::IsTrue(  NormalizeInstanceUrl( "https://Mastodon.Social/" ) == "https://mastodon.social" );
    }
    
    TEST_METHOD(NormalizeInstanceUrl_Preserves_Http_Localhost)
    {
        Assert::IsTrue(  NormalizeInstanceUrl( "http://localhost:3000/" ) == "http://localhost:3000" );
    }
    
    TEST_METHOD(NormalizeInstanceUrl_Rejects_Unsupported_Scheme)
    {
        Assert::IsTrue(  NormalizeInstanceUrl( "ftp://mastodon.social" ).empty( ) );
    }
    
    TEST_METHOD(NormalizeAccountInput_Handles_Handles_And_URLs)
    {
        NormalizedAccount account = NormalizeAccountInput( "@Gargron@mastodon.social" );
        Assert::IsTrue(  account.m_SearchQuery == "Gargron@mastodon.social" );
        Assert::IsTrue(  account.m_Username == "Gargron" );
        Assert::IsTrue(  account.m_Domain == "mastodon.social" );
    
        account = NormalizeAccountInput( "https://mastodon.social/@Gargron" );
        Assert::IsTrue(  account.m_SearchQuery == "Gargron@mastodon.social" );
        Assert::IsTrue(  account.m_Username == "Gargron" );
        Assert::IsTrue(  account.m_Domain == "mastodon.social" );
    }
    
    TEST_METHOD(SelectAccountFromSearchResponse_Matches_Remote_Acct_Exactly)
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
        Assert::IsTrue(  account.has_value( ) );
        Assert::IsTrue(  account->m_Id == "2" );
    }
    
    TEST_METHOD(SelectAccountFromSearchResponse_Treats_Instance_Local_Account_As_Exact_Match)
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
        Assert::IsTrue(  account.has_value( ) );
        Assert::IsTrue(  account->m_Id == "7" );
    }
    
    TEST_METHOD(GetMediaItemsFromStatusesResponse_Extracts_Supported_Attachments)
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
        Assert::IsTrue(  items.size( ) == 3 );
        Assert::IsTrue(  items[ 0 ].m_Kind == MediaKind::Image );
        Assert::IsTrue(  items[ 0 ].m_DownloadUrl == "https://cdn.example.com/media/photo.jpeg?name=small" );
        Assert::IsTrue(  items[ 0 ].m_PreviewUrl == "https://cdn.example.com/media/photo-preview.jpeg" );
        Assert::IsTrue(  items[ 0 ].m_Description == "photo alt" );
        Assert::IsTrue(  items[ 0 ].m_AccountAcct == "alice@example.com" );
        Assert::IsTrue(  items[ 1 ].m_Kind == MediaKind::Gifv );
        Assert::IsTrue(  items[ 2 ].m_Kind == MediaKind::Video );
        Assert::IsTrue(  items[ 2 ].m_DownloadUrl == "https://remote.example.com/media/video.mov" );
    }
    
    TEST_METHOD(GetMediaItemsFromStatusesResponse_Respects_MaxItems)
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
        Assert::IsTrue(  items.size( ) == 1 );
        Assert::IsTrue(  items[ 0 ].m_DownloadUrl == "https://cdn.example.com/one.jpg" );
    }
    
    TEST_METHOD(GetLastStatusIdFromStatusesResponse_Returns_Last_Available_Status_Id)
    {
        const Json response = Json::array( {
            { { "id", "100" } },
            { { "content", "missing id" } },
            { { "id", "099" } }
        } );
    
        Assert::IsTrue(  GetLastStatusIdFromStatusesResponse( response ) == "099" );
    }
    
    TEST_METHOD(GetLastStatusIdFromStatusesResponse_Handles_Invalid_Responses)
    {
        Assert::IsTrue(  GetLastStatusIdFromStatusesResponse( Json::object( ) ).empty( ) );
        Assert::IsTrue(  GetLastStatusIdFromStatusesResponse( Json::array( ) ).empty( ) );
    }
    
    TEST_METHOD(PrepareMediaDownloads_Builds_Deterministic_Paths_And_Filenames)
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
        Assert::IsTrue(  downloads.size( ) == 2 );
        Assert::IsTrue(  downloads[ 0 ].m_InstanceDirectory == "mastodon.social" );
        Assert::IsTrue(  downloads[ 0 ].m_AccountDirectory == "alice_example.com" );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "100_01.jpg" );
        Assert::IsTrue(  downloads[ 1 ].m_FileName == "100_02.mp4" );
    }
    
    TEST_METHOD(PrepareMediaDownloads_Dedupes_URLs_Without_Query_Strings)
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
        Assert::IsTrue(  downloads.size( ) == 1 );
        Assert::IsTrue(  downloads[ 0 ].m_FileName == "100_01.jpg" );
    }
    
    };
}
