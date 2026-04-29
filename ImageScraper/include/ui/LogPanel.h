#pragma once

#include "ui/IUiPanel.h"
#include "log/Logger.h"
#include "collections/RingBuffer.h"
#include "io/JsonFile.h"
#include "imgui/imgui.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

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

        // Path of the current session's log file - used by the right-click
        // "Open log file location" context menu item.
        void SetLogFilePath( const std::string& path ) { m_LogFilePath = path; }
        const std::string& GetLogFilePath( ) const { return m_LogFilePath; }

        LogLevel GetLogLevel( ) const { return m_LogLevel; }
        bool IsWordWrapEnabled( ) const { return m_WordWrap; }

    private:
        void PersistWordWrapState( );
        void CopyLinesToClipboard( bool selectedOnly );
        void OpenLogFileLocation( ) const;

        RingBuffer<LogLine> m_LogContent;
        ImGuiTextFilter     m_Filter{ };
        LogLevel            m_LogLevel{ LogLevel::Error };
        bool                m_AutoScroll{ true };
        bool                m_DebugLogging{ false };
        bool                m_WordWrap{ true };
        bool                m_ScrollToBottom{ false };

        // Selection - keyed by LogLine::m_Id so it survives ring-buffer eviction.
        std::unordered_set<uint64_t> m_SelectedIds{ };
        uint64_t                     m_SelectionAnchorId{ 0 };

        std::string               m_LogFilePath{ };
        std::shared_ptr<JsonFile> m_AppConfig{ };
        bool m_Running{ false };
    };
}
