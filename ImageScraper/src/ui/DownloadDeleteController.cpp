#include "ui/DownloadDeleteController.h"

#include <algorithm>

namespace
{
    constexpr auto k_MinDirectoryDeleteProgressVisible = std::chrono::seconds{ 1 };
    constexpr auto k_MinFileDeleteProgressVisible      = std::chrono::milliseconds{ 300 };

    std::string MakeFastPreferredPathString( const std::filesystem::path& path )
    {
        if( path.empty( ) )
        {
            return { };
        }

        std::filesystem::path result = path.lexically_normal( );
        result.make_preferred( );
        return result.string( );
    }
}

ImageScraper::DownloadDeleteController::~DownloadDeleteController( )
{
    if( m_DeleteFuture.valid( ) )
    {
        m_DeleteFuture.wait( );
    }
}

bool ImageScraper::DownloadDeleteController::IsActive( ) const
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    return m_Stage != Stage::Idle;
}

bool ImageScraper::DownloadDeleteController::HasCompletedWork( ) const
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    return m_DeleteCompletedResult.has_value( );
}

void ImageScraper::DownloadDeleteController::Start( const Request& request )
{
    if( IsActive( ) )
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock( m_StateMutex );
        m_DeleteCompletedResult.reset( );
        m_StartedAt = std::chrono::steady_clock::now( );
        const auto minVisibleDuration = request.m_IsDirectory ? k_MinDirectoryDeleteProgressVisible : k_MinFileDeleteProgressVisible;
        m_VisibleUntil = m_StartedAt + minVisibleDuration;
        m_Progress = Progress{ };
        m_Progress.m_Message = request.m_IsDirectory ? "Scanning folder for deletion..." : "Preparing deletion...";
        m_Stage = Stage::Scanning;
    }

    m_DeleteFuture = std::async( std::launch::async, [ this, request ]( ) -> std::pair<bool, std::string>
    {
        Progress progress{ };
        progress.m_Message = request.m_IsDirectory ? "Scanning folder for deletion..." : "Preparing deletion...";
        UpdateProgress( progress );

        std::vector<DeleteOperationEntry> deletePlan{ };
        BuildDeletePlan( request.m_TargetPath, deletePlan, progress );
        if( !progress.m_Error.empty( ) )
        {
            return { false, progress.m_Error };
        }

        SetStage( Stage::Deleting, "Deleting from disk..." );
        progress.m_Message = "Deleting from disk...";
        UpdateProgress( progress );

        for( const DeleteOperationEntry& entry : deletePlan )
        {
            if( !ExecuteDeletePlanEntry( entry, progress ) )
            {
                return { false, progress.m_Error };
            }
        }

        return { true, std::string{ } };
    } );
}

void ImageScraper::DownloadDeleteController::Update( )
{
    {
        std::lock_guard<std::mutex> lock( m_StateMutex );
        if( m_DeleteCompletedResult.has_value( ) )
        {
            return;
        }
    }

    if( !m_DeleteFuture.valid( ) )
    {
        return;
    }

    if( m_DeleteFuture.wait_for( std::chrono::milliseconds{ 0 } ) != std::future_status::ready )
    {
        return;
    }

    std::lock_guard<std::mutex> lock( m_StateMutex );
    if( !m_DeleteCompletedResult.has_value( ) )
    {
        m_DeleteCompletedResult = m_DeleteFuture.get( );
    }
}

std::optional<ImageScraper::DownloadDeleteController::Result>
ImageScraper::DownloadDeleteController::TryConsumeCompletedResult( std::chrono::steady_clock::time_point now )
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    if( !m_DeleteCompletedResult.has_value( ) )
    {
        return std::nullopt;
    }

    if( ShouldKeepProgressVisible( m_VisibleUntil, now, true ) )
    {
        return std::nullopt;
    }

    const std::pair<bool, std::string> rawResult = *m_DeleteCompletedResult;
    m_DeleteCompletedResult.reset( );
    m_Stage        = Stage::Idle;
    m_Progress     = Progress{ };
    m_StartedAt    = std::chrono::steady_clock::time_point{ };
    m_VisibleUntil = std::chrono::steady_clock::time_point{ };
    return Result{ rawResult.first, rawResult.second };
}

ImageScraper::DownloadDeleteController::Stage ImageScraper::DownloadDeleteController::GetStage( ) const
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    return m_Stage;
}

ImageScraper::DownloadDeleteController::Progress ImageScraper::DownloadDeleteController::GetProgressSnapshot( ) const
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    return m_Progress;
}

float ImageScraper::DownloadDeleteController::GetModalProgressFraction( const std::chrono::steady_clock::time_point now ) const
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    const bool deleteReady = m_DeleteCompletedResult.has_value( );
    const bool deleteWorkStarted = m_Stage == Stage::Deleting || deleteReady;
    return CalculateModalProgressFraction(
        m_Progress.m_TotalBytes,
        m_Progress.m_ProcessedBytes,
        m_Progress.m_TotalEntries,
        m_Progress.m_ProcessedEntries,
        m_StartedAt,
        m_VisibleUntil,
        now,
        deleteWorkStarted,
        deleteReady );
}

