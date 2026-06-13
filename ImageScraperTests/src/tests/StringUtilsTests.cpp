#include "CppUnitTest.h"
#include "utils/StringUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    // ---------------------------------------------------------------------------
    // ExtractQueryParam
    // ---------------------------------------------------------------------------


    TEST_CLASS(StringUtilsTests)
    {
    public:
    TEST_METHOD(ExtractQueryParam_Returns_Value_For_First_Parameter)
    {
        const std::string request = "GET /?code=abc123&state=xyz HTTP/1.1";
        Assert::IsTrue(  StringUtils::ExtractQueryParam( request, "code" ) == "abc123" );
    }
    
    TEST_METHOD(ExtractQueryParam_Returns_Value_For_Middle_Parameter)
    {
        const std::string request = "GET /?error=access_denied&state=xyz&foo=bar HTTP/1.1";
        Assert::IsTrue(  StringUtils::ExtractQueryParam( request, "state" ) == "xyz" );
    }
    
    TEST_METHOD(ExtractQueryParam_Returns_Value_For_Last_Parameter_Before_HTTP_Version)
    {
        const std::string request = "GET /?code=abc123&state=xyz HTTP/1.1";
        Assert::IsTrue(  StringUtils::ExtractQueryParam( request, "state" ) == "xyz" );
    }
    
    TEST_METHOD(ExtractQueryParam_Returns_Empty_String_When_Key_Is_Absent)
    {
        const std::string request = "GET /?code=abc123&state=xyz HTTP/1.1";
        Assert::IsTrue(  StringUtils::ExtractQueryParam( request, "refresh_token" ).empty( ) );
    }
    
    TEST_METHOD(ExtractQueryParam_Does_Not_Partially_Match_A_Longer_Key)
    {
        // 'error' must not match 'error_description'
        const std::string request = "GET /?error_description=some+message&state=xyz HTTP/1.1";
        Assert::IsTrue(  StringUtils::ExtractQueryParam( request, "error" ).empty( ) );
    }
    
    TEST_METHOD(ExtractQueryParam_Returns_Empty_String_For_Empty_Input)
    {
        Assert::IsTrue(  StringUtils::ExtractQueryParam( "", "code" ).empty( ) );
    }
    
    // ---------------------------------------------------------------------------
    // UrlEncode
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(UrlEncode_Leaves_Unreserved_Characters_Unchanged)
    {
        Assert::IsTrue(  StringUtils::UrlEncode( "abc-_.~" ) == "abc-_.~" );
    }
    
    TEST_METHOD(UrlEncode_Percent_Encodes_Reserved_Characters)
    {
        Assert::IsTrue(  StringUtils::UrlEncode( "http://localhost:8080" ) == "http%3A%2F%2Flocalhost%3A8080" );
    }
    
    TEST_METHOD(UrlEncode_Percent_Encodes_Spaces)
    {
        Assert::IsTrue(  StringUtils::UrlEncode( "basic offline_access" ) == "basic%20offline_access" );
    }
    
    TEST_METHOD(UrlEncode_Returns_Empty_String_For_Empty_Input)
    {
        Assert::IsTrue(  StringUtils::UrlEncode( "" ).empty( ) );
    }
    
    // ---------------------------------------------------------------------------
    // UrlDecode
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(UrlDecode_Converts_Plus_Signs_To_Spaces)
    {
        Assert::IsTrue(  StringUtils::UrlDecode( "hello+world" ) == "hello world" );
    }
    
    TEST_METHOD(UrlDecode_Converts_Percent_Encoded_Sequences)
    {
        Assert::IsTrue(  StringUtils::UrlDecode( "http%3A%2F%2Flocalhost%3A8080" ) == "http://localhost:8080" );
    }
    
    TEST_METHOD(UrlDecode_Converts_Percent_Encoded_Space)
    {
        Assert::IsTrue(  StringUtils::UrlDecode( "hello%20world" ) == "hello world" );
    }
    
    TEST_METHOD(UrlDecode_Leaves_Plain_Text_Unchanged)
    {
        Assert::IsTrue(  StringUtils::UrlDecode( "basic" ) == "basic" );
    }
    
    TEST_METHOD(UrlDecode_Returns_Empty_String_For_Empty_Input)
    {
        Assert::IsTrue(  StringUtils::UrlDecode( "" ).empty( ) );
    }
    
    TEST_METHOD(UrlEncode_And_UrlDecode_Are_Inverse_Operations)
    {
        const std::string original = "http://localhost:8080/callback?foo=bar&baz=qux";
        Assert::IsTrue(  StringUtils::UrlDecode( StringUtils::UrlEncode( original ) ) == original );
    }
    
    // ---------------------------------------------------------------------------
    // ToLower
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(ToLower_Lowercases_ASCII_Letters)
    {
        Assert::IsTrue(  StringUtils::ToLower( "Hello World" ) == "hello world" );
        Assert::IsTrue(  StringUtils::ToLower( "ALLCAPS" )     == "allcaps" );
        Assert::IsTrue(  StringUtils::ToLower( "MixedCase42" ) == "mixedcase42" );
    }
    
    TEST_METHOD(ToLower_Leaves_Non_ASCII_Bytes_Alone)
    {
        // The bytes for u00e9 ("é") in UTF-8 are 0xC3 0xA9 - both must pass through.
        const std::string input  = "Caf\xC3\xA9";
        const std::string output = StringUtils::ToLower( input );
        Assert::IsTrue(  output == "caf\xC3\xA9" );
    }
    
    TEST_METHOD(ToLower_Returns_Empty_String_For_Empty_Input)
    {
        Assert::IsTrue(  StringUtils::ToLower( "" ) == "" );
    }
    
    // ---------------------------------------------------------------------------
    // Trim
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(Trim_Removes_Leading_And_Trailing_Whitespace)
    {
        Assert::IsTrue(  StringUtils::Trim( "  hello  " )      == "hello" );
        Assert::IsTrue(  StringUtils::Trim( "\t\n hello \r\n" ) == "hello" );
    }
    
    TEST_METHOD(Trim_Leaves_Interior_Whitespace_Alone)
    {
        Assert::IsTrue(  StringUtils::Trim( "  hello world  " ) == "hello world" );
    }
    
    TEST_METHOD(Trim_Returns_Empty_String_When_Input_Is_Whitespace_Only)
    {
        Assert::IsTrue(  StringUtils::Trim( "   " )    == "" );
        Assert::IsTrue(  StringUtils::Trim( "\t\r\n" ) == "" );
        Assert::IsTrue(  StringUtils::Trim( "" )       == "" );
    }
    
    // ---------------------------------------------------------------------------
    // StartsWith
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(StartsWith_Matches_A_True_Prefix)
    {
        Assert::IsTrue(  StringUtils::StartsWith( "https://example.com", "https://" ) );
        Assert::IsTrue(  StringUtils::StartsWith( "abcdef", "abc" ) );
    }
    
    TEST_METHOD(StartsWith_Returns_False_When_Prefix_Doesn_T_Match)
    {
        Assert::IsFalse(  StringUtils::StartsWith( "https://example.com", "http://" ) );
        Assert::IsFalse(  StringUtils::StartsWith( "ab", "abc" ) ); // prefix longer than value
    }
    
    TEST_METHOD(StartsWith_With_Empty_Prefix_Is_Always_True)
    {
        Assert::IsTrue(  StringUtils::StartsWith( "anything", "" ) );
        Assert::IsTrue(  StringUtils::StartsWith( "", "" ) );
    }
    
    // ---------------------------------------------------------------------------
    // StripUrlQueryAndFragment
    // ---------------------------------------------------------------------------
    
    TEST_METHOD(StripUrlQueryAndFragment_Removes_Query_String)
    {
        Assert::IsTrue(  StringUtils::StripUrlQueryAndFragment( "https://example.com/path?foo=bar" ) == "https://example.com/path" );
    }
    
    TEST_METHOD(StripUrlQueryAndFragment_Removes_Fragment)
    {
        Assert::IsTrue(  StringUtils::StripUrlQueryAndFragment( "https://example.com/path#section" ) == "https://example.com/path" );
    }
    
    TEST_METHOD(StripUrlQueryAndFragment_Removes_Whichever_Terminator_Comes_First)
    {
        // Query before fragment.
        Assert::IsTrue(  StringUtils::StripUrlQueryAndFragment( "https://x.test/p?q=1#frag" ) == "https://x.test/p" );
        // Fragment before query (rare, but valid byte order).
        Assert::IsTrue(  StringUtils::StripUrlQueryAndFragment( "https://x.test/p#frag?q=1" ) == "https://x.test/p" );
    }
    
    TEST_METHOD(StripUrlQueryAndFragment_Passes_URLs_Without_Terminators_Through_Unchanged)
    {
        Assert::IsTrue(  StringUtils::StripUrlQueryAndFragment( "https://example.com/path" ) == "https://example.com/path" );
        Assert::IsTrue(  StringUtils::StripUrlQueryAndFragment( "" )                         == "" );
    }
    
    };
}
