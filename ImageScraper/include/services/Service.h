#pragma once

#include <string>
#include <memory>

namespace ImageScraper
{
    class JsonFile;

    enum class ContentProvider
    {
        Reddit = 0,
        Twitter = 1
    };

    struct UserInputOptions
    {
        ContentProvider m_Provider;

        // Reddit
        std::string m_SubredditName;

        // Twitter
        std::string m_TwitterHandle;
    };

    class Service
    {
    public:
        Service( std::shared_ptr<JsonFile> appConfig, std::shared_ptr<JsonFile> userConfig, const std::string& caBundle )
            : m_AppConfig{ appConfig }
            , m_UserConfig{ userConfig }
            , m_CaBundle{ caBundle }
        {
        }

        virtual ~Service( ) = default;
        virtual bool HandleUserInput( const UserInputOptions& options ) = 0;

    protected:
        std::shared_ptr<JsonFile> m_AppConfig{ nullptr };
        std::shared_ptr<JsonFile> m_UserConfig{ nullptr };
        std::string m_CaBundle{ };
    };
}