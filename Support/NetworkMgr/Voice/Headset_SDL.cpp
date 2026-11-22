//==============================================================================
//
//  Headset_SDL.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Headset.hpp"
#include "x_debug.hpp"

#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_init.h"

#ifdef TARGET_ANDROID
#include "x_atomic.hpp"
#include "SDL3/SDL_system.h"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

static const f32 DEVICE_RETRY_TIME   = 2.0f;
static const s32 PCM_FRAME_BYTES    = voice_codec::SAMPLES_PER_FRAME * sizeof( s16 );
static const s32 PLAYBACK_MAX_BYTES = voice_update::MAX_FRAMES * PCM_FRAME_BYTES;
static const s32 CAPTURE_MAX_BYTES  = 2 * PLAYBACK_MAX_BYTES;

//==============================================================================
//  TYPES
//==============================================================================

#ifdef TARGET_ANDROID
enum microphone_permission
{
    MIC_PERMISSION_UNKNOWN,
    MIC_PERMISSION_PENDING,
    MIC_PERMISSION_DENIED,
    MIC_PERMISSION_GRANTED,
    MIC_PERMISSION_RECHECK,
};

//==============================================================================
//  VARIABLES
//==============================================================================

// The Android callback can outlive the headset session.
static x_atomic_s32 s_MicrophonePermission = X_ATOMIC_S32_INIT( MIC_PERMISSION_UNKNOWN );

//==============================================================================
//  MICROPHONE PERMISSION
//==============================================================================

static 
void SDLCALL OnMicrophonePermission( void*, const char*, bool Granted )
{
    x_AtomicStoreRelaxed( &s_MicrophonePermission, Granted ? MIC_PERMISSION_GRANTED : MIC_PERMISSION_DENIED );
}

//==============================================================================

static 
void ResetMicrophoneRequest( void )
{
    s32 const Permission = x_AtomicLoadRelaxed( &s_MicrophonePermission );
    if( Permission == MIC_PERMISSION_GRANTED )
    {
        x_AtomicStoreRelaxed( &s_MicrophonePermission, MIC_PERMISSION_RECHECK );
    }
    else if( Permission == MIC_PERMISSION_DENIED )
    {
        x_AtomicStoreRelaxed( &s_MicrophonePermission, MIC_PERMISSION_UNKNOWN );
    }
}

//==============================================================================

static 
xbool CanOpenMicrophone( void )
{
    s32 Permission = x_AtomicLoadRelaxed( &s_MicrophonePermission );
    if( (Permission == MIC_PERMISSION_UNKNOWN) || (Permission == MIC_PERMISSION_RECHECK) )
    {
        // SDL's Android drivers wait for the permission response during open.
        // Request it here first so the game keeps running while the dialog is up.
        x_AtomicStoreRelaxed( &s_MicrophonePermission, MIC_PERMISSION_PENDING );
        if( !SDL_RequestAndroidPermission( "android.permission.RECORD_AUDIO", OnMicrophonePermission, NULL ) )
        {
            x_DebugMsg( "CanOpenMicrophone: Unable to request microphone permission: %s\n", SDL_GetError() );
            x_AtomicStoreRelaxed( &s_MicrophonePermission, MIC_PERMISSION_DENIED );
            return FALSE;
        }

        Permission = x_AtomicLoadRelaxed( &s_MicrophonePermission );
    }

    return Permission == MIC_PERMISSION_GRANTED;
}
#endif // TARGET_ANDROID

//==============================================================================
//  AUDIO STREAMS
//==============================================================================

static 
void SDLCALL LimitCaptureQueue( void*, SDL_AudioStream* pStream, s32, s32 )
{
    // A stalled game thread must not leave a backlog of old microphone audio.
    if( SDL_GetAudioStreamAvailable( pStream ) > CAPTURE_MAX_BYTES )
    {
        SDL_ClearAudioStream( pStream );
    }
}

//==============================================================================

static 
xbool HasAudioDevice( xbool Recording, xbool WasAvailable )
{
    s32 Count;
    SDL_AudioDeviceID* pDevices = Recording ? SDL_GetAudioRecordingDevices( &Count )
                                            : SDL_GetAudioPlaybackDevices( &Count );
    if( !pDevices )
    {
        // An enumeration error is not a device removal.
        return WasAvailable;
    }

    SDL_free( pDevices );
    return Count > 0;
}

