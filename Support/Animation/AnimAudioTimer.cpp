//=========================================================================
//
//  AnimAudioTimer.cpp
//
//  A simple class to smoothly predict the current elapsed time of a voice
//
//=========================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "AnimAudioTimer.hpp"
#include "AnimPlayer.hpp"
#include "Obj_mgr/obj_mgr.hpp"


//==============================================================================
//  DEFINES
//==============================================================================

// Time error defines
static f32 MAX_ERROR_TIME = 2.5f / 30.0f;   // Max error between predicted audio and anim time
static f32 MAX_ERROR_FIX  = 1.0f / 3.0f;    // Max error fix for catching up to audio time


//==============================================================================
//  FUNCTIONS
//==============================================================================

 anim_audio_timer::anim_audio_timer()
 {
    m_VoiceID               = 0;            // Audio manager voice ID
    m_CameraGuid            = NULL_GUID;    // Camera guid object (or NULL_GUID if none)
    m_iCameraBone           = -1;           // Camera bone (or -1 if none)
    m_AudioPrevTime         = 0.0f;         // Hardware voice elapsed time
    m_AudioCurrTime         = 0.0f;         // Hardware voice elapsed time
    m_AudioPredictedTime    = 0.0f;         // Smoothed out audio elapsed time
    m_AnimPredictedTime     = 0.0f;         // Smoothed out anim elapsed time
    m_Time                  = 0.0f;         // Final cinema time
 }
 
//==============================================================================

// Starts timer using audio
void anim_audio_timer::Start( s32 VoiceID, guid CameraGuid, s32 iCameraBone )
{
    m_VoiceID               = VoiceID;      // Audio manager voice ID
    m_CameraGuid            = CameraGuid;   // Camera guid object (or NULL_GUID if none)
    m_iCameraBone           = iCameraBone;  // Camera bone (or -1 if none)
    m_AudioPrevTime         = 0.0f;         // Hardware voice elapsed time from last frame
    m_AudioCurrTime         = 0.0f;         // Hardware voice elapsed time
    m_AudioPredictedTime    = 0.0f;         // Smoothed out audio elapsed time
    m_AnimPredictedTime     = 0.0f;         // Smoothed out anim elapsed time
    m_Time                  = 0.0f;         // Final cinema time
}

//==============================================================================

// Stops timer
void anim_audio_timer::Stop( void )
{
    // Clear voice ID
    m_VoiceID = 0;
}

//==============================================================================

// Returns TRUE if the voice is valid
xbool anim_audio_timer::IsVoiceValid( void )
{
    xbool bIsValid = g_AudioMgr.IsValidVoiceIdCached( m_VoiceID );
    return bIsValid;
}

//==============================================================================

// Returns TRUE if the voice has warmed up and is playing
xbool anim_audio_timer::IsPlaying( void )
{
    xbool bIsValid      = g_AudioMgr.IsValidVoiceIdCached( m_VoiceID );
    xbool bAudioRunning = ( m_AudioCurrTime > 0.0f );
    return( bIsValid && bAudioRunning );
}

//==============================================================================

// Returns camera local to world (if there is one) for specified time
xbool anim_audio_timer::IsCameraCut( f32 Time )
{
    if( m_iCameraBone == -1 )
        return FALSE;

    object* pCamera = g_ObjMgr.GetObjectByGuid( m_CameraGuid );
    if( !pCamera )
        return FALSE;
    simple_anim_player* pAnimPlayer = pCamera->GetSimpleAnimPlayer();
    if( !pAnimPlayer || (pAnimPlayer->GetAnimIndex() == -1) )
        return FALSE;

    const anim_info& AnimInfo = pAnimPlayer->GetAnimInfo();
    s32 const Frame = (s32)( AnimInfo.GetFPS() * Time );
    if( (Frame < 0) || (Frame >= AnimInfo.GetNFrames()-1) )
        return FALSE;
    return AnimInfo.IsCameraCut( m_iCameraBone, Frame, Frame+1 );
}

//==============================================================================

// Updates the predicted time
void anim_audio_timer::Advance( f32 DeltaTime )
{
    // Paused?
    if( DeltaTime == 0.0f )
        return;
        
    // Read sound position first incase voice becomes invalid
    // (also truncate to nearest animation frame for 30FPS playback)
    m_AudioCurrTime = g_AudioMgr.GetCurrentPlayTimeCached( m_VoiceID );

    // Is sound playing?        
    if( g_AudioMgr.IsValidVoiceIdCached( m_VoiceID ) )
    {
        // Read hardware value and make sure it's valid
/* // Removed this assert - rbrannon
        ASSERTS( ( ( m_AudioCurrTime == 0.0f ) || 
                   ( m_AudioCurrTime >= m_AudioPrevTime ) ), 
                  xfs("Audio Stream: %0.5f %0.5f", m_AudioPrevTime, m_AudioCurrTime ) );
*/

        // Is audio playing yet?
        if( m_AudioCurrTime > 0.0f )
        {        
            const xbool IsFirstAudioSample = (m_AudioPrevTime <= 0.0f);

            // Compute hardware delta time
            f32 AudioDeltaTime = m_AudioCurrTime - m_AudioPrevTime;
            m_AudioPrevTime = m_AudioCurrTime;

            // Compute smoothed out audio time
            if( AudioDeltaTime != 0.0f )
                m_AudioPredictedTime = m_AudioCurrTime;    // Audio thread updated
            else
                m_AudioPredictedTime += DeltaTime;          // Audio thread has blocked so predict using frame dt

            // Compute smoothed out anim time using frame dt
            m_AnimPredictedTime += DeltaTime;     
            
            // If this is the first time, sync up the anim exactly
            if( IsFirstAudioSample )
                m_AnimPredictedTime = m_AudioCurrTime;

            // Compute error of predicted anim time from predicted audio time
            f32 Error = m_AudioPredictedTime - m_AnimPredictedTime;

            // More than "x" frames out?
            if( x_abs( Error ) > MAX_ERROR_TIME )
            {
                // Limit error so it's applied over several frames to avoid pops
                f32 MaxError = DeltaTime * MAX_ERROR_FIX;
                Error = x_clamp( Error, -MaxError, MaxError );

                // Apply error adjustment
                m_AnimPredictedTime += Error;
            }
        }            
    }
    
    // Keep for later
    f32 PreviousTime = m_Time;
    
    // Start with using predicted animation time
    m_Time = m_AnimPredictedTime;

    xbool const bSnap = IsCameraCut( m_Time );

    // Turn off interpolation?
    if( bSnap )
    {
        // Truncate to next animation frame
        m_Time += 1.0f / 30.0f;
        m_Time = x_floor( m_Time * 30.0f ) / 30.0f;
        
#ifdef X_EDITOR
        // Flash text
        x_printfxy( 10,10, "******** CAMERA CUT ********" );
#endif        
    }
    
    // Do not let the time go backwards otherwise events will not fire off
    if( m_Time < PreviousTime )
        m_Time = PreviousTime;
}

//==============================================================================