float ImageScraper::DownloadDeleteController::CalculateWorkProgressFraction(
    const uintmax_t totalBytes,
    const uintmax_t processedBytes,
    const int totalEntries,
    const int processedEntries )
{
    if( totalBytes > 0 )
    {
        return (std::min)( 1.0f, static_cast<float>( static_cast<double>( processedBytes ) / static_cast<double>( totalBytes ) ) );
    }

    if( totalEntries <= 0 )
    {
        return 0.0f;
    }

    return (std::min)( 1.0f, static_cast<float>( processedEntries ) / static_cast<float>( totalEntries ) );
}

float ImageScraper::DownloadDeleteController::CalculateModalProgressFraction(
    const uintmax_t totalBytes,
    const uintmax_t processedBytes,
    const int totalEntries,
    const int processedEntries,
    const std::chrono::steady_clock::time_point startedAt,
    const std::chrono::steady_clock::time_point visibleUntil,
    const std::chrono::steady_clock::time_point now,
    const bool deleteWorkStarted,
    const bool deleteReady )
{
    if( !deleteWorkStarted )
    {
        return 0.0f;
    }

    const float workFraction = CalculateWorkProgressFraction( totalBytes, processedBytes, totalEntries, processedEntries );
    const auto visibleDuration = visibleUntil - startedAt;
    if( visibleDuration <= std::chrono::steady_clock::duration::zero( ) )
    {
        return deleteReady ? 1.0f : workFraction;
    }

    const auto elapsed = (std::max)( std::chrono::steady_clock::duration::zero( ), now - startedAt );
    const double timeFraction = static_cast<double>( elapsed.count( ) ) / static_cast<double>( visibleDuration.count( ) );
    const float clampedTimeFraction = (std::min)( 1.0f, static_cast<float>( timeFraction ) );
    if( deleteReady )
    {
        return clampedTimeFraction;
    }

    return (std::min)( workFraction, clampedTimeFraction );
}

bool ImageScraper::DownloadDeleteController::ShouldKeepProgressVisible(
    const std::chrono::steady_clock::time_point visibleUntil,
    const std::chrono::steady_clock::time_point now,
    const bool deleteReady )
{
    return deleteReady && now < visibleUntil;
}

void ImageScraper::DownloadDeleteController::BuildDeletePlan(
    const std::filesystem::path& path,
    std::vector<DeleteOperationEntry>& deletePlan,
    Progress& progress )
{
    std::error_code ec;
    const bool isDirectory = std::filesystem::is_directory( path, ec );
    if( ec )
    {
        progress.m_Error = ec.message( );
        UpdateProgress( progress );
        return;
    }

    if( !isDirectory )
    {
        uintmax_t sizeBytes = 0;
        if( std::filesystem::is_regular_file( path, ec ) && !ec )
        {
            sizeBytes = std::filesystem::file_size( path, ec );
            if( ec )
            {
                sizeBytes = 0;
            }
        }

        deletePlan.push_back( { path, sizeBytes, false } );
        progress.m_TotalBytes = sizeBytes;
        progress.m_TotalEntries = 1;
        progress.m_CurrentPath = MakeFastPreferredPathString( path );
        UpdateProgress( progress );
        return;
    }

    std::vector<DeleteOperationEntry> stagedEntries{ };
    stagedEntries.push_back( { path, 0, true } );
    for( std::filesystem::recursive_directory_iterator it{ path, std::filesystem::directory_options::skip_permission_denied, ec }, end;
         !ec && it != end;
         it.increment( ec ) )
    {
        const std::filesystem::path entryPath = it->path( );
        const bool entryIsDirectory = it->is_directory( ec ) && !ec;
        uintmax_t sizeBytes = 0;
        if( !entryIsDirectory && it->is_regular_file( ec ) && !ec )
        {
            sizeBytes = it->file_size( ec );
            if( ec )
            {
                sizeBytes = 0;
            }
            progress.m_TotalBytes += sizeBytes;
        }

        stagedEntries.push_back( { entryPath, sizeBytes, entryIsDirectory } );
        progress.m_TotalEntries = static_cast<int>( stagedEntries.size( ) );
        progress.m_CurrentPath = MakeFastPreferredPathString( entryPath );
        UpdateProgress( progress );
    }

    if( ec )
    {
        progress.m_Error = ec.message( );
        UpdateProgress( progress );
        return;
    }

    std::reverse( stagedEntries.begin( ), stagedEntries.end( ) );
    deletePlan = std::move( stagedEntries );
}

bool ImageScraper::DownloadDeleteController::ExecuteDeletePlanEntry(
    const DeleteOperationEntry& entry,
    Progress& progress )
{
    progress.m_CurrentPath = MakeFastPreferredPathString( entry.m_Path );
    UpdateProgress( progress );

    std::error_code ec;
    std::filesystem::remove( entry.m_Path, ec );
    if( ec )
    {
        progress.m_Error = ec.message( );
        UpdateProgress( progress );
        return false;
    }

    ++progress.m_ProcessedEntries;
    if( !entry.m_IsDirectory )
    {
        progress.m_ProcessedBytes += entry.m_SizeBytes;
    }
    UpdateProgress( progress );
    return true;
}

void ImageScraper::DownloadDeleteController::SetStage( const Stage stage, const std::string& message )
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    m_Stage = stage;
    if( !message.empty( ) )
    {
        m_Progress.m_Message = message;
    }
}

void ImageScraper::DownloadDeleteController::UpdateProgress( const Progress& progress )
{
    std::lock_guard<std::mutex> lock( m_StateMutex );
    m_Progress = progress;
}
