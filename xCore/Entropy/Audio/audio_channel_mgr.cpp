//==============================================================================
//
//  audio_channel_mgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_package.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/audio_voice_mgr.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_package.hpp"
#include "Audio/audio_helpers.hpp"
#include "e_ScratchMem.hpp"
#include "x_log.hpp"

//==============================================================================

static xbool s_IsInitialized = FALSE;        // Sentinel

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_channel_mgr::audio_channel_mgr( void )
{
    m_pRuntime = NULL;
}

//==============================================================================

audio_channel_mgr::~audio_channel_mgr( void )
{
}
        
//==============================================================================

void audio_channel_mgr::Init( audio_runtime& AudioRuntime )
{
    s32 i;
    s32 n;
    channel* pChannel;
    channel* pPrev;

    // Error check.
    ASSERT( s_IsInitialized == FALSE );
    m_pRuntime = &AudioRuntime;

    // Init previous.
    pPrev = FreeList();

    // Get first channel and number of channels.
    pChannel = Runtime().Backend.GetChannelBuffer();
    n        = Runtime().Backend.NumChannels();

    // For each hardware channel...
    for( i=0 ; i<n ; i++, pChannel++ )
    {
        // Put link it.
        pChannel->Link.pPrev = pPrev;
        pChannel->Link.pNext = (pChannel+1);

        // Update previous.
        pPrev = pChannel;
    }

    // Back up one...
    pChannel = pPrev;

    // Last one is tail of free list.
    pChannel->Link.pNext = FreeList();

    // Initialize the free list.
    FreeList()->Link.pPrev = pChannel;
    FreeList()->Link.pNext = Runtime().Backend.GetChannelBuffer();

    // Initialize used list.
    UsedList()->Link.pPrev =
    UsedList()->Link.pNext = UsedList();

    // Head/Tail is now lowest priority.
    UsedList()->Priority = -1;

    // It's initialized!
    s_IsInitialized = TRUE;
}

//==============================================================================

void audio_channel_mgr::Kill( void )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Not initialized anymore...
    s_IsInitialized = FALSE;
    m_pRuntime = NULL;
}

//==============================================================================

xbool DEBUG_ACQUIRE_CHANNEL_FAIL = 0;

//==============================================================================

xbool audio_channel_mgr::Acquire( element* pElement )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_channel_mgr::Acquire" );

    channel* pResult;
    channel* pHead;
    channel* pChannel;

    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Voices.IsValidElement(pElement) );

    // Get head/tail of free list and first free channel.
    pHead    = FreeList();
    pChannel = pHead->Link.pNext;

    // Is free list empty?
    if( pChannel == pHead )
    {
        // Get oldest, lowest priority channel.
        pChannel = UsedList()->Link.pPrev;

        // Which channel is more important?
        if( (pChannel->Priority < pElement->Params.Priority) )
//        || ((pChannel->Priority == pElement->Params.Priority) && (pChannel->Volume <= pElement->Params.Volume)) ) 
        {
#if defined(rbrannon)
            extern channel* g_DebugChannel;
            if( pChannel == g_DebugChannel )
            {
                LOG_WARNING( "AudioDebug(audio_channel_mgr::Acquire)",
                             "Freed g_DebugChannel!" );
                LOG_FLUSH();
            }
#endif // defined(rbrannon)

            if( Free( pChannel, FALSE, TRUE ) )
            {
                // Get outta my way!  I'm more important!
                pResult = pChannel;
            }
            else
            {
                pResult = NULL;
            }
        }
        else
        {
            // Cannot acquire a channel...
            pResult = NULL;
        }
    }
    else
    {
        // Take it out of the free list.
        RemoveChannelFromList( pChannel );

        // Use this one!
        pResult = pChannel;
    }

    // Was a channel acquired?
    if( pResult )
    {
        // Init state and dirty bits.
        pResult->State = STATE_NOT_STARTED;
        pResult->Dirty = 0;

        // Init the stream.
        pResult->StreamData.pStream = NULL;

        // Set pointer to parent voice element.
        pResult->pElement           = pElement;

        // Clear the release position.
        pResult->ReleasePosition    = 0;

        // Inherit data from the voice element.
        pResult->Priority   = pElement->Params.Priority;
        pResult->Type       = pElement->Type;
        pResult->Sample     = pElement->Sample;
        pResult->Volume     = pElement->Volume; 
        pResult->Pitch      = pElement->Pitch;
        pResult->EffectSend = pElement->EffectSend;
        pResult->Pan2d      = pElement->Params.Pan2d;
        pResult->Pan3d      = pElement->Params.Pan3d;

        // Insert it into the used list based on the priority/volume.
        UpdatePriorityList( pResult, FALSE );

        // Acquire the hardware channel, if we can...
        if( Runtime().Backend.AcquireChannel( pResult ) )
        {
            // Can initialize hot samples...
            if( pElement->Type == HOT_SAMPLE )
            {
                // Now initilize the hardware channel.
                Runtime().Backend.InitChannel( pResult );
            }

            // Set elements channel.
            pElement->pChannel = pResult;

        }
        // DOH! Could not acquire a hardware channel...
        else
        {
            // Take it out of the used list.
            RemoveChannelFromList( pResult );

             // Put channel back in free list.
            InsertChannelIntoList( pResult, FreeList() );

            // Too bad...so sad...
            pResult = NULL;
        }
    }

    #if !defined(X_RETAIL) || defined(X_QA)
    if( pResult == NULL && DEBUG_ACQUIRE_CHANNEL_FAIL )
    {
        if( pElement && pElement->pVoice )
        {
            x_DebugMsg( 7, "%s Failed to acquire hardware channel!", pElement->pVoice->pDescriptorName );
        }
    }
