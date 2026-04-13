#pragma once

#include "ui/IUiPanel.h"
#include "services/Service.h"
#include "services/IServiceSink.h"
#include "imgui/imgui.h"

#include <vector>
#include <memory>
#include <atomic>
#include <string>

namespace ImageScraper
{
    class DownloadOptionsPanel : public IUiPanel
    {
    public:
        DownloadOptionsPanel( const std::vector<std::shared_ptr<Service>>& services );

        void Update( ) override;

        // State queries used by FrontEnd to coordinate with other panels
        bool IsRunning( ) const { return m_Running; }

        // IServiceSink delegation targets — called by FrontEnd
        bool IsCancelled( ) const { return m_DownloadCancelled.load( ); }
        void OnRunComplete( );
        void SetInputState( InputState state );
        int  GetSigningInProvider( ) const { return m_SigningInProvider.load( ); }
        void OnSignInComplete( ContentProvider provider );

    private:
        void UpdateProviderWidgets( );
        void UpdateRedditWidgets( );
        void UpdateTumblrWidgets( );
        void UpdateFourChanWidgets( );
        void UpdateSignInButton( );
        void UpdateRunCancelButton( );

        bool HandleUserInput( );
        void Reset( );
        bool CanSignIn( ) const;
        void CancelSignIn( );
        std::shared_ptr<Service> GetCurrentProvider( );

        UserInputOptions BuildRedditInputOptions( );
        UserInputOptions BuildTumblrInputOptions( );
        UserInputOptions BuildFourChanInputOptions( );

        std::vector<std::shared_ptr<Service>> m_Services{ };
        InputState m_InputState{ InputState::Free };

        int  m_ContentProvider{ 0 };
        bool m_StartProcess{ false };
        bool m_Running{ false };
        std::atomic_bool    m_DownloadCancelled{ false };
        std::atomic<int>    m_SigningInProvider{ INVALID_CONTENT_PROVIDER };

        // Reddit
        std::string          m_SubredditName{ };
        RedditScope          m_RedditScope{ RedditScope::Hot };
        RedditScopeTimeFrame m_RedditScopeTimeFrame{ RedditScopeTimeFrame::All };
        int                  m_RedditMaxMediaItems{ REDDIT_LIMIT_DEFAULT };

        // Tumblr
        std::string m_TumblrUser{ };

        // 4chan
        std::string m_FourChanBoard{ };
        int         m_FourChanMaxThreads{ FOURCHAN_THREAD_MAX };
        int         m_FourChanMaxMediaItems{ FOURCHAN_MEDIA_DEFAULT };
    };
}
