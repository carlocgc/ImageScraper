#include "CppUnitTest.h"
#include "mocks\MockHttpClient.h"
#include "updater\UpdateChecker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;


    TEST_CLASS(UpdateCheckerTests)
    {
    public:
    TEST_METHOD(TryParseSemanticVersion_Accepts_Strict_Numeric_Release_Tags)
    {
        SemanticVersion version{ };
    
        Assert::IsTrue(  TryParseSemanticVersion( "v1.2.3", version ) );
        Assert::IsTrue(  version.m_Major == 1 );
        Assert::IsTrue(  version.m_Minor == 2 );
        Assert::IsTrue(  version.m_Patch == 3 );
    
        Assert::IsTrue(  TryParseSemanticVersion( "1.2.3", version ) );
        Assert::IsTrue(  version.m_Major == 1 );
        Assert::IsTrue(  version.m_Minor == 2 );
        Assert::IsTrue(  version.m_Patch == 3 );
    }
    
    TEST_METHOD(TryParseSemanticVersion_Rejects_Non_Strict_Release_Tags)
    {
        SemanticVersion version{ };
    
        Assert::IsFalse(  TryParseSemanticVersion( "v1.2", version ) );
        Assert::IsFalse(  TryParseSemanticVersion( "v1.2.3-beta", version ) );
        Assert::IsFalse(  TryParseSemanticVersion( "release-1.2.3", version ) );
        Assert::IsFalse(  TryParseSemanticVersion( "v1.2.3.4", version ) );
    }
    
    TEST_METHOD(CompareVersions_Compares_Numeric_Components)
    {
        Assert::IsTrue(  CompareVersions( { 1, 10, 0 }, { 1, 2, 9 } ) > 0 );
        Assert::IsTrue(  CompareVersions( { 1, 2, 0 }, { 1, 2, 1 } ) < 0 );
        Assert::IsTrue(  CompareVersions( { 2, 0, 0 }, { 2, 0, 0 } ) == 0 );
    }
    
    TEST_METHOD(TryParseGitHubLatestRelease_Reads_Tag_And_Release_URL)
    {
        GitHubReleaseInfo release{ };
        std::string error{ };
    
        Assert::IsTrue(  TryParseGitHubLatestRelease(
            R"({ "tag_name": "v1.2.3", "html_url": "https://github.com/carlocgc/ImageScraper/releases/tag/v1.2.3" })",
            release,
            error ) );
    
        Assert::IsTrue(  release.m_TagName == "v1.2.3" );
        Assert::IsTrue(  release.m_HtmlUrl == "https://github.com/carlocgc/ImageScraper/releases/tag/v1.2.3" );
        Assert::IsTrue(  release.m_Version.m_Major == 1 );
        Assert::IsTrue(  release.m_Version.m_Minor == 2 );
        Assert::IsTrue(  release.m_Version.m_Patch == 3 );
    }
    
    TEST_METHOD(CheckForUpdates_Reports_Update_When_GitHub_Version_Is_Newer)
    {
        MockHttpClient httpClient{ };
        httpClient.m_Response.m_Success = true;
        httpClient.m_Response.m_StatusCode = 200;
        httpClient.m_Response.m_Body = R"({ "tag_name": "v1.3.0", "html_url": "https://github.com/carlocgc/ImageScraper/releases/tag/v1.3.0" })";
    
        const UpdateCheckResult result = CheckForUpdates( httpClient, { "1.2.0", "ca.pem", "ImageScraper/1.2.0", 10 } );
    
        Assert::IsTrue(  result.m_Status == UpdateCheckStatus::UpdateAvailable );
        Assert::IsTrue(  result.m_LatestRelease.m_TagName == "v1.3.0" );
        Assert::IsTrue(  httpClient.m_LastRequest.m_Url == "https://api.github.com/repos/carlocgc/ImageScraper/releases/latest" );
        Assert::IsTrue(  httpClient.m_LastRequest.m_CaBundle == "ca.pem" );
    }
    
    TEST_METHOD(CheckForUpdates_Reports_Up_To_Date_For_Equal_GitHub_Version)
    {
        MockHttpClient httpClient{ };
        httpClient.m_Response.m_Success = true;
        httpClient.m_Response.m_StatusCode = 200;
        httpClient.m_Response.m_Body = R"({ "tag_name": "v1.2.0" })";
    
        const UpdateCheckResult result = CheckForUpdates( httpClient, { "1.2.0", "", "ImageScraper/1.2.0", 10 } );
    
        Assert::IsTrue(  result.m_Status == UpdateCheckStatus::UpToDate );
    }
    
    TEST_METHOD(CheckForUpdates_Fails_On_Malformed_GitHub_Release_Metadata)
    {
        MockHttpClient httpClient{ };
        httpClient.m_Response.m_Success = true;
        httpClient.m_Response.m_StatusCode = 200;
        httpClient.m_Response.m_Body = R"({ "tag_name": "v1.2.0-beta" })";
    
        const UpdateCheckResult result = CheckForUpdates( httpClient, { "1.2.0", "", "ImageScraper/1.2.0", 10 } );
    
        Assert::IsTrue(  result.m_Status == UpdateCheckStatus::Failed );
        Assert::IsFalse(  result.m_Error.empty( ) );
    }
    
    TEST_METHOD(CheckForUpdates_Skips_Development_Builds_Before_Making_A_Request)
    {
        MockHttpClient httpClient{ };
    
        const UpdateCheckResult result = CheckForUpdates( httpClient, { "0.0.0-dev", "", "ImageScraper/0.0.0-dev", 10 } );
    
        Assert::IsTrue(  result.m_Status == UpdateCheckStatus::Skipped );
        Assert::IsTrue(  result.m_Error == "Update checks are disabled for development builds." );
        Assert::IsTrue(  httpClient.m_CallCount == 0 );
    }
    
    TEST_METHOD(CheckForUpdates_Can_Allow_Development_Builds_For_Update_UI_Debugging)
    {
        MockHttpClient httpClient{ };
        httpClient.m_Response.m_Success = true;
        httpClient.m_Response.m_StatusCode = 200;
        httpClient.m_Response.m_Body = R"({ "tag_name": "v1.0.0", "html_url": "https://github.com/carlocgc/ImageScraper/releases/tag/v1.0.0" })";
    
        UpdateCheckRequest request{ "0.0.0-dev", "", "ImageScraper/0.0.0-dev", 10 };
        request.m_AllowDevelopmentVersion = true;
    
        const UpdateCheckResult result = CheckForUpdates( httpClient, request );
    
        Assert::IsTrue(  result.m_Status == UpdateCheckStatus::UpdateAvailable );
        Assert::IsTrue(  result.m_LatestRelease.m_TagName == "v1.0.0" );
        Assert::IsTrue(  httpClient.m_CallCount == 1 );
    }
    
    };
}