#endif // !defined(X_RETAIL)

    // Tell the world.
    return( pResult != NULL );
}

//==============================================================================

xbool audio_channel_mgr::Release( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Free the channel, put it into the free channel list, Don't nuke the element.
    return Free( pChannel, TRUE, FALSE );
}
        
//==============================================================================

void audio_channel_mgr::Start( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Set state to starting.
    pChannel->State = STATE_STARTING;

}

//==============================================================================

void audio_channel_mgr::Pause( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Set the state to stopping
    pChannel->State = STATE_PAUSING;
}

//==============================================================================

void audio_channel_mgr::Resume( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Set the state to stopping.
    pChannel->State = STATE_RESUMING;
}

//==============================================================================

xbool audio_channel_mgr::IsPlaying( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    return Runtime().Backend.IsChannelActive( pChannel );
}

//==============================================================================

s32 audio_channel_mgr::GetPriority( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    return( pChannel->Priority );
}

//==============================================================================

void audio_channel_mgr::SetPriority( channel* pChannel, s32 Priority ) 
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );
    ASSERT( (Priority >= 0) && (Priority <= 255) );

    // Set the channels priority.
    pChannel->Priority = Priority;

    // Update the channels position in the used list.
    UpdatePriorityList( pChannel, TRUE );
    
}

//==============================================================================

f32 audio_channel_mgr::GetVolume( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    return( pChannel->Volume );
}

//==============================================================================

void audio_channel_mgr::SetVolume( channel* pChannel, f32 Volume )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Limit the volume.
    if( Volume < 0.0f )
    {
        Volume = 0.0f;

        // TODO: Issue warning.
    }

    if( Volume > 1.0f )
    {
        Volume = 1.0f;

        // TODO: Issue warning.
    }

    // Set the volume.
    pChannel->Volume = Volume;

    // Update the channels position in the used list.
    UpdatePriorityList( pChannel, TRUE );
    
    // Set the dirty bit.
    pChannel->Dirty |= CHANNEL_DB_VOLUME;

}

//==============================================================================

void audio_channel_mgr::GetPan( channel* pChannel, vector4& Pan )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    Pan = pChannel->Pan3d;
}

//==============================================================================

void audio_channel_mgr::SetPan( channel* pChannel, vector4& Pan )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Set the pan
    pChannel->Pan3d = Pan;

    // Set the dirty bit
    pChannel->Dirty |= CHANNEL_DB_PAN;
}

//==============================================================================

f32 audio_channel_mgr::GetPitch( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    return( pChannel->Pitch );
}

//==============================================================================

void audio_channel_mgr::SetPitch( channel* pChannel, f32 Pitch )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    if( Pitch > 2.0f )
    {
        Pitch = 2.0f;
        // TODO: Put in warning.
    }
    else if( Pitch < 1.0f/64.0f )
    {
        Pitch = 1.0f/64.0f;
        // TODO: Put in warning.
    }

    // Set the pitch
    pChannel->Pitch = Pitch;

    // Set the dirty bit
    pChannel->Dirty |= CHANNEL_DB_PITCH;
}

//==============================================================================

f32 audio_channel_mgr::GetEffectSend( channel* pChannel )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    return( pChannel->EffectSend );
}

//==============================================================================

