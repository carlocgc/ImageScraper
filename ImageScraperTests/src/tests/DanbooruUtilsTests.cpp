#include "CppUnitTest.h"
#include "utils/DanbooruUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{
    using namespace ImageScraper::DanbooruUtils;

    TEST_CLASS(DanbooruUtilsTests)
    {
    public:
    TEST_METHOD(NormalizeInput_Treats_Raw_Text_As_Query)
    {
        const NormalizedInput input = NormalizeInput( "pool:19776 rating:g" );

        Assert::IsTrue( input.m_Type == InputType::Query );
        Assert::IsTrue( input.m_Query == "pool:19776 rating:g" );
    }

    TEST_METHOD(NormalizeInput_Extracts_Q_From_Post_Page_Url)
    {
        const NormalizedInput input = NormalizeInput( "https://danbooru.donmai.us/posts/11661839?q=pool%3A19776" );

        Assert::IsTrue( input.m_Type == InputType::Query );
        Assert::IsTrue( input.m_Query == "pool:19776" );
        Assert::IsTrue( input.m_PostId.empty( ) );
    }

    TEST_METHOD(NormalizeInput_Extracts_Tags_From_Api_Url_And_Ignores_Limit)
    {
        const NormalizedInput input = NormalizeInput( "https://danbooru.donmai.us/posts.json?tags=pool%3A19776+rating%3Ag&limit=500&page=3" );

        Assert::IsTrue( input.m_Type == InputType::Query );
        Assert::IsTrue( input.m_Query == "pool:19776 rating:g" );
    }

    TEST_METHOD(NormalizeInput_Detects_Single_Post_Url_Without_Query)
    {
        const NormalizedInput input = NormalizeInput( "https://danbooru.donmai.us/posts/11661839" );

        Assert::IsTrue( input.m_Type == InputType::SinglePost );
        Assert::IsTrue( input.m_PostId == "11661839" );
    }

    TEST_METHOD(NormalizeInput_Rejects_Danbooru_Url_Without_Query_Or_Post_Id)
    {
        const NormalizedInput input = NormalizeInput( "https://danbooru.donmai.us/artists/661758" );

        Assert::IsTrue( input.m_Type == InputType::Invalid );
        Assert::IsFalse( input.m_Error.empty( ) );
    }

    TEST_METHOD(SanitizePathComponent_Replaces_Query_Separators)
    {
        Assert::IsTrue( SanitizePathComponent( "pool:19776 rating:g order:score" ) == "pool_19776_rating_g_order_score" );
    }

    TEST_METHOD(PreparedDownloadFromPost_Uses_Post_Id_And_File_Extension)
    {
        Json post = {
            { "id", 11661839 },
            { "file_url", "https://cdn.donmai.us/original/file.jpg" },
            { "file_ext", "jpg" }
        };

        const auto download = PreparedDownloadFromPost( post );

        Assert::IsTrue( download.has_value( ) );
        Assert::IsTrue( download->m_PostId == "11661839" );
        Assert::IsTrue( download->m_FileName == "11661839.jpg" );
        Assert::IsTrue( download->m_SourceUrl == "https://cdn.donmai.us/original/file.jpg" );
    }

    TEST_METHOD(PreparedDownloadFromPost_Falls_Back_To_Url_Extension)
    {
        Json post = {
            { "id", 123 },
            { "file_url", "https://cdn.donmai.us/original/hash.webm?download=1" }
        };

        const auto download = PreparedDownloadFromPost( post );

        Assert::IsTrue( download.has_value( ) );
        Assert::IsTrue( download->m_FileName == "123.webm" );
    }

    TEST_METHOD(PreparedDownloadFromPost_Skips_Posts_Missing_File_Url)
    {
        Json post = {
            { "id", 123 },
            { "file_ext", "jpg" }
        };

        Assert::IsFalse( PreparedDownloadFromPost( post ).has_value( ) );
    }

    TEST_METHOD(GetDownloadablePostsFromResponse_Dedupes_Post_Ids)
    {
        Json response = Json::array( {
            {
                { "id", 1 },
                { "file_url", "https://cdn.donmai.us/one.jpg" },
                { "file_ext", "jpg" }
            },
            {
                { "id", 1 },
                { "file_url", "https://cdn.donmai.us/one-again.jpg" },
                { "file_ext", "jpg" }
            },
            {
                { "id", 2 },
                { "file_url", "https://cdn.donmai.us/two.png" },
                { "file_ext", "png" }
            }
        } );

        const std::vector<PreparedDownload> downloads = GetDownloadablePostsFromResponse( response, 10 );

        Assert::IsTrue( downloads.size( ) == 2 );
        Assert::IsTrue( downloads[ 0 ].m_FileName == "1.jpg" );
        Assert::IsTrue( downloads[ 1 ].m_FileName == "2.png" );
    }

    TEST_METHOD(GetDownloadablePostsFromResponse_Respects_MaxItems)
    {
        Json response = Json::array( {
            {
                { "id", 1 },
                { "file_url", "https://cdn.donmai.us/one.jpg" },
                { "file_ext", "jpg" }
            },
            {
                { "id", 2 },
                { "file_url", "https://cdn.donmai.us/two.png" },
                { "file_ext", "png" }
            }
        } );

        const std::vector<PreparedDownload> downloads = GetDownloadablePostsFromResponse( response, 1 );

        Assert::IsTrue( downloads.size( ) == 1 );
        Assert::IsTrue( downloads[ 0 ].m_PostId == "1" );
    }
    };
}
