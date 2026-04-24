#include "ui/BlueskyPanel.h"

#include "ui/DownloadOptionControls.h"

#include <algorithm>

void ImageScraper::BlueskyPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "bluesky_actor_history" );
    m_BlueskyActor = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = BLUESKY_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( "bluesky_max_downloads", saved ) )
        {
            m_BlueskyMaxMediaItems = std::clamp( saved, BLUESKY_LIMIT_MIN, BLUESKY_LIMIT_MAX );
        }
    }
}

void ImageScraper::BlueskyPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_BlueskyActor );
}

void ImageScraper::BlueskyPanel::Update( )
{
    DownloadOptionControls::DrawSearchInput(
        {
            "BlueskyActor",
            "##bluesky_actor",
            "##bluesky_hist_btn",
            "##bluesky_hist",
            "Actor",
            "Examples:\nalice.bsky.social - handle\ndid:plc:abc123 - decentralized identifier",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_BlueskyActor,
        m_SearchHistory );

    const int prev = m_BlueskyMaxMediaItems;
    if( DownloadOptionControls::DrawClampedInputInt(
        {
            "BlueskyMaxMediaItems",
            "##bluesky_max_media_items",
            "Limit",
            DownloadOptionControls::s_MaxMediaDownloadsTooltip
        },
        m_BlueskyMaxMediaItems,
        BLUESKY_LIMIT_MIN,
        BLUESKY_LIMIT_MAX ) )
    {
        if( m_BlueskyMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "bluesky_max_downloads", m_BlueskyMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }
}

ImageScraper::UserInputOptions ImageScraper::BlueskyPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider              = ContentProvider::Bluesky;
    options.m_BlueskyActor          = m_BlueskyActor;
    options.m_BlueskyMaxMediaItems  = m_BlueskyMaxMediaItems;
    return options;
}
