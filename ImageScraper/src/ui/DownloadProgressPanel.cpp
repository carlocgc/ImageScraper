#include "ui/DownloadProgressPanel.h"
#include <cstdio>

void ImageScraper::DownloadProgressPanel::Update( )
{
    ImGui::SetNextWindowSize( ImVec2( 640, 80 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Download Progress", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    const float progressLabelWidth = 150.f;

    ImGui::ProgressBar( m_CurrentDownloadProgress, ImVec2( ImGui::GetStyle( ).ItemInnerSpacing.x - progressLabelWidth, 0.0f ) );
    ImGui::SameLine( 0.0f, ImGui::GetStyle( ).ItemInnerSpacing.x );
    ImGui::Text( "Current Download" );

    char buf[ 32 ] = "";
    if( m_Running )
    {
        sprintf_s( buf, 32, "%d/%d", m_CurrentDownloadNum.load( ), m_TotalDownloadsCount.load( ) );
    }
    ImGui::ProgressBar( m_TotalProgress, ImVec2( ImGui::GetStyle( ).ItemInnerSpacing.x - progressLabelWidth, 0.f ), buf );
    ImGui::SameLine( 0.0f, ImGui::GetStyle( ).ItemInnerSpacing.x );
    ImGui::Text( "Total Downloads" );

    ImGui::End( );
}

void ImageScraper::DownloadProgressPanel::OnCurrentDownloadProgress( float progress )
{
    m_CurrentDownloadProgress.store( progress, std::memory_order_relaxed );
}

void ImageScraper::DownloadProgressPanel::OnTotalDownloadProgress( int current, int total )
{
    m_CurrentDownloadNum.store( current );
    m_TotalDownloadsCount.store( total );
    const float progress = static_cast<float>( current ) / static_cast<float>( total );
    m_TotalProgress.store( progress );
}

void ImageScraper::DownloadProgressPanel::SetRunning( bool running )
{
    m_Running = running;
    if( !running )
    {
        m_CurrentDownloadProgress.store( 0.f, std::memory_order_relaxed );
        m_TotalProgress.store( 0.f, std::memory_order_relaxed );
        m_CurrentDownloadNum.store( 0 );
        m_TotalDownloadsCount.store( 0 );
    }
}
