#pragma once
#include <string>
#include <mutex>

class ConsoleUI
{
public:
    ConsoleUI( );
    void Init( );
    bool GetUserInput( std::string& out);

    static std::mutex& GetMutex( )
    {
        static std::mutex mutex;
        return mutex;
    }

private:
    std::string m_Input{ };
};

