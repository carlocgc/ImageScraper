#include "ui/GifFrameAnimator.h"

#include <algorithm>

void ImageScraper::GifFrameAnimator::Initialise( std::vector<int> frameDelaysMs, int frameCount )
{
    m_FrameDelaysMs = std::move( frameDelaysMs );
    m_FrameCount    = frameCount;
    m_CurrentFrame  = 0;
    m_FrameAccumMs  = 0.0f;
}

void ImageScraper::GifFrameAnimator::Advance( float deltaMs )
{
    if( m_FrameCount <= 1 )
    {
        return;
    }

    m_FrameAccumMs += deltaMs;

    while( true )
    {
        const int delayMs = m_FrameDelaysMs.empty( ) ? 100
                          : m_FrameDelaysMs[ m_CurrentFrame ];
        if( m_FrameAccumMs < static_cast<float>( delayMs ) )
        {
            break;
        }

        m_FrameAccumMs -= static_cast<float>( delayMs );
        m_CurrentFrame  = ( m_CurrentFrame + 1 ) % m_FrameCount;
    }
}

void ImageScraper::GifFrameAnimator::SetFrame( int frame )
{
    if( m_FrameCount > 0 )
    {
        m_CurrentFrame = std::clamp( frame, 0, m_FrameCount - 1 );
    }

    m_FrameAccumMs = 0.0f;
}

void ImageScraper::GifFrameAnimator::Reset( )
{
    m_CurrentFrame = 0;
    m_FrameAccumMs = 0.0f;
}

int ImageScraper::GifFrameAnimator::CurrentFrame( ) const
{
    return m_CurrentFrame;
}

int ImageScraper::GifFrameAnimator::FrameCount( ) const
{
    return m_FrameCount;
}
