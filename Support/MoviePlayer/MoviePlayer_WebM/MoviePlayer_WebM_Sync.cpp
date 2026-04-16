//==============================================================================
//
//  MoviePlayer_WebM_Sync.cpp
//
//  Playback timing utilities for WebM movies.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

// Let it be only for PC, for now...

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_time.hpp"

#include "MoviePlayer_WebM_Private.hpp"

using namespace movie_webm;

//==============================================================================

sync_clock::sync_clock(void)
{
    m_StartTime      = 0;
    m_PauseTime      = 0;
    m_PausedDuration = 0.0;
    m_bRunning       = FALSE;
    m_bPaused        = FALSE;
}

//==============================================================================

void sync_clock::Start(void)
{
    m_StartTime      = x_GetTime();
    m_PauseTime      = 0;
    m_PausedDuration = 0.0;
    m_bRunning       = TRUE;
    m_bPaused        = FALSE;
}

//==============================================================================

void sync_clock::Stop(void)
{
    m_bRunning       = FALSE;
    m_bPaused        = FALSE;
    m_PausedDuration = 0.0;
}

//==============================================================================

void sync_clock::Pause(void)
{
    if (m_bRunning && !m_bPaused)
    {
        m_PauseTime = x_GetTime();
        m_bPaused   = TRUE;
    }
}

//==============================================================================

void sync_clock::Resume(void)
{
    if (m_bRunning && m_bPaused)
    {
        const xtick currentTime = x_GetTime();
        m_PausedDuration += x_TicksToSec(currentTime - m_PauseTime);
        m_bPaused = FALSE;
    }
}

//==============================================================================

void sync_clock::Reset(void)
{
    m_StartTime      = 0;
    m_PauseTime      = 0;
    m_PausedDuration = 0.0;
    m_bRunning       = FALSE;
    m_bPaused        = FALSE;
}

//==============================================================================

f64 sync_clock::GetTime(void) const
{
    if (!m_bRunning)
        return 0.0;

    if (m_bPaused)
    {
        return x_TicksToSec(m_PauseTime - m_StartTime) - m_PausedDuration;
    }

    const xtick currentTime = x_GetTime();
    return x_TicksToSec(currentTime - m_StartTime) - m_PausedDuration;
}

//==============================================================================