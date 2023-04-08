#pragma once
#include <string>
#include <mutex>

class ConsoleInput
{
public:
    ConsoleInput( );
    bool GetUserInput( std::string& out);

private:
    std::string m_Input{ };
    std::mutex m_InputMutex{ };
};

