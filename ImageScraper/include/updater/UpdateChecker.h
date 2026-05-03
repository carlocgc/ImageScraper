#pragma once

#include "network\HttpClientTypes.h"

#include <string>
#include <string_view>

namespace ImageScraper
{
    struct SemanticVersion
    {
        int m_Major{ 0 };
        int m_Minor{ 0 };
        int m_Patch{ 0 };
    };

    struct GitHubReleaseInfo
    {
        std::string m_TagName{ };
        std::string m_HtmlUrl{ };
        SemanticVersion m_Version{ };
    };

    enum class UpdateCheckStatus
    {
        UpToDate,
        UpdateAvailable,
        Skipped,
        Failed,
    };

    struct UpdateCheckRequest
    {
        std::string m_CurrentVersion{ };
        std::string m_CaBundlePath{ };
        std::string m_UserAgent{ };
        int m_TimeoutSeconds{ 10 };
        bool m_AllowDevelopmentVersion{ false };
    };

    struct UpdateCheckResult
    {
        UpdateCheckStatus m_Status{ UpdateCheckStatus::Failed };
        GitHubReleaseInfo m_LatestRelease{ };
        std::string m_Error{ };
    };

    class IHttpClient;

    bool TryParseSemanticVersion( std::string_view text, SemanticVersion& outVersion );
    int CompareVersions( const SemanticVersion& lhs, const SemanticVersion& rhs );
    std::string VersionToString( const SemanticVersion& version );
    bool TryParseGitHubLatestRelease( const std::string& body, GitHubReleaseInfo& outRelease, std::string& outError );
    UpdateCheckResult CheckForUpdates( IHttpClient& httpClient, const UpdateCheckRequest& request );
}
