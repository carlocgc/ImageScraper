#include "CppUnitTest.h"
#include "utils/RedgifsUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    namespace Utils = ImageScraper::RedgifsUtils;

    // IsRedgifsUrl ----------------------------------------------------------------


    TEST_CLASS(RedgifsUtilsTests)
    {
    public:
    TEST_METHOD(IsRedgifsUrl_Matches_Canonical_Watch_URLs)
    {
        Assert::IsTrue(  Utils::IsRedgifsUrl( "https://redgifs.com/watch/someslug" ) );
        Assert::IsTrue(  Utils::IsRedgifsUrl( "https://www.redgifs.com/watch/SomeSlug" ) );
        Assert::IsTrue(  Utils::IsRedgifsUrl( "https://v3.redgifs.com/watch/another" ) );
        Assert::IsTrue(  Utils::IsRedgifsUrl( "http://redgifs.com/ifr/embedded" ) );
    }
    
    TEST_METHOD(IsRedgifsUrl_Rejects_Unrelated_URLs)
    {
        Assert::IsFalse(  Utils::IsRedgifsUrl( "https://gfycat.com/Slug" ) );
        Assert::IsFalse(  Utils::IsRedgifsUrl( "https://i.redd.it/foo.jpg" ) );
        Assert::IsFalse(  Utils::IsRedgifsUrl( "" ) );
    }
    
    TEST_METHOD(IsRedgifsUrl_Rejects_Direct_CDN_URLs_That_Need_No_Resolution)
    {
        Assert::IsFalse(  Utils::IsRedgifsUrl( "https://media.redgifs.com/PersonalInfamousCanadagoose.mp4" ) );
        Assert::IsFalse(  Utils::IsRedgifsUrl( "https://media.redgifs.com/Slug-mobile.mp4" ) );
        Assert::IsFalse(  Utils::IsRedgifsUrl( "https://media.redgifs.com/Slug-poster.jpg" ) );
        Assert::IsFalse(  Utils::IsRedgifsUrl( "https://thumbs.redgifs.com/Foo.gif" ) );
        Assert::IsFalse(  Utils::IsRedgifsUrl( "https://www.redgifs.com/" ) );
    }
    
    TEST_METHOD(IsRedgifsUrl_Is_Case_Insensitive_On_The_Host)
    {
        Assert::IsTrue(  Utils::IsRedgifsUrl( "https://RedGifs.com/watch/X" ) );
        Assert::IsTrue(  Utils::IsRedgifsUrl( "https://WWW.REDGIFS.COM/watch/X" ) );
    }
    
    // ExtractSlug -----------------------------------------------------------------
    
    TEST_METHOD(ExtractSlug_Pulls_The_Lowercased_Slug_From_Watch_URLs)
    {
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/watch/someslug" ) == "someslug" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://www.redgifs.com/watch/MixedCaseSlug" ) == "mixedcaseslug" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://v3.redgifs.com/watch/AnotherSlug" ) == "anotherslug" );
    }
    
    TEST_METHOD(ExtractSlug_Supports_The_Ifr_Embed_Form)
    {
        Assert::IsTrue(  Utils::ExtractSlug( "https://www.redgifs.com/ifr/EmbedSlug" ) == "embedslug" );
    }
    
    TEST_METHOD(ExtractSlug_Strips_Query_String_And_Fragment)
    {
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/watch/someslug?utm=foo" ) == "someslug" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/watch/someslug#t=10" )    == "someslug" );
    }
    
    TEST_METHOD(ExtractSlug_Strips_Trailing_Path_Segments)
    {
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/watch/someslug/extra" ) == "someslug" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/watch/someslug/" )      == "someslug" );
    }
    
    TEST_METHOD(ExtractSlug_Strips_SEO_Descriptor_Tails_After_The_Slug)
    {
        Assert::IsTrue(  Utils::ExtractSlug( "https://www.redgifs.com/watch/assuredunsungbeaver-homemade-orgasms-with-taleri-love" ) == "assuredunsungbeaver" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/watch/MixedCaseSlug-some-descriptive-words" ) == "mixedcaseslug" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://www.redgifs.com/ifr/EmbedSlug-with-tail" ) == "embedslug" );
    }
    
    TEST_METHOD(ExtractSlug_Returns_Empty_For_URLs_That_Don_T_Carry_A_Slug)
    {
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/" )         == "" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://redgifs.com/watch/" )   == "" );
        Assert::IsTrue(  Utils::ExtractSlug( "https://gfycat.com/SomeSlug" )  == "" );
        Assert::IsTrue(  Utils::ExtractSlug( "" )                             == "" );
    }
    
    // ExtractTokenFromAuthResponse ------------------------------------------------
    
    TEST_METHOD(ExtractTokenFromAuthResponse_Pulls_Token_And_Ttl_From_A_Valid_Response)
    {
        const std::string body = R"({"token":"abc.def.ghi","expiresIn":7200,"addr":"1.2.3.4"})";
        std::string token{ };
        int ttl = 0;
        Assert::IsTrue(  Utils::ExtractTokenFromAuthResponse( body, token, ttl ) );
        Assert::IsTrue(  token == "abc.def.ghi" );
        Assert::IsTrue(  ttl == 7200 );
    }
    
    TEST_METHOD(ExtractTokenFromAuthResponse_Defaults_Ttl_When_ExpiresIn_Is_Absent)
    {
        const std::string body = R"({"token":"abc","addr":"1.2.3.4"})";
        std::string token{ };
        int ttl = 0;
        Assert::IsTrue(  Utils::ExtractTokenFromAuthResponse( body, token, ttl ) );
        Assert::IsTrue(  token == "abc" );
        Assert::IsTrue(  ttl == 3600 );
    }
    
    TEST_METHOD(ExtractTokenFromAuthResponse_Fails_When_Token_Is_Missing)
    {
        const std::string body = R"({"addr":"1.2.3.4"})";
        std::string token = "untouched";
        int ttl = 1;
        Assert::IsFalse(  Utils::ExtractTokenFromAuthResponse( body, token, ttl ) );
        Assert::IsTrue(  token == "untouched" );
        Assert::IsTrue(  ttl == 1 );
    }
    
    TEST_METHOD(ExtractTokenFromAuthResponse_Fails_On_Malformed_JSON)
    {
        std::string token{ };
        int ttl = 0;
        Assert::IsFalse(  Utils::ExtractTokenFromAuthResponse( "not json at all", token, ttl ) );
        Assert::IsTrue(  token.empty( ) );
    }
    
    // ExtractMediaUrlFromGifResponse ----------------------------------------------
    
    TEST_METHOD(ExtractMediaUrlFromGifResponse_Prefers_HD_Over_SD)
    {
        const std::string body = R"({"gif":{"id":"slug","urls":{"hd":"https://media.redgifs.com/Slug.mp4","sd":"https://media.redgifs.com/Slug-mobile.mp4"}}})";
        Assert::IsTrue(  Utils::ExtractMediaUrlFromGifResponse( body ) == "https://media.redgifs.com/Slug.mp4" );
    }
    
    TEST_METHOD(ExtractMediaUrlFromGifResponse_Falls_Back_To_SD_When_HD_Is_Missing)
    {
        const std::string body = R"({"gif":{"urls":{"sd":"https://media.redgifs.com/Slug-mobile.mp4"}}})";
        Assert::IsTrue(  Utils::ExtractMediaUrlFromGifResponse( body ) == "https://media.redgifs.com/Slug-mobile.mp4" );
    }
    
    TEST_METHOD(ExtractMediaUrlFromGifResponse_Returns_Empty_When_No_HD_Or_SD_URL_Is_Present)
    {
        const std::string body = R"({"gif":{"urls":{"poster":"https://media.redgifs.com/Slug-poster.jpg"}}})";
        Assert::IsTrue(  Utils::ExtractMediaUrlFromGifResponse( body ).empty( ) );
    }
    
    TEST_METHOD(ExtractMediaUrlFromGifResponse_Returns_Empty_On_Missing_Gif_Object)
    {
        Assert::IsTrue(  Utils::ExtractMediaUrlFromGifResponse( R"({"error":"not found"})" ).empty( ) );
        Assert::IsTrue(  Utils::ExtractMediaUrlFromGifResponse( "totally not json" ).empty( ) );
    }
    
    // ExtractMediaUrlsFromUserSearchResponse --------------------------------------
    
    TEST_METHOD(ExtractMediaUrlsFromUserSearchResponse_Returns_One_URL_Per_Gif_HD_Preferred)
    {
        const std::string body = R"({
            "page": 1,
            "pages": 3,
            "gifs": [
                { "urls": { "hd": "https://media.redgifs.com/A.mp4", "sd": "https://media.redgifs.com/A-mobile.mp4" } },
                { "urls": { "sd": "https://media.redgifs.com/B-mobile.mp4" } },
                { "urls": { "poster": "https://media.redgifs.com/C-poster.jpg" } }
            ]
        })";
    
        int totalPages = 0;
        const std::vector<std::string> urls = Utils::ExtractMediaUrlsFromUserSearchResponse( body, totalPages );
        Assert::IsTrue(  totalPages == 3 );
        Assert::IsTrue(  urls.size( ) == 2 );
        Assert::IsTrue(  urls[ 0 ] == "https://media.redgifs.com/A.mp4" );
        Assert::IsTrue(  urls[ 1 ] == "https://media.redgifs.com/B-mobile.mp4" );
    }
    
    TEST_METHOD(ExtractMediaUrlsFromUserSearchResponse_Handles_Empty_Gifs_Array)
    {
        int totalPages = 0;
        const std::vector<std::string> urls = Utils::ExtractMediaUrlsFromUserSearchResponse( R"({"page":1,"pages":0,"gifs":[]})", totalPages );
        Assert::IsTrue(  urls.empty( ) );
        Assert::IsTrue(  totalPages == 0 );
    }
    
    TEST_METHOD(ExtractMediaUrlsFromUserSearchResponse_Returns_Empty_On_Malformed_JSON)
    {
        int totalPages = 0;
        const std::vector<std::string> urls = Utils::ExtractMediaUrlsFromUserSearchResponse( "not json", totalPages );
        Assert::IsTrue(  urls.empty( ) );
        Assert::IsTrue(  totalPages == 0 );
    }
    
    };
}
