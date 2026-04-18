#pragma once
#include "ui/IUiPanel.h"
#include "io/JsonFile.h"

#include <memory>
#include <array>
#include <string>

namespace ImageScraper
{
    class CredentialsPanel : public IUiPanel
    {
    public:
        explicit CredentialsPanel( std::shared_ptr<JsonFile> userConfig );
        void Update( ) override;

    private:
        void SaveField( const std::string& key, const char* value );

        std::shared_ptr<JsonFile> m_UserConfig;

        static constexpr size_t k_BufSize = 256;

        std::array<char, k_BufSize> m_RedditClientId{ };
        std::array<char, k_BufSize> m_RedditClientSecret{ };
        std::array<char, k_BufSize> m_TumblrConsumerKey{ };
        std::array<char, k_BufSize> m_TumblrConsumerSecret{ };
        std::array<char, k_BufSize> m_DiscordClientId{ };
        std::array<char, k_BufSize> m_DiscordClientSecret{ };

        bool m_ShowRedditSecret{ false };
        bool m_ShowTumblrKey{ false };
        bool m_ShowTumblrSecret{ false };
        bool m_ShowDiscordSecret{ false };

#ifdef _DEBUG
        bool m_SaveDevCredentials{ false };
#endif
    };
}
