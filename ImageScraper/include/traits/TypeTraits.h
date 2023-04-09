#pragma once

namespace ImageScraper
{
    class NonCopyable
    {
    protected:
        NonCopyable( ) = default;
        virtual ~NonCopyable( ) = default;
    private:
        NonCopyable( const NonCopyable& ) = delete;
        NonCopyable& operator=( const NonCopyable& ) = delete;
    };

    class NonMovable
    {
    protected:
        NonMovable( ) = default;
        virtual ~NonMovable( ) = default;
    private:
        NonMovable( NonMovable&& ) = delete;
        NonMovable& operator=( NonMovable&& ) = delete;
    };

    class NonCopyMovable
    {
    protected:
        NonCopyMovable( ) = default;
        virtual ~NonCopyMovable( ) = default;
    private:
        NonCopyMovable( const NonCopyMovable& ) = delete;
        NonCopyMovable( NonCopyMovable&& ) = delete;
        NonCopyMovable& operator=( const NonCopyMovable& ) = delete;
        NonCopyMovable& operator=( NonCopyMovable&& ) = delete;
    };
}