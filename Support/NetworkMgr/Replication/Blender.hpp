//==============================================================================
//
//  Blender.hpp
//
//==============================================================================

#ifndef BLENDER_HPP
#define BLENDER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_plus.hpp"
#include "x_math.hpp"
#include "x_log.hpp"

//==============================================================================
//  TYPES
//==============================================================================

class blender
{
    f32     m_DeltaToRestart;
    f32     m_DeltaToPop;
    f32     m_Current;
    f32     m_Target;
    f32     m_RemainingSeconds;
    f32     m_BlendDurationSeconds;
    xbool   m_IsAngular;
    xbool   m_Log;

public:
    blender()
    {
        m_DeltaToRestart = 0.0f;
        m_DeltaToPop     = 1.0f;
        m_Current        = 0.0f;
        m_Target         = 0.0f;
        m_RemainingSeconds     = 0.0f;
        m_BlendDurationSeconds = 6.0f / 60.0f;
        m_IsAngular      = FALSE;
        m_Log            = FALSE;
    }

    f32 GetValue( void ) const
    {
        return( m_Current );
    }

    void Init( f32 DeltaToRestart, f32 DeltaToPop, f32 BlendDurationSeconds, xbool IsAngular )
    {
        m_DeltaToRestart = DeltaToRestart;
        m_DeltaToPop     = DeltaToPop;
        m_BlendDurationSeconds = MAX( 0.0f, BlendDurationSeconds );
        m_IsAngular      = IsAngular;
    }

    void SetBlendDuration( f32 BlendDurationSeconds )
    {
        m_BlendDurationSeconds = MAX( 0.0f, BlendDurationSeconds );
    }

    xbool SetTarget( f32 Target )
    {
        #if defined(X_LOGGING) && !defined(X_SUPPRESS_LOGS)
        f32 Old      = m_Target;
        #endif
        f32 Delta    = Target - m_Target;
        f32 AbsDelta = (Delta >= 0.0f) ? Delta : -Delta;
        xbool Popped = FALSE;

        // Check for wrapping on angles
        if( m_IsAngular )
        {
            if( AbsDelta > PI )
            {
                if( Delta >= 0.0f )
                {
                    m_Target  += PI*2;
                    m_Current += PI*2;
                }
                else
                {
                    m_Target  -= PI*2;
                    m_Current -= PI*2;
                }

                Delta    = Target - m_Target;
                AbsDelta = (Delta > 0.0f) ? Delta : -Delta;
            }
        }

        // Accept a small correction without restarting an in-flight blend.
        if( AbsDelta < m_DeltaToRestart )
        {
            m_Target = Target;
            if( m_RemainingSeconds <= 0.0f )
                m_RemainingSeconds = MIN( 1.0f / 60.0f, m_BlendDurationSeconds );
        }

        // Accept the new target and restart the correction window.
        else if( AbsDelta < m_DeltaToPop )
        {
            m_RemainingSeconds = m_BlendDurationSeconds;
            m_Target           = Target;
        }

        // Just pop to the new location and reset the number of blend frames
        else
        {
            Teleport( Target );
            Popped = TRUE;
        }

#if defined(X_LOGGING) && !defined(X_SUPPRESS_LOGS)
        CLOG_MESSAGE( m_Log, "blender::SetTarget",
                      "%p - Value:%d - Target:%d - RemainingMs:%d - PreviousTarget:%d",
                      (const void*)this, (s32)m_Current, (s32)m_Target,
                      (s32)(m_RemainingSeconds * 1000.0f), (s32)Old );
#endif

        return Popped;
    }

    void Teleport( f32 Target )
    {
        #if defined(X_LOGGING) && !defined(X_SUPPRESS_LOGS)
        f32 Old   = m_Target;
        #endif
        m_RemainingSeconds = 0.0f;
        m_Target           = Target;
        m_Current          = Target;
#if defined(X_LOGGING) && !defined(X_SUPPRESS_LOGS)
        CLOG_MESSAGE( m_Log, "blender::Teleport",
                      "%p - Value:%d - Target:%d - RemainingMs:%d - PreviousTarget:%d",
                      (const void*)this, (s32)m_Current, (s32)m_Target,
                      (s32)(m_RemainingSeconds * 1000.0f), (s32)Old );
#endif
    }       

    f32 Advance( f32 DeltaSeconds )
    {
        ASSERT( DeltaSeconds >= 0.0f );

        if( m_RemainingSeconds <= 0.0f )
        {
            m_Current = m_Target;
            return m_Current;
        }

        if( DeltaSeconds <= 0.0f )
            return m_Current;

        #if defined(X_LOGGING) && !defined(X_SUPPRESS_LOGS)
        const f32 Previous = m_Current;
        #endif
        if( DeltaSeconds >= m_RemainingSeconds )
        {
            m_Current          = m_Target;
            m_RemainingSeconds = 0.0f;
        }
        else
        {
            const f32 Alpha = DeltaSeconds / m_RemainingSeconds;
            m_Current          += (m_Target - m_Current) * Alpha;
            m_RemainingSeconds -= DeltaSeconds;
        }

        #if defined(X_LOGGING) && !defined(X_SUPPRESS_LOGS)
        CLOG_MESSAGE( m_Log, "blender::Advance",
                      "%p - Value:%d - Target:%d - RemainingMs:%d - PreviousValue:%d",
                      (const void*)this, (s32)m_Current, (s32)m_Target,
                      (s32)(m_RemainingSeconds * 1000.0f), (s32)Previous );
        #endif

        return( m_Current );
    }
};

//==============================================================================
#endif // BLENDER_HPP
//==============================================================================
