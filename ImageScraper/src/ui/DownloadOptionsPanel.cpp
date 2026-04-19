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

void ImageScraper::DownloadOptionsPanel::LoadPanelState( std::shared_ptr<JsonFile> appConfig )
{
    for( auto& panel : m_ProviderPanels )
    {
        panel->LoadPanelState( appConfig );
    }

    m_AppConfig = appConfig;

    int savedProvider = 0;
    if( appConfig->GetValue<int>( "active_provider", savedProvider ) )
    {
        m_ContentProvider = savedProvider;
    }
}

void ImageScraper::DownloadOptionsPanel::Update( )
{
    ImGui::SetNextWindowSize( ImVec2( 640, 200 ), ImGuiCond_FirstUseEver );

    ImVec2 contentScreenMin{ 0.0f, 0.0f };
    ImVec2 contentScreenMax{ 0.0f, 0.0f };
    bool windowOpen = false;

    if( ImGui::Begin( "Download Options", nullptr ) )
    {
        windowOpen = true;

        const ImVec2 winPos = ImGui::GetWindowPos( );
        contentScreenMin = ImVec2( winPos.x + ImGui::GetWindowContentRegionMin( ).x,
                                   winPos.y + ImGui::GetWindowContentRegionMin( ).y );
        contentScreenMax = ImVec2( winPos.x + ImGui::GetWindowContentRegionMax( ).x,
                                   winPos.y + ImGui::GetWindowContentRegionMax( ).y );

        UpdateProviderWidgets( );
        UpdateRunCancelButton( );
        UpdateSignInButton( );
        UpdateWarningPopup( );
    }

    ImGui::End( );

    if( windowOpen && m_Running )
    {
        constexpr float k_BarH = 4.0f;
        constexpr float k_SegW = 0.3f;

        const float barW  = contentScreenMax.x - contentScreenMin.x;
        const float barY0 = contentScreenMax.y - k_BarH;
        const float barY1 = contentScreenMax.y;
        const float segW  = barW * k_SegW;

        const float t     = static_cast<float>( fabs( sin( ImGui::GetTime( ) * 1.5 ) ) );
        const float segX0 = contentScreenMin.x + ( barW - segW ) * t;
        const float segX1 = segX0 + segW;

        ImDrawList* dl = ImGui::GetForegroundDrawList( );
        dl->AddRectFilled(
            ImVec2( contentScreenMin.x, barY0 ),
            ImVec2( contentScreenMax.x, barY1 ),
            IM_COL32( 0, 0, 0, 120 ) );
        dl->AddRectFilled(
            ImVec2( segX0, barY0 ),
            ImVec2( segX1, barY1 ),
            IM_COL32( 100, 180, 255, 220 ) );
    }

    HandleUserInput( );
}

void ImageScraper::DownloadOptionsPanel::SetInputState( InputState state )
{
    m_InputState = state;
}

void ImageScraper::DownloadOptionsPanel::OnRunComplete( )
{
    FinishRun( );
    SetInputState( InputState::Free );
}

void ImageScraper::DownloadOptionsPanel::OnSignInComplete( ContentProvider provider )
{
    CompleteSignIn( provider );
}

void ImageScraper::DownloadOptionsPanel::BeginRun( )
{
    m_Running = true;
    m_DownloadCancelled.store( false );
}

void ImageScraper::DownloadOptionsPanel::FinishRun( )
{
    m_Running = false;
    m_StartProcess = false;
    m_DownloadCancelled.store( false );
}

void ImageScraper::DownloadOptionsPanel::RequestCancel( )
{
    if( m_Running )
    {
        m_DownloadCancelled.store( true );
    }
}

void ImageScraper::DownloadOptionsPanel::BeginSignIn( ContentProvider provider )
{
    m_SigningInProvider.store( static_cast<int>( provider ) );
}

void ImageScraper::DownloadOptionsPanel::CompleteSignIn( ContentProvider provider )
{
    const int signingInProvider = m_SigningInProvider.load( );
    if( signingInProvider == INVALID_CONTENT_PROVIDER )
    {
        LogDebug( "[%s] Sign in completion ignored, no sign in is active.", __FUNCTION__ );
        return;
    }

    if( signingInProvider != static_cast<int>( provider ) )
    {
        LogDebug( "[%s] Sign in completion ignored for inactive provider.", __FUNCTION__ );
        return;
    }

    m_SigningInProvider.store( INVALID_CONTENT_PROVIDER );
}

