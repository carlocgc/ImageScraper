#pragma once
#include "asyncgc/ThreadPool.h"
#include "traits/TypeTraits.h"

namespace ImageScraper
{
    class ThreadPool : public NonCopyMovable
    {
    public:
        static Asyncgc::ThreadPool& Instance( )
        {
            static Asyncgc::ThreadPool instance{ 2 };
            return instance;
        }

        static const Asyncgc::ThreadContext s_UIContext = 0;
        static const Asyncgc::ThreadContext s_NetworkContext = 1;

    private:
        ThreadPool( ) = default;
        ~ThreadPool( ) = default;
    };
}