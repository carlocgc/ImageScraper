#include "CppUnitTest.h"
#include "ui/CredentialsPanel.h"
#include "ui/DownloadDeleteController.h"
#include "ui/DownloadHistoryPanel.h"
#include "ui/MediaPreviewPanel.h"
#include "ui/SettingsPanel.h"

#include <cmath>
#include <chrono>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    static bool ApproximatelyEqual( double actual, double expected, double epsilon = 0.0001 )
    {
        return std::abs( actual - expected ) < epsilon;
    }


    TEST_CLASS(UiStateTests)
    {
    public:
    TEST_METHOD(SettingsPanel_Disables_Check_Now_While_Blocked_Or_Already_Checking)
    {
        Assert::IsTrue(  SettingsPanel::ShouldDisableCheckNowButton( false, false ) == false );
        Assert::IsTrue(  SettingsPanel::ShouldDisableCheckNowButton( true, false ) == true );
        Assert::IsTrue(  SettingsPanel::ShouldDisableCheckNowButton( false, true ) == true );
    }
    
    TEST_METHOD(CredentialsPanel_Disables_Editing_While_Blocked)
    {
        Assert::IsTrue(  CredentialsPanel::ShouldDisableEditing( false ) == false );
        Assert::IsTrue(  CredentialsPanel::ShouldDisableEditing( true ) == true );
    }
    
    TEST_METHOD(MediaPreviewPanel_Stops_Playback_Only_When_A_New_Download_Run_Starts)
    {
        Assert::IsTrue(  MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( false, false ) == false );
        Assert::IsTrue(  MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( true, true ) == false );
        Assert::IsTrue(  MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( true, false ) == false );
        Assert::IsTrue(  MediaPreviewPanel::ShouldStopPlaybackForDownloadTransition( false, true ) == true );
    }
    
    TEST_METHOD(DownloadDeleteController_Computes_Deletion_Progress_From_Bytes_When_Available)
    {
        Assert::IsTrue(  ApproximatelyEqual( DownloadDeleteController::CalculateWorkProgressFraction( 100, 25, 0, 0 ), 0.25 ) );
        Assert::IsTrue(  ApproximatelyEqual( DownloadDeleteController::CalculateWorkProgressFraction( 100, 250, 0, 0 ), 1.0 ) );
    }
    
    TEST_METHOD(DownloadDeleteController_Falls_Back_To_Entry_Counts_When_Total_Bytes_Are_Unknown)
    {
        Assert::IsTrue(  ApproximatelyEqual( DownloadDeleteController::CalculateWorkProgressFraction( 0, 0, 4, 1 ), 0.25 ) );
        Assert::IsTrue(  ApproximatelyEqual( DownloadDeleteController::CalculateWorkProgressFraction( 0, 0, 0, 0 ), 0.0 ) );
    }
    
    TEST_METHOD(DownloadDeleteController_Keeps_Delete_Progress_Visible_Until_The_Minimum_Duration_Elapses)
    {
        const auto startedAt = std::chrono::steady_clock::time_point{ std::chrono::milliseconds{ 1000 } };
        const auto visibleUntil = startedAt + std::chrono::milliseconds{ 1000 };
    
        Assert::IsTrue(  DownloadDeleteController::ShouldKeepProgressVisible( visibleUntil, startedAt + std::chrono::milliseconds{ 500 }, true ) == true );
        Assert::IsTrue(  DownloadDeleteController::ShouldKeepProgressVisible( visibleUntil, startedAt + std::chrono::milliseconds{ 999 }, true ) == true );
        Assert::IsTrue(  DownloadDeleteController::ShouldKeepProgressVisible( visibleUntil, startedAt + std::chrono::milliseconds{ 1000 }, true ) == false );
        Assert::IsTrue(  DownloadDeleteController::ShouldKeepProgressVisible( visibleUntil, startedAt + std::chrono::milliseconds{ 500 }, false ) == false );
    }
    
    TEST_METHOD(DownloadDeleteController_Combines_Delete_Work_And_Minimum_Visible_Time_Into_Modal_Progress)
    {
        const auto startedAt = std::chrono::steady_clock::time_point{ std::chrono::milliseconds{ 1000 } };
        const auto visibleUntil = startedAt + std::chrono::milliseconds{ 1000 };
    
        Assert::IsTrue(
            ApproximatelyEqual(
                DownloadDeleteController::CalculateModalProgressFraction(
                    100,
                    25,
                    0,
                    0,
                    startedAt,
                    visibleUntil,
                    startedAt + std::chrono::milliseconds{ 250 },
                    true,
                    false ),
                0.25 ) );

        Assert::IsTrue(
            ApproximatelyEqual(
                DownloadDeleteController::CalculateModalProgressFraction(
                    100,
                    60,
                    0,
                    0,
                    startedAt,
                    visibleUntil,
                    startedAt + std::chrono::milliseconds{ 250 },
                    true,
                    false ),
                0.25 ) );

        Assert::IsTrue(
            ApproximatelyEqual(
                DownloadDeleteController::CalculateModalProgressFraction(
                    100,
                    60,
                    0,
                    0,
                    startedAt,
                    visibleUntil,
                    startedAt + std::chrono::milliseconds{ 1500 },
                    true,
                    false ),
                0.6 ) );

        Assert::IsTrue(
            ApproximatelyEqual(
                DownloadDeleteController::CalculateModalProgressFraction(
                    100,
                    100,
                    0,
                    0,
                    startedAt,
                    visibleUntil,
                    startedAt + std::chrono::milliseconds{ 250 },
                    true,
                    true ),
                0.25 ) );

        Assert::IsTrue(
            ApproximatelyEqual(
                DownloadDeleteController::CalculateModalProgressFraction(
                    100,
                    100,
                    0,
                    0,
                    startedAt,
                    visibleUntil,
                    startedAt + std::chrono::milliseconds{ 750 },
                    true,
                    true ),
                0.75 ) );

        Assert::IsTrue(
            ApproximatelyEqual(
                DownloadDeleteController::CalculateModalProgressFraction(
                    100,
                    100,
                    0,
                    0,
                    startedAt,
                    visibleUntil,
                    startedAt + std::chrono::milliseconds{ 1000 },
                    true,
                    true ),
                1.0 ) );

        Assert::IsTrue(
            ApproximatelyEqual(
                DownloadDeleteController::CalculateModalProgressFraction(
                    100,
                    100,
                    10,
                    10,
                    startedAt,
                    visibleUntil,
                    startedAt + std::chrono::milliseconds{ 250 },
                    false,
                    false ),
                0.0 ) );
    }
    
    };
}
