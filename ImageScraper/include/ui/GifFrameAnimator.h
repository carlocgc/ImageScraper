#pragma once

#include <vector>

namespace ImageScraper
{
    // Tracks the current frame index and advance timing for animated GIFs.
    // Pure logic with no OpenGL dependency — fully unit-testable.
    class GifFrameAnimator
    {
    public:
        // Set per-frame delays (ms) and total frame count; resets to frame 0.
        void Initialise( std::vector<int> frameDelaysMs, int frameCount );

        // Accumulate deltaMs and advance the frame counter if enough time has elapsed.
        // Falls back to 100 ms per frame when the delays vector is empty.
        void Advance( float deltaMs );

        // Jump to a specific frame and clear the accumulator (e.g. for scrubbing or pause).
        void SetFrame( int frame );

        // Reset to frame 0 and clear the accumulator.
        void Reset( );

        int CurrentFrame( ) const;
        int FrameCount( )   const;

    private:
        std::vector<int> m_FrameDelaysMs{ };
        int              m_FrameCount{ 0 };
        int              m_CurrentFrame{ 0 };
        float            m_FrameAccumMs{ 0.0f };
    };
}
