#include "ui/DanbooruPanel.h"

#include "ui/DownloadOptionControls.h"

#include <algorithm>

void ImageScraper::DanbooruPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_SearchHistory.Load( std::move( appConfig ), "danbooru_query_history" );
    m_DanbooruQuery = m_SearchHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = DANBOORU_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( "danbooru_max_downloads", saved ) )
        {
            m_DanbooruMaxMediaItems = std::clamp( saved, DANBOORU_LIMIT_MIN, DANBOORU_LIMIT_MAX );
        }
    }
}

void ImageScraper::DanbooruPanel::OnSearchCommitted( )
{
    m_SearchHistory.Push( m_DanbooruQuery );
}

void ImageScraper::DanbooruPanel::Update( )
{
    DownloadOptionControls::DrawSearchInput(
        {
            "DanbooruQuery",
            "##danbooru_query",
            "##danbooru_hist_btn",
            "##danbooru_hist",
            "Query",
            "Examples:\npool:19776 rating:g\nartist_name rating:g\nhttps://danbooru.donmai.us/posts/11661839",
            0
        },
        m_DanbooruQuery,
        m_SearchHistory );

    const int prev = m_DanbooruMaxMediaItems;
    if( DownloadOptionControls::DrawClampedInputInt(
        {
            "DanbooruMaxMediaItems",
            "##danbooru_max_media_items",
            "Limit",
            DownloadOptionControls::s_MaxMediaDownloadsTooltip
        },
        m_DanbooruMaxMediaItems,
        DANBOORU_LIMIT_MIN,
        DANBOORU_LIMIT_MAX ) )
    {
        if( m_DanbooruMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "danbooru_max_downloads", m_DanbooruMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }
}

ImageScraper::UserInputOptions ImageScraper::DanbooruPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::Danbooru;
    options.m_DanbooruQuery = m_DanbooruQuery;
    options.m_DanbooruMaxMediaItems = m_DanbooruMaxMediaItems;
    return options;
}
