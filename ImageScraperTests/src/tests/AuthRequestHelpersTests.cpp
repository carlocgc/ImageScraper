#include "CppUnitTest.h"
#include "requests/AuthRequestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    static RequestOptions MakeOptions( )
    {
        RequestOptions opts{ };
        opts.m_UserAgent = "TestAgent/1.0";
        opts.m_CaBundle  = "ca-bundle.crt";
        return opts;
    }

    static HttpResponse MakeSuccess( )
    {
        HttpResponse r{ };
        r.m_Success    = true;
        r.m_StatusCode = 200;
        r.m_Body       = "ok";
        return r;
    }

    static HttpResponse MakeFailure( int statusCode, const std::string& error = "http error" )
    {
        HttpResponse r{ };
        r.m_Success    = false;
        r.m_StatusCode = statusCode;
        r.m_Error      = error;
        return r;
    }

    // ---------------------------------------------------------------------------
    // ValidateOAuthCredentials
    // ---------------------------------------------------------------------------

    TEST_CLASS(AuthRequestHelpersTests)
    {
    public:
    TEST_METHOD(ValidateOAuthCredentials_Empty_ClientId_Returns_False)
    {
        auto opts = MakeOptions( );
        opts.m_ClientId     = "";
        opts.m_ClientSecret = "secret";
    
        RequestResult result{ };
        const bool valid = AuthRequestHelpers::ValidateOAuthCredentials( opts, result );
    
        Assert::IsFalse(  valid );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
        Assert::IsFalse(  result.m_Success );
    }
    
    TEST_METHOD(ValidateOAuthCredentials_Empty_ClientSecret_Returns_False)
    {
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "";
    
        RequestResult result{ };
        const bool valid = AuthRequestHelpers::ValidateOAuthCredentials( opts, result );
    
        Assert::IsFalse(  valid );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    }
    
    TEST_METHOD(ValidateOAuthCredentials_Both_Populated_Returns_True)
    {
        auto opts = MakeOptions( );
        opts.m_ClientId     = "id";
        opts.m_ClientSecret = "secret";
    
        RequestResult result{ };
        const bool valid = AuthRequestHelpers::ValidateOAuthCredentials( opts, result );
    
        Assert::IsTrue(  valid );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::OK );
    }
    
    // ---------------------------------------------------------------------------
    // BuildQueryString
    // ---------------------------------------------------------------------------
    TEST_METHOD(BuildQueryString_No_Params_Returns_Base_Unchanged)
    {
        auto opts = MakeOptions( );
        const std::string result = AuthRequestHelpers::BuildQueryString( "grant_type=auth_code", opts );
        Assert::IsTrue(  result == "grant_type=auth_code" );
    }
    
    TEST_METHOD(BuildQueryString_Single_Param_Appended_Correctly)
    {
        auto opts = MakeOptions( );
        opts.m_QueryParams = { { "code", "abc123" } };
        const std::string result = AuthRequestHelpers::BuildQueryString( "grant_type=auth_code", opts );
        Assert::IsTrue(  result == "grant_type=auth_code&code=abc123" );
    }
    
    TEST_METHOD(BuildQueryString_Multiple_Params_All_Appended_In_Order)
    {
        auto opts = MakeOptions( );
        opts.m_QueryParams = {
            { "code",         "abc123"           },
            { "redirect_uri", "http://localhost" }
        };
        const std::string result = AuthRequestHelpers::BuildQueryString( "grant_type=auth_code", opts );
        Assert::IsTrue(  result == "grant_type=auth_code&code=abc123&redirect_uri=http://localhost" );
    }
    
    TEST_METHOD(BuildQueryString_Empty_Base_With_Params_Produces_Leading)
    {
        auto opts = MakeOptions( );
        opts.m_QueryParams = { { "key", "val" } };
        const std::string result = AuthRequestHelpers::BuildQueryString( "", opts );
        Assert::IsTrue(  result == "&key=val" );
    }
    
    // ---------------------------------------------------------------------------
    // HandleHttpError
    // ---------------------------------------------------------------------------
    TEST_METHOD(HandleHttpError_Success_Response_Returns_False_And_Leaves_Result_Clean)
    {
        RequestResult result{ };
        const bool hadError = AuthRequestHelpers::HandleHttpError( MakeSuccess( ), result );
    
        Assert::IsFalse(  hadError );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::OK );
    }
    
    TEST_METHOD(HandleHttpError_401_Response_Returns_True_And_Maps_Unauthorized)
    {
        RequestResult result{ };
        const bool hadError = AuthRequestHelpers::HandleHttpError( MakeFailure( 401, "Unauthorized" ), result );
    
        Assert::IsTrue(  hadError );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::Unauthorized );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "Unauthorized" );
    }
    
    TEST_METHOD(HandleHttpError_404_Response_Returns_True_And_Maps_NotFound)
    {
        RequestResult result{ };
        const bool hadError = AuthRequestHelpers::HandleHttpError( MakeFailure( 404, "not found" ), result );
    
        Assert::IsTrue(  hadError );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::NotFound );
        Assert::IsTrue(  result.m_Error.m_ErrorString == "not found" );
    }
    
    TEST_METHOD(HandleHttpError_500_Response_Returns_True_And_Maps_InternalServerError)
    {
        RequestResult result{ };
        const bool hadError = AuthRequestHelpers::HandleHttpError( MakeFailure( 500, "server error" ), result );
    
        Assert::IsTrue(  hadError );
        Assert::IsTrue(  result.m_Error.m_ErrorCode == ResponseErrorCode::InternalServerError );
    }
    
    };
}