void audio_channel_mgr::SetEffectSend( channel* pChannel, f32 EffectSend )
{
    // Error check.
    ASSERT( s_IsInitialized );
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Limit the effect send.
    if( EffectSend < 0.0f )
    {
        EffectSend = 0.0f;

        // TODO: Issue warning.
    }

    if( EffectSend > 1.0f )
    {
        EffectSend = 1.0f;

        // TODO: Issue warning.
    }

    // Set the volume.
    pChannel->EffectSend = EffectSend;

    // Set the dirty bit.
    pChannel->Dirty |= CHANNEL_DB_EFFECTSEND;
}

//==============================================================================

void audio_channel_mgr::Update( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_channel_mgr::Update" );

    channel* pChannel;
    channel* pHead;

    {
        X_PROFILE_SCOPE_CATEGORY( "Context", "audio_channel_mgr::Update" );
        // Get head/tail of used channel list and first member.
        pHead    = UsedList();
        pChannel = pHead->Link.pNext;

        // For each channel...
        while( pChannel != pHead )
        {
            channel* pNext;

            // Error check.
            ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

            // Get next used channel.
            pNext = pChannel->Link.pNext;

            // Was the hardware channel lost?
            if( !Runtime().Backend.IsChannelActive( pChannel) )
            {
                element* pElement;
                element* pStereo=NULL;

                // Get the channels element.
                pElement = pChannel->pElement;
            
                // Get the elements stereo partner.
                if( pElement )
                {
                    pStereo = pElement->pStereoElement;
                }

                // Free the channel, put it in the free channel list, nuke the element as well.
                (void)Free( pChannel, TRUE, TRUE );

                // Did channel have an element?
                if( pElement )
                {
                    // Was the element stereo?
                    ASSERT( Runtime().Voices.IsValidElement(pElement) );
                    if( pStereo )
                    {
                        // Get the stereo channel.
                        ASSERT( Runtime().Voices.IsValidElement(pStereo) );
                        pChannel = pStereo->pChannel;

                        // Only if the stereo channel is valid...
                        if( pChannel )
                        {
                            // Error check.
                            ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

                            // Are we about to nuke the next?
                            if( pChannel == pNext )
                            {
                                // Keep walking...
                                pNext = pChannel->Link.pNext;
                            }

                            // Stereo channel active?
                            if( Runtime().Backend.IsChannelActive( pChannel) )
                            {
                                // Free the stereo channel, put it in the free channel list, nuke the element as well.
                                (void)Free( pChannel, TRUE, TRUE );
                            }
                        }
                    }
                }

            }

            // Next!
            pChannel = pNext;
        }
    }

}

//==============================================================================

xbool audio_channel_mgr::Free( channel* pChannel, xbool PutInFreeList, xbool FreeParent )
{
    element* pElement;

    // Error check.
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Nuke the hardware channel.
    
    ASSERT(pChannel->pElement->Type != (element_type)-1);
    if( !Runtime().Backend.ReleaseChannel( pChannel ) )
    {
        return FALSE;
    }

    // Get the channels element.
    pElement = pChannel->pElement;
    ASSERT(pElement->Type != (element_type)-1);

    // Now remove it from the used list.
    RemoveChannelFromList( pChannel );

    // Put it into the free channel list?
    if( PutInFreeList )
    {
        // Put the channel into freelist.
        InsertChannelIntoList( pChannel, FreeList() );

        // Clear the channels element (it doesn't need this in the free list...).
        pChannel->pElement = NULL;
    }

    // Element valid?
    if( FreeParent && pElement )
    {
        // Release the element, don't recurse and release the channel..hehe...
        ASSERT( Runtime().Voices.IsValidElement(pElement) );
        (void)Runtime().Voices.ReleaseElement( pElement, FALSE );
    }

    return TRUE;
}

//==============================================================================

void audio_channel_mgr::UpdatePriorityList( channel* pChannel, xbool RemoveFromList )
{
    channel* pInsert;

    // Error check.
    ASSERT( Runtime().Backend.IsValidChannel(pChannel) );

    // Remove the channel from the used list?
    if( RemoveFromList )
    {
        RemoveChannelFromList( pChannel );
    }

    // Get first used channel.
    pInsert = UsedList()->Link.pNext;

    // Find the insertion point (based on priority only).
    while( pInsert->Priority > pChannel->Priority )
    {
        pInsert = pInsert->Link.pNext;
    }

    // Volume is secondary key.
    while( (pInsert->Priority == pChannel->Priority) && (pInsert->Volume > pChannel->Volume) )
    {
        pInsert = pInsert->Link.pNext;
    }

    // Insert it into the used list.
    InsertChannelIntoList( pChannel, pInsert );

}
