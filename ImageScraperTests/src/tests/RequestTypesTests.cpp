#include "CppUnitTest.h"
#include "requests/RequestTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;


    TEST_CLASS(RequestTypesTests)
    {
    public:
    TEST_METHOD(ResponseErrorCodefromInt_Maps_Known_HTTP_Codes)
    {
        Assert::IsTrue(  ResponseErrorCodefromInt( 200 ) == ResponseErrorCode::OK );
        Assert::IsTrue(  ResponseErrorCodefromInt( 400 ) == ResponseErrorCode::BadRequest );
        Assert::IsTrue(  ResponseErrorCodefromInt( 401 ) == ResponseErrorCode::Unauthorized );
        Assert::IsTrue(  ResponseErrorCodefromInt( 403 ) == ResponseErrorCode::Forbidden );
        Assert::IsTrue(  ResponseErrorCodefromInt( 404 ) == ResponseErrorCode::NotFound );
        Assert::IsTrue(  ResponseErrorCodefromInt( 500 ) == ResponseErrorCode::InternalServerError );
    }
    
    TEST_METHOD(ResponseErrorCodefromInt_Returns_InternalServerError_For_Unknown_Codes)
    {
        Assert::IsTrue(  ResponseErrorCodefromInt( 0 ) == ResponseErrorCode::InternalServerError );
        Assert::IsTrue(  ResponseErrorCodefromInt( 429 ) == ResponseErrorCode::InternalServerError );
        Assert::IsTrue(  ResponseErrorCodefromInt( 503 ) == ResponseErrorCode::InternalServerError );
    }
    
    TEST_METHOD(ResponseErrorCodeToString_Returns_Correct_Strings)
    {
        Assert::IsTrue(  ResponseErrorCodeToString( ResponseErrorCode::OK ) == "OK" );
        Assert::IsTrue(  ResponseErrorCodeToString( ResponseErrorCode::BadRequest ) == "Bad Request" );
        Assert::IsTrue(  ResponseErrorCodeToString( ResponseErrorCode::Unauthorized ) == "Unauthorized" );
        Assert::IsTrue(  ResponseErrorCodeToString( ResponseErrorCode::Forbidden ) == "Forbidden" );
        Assert::IsTrue(  ResponseErrorCodeToString( ResponseErrorCode::NotFound ) == "Not Found" );
        Assert::IsTrue(  ResponseErrorCodeToString( ResponseErrorCode::InternalServerError ) == "Internal Server Error" );
    }
    
    TEST_METHOD(RequestResult_SetError_Sets_Code_And_String_Together)
    {
        RequestResult result;
        result.SetError( ResponseErrorCode::Unauthorized );
    
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "Unauthorized" );
    }
    
    TEST_METHOD(RequestResult_Defaults_To_Unsuccessful_With_OK_Error_Code)
    {
        RequestResult result;
    
        Assert::IsTrue(  result.m_Success == false );
        Assert::IsTrue(  result.m_Response.empty() );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::OK );
        Assert::IsTrue(  result.m_Error.m_ErrorString.empty() );
    }
    
    };
}
