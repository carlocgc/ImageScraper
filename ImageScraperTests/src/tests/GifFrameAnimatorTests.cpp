#include "CppUnitTest.h"
#include "ui/GifFrameAnimator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{

    using namespace ImageScraper;

    // ----------------------------------------------------------------------------
    // Initialise
    // ----------------------------------------------------------------------------


    TEST_CLASS(GifFrameAnimatorTests)
    {
    public:
    TEST_METHOD(GifFrameAnimator_Starts_At_Frame_0_After_Initialise)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        Assert::IsTrue(  anim.CurrentFrame( ) == 0 );
    }
    
    TEST_METHOD(GifFrameAnimator_FrameCount_Returns_The_Initialised_Frame_Count)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 200, 150 }, 3 );
        Assert::IsTrue(  anim.FrameCount( ) == 3 );
    }
    
    // ----------------------------------------------------------------------------
    // Advance
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(GifFrameAnimator_Does_Not_Advance_When_Delta_Is_Less_Than_Delay)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        anim.Advance( 50.0f );
        Assert::IsTrue(  anim.CurrentFrame( ) == 0 );
    }
    
    TEST_METHOD(GifFrameAnimator_Advances_Frame_When_Accumulated_Delta_Exceeds_Delay)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        anim.Advance( 110.0f );
        Assert::IsTrue(  anim.CurrentFrame( ) == 1 );
    }
    
    TEST_METHOD(GifFrameAnimator_Wraps_Around_Past_The_Last_Frame)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        anim.Advance( 350.0f );  // advances 3 full frames (3 x 100ms = 300ms consumed, 50ms left)
        Assert::IsTrue(  anim.CurrentFrame( ) == 0 );
    }
    
    TEST_METHOD(GifFrameAnimator_Does_Not_Advance_When_Frame_Count_Is_1)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100 }, 1 );
        anim.Advance( 1000.0f );
        Assert::IsTrue(  anim.CurrentFrame( ) == 0 );
    }
    
    TEST_METHOD(GifFrameAnimator_Uses_Default_100ms_Delay_When_Delays_Vector_Is_Empty)
    {
        GifFrameAnimator anim;
        anim.Initialise( { }, 3 );
        anim.Advance( 150.0f );  // should advance once at 100ms default
        Assert::IsTrue(  anim.CurrentFrame( ) == 1 );
    }
    
    TEST_METHOD(GifFrameAnimator_Respects_Per_Frame_Delay_Differences)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 200, 50, 50 }, 3 );
        anim.Advance( 210.0f );  // crosses first frame boundary only (200ms)
        Assert::IsTrue(  anim.CurrentFrame( ) == 1 );
    }
    
    // ----------------------------------------------------------------------------
    // Reset
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(GifFrameAnimator_Reset_Returns_To_Frame_0_And_Clears_Accumulator)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        anim.Advance( 250.0f );
        anim.Reset( );
        Assert::IsTrue(  anim.CurrentFrame( ) == 0 );
    
        // accumulator must be cleared — a small delta must not advance
        anim.Advance( 10.0f );
        Assert::IsTrue(  anim.CurrentFrame( ) == 0 );
    }
    
    // ----------------------------------------------------------------------------
    // SetFrame
    // ----------------------------------------------------------------------------
    
    TEST_METHOD(GifFrameAnimator_SetFrame_Jumps_To_The_Target_Frame)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        anim.SetFrame( 2 );
        Assert::IsTrue(  anim.CurrentFrame( ) == 2 );
    }
    
    TEST_METHOD(GifFrameAnimator_SetFrame_Clears_The_Accumulator)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        anim.Advance( 250.0f );  // lands on frame 2 with 50ms left in accum
        anim.SetFrame( 1 );
    
        Assert::IsTrue(  anim.CurrentFrame( ) == 1 );
    
        // accumulator must be zero — a small delta should not advance
        anim.Advance( 10.0f );
        Assert::IsTrue(  anim.CurrentFrame( ) == 1 );
    }
    
    TEST_METHOD(GifFrameAnimator_SetFrame_Clamps_To_Valid_Range)
    {
        GifFrameAnimator anim;
        anim.Initialise( { 100, 100, 100 }, 3 );
        anim.SetFrame( 99 );
        Assert::IsTrue(  anim.CurrentFrame( ) == 2 );
    }
    
    };
}
