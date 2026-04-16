#pragma once

#include "ui/IUiPanel.h"
#include "ui/IProviderPanel.h"
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

        // IServiceSink delegation targets - called by FrontEnd
        bool IsCancelled( ) const { return m_DownloadCancelled.load( ); }
        void OnRunComplete( );
        void SetInputState( InputState state );
        int  GetSigningInProvider( ) const { return m_SigningInProvider.load( ); }
        void OnSignInComplete( ContentProvider provider );

    private:
        void UpdateProviderWidgets( );
        void UpdateSignInButton( );
        void UpdateRunCancelButton( );
        void UpdateWarningPopup( );

        bool HandleUserInput( );
        void OpenWarning( const std::string& message );
        void Reset( );
        void CancelSignIn( );

        IProviderPanel*          GetActivePanel( ) const;
        std::shared_ptr<Service> GetCurrentService( ) const;

        std::vector<std::shared_ptr<Service>>    m_Services{ };
        std::vector<std::unique_ptr<IProviderPanel>> m_ProviderPanels{ };
        InputState m_InputState{ InputState::Free };

        int  m_ContentProvider{ 0 };
        bool m_StartProcess{ false };
        bool m_Running{ false };
        bool m_OpenWarningPopup{ false };
        std::string m_WarningMessage{ };
        std::atomic_bool m_DownloadCancelled{ false };
        std::atomic<int> m_SigningInProvider{ INVALID_CONTENT_PROVIDER };
    };
}
