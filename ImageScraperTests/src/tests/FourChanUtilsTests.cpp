#include "CppUnitTest.h"
#include "utils/FourChanUtils.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper::FourChanUtils;

    // ----------------------------------------------------------------------------
    // GetPageCountForBoard
    // ----------------------------------------------------------------------------


    TEST_CLASS(FourChanUtilsTests)
    {
    public:
    TEST_METHOD(GetPageCountForBoard_Returns_0_When_Boards_Field_Missing)
    {
        Json response = { { "other", "value" } };
        Assert::IsTrue(  GetPageCountForBoard( "g", response ) == 0 );
    }
    
    TEST_METHOD(GetPageCountForBoard_Returns_0_When_Board_Not_Found)
    {
        Json response = {
            { "boards", {
                { { "board", "a" }, { "pages", 10 } }
            } }
        };
        Assert::IsTrue(  GetPageCountForBoard( "g", response ) == 0 );
    }
    
    TEST_METHOD(GetPageCountForBoard_Returns_Page_Count_For_Matching_Board)
    {
        Json response = {
            { "boards", {
                { { "board", "a" }, { "pages", 10 } },
                { { "board", "g" }, { "pages", 15 } },
                { { "board", "v" }, { "pages", 12 } }
            } }
        };
        Assert::IsTrue(  GetPageCountForBoard( "g", response ) == 15 );
    }
    
    TEST_METHOD(GetPageCountForBoard_Returns_0_For_Empty_Boards_Array)
    {
        Json response = { { "boards", Json::array( ) } };
        Assert::IsTrue(  GetPageCountForBoard( "g", response ) == 0 );
    }
    
    // ----------------------------------------------------------------------------
    // GetFileNamesFromResponse
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(GetFileNamesFromResponse_Returns_Empty_When_Threads_Field_Missing)
    {
        Json response = { { "other", "value" } };
        Assert::IsTrue(  GetFileNamesFromResponse( response ).empty( ) );
    }
    
    TEST_METHOD(GetFileNamesFromResponse_Returns_Empty_For_Empty_Threads)
    {
        Json response = { { "threads", Json::array( ) } };
        Assert::IsTrue(  GetFileNamesFromResponse( response ).empty( ) );
    }
    
    TEST_METHOD(GetFileNamesFromResponse_Builds_Filename_From_Tim_And_Ext)
    {
        Json response = {
            { "threads", {
                { { "posts", {
                    { { "tim", 1234567890123ull }, { "ext", ".jpg" } }
                } } }
            } }
        };
    
        auto result = GetFileNamesFromResponse( response );
        Assert::IsTrue(  result.size( ) == 1 );
        Assert::IsTrue(  result[ 0 ] == "1234567890123.jpg" );
    }
    
    TEST_METHOD(GetFileNamesFromResponse_Skips_Posts_Missing_Tim_Or_Ext)
    {
        Json response = {
            { "threads", {
                { { "posts", {
                    { { "tim", 111ull } },                          // missing ext
                    { { "ext", ".png" } },                          // missing tim
                    { { "tim", 222ull }, { "ext", ".gif" } }        // valid
                } } }
            } }
        };
    
        auto result = GetFileNamesFromResponse( response );
        Assert::IsTrue(  result.size( ) == 1 );
        Assert::IsTrue(  result[ 0 ] == "222.gif" );
    }
    
    TEST_METHOD(GetFileNamesFromResponse_Collects_Files_Across_Multiple_Threads)
    {
        Json response = {
            { "threads", {
                { { "posts", {
                    { { "tim", 111ull }, { "ext", ".jpg" } }
                } } },
                { { "posts", {
                    { { "tim", 222ull }, { "ext", ".png" } }
                } } }
            } }
        };
    
        auto result = GetFileNamesFromResponse( response );
        Assert::IsTrue(  result.size( ) == 2 );
    }
    
    TEST_METHOD(GetFileNamesFromResponse_Skips_Threads_With_No_Posts_Field)
    {
        Json response = {
            { "threads", {
                { { "no_posts", "here" } }
            } }
        };
    
        Assert::IsTrue(  GetFileNamesFromResponse( response ).empty( ) );
    }
    
    };
}
