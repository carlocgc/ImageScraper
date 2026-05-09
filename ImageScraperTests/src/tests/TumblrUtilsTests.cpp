#include "CppUnitTest.h"
#include "utils/TumblrUtils.h"
#include "services/ServiceOptionTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;
    using namespace ImageScraper::TumblrUtils;


    TEST_CLASS(TumblrUtilsTests)
    {
    public:
    TEST_METHOD(GetMediaUrlsFromResponse_Returns_Empty_For_No_Posts)
    {
        Json response = { { "response", { { "posts", Json::array( ) } } } };
        Assert::IsTrue(  GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Extracts_Photo_URLs)
    {
        Json response = {
            { "response", {
                { "posts", {
                    {
                        { "type", "photo" },
                        { "photos", {
                            { { "original_size", { { "url", "https://example.tumblr.com/photo1.jpg" } } } },
                            { { "original_size", { { "url", "https://example.tumblr.com/photo2.jpg" } } } }
                        } }
                    }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 2 );
        Assert::IsTrue(  result[ 0 ] == "https://example.tumblr.com/photo1.jpg" );
        Assert::IsTrue(  result[ 1 ] == "https://example.tumblr.com/photo2.jpg" );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Extracts_Video_URL)
    {
        Json response = {
            { "response", {
                { "posts", {
                    {
                        { "type", "video" },
                        { "video_url", "https://example.tumblr.com/video.mp4" }
                    }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 1 );
        Assert::IsTrue(  result[ 0 ] == "https://example.tumblr.com/video.mp4" );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Skips_Video_Posts_Without_Video_Url)
    {
        Json response = {
            { "response", {
                { "posts", {
                    { { "type", "video" } }
                } }
            } }
        };
    
        Assert::IsTrue(  GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Skips_Photo_Posts_Without_Original_Size)
    {
        Json response = {
            { "response", {
                { "posts", {
                    {
                        { "type", "photo" },
                        { "photos", {
                            { { "alt_size", { { "url", "https://example.tumblr.com/photo.jpg" } } } }
                        } }
                    }
                } }
            } }
        };
    
        Assert::IsTrue(  GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Returns_Empty_For_Text_Post_With_No_Embedded_Media)
    {
        Json response = {
            { "response", {
                { "posts", {
                    { { "type", "text" }, { "body", "some text post with no images" } }
                } }
            } }
        };
    
        Assert::IsTrue(  GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Handles_Mixed_Post_Types)
    {
        Json response = {
            { "response", {
                { "posts", {
                    {
                        { "type", "photo" },
                        { "photos", {
                            { { "original_size", { { "url", "https://example.tumblr.com/photo.jpg" } } } }
                        } }
                    },
                    { { "type", "text" }, { "body", "ignored" } },
                    {
                        { "type", "video" },
                        { "video_url", "https://example.tumblr.com/video.mp4" }
                    }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 2 );
    }
    
    // ---------------------------------------------------------------------------
    // Text post - HTML body parsing
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(GetMediaUrlsFromResponse_Extracts_Img_Src_From_Text_Post_Body)
    {
        Json response = {
            { "response", {
                { "posts", {
                    {
                        { "type", "text" },
                        { "body", R"(<p>Hello</p><img src="https://cdn.tumblr.com/image.jpg" alt="pic">)" }
                    }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 1 );
        Assert::IsTrue(  result[ 0 ] == "https://cdn.tumblr.com/image.jpg" );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Prefers_Highest_Resolution_Srcset_Entry_Over_Src)
    {
        // srcset is ordered ascending by width - last entry is the original resolution
        const std::string body =
            R"(<img src="https://cdn.tumblr.com/s640x960/img.jpg" )"
            R"(srcset="https://cdn.tumblr.com/s75x75/img.jpg 75w, )"
            R"(https://cdn.tumblr.com/s400x600/img.jpg 400w, )"
            R"(https://cdn.tumblr.com/s640x960/img.jpg 640w, )"
            R"(https://cdn.tumblr.com/original/img.jpg 2448w">)";
    
        Json response = {
            { "response", {
                { "posts", {
                    { { "type", "text" }, { "body", body } }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 1 );
        Assert::IsTrue(  result[ 0 ] == "https://cdn.tumblr.com/original/img.jpg" );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Extracts_Multiple_Images_From_Text_Post_Body)
    {
        const std::string body =
            R"(<img src="https://cdn.tumblr.com/img1.jpg">)"
            R"(<p>some text</p>)"
            R"(<img src="https://cdn.tumblr.com/img2.jpg">)";
    
        Json response = {
            { "response", {
                { "posts", {
                    { { "type", "text" }, { "body", body } }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 2 );
        Assert::IsTrue(  result[ 0 ] == "https://cdn.tumblr.com/img1.jpg" );
        Assert::IsTrue(  result[ 1 ] == "https://cdn.tumblr.com/img2.jpg" );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Extracts_Video_Src_From_Text_Post_Body)
    {
        Json response = {
            { "response", {
                { "posts", {
                    {
                        { "type", "text" },
                        { "body", R"(<video src="https://cdn.tumblr.com/video.mp4"></video>)" }
                    }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 1 );
        Assert::IsTrue(  result[ 0 ] == "https://cdn.tumblr.com/video.mp4" );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Extracts_Source_Src_From_Text_Post_Body)
    {
        const std::string body =
            R"(<video><source src="https://cdn.tumblr.com/video.mp4" type="video/mp4"></video>)";
    
        Json response = {
            { "response", {
                { "posts", {
                    { { "type", "text" }, { "body", body } }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 1 );
        Assert::IsTrue(  result[ 0 ] == "https://cdn.tumblr.com/video.mp4" );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Extracts_Mixed_Img_And_Video_From_Text_Post_Body)
    {
        const std::string body =
            R"(<img src="https://cdn.tumblr.com/img.jpg">)"
            R"(<video src="https://cdn.tumblr.com/clip.mp4"></video>)";
    
        Json response = {
            { "response", {
                { "posts", {
                    { { "type", "text" }, { "body", body } }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX );
        Assert::IsTrue(  result.size( ) == 2 );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Skips_Text_Post_Without_Body_Field)
    {
        Json response = {
            { "response", {
                { "posts", {
                    { { "type", "text" } }
                } }
            } }
        };
    
        Assert::IsTrue(  GetMediaUrlsFromResponse( response, TUMBLR_LIMIT_MAX ).empty( ) );
    }
    
    // ---------------------------------------------------------------------------
    // Helper function unit tests
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(BestUrlFromSrcset_Returns_The_Last_Entry_URL)
    {
        const std::string srcset =
            "https://cdn.tumblr.com/s75x75/img.jpg 75w, "
            "https://cdn.tumblr.com/s400x600/img.jpg 400w, "
            "https://cdn.tumblr.com/original/img.jpg 2448w";
    
        Assert::IsTrue(  BestUrlFromSrcset( srcset ) == "https://cdn.tumblr.com/original/img.jpg" );
    }
    
    TEST_METHOD(BestUrlFromSrcset_Handles_A_Single_Entry_With_No_Comma)
    {
        Assert::IsTrue(  BestUrlFromSrcset( "https://cdn.tumblr.com/img.jpg 640w" ) == "https://cdn.tumblr.com/img.jpg" );
    }
    
    TEST_METHOD(BestUrlFromSrcset_Returns_Empty_For_An_Empty_Srcset)
    {
        Assert::IsTrue(  BestUrlFromSrcset( "" ).empty( ) );
    }
    
    TEST_METHOD(ExtractAttr_Returns_Value_For_Double_Quoted_Attribute)
    {
        Assert::IsTrue(  ExtractAttr( R"(<img src="https://example.com/img.jpg" alt="pic">)", "src" )
                 == "https://example.com/img.jpg" );
    }
    
    TEST_METHOD(ExtractAttr_Returns_Value_For_Single_Quoted_Attribute)
    {
        Assert::IsTrue(  ExtractAttr( "<img src='https://example.com/img.jpg'>", "src" )
                 == "https://example.com/img.jpg" );
    }
    
    TEST_METHOD(ExtractAttr_Returns_Empty_For_Missing_Attribute)
    {
        Assert::IsTrue(  ExtractAttr( R"(<img alt="pic">)", "src" ).empty( ) );
    }
    
    TEST_METHOD(GetMediaUrlsFromResponse_Respects_MaxItems_Limit)
    {
        Json response = {
            { "response", {
                { "posts", {
                    {
                        { "type", "photo" },
                        { "photos", {
                            { { "original_size", { { "url", "https://example.tumblr.com/photo1.jpg" } } } },
                            { { "original_size", { { "url", "https://example.tumblr.com/photo2.jpg" } } } },
                            { { "original_size", { { "url", "https://example.tumblr.com/photo3.jpg" } } } }
                        } }
                    },
                    {
                        { "type", "video" },
                        { "video_url", "https://example.tumblr.com/video.mp4" }
                    }
                } }
            } }
        };
    
        auto result = GetMediaUrlsFromResponse( response, 2 );
        Assert::IsTrue(  result.size( ) == 2 );
    }
    
    };
}
