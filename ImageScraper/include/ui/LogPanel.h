#pragma once

#include "ui/IUiPanel.h"
#include "log/Logger.h"
#include "collections/RingBuffer.h"
#include "io/JsonFile.h"
#include "imgui/imgui.h"

#include <memory>

namespace ImageScraper
{
    class LogPanel : public IUiPanel
    {
    public:
        static constexpr const char* s_WordWrapConfigKey = "log_panel_word_wrap";

        LogPanel( int maxLogLines );

        static bool LoadWordWrapEnabledFromConfig( const std::shared_ptr<JsonFile>& appConfig )
        {
            bool wordWrapEnabled = true;
            if( !appConfig )
            {
                return wordWrapEnabled;
            }

            if( appConfig->GetValue<bool>( s_WordWrapConfigKey, wordWrapEnabled ) )
            {
                return wordWrapEnabled;
            }

            if( !SaveWordWrapEnabledToConfig( appConfig, wordWrapEnabled ) )
            {
                LogError( "[%s] Failed to save default log panel word wrap state.", __FUNCTION__ );
            }

            return wordWrapEnabled;
        }

        static bool SaveWordWrapEnabledToConfig( const std::shared_ptr<JsonFile>& appConfig, bool enabled )
        {
            if( !appConfig )
            {
                return false;
            }

            appConfig->SetValue<bool>( s_WordWrapConfigKey, enabled );
            return appConfig->Serialise( );
        }

        void Update( ) override;
        void LoadPanelState( std::shared_ptr<JsonFile> appConfig );

        void Log( const LogLine& line );

        // Pass false to reset scroll state when a run ends
        void SetRunning( bool running );
        void SetWordWrapEnabled( bool enabled );

        LogLevel GetLogLevel( ) const { return m_LogLevel; }
        bool IsWordWrapEnabled( ) const { return m_WordWrap; }

    private:
        void PersistWordWrapState( );

        RingBuffer<LogLine> m_LogContent;
        ImGuiTextFilter     m_Filter{ };
        LogLevel            m_LogLevel{ LogLevel::Error };
        bool                m_AutoScroll{ true };
        bool                m_DebugLogging{ false };
        bool                m_WordWrap{ true };
        bool                m_ScrollToBottom{ false };

        std::shared_ptr<JsonFile> m_AppConfig{ };
        bool m_Running{ false };
    };
}
