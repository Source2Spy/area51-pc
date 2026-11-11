//==============================================================================
//
//  audio_element_channel_runtime.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_element_channel_runtime.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_helpers.hpp"
#include "Audio/audio_spatial_mgr.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "x_log.hpp"

//==============================================================================
//  DEBUG
//==============================================================================

#if defined(rbrannon)
extern element* g_DebugElement;
#endif

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_element_channel_runtime::audio_element_channel_runtime( void )
{
    m_pRuntime = NULL;
}

//==============================================================================

audio_element_channel_runtime::~audio_element_channel_runtime( void )
{
}

//==============================================================================

void audio_element_channel_runtime::Init( audio_runtime& Runtime )
{
    m_pRuntime = &Runtime;
}

//==============================================================================

void audio_element_channel_runtime::Kill( void )
{
    m_pRuntime = NULL;
}

//==============================================================================

xbool audio_element_channel_runtime::ReleaseChannel( audio_voice_mgr& Voices, element* pElement )
{
    ASSERT( Voices.IsValidElement( pElement ) );

#if defined(rbrannon)
    if( pElement == g_DebugElement )
    {
        LOG_WARNING( "AudioDebug(audio_voice_mgr::ReleaseElement)",
                        "Freed g_DebugElement!" );
        LOG_FLUSH();
    }
#endif

    // Acquire the audio hardware.

    // Give up the hardware resource.
    if( !Runtime().Backend.ReleaseChannel( pElement->pChannel ) )
    {
        return FALSE;
    }

    // Now remove it from the channel list damnit.
    RemoveChannelFromList( pElement->pChannel );
    InsertChannelIntoList( pElement->pChannel, Runtime().Channels.FreeList() );

    // Release it.
    return TRUE;
}

//==============================================================================

void audio_element_channel_runtime::PauseElement( audio_voice_mgr& Voices, element* pElement )
{
    // Error check.
    ASSERT( Voices.IsValidElement( pElement ) );

    // Stop the channel!
    if( Runtime().Backend.IsValidChannel( pElement->pChannel ) && (pElement->pChannel->State == STATE_RUNNING) )
    {
        // Pause it!
        Runtime().Channels.Pause( pElement->pChannel );

        // Its not playing.
        pElement->State = ELEMENT_PAUSED;
    }
}

//==============================================================================

void audio_element_channel_runtime::ResumeElement( audio_voice_mgr& Voices, element* pElement )
{
    // Error check.
    ASSERT( Voices.IsValidElement( pElement ) );

    // Stop the channel!
    if( Runtime().Backend.IsValidChannel( pElement->pChannel ) && (pElement->pChannel->State == STATE_PAUSED) )
    {
        Runtime().Channels.Resume( pElement->pChannel );

        // Its playing.
        pElement->State = ELEMENT_PLAYING;
    }
}

//==============================================================================

void audio_element_channel_runtime::ApplyElementVolume( audio_voice_mgr& Voices, element* pElement )
{
    // Error check.
    ASSERT( Voices.IsValidElement( pElement ) );
    ASSERT( Runtime().Backend.IsValidChannel( pElement->pChannel ) );

    // Set the volume.
    Runtime().Channels.SetVolume( pElement->pChannel, pElement->Volume );
}

//==============================================================================

void audio_element_channel_runtime::ApplyElementPan( audio_voice_mgr& Voices, element* pElement )
{
    // Error check.
    ASSERT( Voices.IsValidElement( pElement ) );
    ASSERT( Runtime().Backend.IsValidChannel( pElement->pChannel ) );
    ASSERT( pElement->pVoice );

    // If its not positional, then use the stereo pan?
    if( !pElement->pVoice->IsPositional )
    {
        Runtime().Spatial.Calculate2dPan( pElement->Params.Pan2d, pElement->Params.Pan3d );
    }

    // Set the actual pan now.
    Runtime().Channels.SetPan( pElement->pChannel, pElement->Params.Pan3d );
}

//==============================================================================

void audio_element_channel_runtime::ApplyElementPitch( audio_voice_mgr& Voices, element* pElement )
{
    // Error check.
    ASSERT( Voices.IsValidElement( pElement ) );
    ASSERT( Runtime().Backend.IsValidChannel( pElement->pChannel ) );

    // Set the pitch.
    Runtime().Channels.SetPitch( pElement->pChannel, pElement->Pitch );
}

//==============================================================================

void audio_element_channel_runtime::ApplyElementEffectSend( audio_voice_mgr& Voices, element* pElement )
{
    // Error check.
    ASSERT( Voices.IsValidElement( pElement ) );
    ASSERT( Runtime().Backend.IsValidChannel( pElement->pChannel ) );

    // Set the effect send.
    Runtime().Channels.SetEffectSend( pElement->pChannel, pElement->EffectSend );
}

//==============================================================================

void audio_element_channel_runtime::StartElement( audio_voice_mgr& Voices, element* pElement )
{
    channel* pChannel = pElement->pChannel;

    // Error check.
    ASSERT( Voices.IsValidElement( pElement ) );
    ASSERT( Runtime().Backend.IsValidChannel( pChannel ) );

    // Calculate volume, pitch and effect send.
    pElement->Volume     = CalculateElementVolume( pElement, Runtime().AudioDuckLevel > 0 );
    pElement->Pitch      = CalculateElementPitch( pElement );
    pElement->EffectSend = CalculateElementEffectSend( pElement );

    // If its not positional, then use the stereo pan.
    if( !pElement->pVoice->IsPositional )
    {
        Runtime().Spatial.Calculate2dPan( pElement->Params.Pan2d, pElement->Params.Pan3d );
    }

    // Set the channel parameters.
    Runtime().Channels.SetVolume( pChannel, pElement->Volume );
    Runtime().Channels.SetPitch( pChannel, pElement->Pitch );
    Runtime().Channels.SetPan( pChannel, pElement->Params.Pan3d );
    Runtime().Channels.SetEffectSend( pChannel, pElement->EffectSend );

    // Start up the sound!
    Runtime().Channels.Start( pChannel );

    // Its playing now!
    pElement->State = ELEMENT_PLAYING;
}

//==============================================================================
