#include "catch2/catch_amalgamated.hpp"
#include "mocks\MockHttpClient.h"
#include "updater\UpdateChecker.h"

using namespace ImageScraper;

TEST_CASE( "TryParseSemanticVersion accepts strict numeric release tags", "[UpdateChecker]" )
{
    SemanticVersion version{ };

    REQUIRE( TryParseSemanticVersion( "v1.2.3", version ) );
    REQUIRE( version.m_Major == 1 );
    REQUIRE( version.m_Minor == 2 );
    REQUIRE( version.m_Patch == 3 );

    REQUIRE( TryParseSemanticVersion( "1.2.3", version ) );
    REQUIRE( version.m_Major == 1 );
    REQUIRE( version.m_Minor == 2 );
    REQUIRE( version.m_Patch == 3 );
}

TEST_CASE( "TryParseSemanticVersion rejects non-strict release tags", "[UpdateChecker]" )
{
    SemanticVersion version{ };

    REQUIRE_FALSE( TryParseSemanticVersion( "v1.2", version ) );
    REQUIRE_FALSE( TryParseSemanticVersion( "v1.2.3-beta", version ) );
    REQUIRE_FALSE( TryParseSemanticVersion( "release-1.2.3", version ) );
    REQUIRE_FALSE( TryParseSemanticVersion( "v1.2.3.4", version ) );
}

TEST_CASE( "CompareVersions compares numeric components", "[UpdateChecker]" )
{
    REQUIRE( CompareVersions( { 1, 10, 0 }, { 1, 2, 9 } ) > 0 );
    REQUIRE( CompareVersions( { 1, 2, 0 }, { 1, 2, 1 } ) < 0 );
    REQUIRE( CompareVersions( { 2, 0, 0 }, { 2, 0, 0 } ) == 0 );
}

TEST_CASE( "TryParseGitHubLatestRelease reads tag and release URL", "[UpdateChecker]" )
{
    GitHubReleaseInfo release{ };
    std::string error{ };

    REQUIRE( TryParseGitHubLatestRelease(
        R"({ "tag_name": "v1.2.3", "html_url": "https://github.com/carlocgc/ImageScraper/releases/tag/v1.2.3" })",
        release,
        error ) );

    REQUIRE( release.m_TagName == "v1.2.3" );
    REQUIRE( release.m_HtmlUrl == "https://github.com/carlocgc/ImageScraper/releases/tag/v1.2.3" );
    REQUIRE( release.m_Version.m_Major == 1 );
    REQUIRE( release.m_Version.m_Minor == 2 );
    REQUIRE( release.m_Version.m_Patch == 3 );
}

TEST_CASE( "CheckForUpdates reports update when GitHub version is newer", "[UpdateChecker]" )
{
    MockHttpClient httpClient{ };
    httpClient.m_Response.m_Success = true;
    httpClient.m_Response.m_StatusCode = 200;
    httpClient.m_Response.m_Body = R"({ "tag_name": "v1.3.0", "html_url": "https://github.com/carlocgc/ImageScraper/releases/tag/v1.3.0" })";

    const UpdateCheckResult result = CheckForUpdates( httpClient, { "1.2.0", "ca.pem", "ImageScraper/1.2.0", 10 } );

    REQUIRE( result.m_Status == UpdateCheckStatus::UpdateAvailable );
    REQUIRE( result.m_LatestRelease.m_TagName == "v1.3.0" );
    REQUIRE( httpClient.m_LastRequest.m_Url == "https://api.github.com/repos/carlocgc/ImageScraper/releases/latest" );
    REQUIRE( httpClient.m_LastRequest.m_CaBundle == "ca.pem" );
}

TEST_CASE( "CheckForUpdates reports up to date for equal GitHub version", "[UpdateChecker]" )
{
    MockHttpClient httpClient{ };
    httpClient.m_Response.m_Success = true;
    httpClient.m_Response.m_StatusCode = 200;
    httpClient.m_Response.m_Body = R"({ "tag_name": "v1.2.0" })";

    const UpdateCheckResult result = CheckForUpdates( httpClient, { "1.2.0", "", "ImageScraper/1.2.0", 10 } );

    REQUIRE( result.m_Status == UpdateCheckStatus::UpToDate );
}

TEST_CASE( "CheckForUpdates fails on malformed GitHub release metadata", "[UpdateChecker]" )
{
    MockHttpClient httpClient{ };
    httpClient.m_Response.m_Success = true;
    httpClient.m_Response.m_StatusCode = 200;
    httpClient.m_Response.m_Body = R"({ "tag_name": "v1.2.0-beta" })";

    const UpdateCheckResult result = CheckForUpdates( httpClient, { "1.2.0", "", "ImageScraper/1.2.0", 10 } );

    REQUIRE( result.m_Status == UpdateCheckStatus::Failed );
    REQUIRE_FALSE( result.m_Error.empty( ) );
}

TEST_CASE( "CheckForUpdates skips development builds before making a request", "[UpdateChecker]" )
{
    MockHttpClient httpClient{ };

    const UpdateCheckResult result = CheckForUpdates( httpClient, { "0.0.0-dev", "", "ImageScraper/0.0.0-dev", 10 } );

    REQUIRE( result.m_Status == UpdateCheckStatus::Skipped );
    REQUIRE( result.m_Error == "Update checks are disabled for development builds." );
    REQUIRE( httpClient.m_CallCount == 0 );
}

TEST_CASE( "CheckForUpdates can allow development builds for update UI debugging", "[UpdateChecker]" )
{
    MockHttpClient httpClient{ };
    httpClient.m_Response.m_Success = true;
    httpClient.m_Response.m_StatusCode = 200;
    httpClient.m_Response.m_Body = R"({ "tag_name": "v1.0.0", "html_url": "https://github.com/carlocgc/ImageScraper/releases/tag/v1.0.0" })";

    UpdateCheckRequest request{ "0.0.0-dev", "", "ImageScraper/0.0.0-dev", 10 };
    request.m_AllowDevelopmentVersion = true;

    const UpdateCheckResult result = CheckForUpdates( httpClient, request );

    REQUIRE( result.m_Status == UpdateCheckStatus::UpdateAvailable );
    REQUIRE( result.m_LatestRelease.m_TagName == "v1.0.0" );
    REQUIRE( httpClient.m_CallCount == 1 );
}
