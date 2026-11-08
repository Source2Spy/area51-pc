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

#include "x_target.hpp"

#if !defined( TARGET_DESKTOP ) && !defined( TARGET_MOBILE )
#error This file should only be compiled for desktop or mobile platforms. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SDL3/SDL.h"

#include "x_time.hpp"
#include "MoviePlayer_WebM_Private.hpp"

using namespace movie_webm;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

sync_clock::sync_clock(void)
{
    m_pMutex         = SDL_CreateMutex();
    m_StartTime      = 0;
    m_PauseTime      = 0;
    m_PausedDuration = 0.0;
    m_bRunning       = FALSE;
    m_bPaused        = FALSE;
}

//==============================================================================

sync_clock::~sync_clock(void)
{
    SDL_DestroyMutex(m_pMutex);
}

//==============================================================================

void sync_clock::Start(void)
{
    if (!m_pMutex)
        return;

    SDL_LockMutex(m_pMutex);
    m_StartTime      = x_GetTime();
    m_PauseTime      = 0;
    m_PausedDuration = 0.0;
    m_bRunning       = TRUE;
    m_bPaused        = FALSE;
    SDL_UnlockMutex(m_pMutex);
}

//==============================================================================

void sync_clock::Stop(void)
{
    if (!m_pMutex)
        return;

    SDL_LockMutex(m_pMutex);
    m_bRunning       = FALSE;
    m_bPaused        = FALSE;
    m_PausedDuration = 0.0;
    SDL_UnlockMutex(m_pMutex);
}

//==============================================================================

void sync_clock::Pause(void)
{
    if (!m_pMutex)
        return;

    SDL_LockMutex(m_pMutex);
    if (m_bRunning && !m_bPaused)
    {
        m_PauseTime = x_GetTime();
        m_bPaused   = TRUE;
    }
    SDL_UnlockMutex(m_pMutex);
}

//==============================================================================

void sync_clock::Resume(void)
{
    if (!m_pMutex)
        return;

    SDL_LockMutex(m_pMutex);
    if (m_bRunning && m_bPaused)
    {
        const xtick currentTime = x_GetTime();
        m_PausedDuration += x_TicksToSec(currentTime - m_PauseTime);
        m_bPaused = FALSE;
    }
    SDL_UnlockMutex(m_pMutex);
}

//==============================================================================

void sync_clock::Reset(void)
{
    if (!m_pMutex)
        return;

    SDL_LockMutex(m_pMutex);
    m_StartTime      = 0;
    m_PauseTime      = 0;
    m_PausedDuration = 0.0;
    m_bRunning       = FALSE;
    m_bPaused        = FALSE;
    SDL_UnlockMutex(m_pMutex);
}

//==============================================================================

f64 sync_clock::GetTime(void) const
{
    if (!m_pMutex)
        return 0.0;

    SDL_LockMutex(m_pMutex);
    f64 time = 0.0;
    if (m_bRunning)
    {
        const xtick now = m_bPaused ? m_PauseTime : x_GetTime();
        time = x_TicksToSec(now - m_StartTime) - m_PausedDuration;
    }
    SDL_UnlockMutex(m_pMutex);
    return time;
}

//==============================================================================

xbool sync_clock::IsRunning(void) const
{
    if (!m_pMutex)
        return FALSE;

    SDL_LockMutex(m_pMutex);
    const xbool running = m_bRunning;
    SDL_UnlockMutex(m_pMutex);
    return running;
}

//==============================================================================

xbool sync_clock::IsPaused(void) const
{
    if (!m_pMutex)
        return FALSE;

    SDL_LockMutex(m_pMutex);
    const xbool paused = m_bPaused;
    SDL_UnlockMutex(m_pMutex);
    return paused;
}

//==============================================================================
