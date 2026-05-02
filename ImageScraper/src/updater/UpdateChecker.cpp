#include "updater\UpdateChecker.h"

#include "network\IHttpClient.h"
#include "nlohmann\json.hpp"

#include <array>
#include <cctype>
#include <sstream>

namespace
{
    constexpr const char* k_LatestReleaseApiUrl = "https://api.github.com/repos/carlocgc/ImageScraper/releases/latest";

    bool TryParseNonNegativeInt( std::string_view text, int& outValue )
    {
        if( text.empty( ) )
        {
            return false;
        }

        int value = 0;
        for( const char ch : text )
        {
            if( !std::isdigit( static_cast<unsigned char>( ch ) ) )
            {
                return false;
            }

            value = ( value * 10 ) + ( ch - '0' );
        }

        outValue = value;
        return true;
    }

    std::string DefaultReleaseUrlForTag( const std::string& tagName )
    {
        return "https://github.com/carlocgc/ImageScraper/releases/tag/" + tagName;
    }
}

bool ImageScraper::TryParseSemanticVersion( std::string_view text, SemanticVersion& outVersion )
{
    if( !text.empty( ) && ( text.front( ) == 'v' || text.front( ) == 'V' ) )
    {
        text.remove_prefix( 1 );
    }

    std::array<std::string_view, 3> parts{ };
    std::size_t start = 0;
    for( std::size_t index = 0; index < parts.size( ); ++index )
    {
        const std::size_t dot = text.find( '.', start );
        if( index < parts.size( ) - 1 )
        {
            if( dot == std::string_view::npos )
            {
                return false;
            }
            parts[ index ] = text.substr( start, dot - start );
            start = dot + 1;
        }
        else
        {
            if( dot != std::string_view::npos )
            {
                return false;
            }
            parts[ index ] = text.substr( start );
        }
    }

    SemanticVersion parsed{ };
    if( !TryParseNonNegativeInt( parts[ 0 ], parsed.m_Major )
        || !TryParseNonNegativeInt( parts[ 1 ], parsed.m_Minor )
        || !TryParseNonNegativeInt( parts[ 2 ], parsed.m_Patch ) )
    {
        return false;
    }

    outVersion = parsed;
    return true;
}

int ImageScraper::CompareVersions( const SemanticVersion& lhs, const SemanticVersion& rhs )
{
    if( lhs.m_Major != rhs.m_Major )
    {
        return lhs.m_Major < rhs.m_Major ? -1 : 1;
    }
    if( lhs.m_Minor != rhs.m_Minor )
    {
        return lhs.m_Minor < rhs.m_Minor ? -1 : 1;
    }
    if( lhs.m_Patch != rhs.m_Patch )
    {
        return lhs.m_Patch < rhs.m_Patch ? -1 : 1;
    }
    return 0;
}

std::string ImageScraper::VersionToString( const SemanticVersion& version )
{
    std::ostringstream out;
    out << version.m_Major << '.' << version.m_Minor << '.' << version.m_Patch;
    return out.str( );
}

bool ImageScraper::TryParseGitHubLatestRelease( const std::string& body, GitHubReleaseInfo& outRelease, std::string& outError )
{
    try
    {
        const nlohmann::json release = nlohmann::json::parse( body );
        if( !release.contains( "tag_name" ) || !release[ "tag_name" ].is_string( ) )
        {
            outError = "GitHub latest release response did not include a string tag_name.";
            return false;
        }

        GitHubReleaseInfo parsed{ };
        parsed.m_TagName = release[ "tag_name" ].get<std::string>( );
        if( !TryParseSemanticVersion( parsed.m_TagName, parsed.m_Version ) )
        {
            outError = "GitHub latest release tag was not a strict MAJOR.MINOR.PATCH version.";
            return false;
        }

        if( release.contains( "html_url" ) && release[ "html_url" ].is_string( ) )
        {
            parsed.m_HtmlUrl = release[ "html_url" ].get<std::string>( );
        }
        else
        {
            parsed.m_HtmlUrl = DefaultReleaseUrlForTag( parsed.m_TagName );
        }

        outRelease = parsed;
        return true;
    }
    catch( const std::exception& error )
    {
        outError = error.what( );
        return false;
    }
}

ImageScraper::UpdateCheckResult ImageScraper::CheckForUpdates( IHttpClient& httpClient, const UpdateCheckRequest& request )
{
    SemanticVersion currentVersion{ };
    if( !TryParseSemanticVersion( request.m_CurrentVersion, currentVersion ) )
    {
        if( !request.m_AllowDevelopmentVersion )
        {
            return UpdateCheckResult{
                UpdateCheckStatus::Skipped,
                { },
                "Update checks are disabled for development builds.",
            };
        }

        currentVersion = { 0, 0, 0 };
    }

    HttpRequest httpRequest{ };
    httpRequest.m_Url = k_LatestReleaseApiUrl;
    httpRequest.m_CaBundle = request.m_CaBundlePath;
    httpRequest.m_UserAgent = request.m_UserAgent.empty( ) ? "ImageScraper" : request.m_UserAgent;
    httpRequest.m_TimeoutSeconds = request.m_TimeoutSeconds;
    httpRequest.m_Headers.push_back( "Accept: application/vnd.github+json" );

    const HttpResponse response = httpClient.Get( httpRequest );
    if( !response.m_Success )
    {
        std::ostringstream error;
        error << "GitHub latest release request failed";
        if( response.m_StatusCode > 0 )
        {
            error << " with HTTP " << response.m_StatusCode;
        }
        if( !response.m_Error.empty( ) )
        {
            error << ": " << response.m_Error;
        }

        return UpdateCheckResult{
            UpdateCheckStatus::Failed,
            { },
            error.str( ),
        };
    }

    GitHubReleaseInfo latestRelease{ };
    std::string parseError{ };
    if( !TryParseGitHubLatestRelease( response.m_Body, latestRelease, parseError ) )
    {
        return UpdateCheckResult{
            UpdateCheckStatus::Failed,
            { },
            parseError,
        };
    }

    return UpdateCheckResult{
        CompareVersions( currentVersion, latestRelease.m_Version ) < 0 ? UpdateCheckStatus::UpdateAvailable
                                                                       : UpdateCheckStatus::UpToDate,
        latestRelease,
        { },
    };
}
