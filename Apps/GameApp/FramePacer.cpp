//==============================================================================
//
//  FramePacer.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "FramePacer.hpp"

//#define FRAME_PACER_LEGACY_WAIT

#ifdef FRAME_PACER_LEGACY_WAIT
#include "x_files/x_threads.hpp"
#else
#include "SDL3/SDL_timer.h"
#endif

#include "x_files/x_time.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

FramePacer::FramePacer( void )
{
    Reset();
}
//==============================================================================

void FramePacer::Configure( FrameRateLimit Limit, eng_present_mode PresentMode )
{
    FrameRateLimit EffectiveLimit = FrameRateLimit::Auto;
    s32 FramesPerSecond = 0;

    if( PresentMode == ENG_PRESENT_IMMEDIATE )
    {
        FramesPerSecond = GetFramesPerSecond( Limit );
        if( FramesPerSecond > 0 )
        {
            EffectiveLimit = Limit;
        }
    }

    if( EffectiveLimit == m_Limit )
    {
        return;
    }

    Reset();
    if( EffectiveLimit == FrameRateLimit::Auto )
    {
        return;
    }

    m_Limit            = EffectiveLimit;
    m_FramePeriodTicks = static_cast<f64>( x_GetTicksPerSecond() ) / static_cast<f64>( FramesPerSecond );
}

//==============================================================================

void FramePacer::WaitForNextFrame( void )
{
    if( m_Limit == FrameRateLimit::Auto )
    {
        return;
    }

    f64 const NowTicks = static_cast<f64>( x_GetTime() );
    if( !m_HasDeadline )
    {
        m_NextDeadlineTicks = NowTicks + m_FramePeriodTicks;
        m_HasDeadline = TRUE;
        return;
    }

    if( NowTicks >= m_NextDeadlineTicks )
    {
        // A late frame starts a new schedule; missed frames are never replayed.
        m_NextDeadlineTicks = NowTicks + m_FramePeriodTicks;
        return;
    }

#ifdef FRAME_PACER_LEGACY_WAIT
    xtick const TicksPerMillisecond = x_GetTicksPerMs();
    f64 const CoarseWaitGuardTicks = static_cast<f64>( TicksPerMillisecond );

    while( TRUE )
    {
        f64 const RemainingTicks = m_NextDeadlineTicks - static_cast<f64>( x_GetTime() );
        if( RemainingTicks <= CoarseWaitGuardTicks )
        {
            break;
        }

        s32 const DelayMilliseconds = static_cast<s32>( (RemainingTicks - CoarseWaitGuardTicks) / static_cast<f64>( TicksPerMillisecond ) );
        if( DelayMilliseconds <= 0 )
        {
            break;
        }

        x_DelayThread( DelayMilliseconds );
    }

    while( static_cast<f64>( x_GetTime() ) < m_NextDeadlineTicks )
    {
        // The coarse wait leaves only the short final interval here.
    }
#else
    f64 const RemainingTicks = m_NextDeadlineTicks - static_cast<f64>( x_GetTime() );
    if( RemainingTicks > 0.0 )
    {
        u64 const RemainingNanoseconds = static_cast<u64>( (RemainingTicks * static_cast<f64>( SDL_NS_PER_SECOND )) / static_cast<f64>( x_GetTicksPerSecond() ) );
        SDL_DelayPrecise( RemainingNanoseconds );
    }
#endif

    m_NextDeadlineTicks += m_FramePeriodTicks;

    f64 const AfterWaitTicks = static_cast<f64>( x_GetTime() );
    if( AfterWaitTicks >= m_NextDeadlineTicks )
    {
        // The wait crossed the next frame boundary, so start a new schedule.
        m_NextDeadlineTicks = AfterWaitTicks + m_FramePeriodTicks;
    }
}

//==============================================================================

void FramePacer::Reset( void )
{
    m_Limit             = FrameRateLimit::Auto;
    m_FramePeriodTicks  = 0.0;
    m_NextDeadlineTicks = 0.0;
    m_HasDeadline       = FALSE;
}

//==============================================================================

s32 FramePacer::GetFramesPerSecond( FrameRateLimit Limit )
{
    switch( Limit )
    {
        case FrameRateLimit::Auto:
        {
            return 0;
        }
        break;

        case FrameRateLimit::Fps30:
        {
            return 30;
        }
        break;

        case FrameRateLimit::Fps60:
        {
            return 60;
        }
        break;

        case FrameRateLimit::Fps90:
        {
            return 90;
        }
        break;

        case FrameRateLimit::Fps120:
        {
            return 120;
        }
        break;

        case FrameRateLimit::Fps144:
        {
            return 144;
        }
        break;

        case FrameRateLimit::Fps165:
        {
            return 165;
        }
        break;

        case FrameRateLimit::Fps240:
        {
            return 240;
        }
        break;

        default:
        {
            ASSERTS( FALSE, "Frame pacer received an invalid frame rate limit" );
            return 0;
        }
        break;
    }
}
