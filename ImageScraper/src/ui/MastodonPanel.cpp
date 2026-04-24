#include "ui/MastodonPanel.h"

#include "ui/ProviderSearchInput.h"

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
    ProviderSearchInput::Draw(
        {
            "MastodonInstance",
            "##mastodon_instance",
            "##mastodon_instance_hist_btn",
            "##mastodon_instance_hist",
            "Instance (e.g. mastodon.social)",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_MastodonInstance,
        m_InstanceHistory );

    ProviderSearchInput::Draw(
        {
            "MastodonAccount",
            "##mastodon_account",
            "##mastodon_account_hist_btn",
            "##mastodon_account_hist",
            "Account (e.g. Gargron or @Gargron@mastodon.social)",
            ImGuiInputTextFlags_CharsNoBlank
        },
        m_MastodonAccount,
        m_AccountHistory );

    if( ImGui::BeginChild( "MastodonMaxMediaItems", ImVec2( 500, 25 ), false ) )
    {
        const int prev = m_MastodonMaxMediaItems;
        ImGui::InputInt( "Max Downloads", &m_MastodonMaxMediaItems );
        m_MastodonMaxMediaItems = std::clamp( m_MastodonMaxMediaItems, MASTODON_LIMIT_MIN, MASTODON_LIMIT_MAX );
        if( m_MastodonMaxMediaItems != prev && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "mastodon_max_downloads", m_MastodonMaxMediaItems );
            m_AppConfig->Serialise( );
        }
    }

    ImGui::EndChild( );
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
