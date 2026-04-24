#include "ui/TumblrPanel.h"
#include "ui/DownloadOptionControls.h"
#include "log/Logger.h"

#include <algorithm>

void ImageScraper::TumblrPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "tumblr_user_history" );
    m_TumblrUser = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = TUMBLR_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( "tumblr_max_downloads", saved ) )
        {
            m_TumblrMaxMediaItems = std::clamp( saved, TUMBLR_LIMIT_MIN, TUMBLR_LIMIT_MAX );
        }
    }
}

void ImageScraper::TumblrPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_TumblrUser );
}

void ImageScraper::TumblrPanel::Update( )
{
    DownloadOptionControls::DrawSearchInput(
        {
            "TumblrUser",
            "##tumblr_user",
            "##tumblr_hist_btn",
            "##tumblr_hist",
            "Blog",
            "Examples:\nstaff - Tumblr blog name\nstaff.tumblr.com - Tumblr blog host",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_TumblrUser,
        m_SearchHistory );

    const int prev = m_TumblrMaxMediaItems;
    if( DownloadOptionControls::DrawClampedInputInt(
        {
            "TumblrMaxMediaItems",
            "##tumblr_max_media_items",
            "Limit",
            DownloadOptionControls::s_MaxMediaDownloadsTooltip
        },
        m_TumblrMaxMediaItems,
        TUMBLR_LIMIT_MIN,
        TUMBLR_LIMIT_MAX ) )
    {
        if( m_TumblrMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "tumblr_max_downloads", m_TumblrMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }
}

ImageScraper::UserInputOptions ImageScraper::TumblrPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider            = ContentProvider::Tumblr;
    options.m_TumblrUser          = m_TumblrUser;
    options.m_TumblrMaxMediaItems = m_TumblrMaxMediaItems;
    return options;
}
