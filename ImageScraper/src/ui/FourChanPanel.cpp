#include "ui/FourChanPanel.h"
#include "ui/DownloadOptionControls.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::FourChanPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "fourchan_board_history" );
    m_FourChanBoard = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = FOURCHAN_MEDIA_DEFAULT;
        if( m_AppConfig->GetValue<int>( "fourchan_max_downloads", saved ) )
        {
            m_FourChanMaxMediaItems = std::clamp( saved, FOURCHAN_MEDIA_MIN, FOURCHAN_MEDIA_MAX );
        }
    }
}

void ImageScraper::FourChanPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_FourChanBoard );
}

void ImageScraper::FourChanPanel::Update( )
{
    DownloadOptionControls::DrawSearchInput(
        {
            "FourChanBoard",
            "##fourchan_board",
            "##fourchan_hist_btn",
            "##fourchan_hist",
            "Board",
            "Examples:\nv - video games\nsci - science and math\ngif - animated GIFs",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_FourChanBoard,
        m_SearchHistory );

    const int prev = m_FourChanMaxMediaItems;
    if( DownloadOptionControls::DrawClampedInputInt(
        {
            "FourChanMaxMediaItems",
            "##fourchan_max_media_items",
            "Limit",
            DownloadOptionControls::s_MaxMediaDownloadsTooltip
        },
        m_FourChanMaxMediaItems,
        FOURCHAN_MEDIA_MIN,
        FOURCHAN_MEDIA_MAX ) )
    {
        if( m_FourChanMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "fourchan_max_downloads", m_FourChanMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }
}

ImageScraper::UserInputOptions ImageScraper::FourChanPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider              = ContentProvider::FourChan;
    options.m_FourChanBoard         = m_FourChanBoard;
    options.m_FourChanMaxThreads    = m_FourChanMaxThreads;
    options.m_FourChanMaxMediaItems = m_FourChanMaxMediaItems;
    return options;
}