void ImageScraper::DownloadOptionsPanel::UpdateProviderWidgets( )
{
    ImGui::BeginDisabled( IsInputBlocked( ) );

    if( ImGui::BeginChild( "ContentProvider", ImVec2( 500, 25 ), false ) )
    {
        const int prevProvider = m_ContentProvider;
        ImGui::Combo( "Content Provider", &m_ContentProvider, s_ContentProviderStrings, IM_ARRAYSIZE( s_ContentProviderStrings ) );
        if( m_ContentProvider != prevProvider && m_AppConfig )
        {
            m_AppConfig->SetValue<int>( "active_provider", m_ContentProvider );
            m_AppConfig->Serialise( );
        }
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

    ImGui::BeginDisabled( m_Running );

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
            CancelSignIn( );
        }
    }
    else
    {
        const bool canSignIn = service && service->HasSignInCredentials( );
        ImGui::BeginDisabled( !canSignIn );

        if( ImGui::Button( "Sign In", ImVec2( 100, 40 ) ) )
        {
            if( service && service->OpenExternalAuth( ) )
            {
                BeginSignIn( static_cast<ContentProvider>( m_ContentProvider ) );
            }
        }

        if( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) && !canSignIn )
        {
            ImGui::SetTooltip( "A Consumer Secret is required to sign in.\nAdd it in the Credentials panel." );
        }

        ImGui::EndDisabled( );
    }

    ImGui::EndDisabled( );
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
            RequestCancel( );
        }

        ImGui::EndDisabled( );
    }
}

bool ImageScraper::DownloadOptionsPanel::HandleUserInput( )
{
    if( IsInputBlocked( ) )
    {
        return false;
    }

    if( !m_StartProcess )
    {
        return false;
    }

    LogDebug( "[%s] Started processing user input...", __FUNCTION__ );

    m_StartProcess = false;

    IProviderPanel* panel = GetActivePanel( );
    if( !panel )
    {
        LogDebug( "[%s] Invalid ContentProvider, check ContentProvider constant", __FUNCTION__ );
        return false;
    }

    if( !panel->IsReadyToRun( ) )
    {
        return false;
    }

    const std::shared_ptr<Service> service = GetCurrentService( );
    if( service && !service->HasRequiredCredentials( ) )
    {
        WarningLog( "[%s] Missing required credentials for this provider, please fill in the Credentials panel.", __FUNCTION__ );
        OpenWarning( "Missing credentials for this provider.\nPlease fill in the required fields in the Credentials panel." );
        return false;
    }

    const UserInputOptions inputOptions = panel->BuildInputOptions( );

    for( const auto& svc : m_Services )
    {
        if( svc->HandleUserInput( inputOptions ) )
        {
            LogDebug( "[%s] User input handled!", __FUNCTION__ );
            panel->OnSearchCommitted( );
            BeginRun( );
            SetInputState( InputState::Blocked );
            return true;
        }
    }

    LogDebug( "[%s] User input not handled, check for missing services!", __FUNCTION__ );
    return false;
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

void ImageScraper::DownloadOptionsPanel::UpdateWarningPopup( )
{
    if( m_OpenWarningPopup )
    {
        ImGui::OpenPopup( "Warning##popup" );
        m_OpenWarningPopup = false;
    }

    ImGui::SetNextWindowSize( ImVec2( 340, 0 ), ImGuiCond_Always );
    if( ImGui::BeginPopupModal( "Warning##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize ) )
    {
        ImGui::Spacing( );
        ImGui::TextWrapped( "%s", m_WarningMessage.c_str( ) );
        ImGui::Spacing( );

        const float buttonW = 80.0f;
        ImGui::SetCursorPosX( ( ImGui::GetContentRegionAvail( ).x - buttonW ) * 0.5f );
        if( ImGui::Button( "OK", ImVec2( buttonW, 0 ) ) )
        {
            ImGui::CloseCurrentPopup( );
        }

        ImGui::EndPopup( );
    }
}

void ImageScraper::DownloadOptionsPanel::OpenWarning( const std::string& message )
{
    m_WarningMessage    = message;
    m_OpenWarningPopup  = true;
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
