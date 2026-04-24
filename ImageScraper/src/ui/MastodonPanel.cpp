#include "ui/MastodonPanel.h"

#include "ui/DownloadOptionControls.h"

#include <algorithm>

void ImageScraper::MastodonPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    m_AppConfig = appConfig;
    m_InstanceHistory.Load( appConfig, "mastodon_instance_history" );
    m_AccountHistory.Load( std::move( appConfig ), "mastodon_account_history" );
    m_MastodonInstance = m_InstanceHistory.GetMostRecent( );
    m_MastodonAccount = m_AccountHistory.GetMostRecent( );

    if( m_AppConfig )
    {
        int saved = MASTODON_LIMIT_DEFAULT;
        if( m_AppConfig->GetValue<int>( "mastodon_max_downloads", saved ) )
        {
            m_MastodonMaxMediaItems = std::clamp( saved, MASTODON_LIMIT_MIN, MASTODON_LIMIT_MAX );
        }
    }
}

void ImageScraper::MastodonPanel::OnSearchCommitted( )
{
    m_InstanceHistory.Push( m_MastodonInstance );
    m_AccountHistory.Push( m_MastodonAccount );
}

void ImageScraper::MastodonPanel::Update( )
{
    DownloadOptionControls::DrawSearchInput(
        {
            "MastodonInstance",
            "##mastodon_instance",
            "##mastodon_instance_hist_btn",
            "##mastodon_instance_hist",
            "Instance",
            "Examples:\nmastodon.social - instance host\nhttps://fosstodon.org - instance URL",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_MastodonInstance,
        m_InstanceHistory );

    DownloadOptionControls::DrawSearchInput(
        {
            "MastodonAccount",
            "##mastodon_account",
            "##mastodon_account_hist_btn",
            "##mastodon_account_hist",
            "Account",
            "Examples:\nGargron - local account\n@Gargron@mastodon.social - federated account\nhttps://mastodon.social/@Gargron - profile URL",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_MastodonAccount,
        m_AccountHistory );

    const int prev = m_MastodonMaxMediaItems;
    if( DownloadOptionControls::DrawClampedInputInt(
        {
            "MastodonMaxMediaItems",
            "##mastodon_max_media_items",
            "Limit",
            DownloadOptionControls::s_MaxMediaDownloadsTooltip
        },
        m_MastodonMaxMediaItems,
        MASTODON_LIMIT_MIN,
        MASTODON_LIMIT_MAX ) )
    {
        if( m_MastodonMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "mastodon_max_downloads", m_MastodonMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }
}

ImageScraper::UserInputOptions ImageScraper::MastodonPanel::BuildInputOptions( ) const
{
    UserInputOptions options{ };
    options.m_Provider                  = ContentProvider::Mastodon;
    options.m_MastodonInstance          = m_MastodonInstance;
    options.m_MastodonAccount           = m_MastodonAccount;
    options.m_MastodonMaxMediaItems     = m_MastodonMaxMediaItems;
    return options;
}
