#include "ui/DownloadOptionsPanel.h"
#include "log/Logger.h"
#include "config/Config.h"

#include <algorithm>

ImageScraper::DownloadOptionsPanel::DownloadOptionsPanel( const std::vector<std::shared_ptr<Service>>& services )
    : m_Services{ services }
{
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
    if( m_InputState == InputState::Free )
    {
        Reset( );
    }
}

void ImageScraper::DownloadOptionsPanel::OnRunComplete( )
{
    SetInputState( InputState::Free );
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

    const ContentProvider provider = static_cast<ContentProvider>( m_ContentProvider );

    switch( provider )
    {
    case ContentProvider::Reddit:
        UpdateRedditWidgets( );
        break;
    case ContentProvider::Tumblr:
        UpdateTumblrWidgets( );
        break;
    case ContentProvider::FourChan:
        UpdateFourChanWidgets( );
        break;
    default:
        break;
    }

    ImGui::EndDisabled( );
}

void ImageScraper::DownloadOptionsPanel::UpdateRedditWidgets( )
{
    if( ImGui::BeginChild( "SubredditName", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_SubredditName.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Subreddit (e.g. Gifs)", buffer, INPUT_STRING_MAX, flags, nullptr, this ) )
        {
            m_SubredditName = buffer;
        }
    }

    ImGui::EndChild( );

    if( ImGui::BeginChild( "SubredditScope", ImVec2( 500, 25 ), false ) )
    {
        int redditScope = static_cast<int>( m_RedditScope );
        ImGui::Combo( "Scope", &redditScope, s_RedditScopeStrings, IM_ARRAYSIZE( s_RedditScopeStrings ) );
        m_RedditScope = static_cast<RedditScope>( redditScope );
    }

    ImGui::EndChild( );

    const RedditScope scope = static_cast<RedditScope>( m_RedditScope );

    if( scope == RedditScope::Top ||
        scope == RedditScope::Controversial ||
        scope == RedditScope::Sort )
    {
        if( ImGui::BeginChild( "SubredditScopeTimeFrame", ImVec2( 500, 25 ), false ) )
        {
            int redditScopeTimeFrame = static_cast<int>( m_RedditScopeTimeFrame );
            ImGui::Combo( "Time Frame", &redditScopeTimeFrame, s_RedditScopeTimeFrameStrings, IM_ARRAYSIZE( s_RedditScopeTimeFrameStrings ) );
            m_RedditScopeTimeFrame = static_cast<RedditScopeTimeFrame>( redditScopeTimeFrame );
        }

        ImGui::EndChild( );
    }

    if( ImGui::BeginChild( "RedditMaxMediaItems", ImVec2( 500, 25 ), false ) )
    {
        ImGui::InputInt( "Max Downloads", &m_RedditMaxMediaItems );
        m_RedditMaxMediaItems = std::clamp( m_RedditMaxMediaItems, REDDIT_LIMIT_MIN, REDDIT_LIMIT_MAX );
    }

    ImGui::EndChild( );
}

void ImageScraper::DownloadOptionsPanel::UpdateTumblrWidgets( )
{
    if( ImGui::BeginChild( "TumblrUser", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_TumblrUser.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Tumblr User", buffer, INPUT_STRING_MAX, flags, nullptr, this ) )
        {
            m_TumblrUser = buffer;
        }
    }

    ImGui::EndChild( );
}

void ImageScraper::DownloadOptionsPanel::UpdateFourChanWidgets( )
{
    if( ImGui::BeginChild( "FourChanBoard", ImVec2( 500, 25 ), false ) )
    {
        char buffer[ INPUT_STRING_MAX ] = "";
        strcpy_s( buffer, INPUT_STRING_MAX, m_FourChanBoard.c_str( ) );
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsNoBlank;
        if( ImGui::InputText( "Board (e.g. v, sci )", buffer, INPUT_STRING_MAX, flags, nullptr, this ) )
        {
            m_FourChanBoard = buffer;
        }
    }

    ImGui::EndChild( );
}

void ImageScraper::DownloadOptionsPanel::UpdateSignInButton( )
{
    if( !CanSignIn( ) )
    {
        return;
    }

    const int signingInProvider = m_SigningInProvider.load( );

    if( signingInProvider != INVALID_CONTENT_PROVIDER && m_ContentProvider != signingInProvider )
    {
        CancelSignIn( );
    }

    const bool signInStarted = signingInProvider == static_cast<int>( m_ContentProvider );

    const std::shared_ptr<Service> service = GetCurrentProvider( );
    if( service && service->IsSignedIn( ) )
    {
        ImGui::BeginDisabled( true );
        ImGui::Button( "Signed In", ImVec2( 120, 40 ) );
        ImGui::EndDisabled( );
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
    m_Running = true;

    UserInputOptions inputOptions{ };

    const ContentProvider provider = static_cast<ContentProvider>( m_ContentProvider );
    switch( provider )
    {
    case ContentProvider::Reddit:
        if( m_SubredditName.empty( ) )
        {
            return false;
        }
        DebugLog( "[%s] Building Reddit user input data", __FUNCTION__ );
        inputOptions = BuildRedditInputOptions( );
        break;
    case ContentProvider::Tumblr:
        if( m_TumblrUser.empty( ) )
        {
            return false;
        }
        DebugLog( "[%s] Building Tumblr user input data", __FUNCTION__ );
        inputOptions = BuildTumblrInputOptions( );
        break;
    case ContentProvider::FourChan:
        if( m_FourChanBoard.empty( ) )
        {
            return false;
        }
        DebugLog( "[%s] Building 4chan user input data", __FUNCTION__ );
        inputOptions = BuildFourChanInputOptions( );
        break;
    default:
        DebugLog( "[%s] Invalid ContentProvider, check ContentProvider constant", __FUNCTION__ );
        return false;
    }

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

bool ImageScraper::DownloadOptionsPanel::CanSignIn( ) const
{
    switch( static_cast<ContentProvider>( m_ContentProvider ) )
    {
    case ContentProvider::Reddit:
        return true;
    default:
        return false;
    }
}

void ImageScraper::DownloadOptionsPanel::CancelSignIn( )
{
    m_SigningInProvider.store( INVALID_CONTENT_PROVIDER );
}

std::shared_ptr<ImageScraper::Service> ImageScraper::DownloadOptionsPanel::GetCurrentProvider( )
{
    for( const auto& service : m_Services )
    {
        if( service->GetContentProvider( ) == static_cast<ContentProvider>( m_ContentProvider ) )
        {
            return service;
        }
    }
    return nullptr;
}

ImageScraper::UserInputOptions ImageScraper::DownloadOptionsPanel::BuildRedditInputOptions( )
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::Reddit;
    options.m_SubredditName = m_SubredditName;

    auto toLower = [ ]( unsigned char ch ) { return std::tolower( ch ); };

    std::string scope = s_RedditScopeStrings[ static_cast<int>( m_RedditScope ) ];
    std::transform( scope.begin( ), scope.end( ), scope.begin( ), toLower );
    options.m_RedditScope = scope;

    if( m_RedditScope == RedditScope::Top || m_RedditScope == RedditScope::Controversial || m_RedditScope == RedditScope::Sort )
    {
        std::string scopeTimeFrame = s_RedditScopeTimeFrameStrings[ static_cast<int>( m_RedditScopeTimeFrame ) ];
        std::transform( scopeTimeFrame.begin( ), scopeTimeFrame.end( ), scopeTimeFrame.begin( ), toLower );
        options.m_RedditScopeTimeFrame = scopeTimeFrame;
    }

    options.m_RedditMaxMediaItems = m_RedditMaxMediaItems;
    return options;
}

ImageScraper::UserInputOptions ImageScraper::DownloadOptionsPanel::BuildTumblrInputOptions( )
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::Tumblr;
    options.m_TumblrUser = m_TumblrUser;
    return options;
}

ImageScraper::UserInputOptions ImageScraper::DownloadOptionsPanel::BuildFourChanInputOptions( )
{
    UserInputOptions options{ };
    options.m_Provider = ContentProvider::FourChan;
    options.m_FourChanBoard = m_FourChanBoard;
    options.m_FourChanMaxThreads = m_FourChanMaxThreads;
    options.m_FourChanMaxMediaItems = m_FourChanMaxMediaItems;
    return options;
}
