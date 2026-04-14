#include "ui/DownloadOptionsPanel.h"
#include "ui/RedditPanel.h"
#include "ui/TumblrPanel.h"
#include "ui/FourChanPanel.h"
#include "log/Logger.h"

ImageScraper::DownloadOptionsPanel::DownloadOptionsPanel( const std::vector<std::shared_ptr<Service>>& services )
    : m_Services{ services }
{
    m_ProviderPanels.push_back( std::make_unique<RedditPanel>( ) );
    m_ProviderPanels.push_back( std::make_unique<TumblrPanel>( ) );
    m_ProviderPanels.push_back( std::make_unique<FourChanPanel>( ) );
}

void ImageScraper::DownloadOptionsPanel::Update( )
{
    ImGui::SetNextWindowSize( ImVec2( 640, 200 ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "Download Options", nullptr ) )
    {
        ImGui::End( );
        return;
    }

    UpdateProviderWidgets( );
    UpdateRunCancelButton( );
    UpdateSignInButton( );

    ImGui::End( );

    if( HandleUserInput( ) )
    {
        SetInputState( InputState::Blocked );
    }
}

void ImageScraper::DownloadOptionsPanel::SetInputState( InputState state )
{
    m_InputState = state;
}

void ImageScraper::DownloadOptionsPanel::OnRunComplete( )
{
    SetInputState( InputState::Free );
    Reset( );
}

void ImageScraper::DownloadOptionsPanel::OnSignInComplete( ContentProvider provider )
{
    const int signingInProvider = m_SigningInProvider.load( );

    if( signingInProvider != static_cast<int>( provider ) )
    {
        ErrorLog( "[%s] Tried to complete sign in for an invalid provider!", __FUNCTION__ );
        return;
    }

    if( signingInProvider == INVALID_CONTENT_PROVIDER )
    {
        ErrorLog( "[%s] Tried to complete sign in when no sign in was started!", __FUNCTION__ );
        return;
    }

    m_SigningInProvider.store( INVALID_CONTENT_PROVIDER );
}

void ImageScraper::DownloadOptionsPanel::UpdateProviderWidgets( )
{
    ImGui::BeginDisabled( m_InputState >= InputState::Blocked );

    if( ImGui::BeginChild( "ContentProvider", ImVec2( 500, 25 ), false ) )
    {
        ImGui::Combo( "Content Provider", &m_ContentProvider, s_ContentProviderStrings, IM_ARRAYSIZE( s_ContentProviderStrings ) );
    }

    ImGui::EndChild( );

    if( IProviderPanel* panel = GetActivePanel( ) )
    {
        panel->Update( );
    }

    ImGui::EndDisabled( );
}

void ImageScraper::DownloadOptionsPanel::UpdateSignInButton( )
{
    IProviderPanel* panel = GetActivePanel( );
    if( !panel || !panel->CanSignIn( ) )
    {
        return;
    }

    const int signingInProvider = m_SigningInProvider.load( );

    if( signingInProvider != INVALID_CONTENT_PROVIDER && m_ContentProvider != signingInProvider )
    {
        CancelSignIn( );
    }

    const bool signInStarted = signingInProvider == static_cast<int>( m_ContentProvider );

    const std::shared_ptr<Service> service = GetCurrentService( );
    if( service && service->IsSignedIn( ) )
    {
        const std::string username = service->GetSignedInUser( );
        if( !username.empty( ) )
        {
            ImGui::SameLine( );
            ImGui::TextColored( ImVec4( 1.0f, 0.45f, 0.0f, 1.0f ), "u/%s", username.c_str( ) );
        }

        if( ImGui::Button( "Sign Out", ImVec2( 100, 40 ) ) )
        {
            service->SignOut( );
        }
    }
    else if( signInStarted )
    {
        if( ImGui::Button( "Cancel Sign In", ImVec2( 120, 40 ) ) )
        {
            m_SigningInProvider.store( INVALID_CONTENT_PROVIDER );
        }
    }
    else
    {
        if( ImGui::Button( "Sign In", ImVec2( 100, 40 ) ) )
        {
            if( service )
            {
                bool success = service->OpenExternalAuth( );
                ( void )success;
                m_SigningInProvider.store( static_cast<int>( m_ContentProvider ) );
            }
        }
    }
}

void ImageScraper::DownloadOptionsPanel::UpdateRunCancelButton( )
{
    if( !m_Running )
    {
        m_StartProcess = ImGui::Button( "Run", ImVec2( 100, 40 ) );
    }
    else
    {
        ImGui::BeginDisabled( m_DownloadCancelled.load( ) );

        if( ImGui::Button( "Cancel", ImVec2( 100, 40 ) ) )
        {
            m_DownloadCancelled.store( true );
        }

        ImGui::EndDisabled( );
    }
}

bool ImageScraper::DownloadOptionsPanel::HandleUserInput( )
{
    if( m_InputState >= InputState::Blocked )
    {
        return false;
    }

    if( !m_StartProcess )
    {
        return false;
    }

    DebugLog( "[%s] Started processing user input...", __FUNCTION__ );

    m_StartProcess = false;
    m_Running      = true;

    IProviderPanel* panel = GetActivePanel( );
    if( !panel )
    {
        DebugLog( "[%s] Invalid ContentProvider, check ContentProvider constant", __FUNCTION__ );
        return false;
    }

    if( !panel->IsReadyToRun( ) )
    {
        return false;
    }

    const UserInputOptions inputOptions = panel->BuildInputOptions( );

    for( auto service : m_Services )
    {
        if( service->HandleUserInput( inputOptions ) )
        {
            DebugLog( "[%s] User input handled!", __FUNCTION__ );
            return true;
        }
    }

    DebugLog( "[%s] User input not handled, check for missing services!", __FUNCTION__ );
    return false;
}

void ImageScraper::DownloadOptionsPanel::Reset( )
{
    m_Running = false;
    m_DownloadCancelled.store( false );
}

void ImageScraper::DownloadOptionsPanel::CancelSignIn( )
{
    m_SigningInProvider.store( INVALID_CONTENT_PROVIDER );
}

ImageScraper::IProviderPanel* ImageScraper::DownloadOptionsPanel::GetActivePanel( ) const
{
    const ContentProvider provider = static_cast<ContentProvider>( m_ContentProvider );
    for( const auto& panel : m_ProviderPanels )
    {
        if( panel->GetContentProvider( ) == provider )
        {
            return panel.get( );
        }
    }
    return nullptr;
}

std::shared_ptr<ImageScraper::Service> ImageScraper::DownloadOptionsPanel::GetCurrentService( ) const
{
    const ContentProvider provider = static_cast<ContentProvider>( m_ContentProvider );
    for( const auto& service : m_Services )
    {
        if( service->GetContentProvider( ) == provider )
        {
            return service;
        }
    }
    return nullptr;
}
