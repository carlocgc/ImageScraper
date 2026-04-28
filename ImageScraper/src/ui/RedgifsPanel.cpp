#include "ui/RedgifsPanel.h"

#include "ui/DownloadOptionControls.h"

#include <algorithm>

void ImageScraper::RedgifsPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "redgifs_user_history" );
    m_RedgifsUser = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = REDGIFS_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( "redgifs_max_downloads", saved ) )
        {
            m_RedgifsMaxMediaItems = std::clamp( saved, REDGIFS_LIMIT_MIN, REDGIFS_LIMIT_MAX );
        }
    }
}

void ImageScraper::RedgifsPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_RedgifsUser );
}

void ImageScraper::RedgifsPanel::Update( )
{
    DownloadOptionControls::DrawSearchInput(
        {
            "RedgifsUser",
            "##redgifs_user",
            "##redgifs_hist_btn",
            "##redgifs_hist",
            "User",
            "The redgifs creator's username, e.g. someuser",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_RedgifsUser,
        m_SearchHistory );

    const int prev = m_RedgifsMaxMediaItems;
    if( DownloadOptionControls::DrawClampedInputInt(
        {
            "RedgifsMaxMediaItems",
            "##redgifs_max_media_items",
            "Limit",
            DownloadOptionControls::s_MaxMediaDownloadsTooltip
        },
        m_RedgifsMaxMediaItems,
        REDGIFS_LIMIT_MIN,
        REDGIFS_LIMIT_MAX ) )
    {
        if( m_RedgifsMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "redgifs_max_downloads", m_RedgifsMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }
}

ImageScraper::UserInputOptions ImageScraper::RedgifsPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider              = ContentProvider::Redgifs;
    options.m_RedgifsUser           = m_RedgifsUser;
    options.m_RedgifsMaxMediaItems  = m_RedgifsMaxMediaItems;
    return options;
}
