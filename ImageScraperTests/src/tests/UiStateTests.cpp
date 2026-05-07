#include "catch2/catch_amalgamated.hpp"
#include "ui/CredentialsPanel.h"
#include "ui/DownloadHistoryPanel.h"
#include "ui/MediaPreviewPanel.h"
#include "ui/SettingsPanel.h"

#include <chrono>

using namespace ImageScraper;

TEST_CASE( "SettingsPanel disables Check now while blocked or already checking", "[UiState]" )
{
    REQUIRE( SettingsPanel::ShouldDisableCheckNowButton( false, false ) == false );
    REQUIRE( SettingsPanel::ShouldDisableCheckNowButton( true, false ) == true );
    REQUIRE( SettingsPanel::ShouldDisableCheckNowButton( false, true ) == true );
}

TEST_CASE( "CredentialsPanel disables editing while blocked", "[UiState]" )
{
    REQUIRE( CredentialsPanel::ShouldDisableEditing( false ) == false );
    REQUIRE( CredentialsPanel::ShouldDisableEditing( true ) == true );
}

TEST_CASE( "MediaPreviewPanel stops playback only when a new download run starts", "[UiState]" )
{
    REQUIRE( MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( false, false ) == false );
    REQUIRE( MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( true, true ) == false );
    REQUIRE( MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( true, false ) == false );
    REQUIRE( MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( false, true ) == true );
}

TEST_CASE( "DownloadHistoryPanel computes deletion progress from bytes when available", "[UiState]" )
{
    REQUIRE( DownloadHistoryPanel::CalculateDeleteWorkProgressFraction( 100, 25, 0, 0 ) == Approx( 0.25f ) );
    REQUIRE( DownloadHistoryPanel::CalculateDeleteWorkProgressFraction( 100, 250, 0, 0 ) == Approx( 1.0f ) );
}

TEST_CASE( "DownloadHistoryPanel falls back to entry counts when total bytes are unknown", "[UiState]" )
{
    REQUIRE( DownloadHistoryPanel::CalculateDeleteWorkProgressFraction( 0, 0, 4, 1 ) == Approx( 0.25f ) );
    REQUIRE( DownloadHistoryPanel::CalculateDeleteWorkProgressFraction( 0, 0, 0, 0 ) == Approx( 0.0f ) );
}

TEST_CASE( "DownloadHistoryPanel keeps delete progress visible until the minimum duration elapses", "[UiState]" )
{
    const auto startedAt = std::chrono::steady_clock::time_point{ std::chrono::milliseconds{ 1000 } };
    const auto visibleUntil = startedAt + std::chrono::milliseconds{ 1000 };

    REQUIRE( DownloadHistoryPanel::ShouldKeepDeleteProgressVisible( startedAt, visibleUntil, startedAt + std::chrono::milliseconds{ 500 }, true ) == true );
    REQUIRE( DownloadHistoryPanel::ShouldKeepDeleteProgressVisible( startedAt, visibleUntil, startedAt + std::chrono::milliseconds{ 999 }, true ) == true );
    REQUIRE( DownloadHistoryPanel::ShouldKeepDeleteProgressVisible( startedAt, visibleUntil, startedAt + std::chrono::milliseconds{ 1000 }, true ) == false );
    REQUIRE( DownloadHistoryPanel::ShouldKeepDeleteProgressVisible( startedAt, visibleUntil, startedAt + std::chrono::milliseconds{ 500 }, false ) == false );
}

TEST_CASE( "DownloadHistoryPanel combines delete work and minimum visible time into modal progress", "[UiState]" )
{
    const auto startedAt = std::chrono::steady_clock::time_point{ std::chrono::milliseconds{ 1000 } };
    const auto visibleUntil = startedAt + std::chrono::milliseconds{ 1000 };

    REQUIRE(
        DownloadHistoryPanel::CalculateDeleteModalProgressFraction(
            100,
            25,
            0,
            0,
            startedAt,
            visibleUntil,
            startedAt + std::chrono::milliseconds{ 250 },
            true,
            false ) == Approx( 0.25f ) );

    REQUIRE(
        DownloadHistoryPanel::CalculateDeleteModalProgressFraction(
            100,
            60,
            0,
            0,
            startedAt,
            visibleUntil,
            startedAt + std::chrono::milliseconds{ 250 },
            true,
            false ) == Approx( 0.25f ) );

    REQUIRE(
        DownloadHistoryPanel::CalculateDeleteModalProgressFraction(
            100,
            60,
            0,
            0,
            startedAt,
            visibleUntil,
            startedAt + std::chrono::milliseconds{ 1500 },
            true,
            false ) == Approx( 0.6f ) );

    REQUIRE(
        DownloadHistoryPanel::CalculateDeleteModalProgressFraction(
            100,
            100,
            0,
            0,
            startedAt,
            visibleUntil,
            startedAt + std::chrono::milliseconds{ 250 },
            true,
            true ) == Approx( 0.25f ) );

    REQUIRE(
        DownloadHistoryPanel::CalculateDeleteModalProgressFraction(
            100,
            100,
            0,
            0,
            startedAt,
            visibleUntil,
            startedAt + std::chrono::milliseconds{ 750 },
            true,
            true ) == Approx( 0.75f ) );

    REQUIRE(
        DownloadHistoryPanel::CalculateDeleteModalProgressFraction(
            100,
            100,
            0,
            0,
            startedAt,
            visibleUntil,
            startedAt + std::chrono::milliseconds{ 1000 },
            true,
            true ) == Approx( 1.0f ) );

    REQUIRE(
        DownloadHistoryPanel::CalculateDeleteModalProgressFraction(
            100,
            100,
            10,
            10,
            startedAt,
            visibleUntil,
            startedAt + std::chrono::milliseconds{ 250 },
            false,
            false ) == Approx( 0.0f ) );
}