//==============================================================================

static 
SDL_AudioStream* OpenVoiceStream( xbool Recording, f32 Gain )
{
    SDL_AudioSpec Spec;
    Spec.format   = SDL_AUDIO_S16;
    Spec.channels = 1;
    Spec.freq     = voice_codec::SAMPLE_RATE;

    SDL_AudioDeviceID const Device = Recording ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                              : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    SDL_AudioStream* pStream = SDL_OpenAudioDeviceStream( Device, &Spec, Recording ? LimitCaptureQueue : NULL, NULL );
    if( !pStream || !SDL_SetAudioStreamGain( pStream, Gain ) || !SDL_ResumeAudioStreamDevice( pStream ) )
    {
        x_DebugMsg( "OpenVoiceStream: Unable to open voice %s: %s\n",
                    Recording ? "capture" : "playback", SDL_GetError() );
        SDL_DestroyAudioStream( pStream );
        return NULL;
    }

    return pStream;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

xbool headset::InitAudio( void )
{
    if( !SDL_InitSubSystem( SDL_INIT_AUDIO ) )
    {
        x_DebugMsg( "headset::InitAudio: Unable to initialize SDL audio: %s\n", SDL_GetError() );
        return FALSE;
    }

    m_DeviceCheckTime = 0.0f;
    m_CaptureRetry    = 0.0f;
    return TRUE;
}

//==============================================================================

void headset::KillAudio( void )
{
    CloseCapture();
    ClosePlayback();
    m_CaptureAvailable = FALSE;
    SDL_QuitSubSystem( SDL_INIT_AUDIO );
}

//==============================================================================

xbool headset::IsTalking( void ) const
{
    return m_pCapture != NULL;
}

//==============================================================================

xbool headset::IsHardwarePresent( void ) const
{
    if( !m_CaptureAvailable || (m_CaptureRetry > 0.0f) )
    {
        return FALSE;
    }

#ifdef TARGET_ANDROID
    s32 const Permission = x_AtomicLoadRelaxed( &s_MicrophonePermission );
    return (Permission == MIC_PERMISSION_GRANTED) || (Permission == MIC_PERMISSION_RECHECK);
#else
    return TRUE;
#endif
}

//==============================================================================

void headset::UpdateDevices( f32 DeltaTime, xbool Enabled, xbool TalkingRequested )
{
    if( !Enabled || !TalkingRequested )
    {
        if( Enabled && m_pCapture && !ReadCaptureFrames() )
        {
            ClearReadFifo();
        }
        // Pausing a logical stream would not necessarily stop the physical mic.
        CloseCapture();
#ifdef TARGET_ANDROID
        ResetMicrophoneRequest();
#endif
    }

    if( !Enabled )
    {
        ClearReadFifo();
        ClosePlayback();
        m_CaptureAvailable = FALSE;
        m_DeviceCheckTime  = 0.0f;
        m_CaptureRetry     = 0.0f;
        return;
    }

    if( !m_VoiceAudible )
    {
        ClosePlayback();
    }

    m_CaptureRetry    = MAX( 0.0f, m_CaptureRetry - DeltaTime );
    m_DeviceCheckTime -= DeltaTime;
    if( m_DeviceCheckTime > 0.0f )
    {
        return;
    }

    // SDL migrates default streams between devices; handle loss of all devices
    // in a direction and retry streams that failed to open.
    m_DeviceCheckTime  = DEVICE_RETRY_TIME;
    m_CaptureAvailable = HasAudioDevice( TRUE, m_CaptureAvailable );
    if( !m_CaptureAvailable )
    {
        CloseCapture();
        ClearReadFifo();
    }

    if( m_VoiceAudible && HasAudioDevice( FALSE, m_pPlayback != NULL ) )
    {
        if( !m_pPlayback )
        {
            m_pPlayback = OpenVoiceStream( FALSE, m_HeadsetVolume );
        }
    }
    else
    {
        ClosePlayback();
    }
}

//==============================================================================

void headset::UpdateCapture( xbool TalkingRequested )
{
    if( !TalkingRequested || !m_CaptureAvailable || (m_CaptureRetry > 0.0f) )
    {
        return;
    }

    if( !m_pCapture )
    {
#ifdef TARGET_ANDROID
        if( !CanOpenMicrophone() )
        {
            return;
        }
#endif

        if( !m_Codec.ResetEncoder() )
        {
            x_DebugMsg( "headset::UpdateCapture: Unable to reset the Opus encoder.\n" );
            m_CaptureRetry = DEVICE_RETRY_TIME;
            return;
        }

        m_pCapture = OpenVoiceStream( TRUE, m_MicrophoneSensitivity );
        if( !m_pCapture )
        {
            m_CaptureRetry = DEVICE_RETRY_TIME;
            return;
        }
    }

    if( !ReadCaptureFrames() )
    {
        CloseCapture();
        ClearReadFifo();
        m_CaptureRetry = DEVICE_RETRY_TIME;
    }
}

//==============================================================================

xbool headset::ReadCaptureFrames( void )
{
    s16 Samples[voice_codec::SAMPLES_PER_FRAME];
    s32 Available = SDL_GetAudioStreamAvailable( m_pCapture );
    while( Available >= PCM_FRAME_BYTES )
    {
        s32 const Bytes = SDL_GetAudioStreamData( m_pCapture, Samples, PCM_FRAME_BYTES );
        if( Bytes != PCM_FRAME_BYTES )
        {
            // The capture callback may have dropped an overrun since our query.
            Available = Bytes;
            break;
        }

        if( !EncodeCapture( Samples ) )
        {
            return FALSE;
        }

        Available -= PCM_FRAME_BYTES;
    }

    if( Available < 0 )
    {
        x_DebugMsg( "headset::ReadCaptureFrames: Unable to read voice capture: %s\n", SDL_GetError() );
        return FALSE;
    }
    return TRUE;
}

//==============================================================================

void headset::UpdatePlayback( void )
{
    if( !m_pPlayback )
    {
        return;
    }

    s32 Queued = SDL_GetAudioStreamQueued( m_pPlayback );
    s16 Samples[voice_codec::SAMPLES_PER_FRAME];
    while( IN_RANGE( 0, Queued, PLAYBACK_MAX_BYTES - PCM_FRAME_BYTES ) && ReadPlayback( Samples ) )
    {
        if( !SDL_PutAudioStreamData( m_pPlayback, Samples, PCM_FRAME_BYTES ) )
        {
            Queued = -1;
            break;
        }
        Queued += PCM_FRAME_BYTES;
    }

    if( Queued < 0 )
    {
        x_DebugMsg( "headset::UpdatePlayback: Unable to queue voice playback: %s\n", SDL_GetError() );
        ClosePlayback();
    }
}

//==============================================================================

void headset::CloseCapture( void )
{
    if( m_pCapture )
    {
        SDL_DestroyAudioStream( m_pCapture );
        m_pCapture = NULL;
    }
}

//==============================================================================

void headset::ClosePlayback( void )
{
    if( m_pPlayback )
    {
        SDL_DestroyAudioStream( m_pPlayback );
        m_pPlayback = NULL;
        ClearWriteFifo();
    }
}

//==============================================================================

void headset::ClearPlayback( void )
{
    if( m_pPlayback && !SDL_ClearAudioStream( m_pPlayback ) )
    {
        x_DebugMsg( "headset::ClearPlayback: Unable to clear voice playback: %s\n", SDL_GetError() );
        SDL_DestroyAudioStream( m_pPlayback );
        m_pPlayback = NULL;
    }
}

//==============================================================================

void headset::ApplyAudioGain( void )
{
    if( m_pCapture && !SDL_SetAudioStreamGain( m_pCapture, m_MicrophoneSensitivity ) )
    {
        x_DebugMsg( "headset::ApplyAudioGain: Unable to set microphone gain: %s\n", SDL_GetError() );
    }
    if( m_pPlayback && !SDL_SetAudioStreamGain( m_pPlayback, m_HeadsetVolume ) )
    {
        x_DebugMsg( "headset::ApplyAudioGain: Unable to set voice playback gain: %s\n", SDL_GetError() );
    }
}
