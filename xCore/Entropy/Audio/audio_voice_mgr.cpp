//==============================================================================
//
//  audio_voice_mgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_voice_mgr.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/audio_channel_mgr.hpp"
#include "Audio/backend/audio_backend.hpp"
#include "Audio/audio_types.hpp"
#include "Audio/audio_package.hpp"
#include "Audio/audio_helpers.hpp"
#include "Audio/audio_spatial_mgr.hpp"
#include "e_Audio.hpp"
#include "x_log.hpp"
#include "IOManager/io_mgr.hpp"

#if defined(rbrannon)
extern voice*   g_DebugVoice;
extern element* g_DebugElement;
extern channel* g_DebugChannel;
extern f32      g_DebugTime;
#endif

//#define UPDATE_STATE_LOGGING "audio_voice_mgr::Update(state)" 
//#define PLAY_TIME_LOGGING    "audio_voice_mgr::GetCurrentPlayTime"

//==============================================================================
// Defines.

#define MAX_VOICES    (48) //(64)
#define MAX_ELEMENTS  (MAX_VOICES * 3)

static_assert( MAX_VOICES < audio_deferred_voice_table::FIRST_INDEX,
               "deferred voice handles overlap physical voice slots" );

#if defined(X_DEBUG)
#define ELEMENT_EXPIRE_DELAY    (0.250f)
#else
#define ELEMENT_EXPIRE_DELAY    (0.250f)
#endif
//==============================================================================
// Static variables.

static xbool        s_IsInitialized = FALSE;    // Semaphore.
static xbool        s_UpdatePriority;           // Update flag.
static voice        s_Voices[ MAX_VOICES ];     // Voice buffer.
static element      s_Elements[ MAX_ELEMENTS ]; // Element buffer.
static x_atomic_u32 s_PublishedVoiceIDs[MAX_VOICES];
static x_atomic_u32 s_ReadyVoiceIDs[MAX_VOICES];
static x_atomic_u32 s_StateReadyVoiceIDs[MAX_VOICES];
static x_atomic_u64 s_VoiceLengths[MAX_VOICES];
static x_atomic_u64 s_VoicePlayTimes[MAX_VOICES];
static x_atomic_u32 s_VoiceStateRequests[MAX_VOICES];
static x_atomic_u64 s_VoiceVolumes[MAX_VOICES];
static x_atomic_u64 s_VoicePriorities[MAX_VOICES];
static x_atomic_u64 s_VoiceNames[MAX_VOICES][AUDIO_PACKAGE_FILENAME_LENGTH / 4];

//==============================================================================

static
u64 PackVoiceTime( u32 VoiceID, f32 Time )
{
    u32 Bits;
    x_memcpy( &Bits, &Time, sizeof( Bits ) );
    return ( (u64)VoiceID << 32 ) | Bits;
}

//==============================================================================

audio_voice_mgr::audio_voice_mgr( void )
{
    m_FirstVoice   = &s_Voices[0];
    m_LastVoice    = &s_Voices[MAX_VOICES-1];
    m_FirstElement = &s_Elements[0];
    m_LastElement  = &s_Elements[MAX_ELEMENTS-1];
    m_NumVoices    = MAX_VOICES;
    m_NumElements  = MAX_ELEMENTS;
    m_pRuntime     = NULL;
}

//==============================================================================

audio_voice_mgr::~audio_voice_mgr( void )
{
}

//==============================================================================

void audio_voice_mgr::Init( audio_runtime& AudioRuntime )
{
    s32 i;
    voice* pVoice;
    voice* pPrevVoice;
    element* pElement;
    element* pPrevElement;

    // Error check
    ASSERT( s_IsInitialized == FALSE );

    m_pRuntime = &AudioRuntime;
    m_ElementChannels.Init( AudioRuntime );
    m_StreamBinder.Init( AudioRuntime );

    // It's initialized!
    s_IsInitialized = TRUE;

    // Previous is the free list!
    pPrevVoice = FreeVoices();

    // For each voice...
    for( i=0, pVoice=s_Voices ; i<MAX_VOICES ; i++, pVoice++ )
    {
        // Initialize it.
        x_memset( pVoice, 0, sizeof( voice ) );
        pVoice->Sequence = 32000;
        x_AtomicStoreRelease( &s_PublishedVoiceIDs[i], 0 );
        x_AtomicStoreRelease( &s_ReadyVoiceIDs[i], 0 );

        // Link it.
        pVoice->Link.pPrev = pPrevVoice;
        pVoice->Link.pNext = (pVoice+1);

        // Update previous.
        pPrevVoice = pVoice;
    }

    // Back up one...
    pVoice = pPrevVoice;

    // Last one is tail of free list.
    pVoice->Link.pNext = FreeVoices();

    // Initialize the free list.
    FreeVoices()->Link.pPrev = pVoice;
    FreeVoices()->Link.pNext = s_Voices;

    // Empty the used list.
    UsedVoices()->Link.pPrev =
    UsedVoices()->Link.pNext = UsedVoices();

    // Head/Tail now has lowest priority.
    UsedVoices()->Params.Priority = -1;

    // Previus is the free list!
    pPrevElement = FreeElements();

    // For each element...
    for( i=0, pElement=s_Elements ; i<MAX_ELEMENTS ; i++, pElement++ )
    {
        // Initialize it.
        x_memset( pElement, 0, sizeof( element ) );

        // Link it.
        pElement->Link.pPrev = pPrevElement;
        pElement->Link.pNext = (pElement+1);
        pElement->Type = (element_type)-1;

        // Update previous.
        pPrevElement = pElement;
    }

    // Back up one...
    pElement = pPrevElement;

    // Last one is tail of free list.
    pElement->Link.pNext = FreeElements();

    // Initialize the free list.
    FreeElements()->Link.pPrev = pElement;
    FreeElements()->Link.pNext = s_Elements;

}

//==============================================================================

void audio_voice_mgr::Kill( void )
{
    // Error check.
    ASSERT( s_IsInitialized );

    for( s32 i = 0; i < MAX_VOICES; i++ )
    {
        x_AtomicStoreRelease( &s_PublishedVoiceIDs[i], 0 );
        x_AtomicStoreRelease( &s_ReadyVoiceIDs[i], 0 );
    }

    // No longer initialized.
    s_IsInitialized = FALSE;
    m_StreamBinder.Kill();
    m_ElementChannels.Kill();
    m_pRuntime = NULL;

}

//==============================================================================

element* audio_voice_mgr::AcquireElement( void )
{
    element* pHead;
    element* pResult;

    // Get head/tail of free list and first free element.
    pHead   = FreeElements();
    pResult = pHead->Link.pNext;

    // Make sure list is not empty.
    if( pResult != pHead )
    {
        // Take it out of the free list.
        RemoveElementFromList( pResult );

        ASSERT(pResult->Type == (element_type)-1);
        pResult->Type = (element_type)-2;

        // Tell the world.
        return pResult;
    }
    else
    {
        // TODO: Put in warning that an element was ignored.
        return NULL;
    }
}

//==============================================================================

xbool audio_voice_mgr::ReleaseElement( element* pElement, xbool ReleaseChannel )
{
    // Error check.
    ASSERT( IsValidElement( pElement ) );

#if defined(rbrannon)
#endif

    // Release the hardware channel?
    if( ReleaseChannel && pElement->pChannel )
    {
        if( !m_ElementChannels.ReleaseChannel( *this, pElement ) )
        {
            return FALSE;
        }
    }

    // Does the element have a voice?    
    if( pElement->pVoice )
    {
        // Error check.
        ASSERT( IsValidVoice(pElement->pVoice) );

        // Notify the voice that an element has changed.
        pElement->pVoice->Dirty |= VOICE_DB_ELEMENT_CHANGE;
    }

    ASSERT(pElement->Type != (element_type)-1);
    // Take the element out of used list.
    RemoveElementFromList( pElement );

    // Nuke it!
    x_memset( pElement, 0, sizeof(element) );
    pElement->Type = (element_type)-1;

    // Put the element into the free list.
    InsertElementIntoList( pElement, FreeElements() );

    return TRUE;
}

//==============================================================================

inline void audio_voice_mgr::PauseElement( element* pElement )
{
    m_ElementChannels.PauseElement( *this, pElement );
}

//==============================================================================

inline void audio_voice_mgr::ResumeElement( element* pElement )
{
    m_ElementChannels.ResumeElement( *this, pElement );
}

//==============================================================================

inline void audio_voice_mgr::ApplyElementVolume( element* pElement )
{
    m_ElementChannels.ApplyElementVolume( *this, pElement );
}

//==============================================================================

inline void audio_voice_mgr::ApplyElementPan( element* pElement )
{
    m_ElementChannels.ApplyElementPan( *this, pElement );
}

//==============================================================================

inline void audio_voice_mgr::ApplyElementPitch( element* pElement )
{
    m_ElementChannels.ApplyElementPitch( *this, pElement );
}

//==============================================================================

inline void audio_voice_mgr::ApplyElementEffectSend( element* pElement )
{
    m_ElementChannels.ApplyElementEffectSend( *this, pElement );
}

//==============================================================================

inline void audio_voice_mgr::StartElement( element* pElement )
{
    m_ElementChannels.StartElement( *this, pElement );
}

//==============================================================================

extern void AudioDebug( const char* pString );

//==============================================================================

voice* audio_voice_mgr::AcquireVoice( s32 Priority, f32 AbsoluteVolume )
{
    voice* pVoice;

    // Error check.
    ASSERT( s_IsInitialized );

    // Get a voice from the free list.
    pVoice = FreeVoices()->Link.pNext;

    // Free list empty?
    if( pVoice == FreeVoices() )
    {
        // Get oldest, lowest priority voice.
        pVoice = UsedVoices()->Link.pPrev;
        ASSERT( IsValidVoice(pVoice) );

        // Which voice is more important?
        if( (pVoice->Params.Priority <= Priority) /*|| ((pVoice->Params.Priority == Priority) && (pVoice->Volume < AbsoluteVolume))*/ ) 
        {
#if defined(rbrannon)
            if( pVoice == g_DebugVoice )
            {
                LOG_WARNING( "AudioDebug(audio_voice_mgr::AcquireVoice)",
                             "Freed - g_DebugVoice" );
            }
#endif // defined(rbrannon)
            if( !FreeVoice( pVoice, FALSE ) )
            {
                pVoice = NULL;
            }
        }
        else
        {
            // Cannot acquire a voice...
            //AudioDebug( xfs( "Cannot acquire voice! Priority = %d, Voice Priority = %d\n", Priority, pVoice->Params.Priority) );
            pVoice = NULL;
        }
    }
    else
    {
        // Take it out of the free list.
        ASSERT( IsValidVoice(pVoice) );
        RemoveVoiceFromList( pVoice );
    }

    // Was a voice acquired?
    if( pVoice )
    {
        // Set the voices priority directly for the priority insertion.
        pVoice->Params.Priority = Priority;

        // Set voices volume directly for the priority insertion.
        pVoice->Volume = AbsoluteVolume;

        // Initialize the degreestosiund
        pVoice->DegreesToSound     = 0;
        pVoice->PrevDegreesToSound = 0;

        // Insert it into the list by its priority and volume (don't remove it from a list).
        PrioritizeVoice( pVoice, FALSE );  
    }

    // Tell the world
    return( pVoice );
}

//==============================================================================

f32 audio_voice_mgr::GetVoiceTime( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        element* pElement;
        element* pHead;

        // Error check.
        ASSERT( IsValidVoice(pVoice) );

        // Get start of element list.
        pHead    = (element*)&pVoice->Elements;
        pElement = pHead->Link.pNext;

        // Only if it has elements...
        if( pElement )
        {
            // For every element...
            while( pElement != pHead )
            {
                f32 Time;

                element* pNextElement = pElement->Link.pNext;

                // Calcualte this elements total time.
                Time = pElement->DeltaTime + ((f32)pElement->Sample.pHotSample->nSamples / (f32)pElement->Sample.pHotSample->SampleRate);

                // Return longest time.
                if( Time > Result )
                {
                    Result = Time;
                }

                // Walk the list
                pElement = pNextElement;
            }
        }
    }
    else
    {
        // TODO: Warning message.
    }

    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetLipSync( voice* pVoice )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        element* pElement;
        element* pHead;

        // Only if its running or paused
        if( (pVoice->State == STATE_RUNNING) || (pVoice->State == STATE_PAUSED) )
        {
            // Error check.
            ASSERT( IsValidVoice(pVoice) );

            // Get start of element list.
            pHead    = (element*)&pVoice->Elements;
            pElement = pHead->Link.pNext;

            // Only if it has elements...
            if( pElement != pHead )
            {
                if( pElement->Sample.pHotSample->LipSyncOffset != 0xffffffff )
                {
                    audio_package*  pPackage = pElement->pVoice->pPackage;
                    f32             dTime    = GetCurrentPlayTime( pVoice );
                    u8*             pLipSync = (u8*)((uaddr)pElement->Sample.pHotSample->LipSyncOffset + (uaddr)pPackage->m_LipSyncTable);
                    s32             Index;
                    s32             Result;

                    // Bump past sample rate (should be 30)
                    pLipSync++;

                    Index = (s32)(dTime * 30.0f);
                        
                    Result = (s32)pLipSync[Index];
                    return (f32)Result / 255.0f;
                }
            }
        }
    }
    else
    {
        // TODO: Warning message.
    }

    return 0.0f;
}

//==============================================================================

xbool audio_voice_mgr::HasLipSync( voice* pVoice )
{

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        element* pElement;
        element* pHead;

        // Error check.
        ASSERT( IsValidVoice(pVoice) );

        // Get start of element list.
        pHead    = (element*)&pVoice->Elements;
        pElement = pHead->Link.pNext;

        // Only if it has elements...
        if( pElement != pHead )
        {
            xbool Result = pElement->Sample.pHotSample->LipSyncOffset != 0xffffffff;
            return Result;
        }
    }
    else
    {
        // TODO: Warning message.
    }

    return FALSE;
}

//==============================================================================

s32 audio_voice_mgr::GetBreakPoints( voice* pVoice, f32* & BreakPoints )
{
    s32 Result = 0;

    // Error check.
    ASSERT( s_IsInitialized );

    // Default is not any...
    BreakPoints = NULL;

    // Only if its valid.
    if( pVoice )
    {
        element* pElement;
        element* pHead;

        // Error check.
        ASSERT( IsValidVoice(pVoice) );

        // Get start of element list.
        pHead    = (element*)&pVoice->Elements;
        pElement = pHead->Link.pNext;

        // Only if it has elements...
        if( pElement != pHead )
        {
            if( pElement->Sample.pHotSample->BreakPointOffset != 0xffffffff )
            {
                audio_package*  pPackage     = pElement->pVoice->pPackage;
                s32*            pBreakPoints = (s32*)((uaddr)pElement->Sample.pHotSample->BreakPointOffset + (uaddr)pPackage->m_BreakPointTable);
                
                Result      = *pBreakPoints++;
                BreakPoints = (f32*)pBreakPoints;
                return Result;
            }
        }
    }
    else
    {
        // TODO: Warning message.
    }

    // No break points!
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetCurrentPlayTime( voice* pVoice )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        element* pElement;
        element* pHead;

        // Only if its running or paused
        if( (pVoice->State == STATE_RUNNING) || (pVoice->State == STATE_PAUSED) )
        {
            // Error check.
            ASSERT( IsValidVoice(pVoice) );

            // Get start of element list.
            pHead    = (element*)&pVoice->Elements;
            pElement = pHead->Link.pNext;

            // Only if it has elements...
            if( pElement != pHead && pElement->pChannel )
            {
                u32 nSamples = Runtime().Backend.GetSamplesPlayed( pElement->pChannel );
#ifdef PLAY_TIME_LOGGING
                LOG_MESSAGE( PLAY_TIME_LOGGING, "nSamples: %d, nSamplesMax: %d", nSamples, pElement->pChannel->Sample.pHotSample->nSamples );
#endif
                if( nSamples > (u32)pElement->pChannel->Sample.pHotSample->nSamples )
                {
                    nSamples = (u32)pElement->pChannel->Sample.pHotSample->nSamples;
                }

                f32 Result = (f32)nSamples / (f32)pElement->pChannel->Sample.pHotSample->SampleRate;
                return Result;
            }
        }
    }
    else
    {
        // TODO: Warning message.
    }

    return 0.0f;
}

//==============================================================================

const char* audio_voice_mgr::GetVoiceDescriptor( voice* pVoice )
{
    const char* pDescriptor = "NULL";

    // Error check.
    ASSERT( s_IsInitialized );

    // Grab descriptor if voice if valid
    if( pVoice )
    {
        pDescriptor = pVoice->pDescriptorName;
    }

    return pDescriptor;
}

//==============================================================================

xbool audio_voice_mgr::GetIsReady( voice* pVoice )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        element* pElement;
        element* pHead;

        // Only if its running or paused
        if( (pVoice->State == STATE_RUNNING) || (pVoice->State == STATE_PAUSED) || (pVoice->State == STATE_NOT_STARTED) )
        {
            // Error check.
            ASSERT( IsValidVoice(pVoice) );

            // Get start of element list.
            pHead    = (element*)&pVoice->Elements;
            pElement = pHead->Link.pNext;

            // Only if it has elements...
            if( pElement != pHead )
            {
                xbool Result = pElement->State == ELEMENT_READY;
                return Result;
            }
        }
    }
    else
    {
        // TODO: Warning message.
    }

    return FALSE;
}

//==============================================================================

void audio_voice_mgr::ReleaseVoice( voice* pVoice, f32 Time )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        xbool    bFreeImmediate = TRUE;
        
        // Gotta be valid.
        ASSERT( IsValidVoice(pVoice) );

        // If the voice is just starting or already running then use the release time provided,
        // otherwise just kill it immediately.
        if( (pVoice->State == STATE_STARTING) || (pVoice->State == STATE_RUNNING) )
        {
            bFreeImmediate = FALSE;
        }

        // Stop it right now? (might cause a pop, but oh well...)
        if( (Time <= 0.0f) || bFreeImmediate )
        {
            // Free the voice, put it in the free list.
            (void)FreeVoice( pVoice, TRUE );
        }
        else
        {
            // Note that the voice is releasing, set the delta volume.
            pVoice->IsReleasing = TRUE;
            pVoice->DeltaVolume = pVoice->Params.Volume / Time;
        }
    }
    else
    {
        // TODO: Warning message.
    }

}
                                   
//==============================================================================

void audio_voice_mgr::ReleaseAllVoices( void )
{
    voice*   pHeadVoice;
    voice*   pVoice;
    voice*   pNextVoice;

    // Error check.
    ASSERT( s_IsInitialized );

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Get next voice.
        ASSERT( IsValidVoice(pVoice) );
        pNextVoice = pVoice->Link.pNext;

        // Free the voice, put it in the free list.
        (void)FreeVoice( pVoice, TRUE );

        // Walk the list...
        pVoice = pNextVoice;
    }

}

//==============================================================================

void audio_voice_mgr::ReleasePackagesVoices( audio_package* pPackage )
{
    voice*   pHeadVoice;
    voice*   pVoice;
    voice*   pNextVoice;

    // Error check.
    ASSERT( s_IsInitialized );

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Get next voice.
        ASSERT( IsValidVoice(pVoice) );
        pNextVoice = pVoice->Link.pNext;

        // Packages match?
        if( pVoice->pPackage == pPackage )
        {
            // Free the voice, put it in the free list.
            (void)FreeVoice( pVoice, TRUE );
        }

        // Walk the list...
        pVoice = pNextVoice;
    }

}

//==============================================================================

void audio_voice_mgr::StartVoice( voice* pVoice )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        // Make it start...
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->State == STATE_NOT_STARTED )
        {
            pVoice->State = STATE_STARTING;
        }
        else
        {
            // TODO: Warning message.
        }
    }
    else
    {
        // TODO: Warning message.
    }

}

//==============================================================================

xbool audio_voice_mgr::Segue( voice* pVoice, voice* pVoiceToQ )
{
    xbool Result = FALSE;

    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        // Make it start...
        ASSERT( IsValidVoice(pVoice) );

        if( pVoiceToQ )
        {
            ASSERT( IsValidVoice(pVoiceToQ) );
            pVoiceToQ->StartQ          = 0;
            pVoiceToQ->pSegueVoicePrev = pVoice;
        }

        pVoice->pSegueVoiceNext = pVoiceToQ;
        Result = TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetReleaseTime( voice* pVoice, f32 Time )
{
    xbool Result = FALSE;

    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        // Make it start...
        ASSERT( IsValidVoice(pVoice) );

        if( Time <= 0.0f )
        {
            Time = 0.0f;
        }
        pVoice->ReleaseTime = Time;
        pVoice->Dirty |= VOICE_DB_RELEASE_TIME_CHANGE;
        Result = TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    return Result;
}

//==============================================================================

void audio_voice_mgr::PauseVoice( voice* pVoice )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        // Make it start...
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->State == STATE_RUNNING )
        {
            pVoice->State = STATE_PAUSING;
        }
        else if( pVoice->State == STATE_RESUMING )
        {
            pVoice->State = STATE_PAUSED;
        }
        else
        {
            // TODO: Warning message.
        }
    }
    else
    {
        // TODO: Warning message.
    }

}

//==============================================================================

void audio_voice_mgr::ResumeVoice( voice* pVoice )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        // Make it start...
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->State == STATE_PAUSED )
        {
            pVoice->State = STATE_RESUMING;
        }
        else
        {
            // TODO: Warning message.
        }
    }
    else
    {
        // TODO: Warning message.
    }

}

//==============================================================================

void audio_voice_mgr::PauseAllVoices( void )
{
    voice*   pHeadVoice;
    voice*   pVoice;
    voice*   pNextVoice;

    // Error check.
    ASSERT( s_IsInitialized );

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Get next voice.
        ASSERT( IsValidVoice(pVoice) );
        pNextVoice = pVoice->Link.pNext;

        // What to do?
        switch( pVoice->State )
        {
            case STATE_RUNNING:
            {
                pVoice->State = STATE_PAUSING;
                break;
            }

            case STATE_RESUMING:
            {
                pVoice->State = STATE_PAUSED;
                break;
            }

            default:
            {
                // TODO: Warning message
                break;
            }
        }

        // Walk the list...
        pVoice = pNextVoice;
    }

}

//==============================================================================

void audio_voice_mgr::ResumeAllVoices( void )
{
    voice*   pHeadVoice;
    voice*   pVoice;
    voice*   pNextVoice;

    // Error check.
    ASSERT( s_IsInitialized );

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Get next voice.
        ASSERT( IsValidVoice(pVoice) );
        pNextVoice = pVoice->Link.pNext;

        // What to do?
        switch( pVoice->State )
        {
            case STATE_PAUSED:
            {
                pVoice->State = STATE_RESUMING;
                break;
            }

            default:
            {
                // TODO: Warning message
                break;
            }
        }

        // Walk the list...
        pVoice = pNextVoice;
    }

}

//==============================================================================

xbool audio_voice_mgr::IsVoicePlaying( voice* pVoice )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = (pVoice->State == STATE_RUNNING);
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::IsVoiceStarting( voice* pVoice )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = (pVoice->State == STATE_STARTING);
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::IsVoiceReleasing( voice* pVoice )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        Result = pVoice->IsReleasing;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::IsVoiceReady( voice* pVoice )
{
    // Error check.
    ASSERT( s_IsInitialized );

    xbool Result = FALSE;

    // Only if its valid.
    if( pVoice )
    {
        // Error check.
        ASSERT( IsValidVoice(pVoice) );

        // Get start of element list.
        element* pHead    = (element*)&pVoice->Elements;
        element* pElement = pHead->Link.pNext;

        // Only if it has elements...
        if( pElement != pHead )
        {
            Result = pElement->State == ELEMENT_READY;
        }
    }

    return Result;
}

//==============================================================================

s32 audio_voice_mgr::GetVoicePriority( voice* pVoice )
{
    s32 Result = -1;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->Params.Priority;
    }
    else
    {
        // TODO: Warning message.
        Result = -1;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetVoiceUserVolume( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->UserVolume;
    }
    else
    {
        // TODO: Warning message.
        Result = 0.0f;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceUserVolume( voice* pVoice, f32 Volume )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        pVoice->UserVolume = Volume;
        pVoice->Dirty |= VOICE_DB_VOLUME_CHANGE;
        Result = TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetVoiceRelativeVolume( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->Params.Volume;
    }
    else
    {
        // TODO: Warning message.
        Result = 0.0f;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceRelativeVolume( voice* pVoice, f32 Volume )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        pVoice->Params.Volume = Volume;
        pVoice->Dirty |= VOICE_DB_VOLUME_CHANGE;
        Result =  TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetVoicePan( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->Params.Pan2d;
    }
    else
    {
        // TODO: Warning message.
        Result = 0.0f;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoicePan( voice* pVoice, f32 Pan )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->IsPanChangeable )
        {
            pVoice->Params.Pan2d = Pan;
            Runtime().Spatial.Calculate2dPan( pVoice->Params.Pan2d, pVoice->Params.Pan3d );
            pVoice->Dirty |= VOICE_DB_PAN_CHANGE;
            Result = TRUE;
        }
        else
        {
            // TODO: Warning message.
            Result = FALSE;
        }
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetVoiceUserPitch( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->UserPitch;
    }
    else
    {
        // TODO: Warning message.
        Result = 0.0f;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceUserPitch( voice* pVoice, f32 Pitch )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        pVoice->UserPitch = Pitch;
        pVoice->Dirty |= VOICE_DB_PITCH_CHANGE;
        Result = TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetVoiceRelativePitch( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->Params.Pitch;
    }
    else
    {
        // TODO: Warning message.
        Result = 0.0f;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceRelativePitch( voice* pVoice, f32 Pitch )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        pVoice->Params.Pitch = Pitch;
        pVoice->Dirty |= VOICE_DB_PITCH_CHANGE;
        Result = TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetVoiceUserEffectSend( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->UserEffectSend;
    }
    else
    {
        // TODO: Warning message.
        Result = 0.0f;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceUserEffectSend ( voice* pVoice, f32 EffectSend )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        pVoice->UserEffectSend = EffectSend;
        pVoice->Dirty |= VOICE_DB_EFFECTSEND_CHANGE;
        Result = TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

f32 audio_voice_mgr::GetVoiceRelativeEffectSend( voice* pVoice )
{
    f32 Result = 0.0f;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        Result = pVoice->Params.EffectSend;
    }
    else
    {
        // TODO: Warning message.
        Result = 0.0f;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceRelativeEffectSend ( voice* pVoice, f32 EffectSend )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );
        pVoice->Params.EffectSend = EffectSend;
        pVoice->Dirty |= VOICE_DB_EFFECTSEND_CHANGE;
        Result = TRUE;
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::GetVoicePosition( voice* pVoice, vector3& Position, s32& ZoneID )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->IsPositional )
        {
            Position = pVoice->Position;
            ZoneID   = pVoice->ZoneID;
            Result   = TRUE;
        }
        else
        {
            // TODO: Warning message.
            Result = FALSE;
        }
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoicePosition( voice* pVoice, const vector3& Position, s32 ZoneID )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->IsPositional )
        {
            pVoice->Position = Position;
            pVoice->ZoneID   = ZoneID;
            Result = TRUE;
        }
        else
        {
            // TODO: Warning message.
            Result = FALSE;
        }
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceUserFalloff( voice* pVoice, f32 Near, f32 Far )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->IsPositional )
        {
            pVoice->UserFarFalloff  = Far;
            pVoice->UserNearFalloff = Near;
            Result = TRUE;
        }
        else
        {
            // TODO: Warning message.
            Result = FALSE;
        }
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceRelativeFalloff( voice* pVoice, f32 Near, f32 Far )
{
    xbool Result = FALSE;

    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->IsPositional )
        {
            pVoice->Params.FarFalloff  = Far;
            pVoice->Params.NearFalloff = Near;
            pVoice->UserFarDiffuse     = Far;
            pVoice->UserNearDiffuse    = Near;
            Result = TRUE;
        }
        else
        {
            // TODO: Warning message.
            Result = FALSE;
        }
    }
    else
    {
        // TODO: Warning message.
        Result = FALSE;
    }

    // Tell the world.
    return Result;
}

//==============================================================================

xbool audio_voice_mgr::SetVoiceUserDiffuse( voice* pVoice, f32 Near, f32 Far )
{
    // Error check.
    ASSERT( s_IsInitialized );

    // Only if its valid.
    if( pVoice )
    {
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->IsPositional )
        {
            pVoice->UserFarDiffuse  = Far;
            pVoice->UserNearDiffuse = Near;
            return TRUE;
        }
        else
        {
            // TODO: Warning message.
            return FALSE;
        }
    }
    else
    {
        // TODO: Warning message.
        return FALSE;
    }

    return FALSE;
}

//==============================================================================

inline void audio_voice_mgr::UpdateStartPending( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateStartPending" );

    element* pElement;
    element* pHeadElement;
    element* pNextElement;
    
    // Any pending elements left to fire off?
    if( (pElement = pVoice->pPendingElement) != NULL )
    {
        // Get list head/tail
        pHeadElement = (element*)&pVoice->Elements;

        // Process each element...
        while( (pElement != pHeadElement) && (pVoice->CursorTime > pElement->DeltaTime) )
        {
            // get next element in the list...
            pNextElement = pElement->Link.pNext;

            // Is the element ready?
            if( pElement->State == ELEMENT_READY )
            {
                // Has the element timed out? 100 millisecond leeway...
                if( (pElement->Type != COLD_SAMPLE) && ((pVoice->CursorTime - pElement->DeltaTime) > ELEMENT_EXPIRE_DELAY) )
                {
                    // Release the element, don't free the channel - it doesn't have one.
                    ReleaseElement( pElement, FALSE );
                }
                else
                {
                    // Should not have a channel...
                    if( pElement->pChannel == NULL )
                    {
                        // Attempt to acquire a hardware channel.
                        if( Runtime().Channels.Acquire( pElement ) )
                        {
#if defined(rbrannon)
                            if( (pVoice == g_DebugVoice) && (g_DebugElement == NULL) )
                            {
                                g_DebugElement = pElement;
                                g_DebugChannel = pElement->pChannel;
                                g_DebugTime    = (f32)pElement->pChannel->Sample.pHotSample->nSamples / (f32)pElement->pChannel->Sample.pHotSample->SampleRate;
                                LOG_MESSAGE( "AudioDebug(audio_voice_mgr::UpdateStartPending)", 
                                             "AQUIRED! %s, pVoice: %08x, pElement: %08x, pChannel: %08x",
                                             g_DebugVoice->pDescriptorName,
                                             g_DebugVoice,
                                             g_DebugElement,
                                             g_DebugChannel );
                                LOG_FLUSH();
                            }
#endif // defined(rbrannon)
                            // Start it up!
                            StartElement( pElement );
                        }
#if defined(rbrannon)
                        else
                        {
                            if( pVoice == g_DebugVoice )
                            {
                                LOG_WARNING( "AudioDebug(audio_voice_mgr::UpdateStartPending)", 
                                             "AQUIRE FAILED! pVoice: %08x",
                                             g_DebugVoice );
                            }
                        }
#endif // defined(rbrannon)
                    }
                    else
                    {
                        // Start it up!
                        StartElement( pElement );
                    }
                }
            }
            else
            {
/*
                // TODO: Put in check to nuke elements that are NEVER ready...
                if (pElement->State == ELEMENT_LOADING)
                {
                    BREAK;
                }
*/
            }

            // Walk the list.
            pElement = pNextElement;
        }
    }
}

//==============================================================================

inline voice* audio_voice_mgr::UpdateCheckElements( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateCheckElements" );

    element* pElement;
    element* pHead;

    // Get head/tail of list and first element in the list.
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    // Empty element list?
    if( pElement == pHead )
    {
        // Free it
        if( !FreeVoice( pVoice, TRUE ) )
        {
            return pVoice;
        }

        // Its dead!
        return( NULL );
    }
    else
    {
        s32 Priority    = -1;
        s32 OldPriority = pVoice->Params.Priority;

        // No pending...
        pVoice->pPendingElement = NULL;

        // Walk the entire list...
        while( pElement != pHead )
        {
            // Is this one ready?
            if( (pElement->State == ELEMENT_READY) && (pVoice->pPendingElement == NULL) )
            {
                // Found it.
                pVoice->pPendingElement = pElement;
            }

            // Find the highest priority element...
            if( (pElement->Params.Priority > Priority) && (pElement->State < ELEMENT_DONE) )
            {
                Priority = pElement->Params.Priority;
            }

            // Walk the list
            pElement = pElement->Link.pNext;
        }
        
        // Different priority?
        if( Priority != OldPriority )
        {
            // Set the voices priority.
            pVoice->Params.Priority = Priority;

            // Update the voices priority.
            s_UpdatePriority = TRUE;
        }

        // Still have a voice...
        return( pVoice );
    }
}

//==============================================================================

void audio_voice_mgr::UpdateReleaseTime( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateReleaseTime" );
    element* pElement;
    element* pHead;

    // Get head/tail of list and first element in the list.
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    // Empty element list?
    if( pElement != pHead )
    {
        // Walk the entire list...
        while( pElement != pHead )
        {
            channel* pChannel = pElement->pChannel;
            
            // Set the release position.
            if( pChannel )
            {
                pChannel->ReleasePosition = (u32)(pVoice->ReleaseTime * (f32)pChannel->Sample.pHotSample->SampleRate);
            }

            // Walk the list
            pElement = pElement->Link.pNext;
        }        
    }
}

//==============================================================================

inline voice* audio_voice_mgr::UpdateStateStarting( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateStateStarting" );

    // Calculate voice priority, find the next pending element...
    pVoice = UpdateCheckElements( pVoice );

    // Only if voice still exists.
    if( pVoice )
    {
        // Voice is running now.
        pVoice->State = STATE_RUNNING;
    }

    return pVoice;
}

//==============================================================================

inline voice* audio_voice_mgr::UpdateStateResuming( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateStateResuming" );

    // Keep paused stream cursors stable until outstanding reads complete.
    if( g_IoMgr.GetDeviceQueueStatus( IO_DEVICE_HOST ) )
    {
        return pVoice;
    }

    // Calculate voice priority, find the next pending element...
    pVoice = UpdateCheckElements( pVoice );

    // Only if voice still exists.
    if( pVoice )
    {
        element* pHead;
        element* pElement;

        // Voice is running now.
        pVoice->State = STATE_RUNNING;

        // For every element in the list...
        pHead    = (element*)&pVoice->Elements;
        pElement = pHead->Link.pNext;

        while( pHead != pElement )
        {
            // Resume each element.
            ASSERT( IsValidElement( pElement ) );

            if( pElement->State == ELEMENT_PAUSED )
            {
                ResumeElement( pElement );
            }

            // Walk the list.
            pElement = pElement->Link.pNext;
        }
    }

    return pVoice;
}

//==============================================================================

inline void audio_voice_mgr::UpdateVoice3d( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateVoice3d" );

    element* pHead;
    element* pElement;

    // Calculate the voices falloff.
    pVoice->NearFalloff = CalculateVoiceNearFalloff( pVoice );
    pVoice->FarFalloff  = CalculateVoiceFarFalloff( pVoice );

    // Calculate the voices diffusion.
    pVoice->NearDiffuse = CalculateVoiceNearDiffuse( pVoice );
    pVoice->FarDiffuse  = CalculateVoiceFarDiffuse( pVoice );

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    while( pHead != pElement )
    {
        // Stop each element.
        ASSERT( IsValidElement( pElement ) );

        // Calculate the falloffs.
        f32 Near = CalculateElementNearFalloff( pElement );
        f32 Far  = CalculateElementFarFalloff( pElement );

        // Calculate the diffusion
        f32 NearDiffuse = CalculateElementNearDiffuse( pElement );
        f32 FarDiffuse  = CalculateElementFarDiffuse( pElement );

        // Calculate the 3d volume and pan.
        Runtime().Spatial.Calculate3dVolumeAndPan( Near, Far, pElement->Params.RolloffCurve,
                                            NearDiffuse, FarDiffuse, pVoice->Position, pVoice->ZoneID,
                                            pElement->PositionalVolume, pElement->Params.Pan3d, 
                                            pVoice->DegreesToSound, pVoice->PrevDegreesToSound,
                                            pVoice->EarID );

        // Now calculate it. (rmb - I think this call is redundant....)
        // TODO: rmb - Check to see if this line can be removed.
        pElement->Volume = CalculateElementVolume( pElement, Runtime().AudioDuckLevel > 0 );
        
        // Walk the list.
        pElement = pElement->Link.pNext;
    }

    // set the dirty bits.
    pVoice->Dirty |= VOICE_DB_VOLUME_CHANGE+VOICE_DB_PAN_CHANGE;
}

//==============================================================================

inline void audio_voice_mgr::UpdateVoiceVolumeAndPan ( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateVoiceVolumeAndPan" );

    element* pHead;
    element* pElement;

    // Calculate the voices volume.
    xbool AudioDuckingEnabled = (Runtime().AudioDuckLevel > 0);

    pVoice->Volume = CalculateVoiceVolume( pVoice, AudioDuckingEnabled );

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    while( pHead != pElement )
    {
        // Stop each element.
        ASSERT( IsValidElement( pElement ) );

        // Calculate and apply the volume.
        pElement->Volume = CalculateElementVolume( pElement, AudioDuckingEnabled );

        if( pElement->State == ELEMENT_PLAYING ) 
        {
            ApplyElementVolume( pElement );
        }

        // Do something.
        if( pElement->IsPanChangeable )
        {
            // Set and apply the pan.
            pElement->Params.Pan2d = pVoice->Params.Pan2d;

            if( pElement->State == ELEMENT_PLAYING )
            {
                ApplyElementPan( pElement );
            }
        }
        else
        {
            // TODO: Warning message.
        }

        // Walk the list.
        pElement = pElement->Link.pNext;
    }
}

//==============================================================================

inline void audio_voice_mgr::UpdateVoiceVolume( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateVoiceVolume" );

    element* pHead;
    element* pElement;

    // Calculate the voices volume.
    xbool AudioDuckingEnabled = (Runtime().AudioDuckLevel > 0);

    pVoice->Volume = CalculateVoiceVolume( pVoice, AudioDuckingEnabled );

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    while( pHead != pElement )
    {
        // Stop each element.
        ASSERT( IsValidElement( pElement ) );

        // Calculate and apply the volume.
        pElement->Volume = CalculateElementVolume( pElement, AudioDuckingEnabled );

        if( pElement->State == ELEMENT_PLAYING ) 
        {
            ApplyElementVolume( pElement );
        }

        // Walk the list.
        pElement = pElement->Link.pNext;
    }
}

//==============================================================================

inline void audio_voice_mgr::UpdateVoicePan( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateVoicePan" );

    element* pHead;
    element* pElement;

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    while( pHead != pElement )
    {
        // Stop each element.
        ASSERT( IsValidElement( pElement ) );

        // Do something.
        if( pElement->IsPanChangeable )
        {
            // Set and apply the pan.
            pElement->Params.Pan2d = pVoice->Params.Pan2d;

            if( pElement->State == ELEMENT_PLAYING ) 
            {
                ApplyElementPan( pElement );
            }
        }
        else
        {
            // TODO: Warning message.
        }

        // Walk the list.
        pElement = pElement->Link.pNext;
    }
}

//==============================================================================

inline void audio_voice_mgr::UpdateVoicePitch( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateVoicePitch" );

    element* pHead;
    element* pElement;

    // Calculate the voices volume.
    pVoice->Pitch = CalculateVoicePitch( pVoice );

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    while( pHead != pElement )
    {
        // Stop each element.
        ASSERT( IsValidElement( pElement ) );

        // Calculate and apply the elements pitch.
        pElement->Pitch = CalculateElementPitch( pElement );

        if( pElement->State == ELEMENT_PLAYING ) 
        {        
            ApplyElementPitch( pElement );
        }

        // Walk the list.
        pElement = pElement->Link.pNext;
    }
}

//==============================================================================

inline void audio_voice_mgr::UpdateVoiceEffectSend( voice* pVoice )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateVoiceEffectSend" );

    element* pHead;
    element* pElement;

    // Calculate the voices volume.
    pVoice->EffectSend = CalculateVoiceEffectSend( pVoice );

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    while( pHead != pElement )
    {
        // Stop each element.
        ASSERT( IsValidElement( pElement ) );

        // Calculate and apply the elements pitch.
        pElement->EffectSend = CalculateElementEffectSend( pElement );

        if( pElement->State == ELEMENT_PLAYING ) 
        {        
            ApplyElementEffectSend( pElement );
        }

        // Walk the list.
        pElement = pElement->Link.pNext;
    }
}

//==============================================================================

inline voice* audio_voice_mgr::UpdateStateRunning( voice* pVoice, f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateStateRunning" );

    u32 Dirty;

    // Update the voices 3d stuff.
    if( pVoice->IsPositional )
    {
        UpdateVoice3d( pVoice );
    }

    // Is the voice releasing?
    if( pVoice->IsReleasing )
    {
        // Has the release state finished?
        if( pVoice->Params.Volume <= 0.0f )
        {
            // Free the voice, put it in the free list.
            if( FreeVoice( pVoice, TRUE ) )
            {
                return NULL;
            }

            return pVoice;
        }
        else
        {
            // Update volume by delta
            pVoice->Params.Volume -= (pVoice->DeltaVolume * DeltaTime);

            if( pVoice->Params.Volume < 0.0f )
            {
                pVoice->Params.Volume = 0.0f;
            }
            pVoice->Dirty |= VOICE_DB_VOLUME_CHANGE;
        }
    }

    // Local copy of dirty bits.
    Dirty = pVoice->Dirty;

    // Any dirty bits set?
    if( Dirty )
    {
        // Don't update the priority unless needed.
        s_UpdatePriority = FALSE;

        if( Dirty & (VOICE_DB_VOLUME_CHANGE+VOICE_DB_PAN_CHANGE) )
        {
            // Update the voices volume and pan (priority as well)
            UpdateVoiceVolumeAndPan( pVoice );

            // Clear the dirty bit.
            Dirty &= ~(VOICE_DB_VOLUME_CHANGE+VOICE_DB_PAN_CHANGE);

           // Need to update the voices priority.
            s_UpdatePriority = TRUE;
        }

        if( Dirty & VOICE_DB_VOLUME_CHANGE )
        {
            // Update the voices volume (and priority)
            UpdateVoiceVolume( pVoice );

            // Clear the dirty bit.
            Dirty &= ~VOICE_DB_VOLUME_CHANGE;

           // Need to update the voices priority.
            s_UpdatePriority = TRUE;
        }

        if( Dirty & VOICE_DB_PAN_CHANGE )
        {
            // Update the voices pan.
            UpdateVoicePan( pVoice );

            // Clear the dirty bit.
            Dirty &= ~VOICE_DB_PAN_CHANGE;
        }

        if( Dirty & VOICE_DB_PITCH_CHANGE )
        {
            // Update the voices pan.
            UpdateVoicePitch( pVoice );

            // Clear the dirty bit.
            Dirty &= ~VOICE_DB_PITCH_CHANGE;
        }

        if( Dirty & VOICE_DB_EFFECTSEND_CHANGE )
        {
            // Update the voices effect send.
            UpdateVoiceEffectSend( pVoice );

            // Clear the dirty bit.
            Dirty &= ~VOICE_DB_EFFECTSEND_CHANGE;
        }
        
        if( Dirty & VOICE_DB_RELEASE_TIME_CHANGE )
        {
            // Update the release time.
            UpdateReleaseTime( pVoice );

            // Clear the dirty bit
            Dirty &= ~VOICE_DB_RELEASE_TIME_CHANGE;
        }

        if( Dirty & VOICE_DB_ELEMENT_CHANGE )
        {
            // Calculate voice priority, find the next pending element...
            pVoice = UpdateCheckElements( pVoice );

            // Clear the dirty bit
            Dirty &= ~VOICE_DB_ELEMENT_CHANGE;
        }

        // Voice still around?
        if( pVoice )
        {
            // Update the dirty flags.
            pVoice->Dirty = Dirty;

            // Need to update the voices priority?
            if( s_UpdatePriority )
            {
                // Update the priority.
                PrioritizeVoice( pVoice, TRUE );
            }
        }
    }

    // Tell the world...
    return pVoice;
}

//==============================================================================

inline void audio_voice_mgr::UpdateStatePausing( voice* pVoice, f32 Time )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::UpdateStatePausing" );

    element* pHead;
    element* pElement;

    // For every element in the list...
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;

    while( pHead != pElement )
    {
        // Stop each element.
        ASSERT( IsValidElement( pElement ) );

        if( pElement->State == ELEMENT_PLAYING )
        {
            PauseElement( pElement );
        }

        // Walk the list.
        pElement = pElement->Link.pNext;
    }

    // Voice is stopped.
    pVoice->StopTime = Time;
    pVoice->State    = STATE_PAUSED;
}

//==============================================================================

inline voice* audio_voice_mgr::UpdateCheckStreams( voice* pVoice )
{
    return m_StreamBinder.UpdateCheckStreams( *this, pVoice );
}

//==============================================================================

void audio_voice_mgr::Update( f32 DeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::Update" );

    voice*   pHeadVoice;
    voice*   pVoice;
    voice*   pNextVoice;

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Get next voice.
        ASSERT( IsValidVoice(pVoice) );
        pNextVoice = pVoice->Link.pNext;

        if( (void*)pVoice->Elements.pNext == (void*)&pVoice->Elements )
        {
            if( FreeVoice( pVoice, TRUE ) )
            {
                pVoice = NULL;
            }
        }
        else
        {
            // Need to warm up any streams?
            pVoice = UpdateCheckStreams( pVoice );
        }

        // What to do?
        if( pVoice )
        {
            switch( pVoice->State )
            {
                case STATE_NOT_STARTED:
                {
#ifdef UPDATE_STATE_LOGGING
                    LOG_MESSAGE( UPDATE_STATE_LOGGING, "pVoice: %08x, State: STATE_NOT_STARTED", pVoice );
#endif
                    break;
                }
 
                case STATE_STARTING:
                {
#ifdef UPDATE_STATE_LOGGING
                    LOG_MESSAGE( UPDATE_STATE_LOGGING, "pVoice: %08x, State: STATE_STARTING", pVoice );
#endif
                    pVoice = UpdateStateStarting( pVoice );

                    if( pVoice )
                    {
                        pVoice->StartTime  = Runtime().Audio.GetAudioTime();
                        pVoice = UpdateStateRunning( pVoice, 0.0f );
                    }

                    break;
                }
 
                case STATE_RESUMING:
                {
#ifdef UPDATE_STATE_LOGGING
                    LOG_MESSAGE( UPDATE_STATE_LOGGING, "pVoice: %08x, State: STATE_RESUMING", pVoice );
#endif
                    pVoice = UpdateStateResuming( pVoice );

                    if( pVoice && pVoice->State == STATE_RUNNING )
                    {
                        pVoice->StartTime = Runtime().Audio.GetAudioTime()-pVoice->StopTime;
                        pVoice = UpdateStateRunning( pVoice, 0.0f );
                    }

                    break;
                }
 
                case STATE_RUNNING:
                {
#ifdef UPDATE_STATE_LOGGING
                    LOG_MESSAGE( UPDATE_STATE_LOGGING, "pVoice: %08x, State: STATE_RUNNING", pVoice );
#endif
                    // Update the voices cursor time.
                    pVoice->CursorTime += DeltaTime;
                    pVoice = UpdateStateRunning( pVoice, DeltaTime );
                    break;
                }
 
                case STATE_PAUSING:
                {
#ifdef UPDATE_STATE_LOGGING
                    LOG_MESSAGE( UPDATE_STATE_LOGGING, "pVoice: %08x, State: STATE_PAUSING", pVoice );
#endif
                    UpdateStatePausing( pVoice, Runtime().Audio.GetAudioTime() );
                    break;
                }
 
                case STATE_PAUSED:
                {
#ifdef UPDATE_STATE_LOGGING
                    LOG_MESSAGE( UPDATE_STATE_LOGGING, "pVoice: %08x, State: STATE_PAUSED", pVoice );
#endif
                    break;
                }
            }
        }
    
        // Only if voice is still around and running...
        if( pVoice && (pVoice->State == STATE_RUNNING) )
        {
            // Attempt to start any pending elements...
            UpdateStartPending( pVoice );
        }

        // Walk the list.
        pVoice = pNextVoice;
    }

}

//==============================================================================

void audio_voice_mgr::UpdateCheckQueued( void )
{
    voice* pVoice;
    voice* pHeadVoice;

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Get next voice.
        ASSERT( IsValidVoice(pVoice) );

        if( pVoice->StartQ == 1 )
        {
            // Error check
            if( pVoice->State == STATE_NOT_STARTED )
            {
                UpdateStateStarting( pVoice );
                UpdateStateRunning( pVoice, 0.0f );
                UpdateStartPending( pVoice );
            }

            pVoice->StartQ = 2;
        }

        // Walk the list.
        pVoice = pVoice->Link.pNext;
    }

}

//==============================================================================

s32 VOICE_PRIORITY_HIT=0;
s32 VOICE_PRIORITY_MISS=0;

//==============================================================================

void audio_voice_mgr::PrioritizeVoice( voice* pVoice, xbool RemoveFromList )
{
    // Error check.
    ASSERT( IsValidVoice(pVoice) );

    // Check if where it currently is is valid
    xbool bRelocate = !RemoveFromList;

    voice* pRoot = (voice*)&UsedVoices()->Link;

    if( pVoice->Link.pPrev != pRoot && (bRelocate==FALSE)  )
    {
        voice* pPrev = pVoice->Link.pPrev;
        bRelocate = (
                     ( pVoice->Params.Priority > pPrev->Params.Priority ) ||
                     ((pVoice->Params.Priority == pPrev->Params.Priority) &&
                      (pVoice->Volume > pPrev->Volume ))
                    );
    }

    if( (pVoice->Link.pNext != pRoot) && (bRelocate==FALSE) )
    {
        voice* pNext = pVoice->Link.pNext;
        bRelocate = (
                     ( pVoice->Params.Priority < pNext->Params.Priority ) ||
                     ((pVoice->Params.Priority == pNext->Params.Priority) &&
                      (pVoice->Volume < pNext->Volume ))
                    );
    }

    if( bRelocate )
    {
        // Remove the channel from its current list?
        if( RemoveFromList )
        {
            RemoveVoiceFromList( pVoice );
        }

        // Get first used voice.
        voice* pInsert = UsedVoices()->Link.pNext;

        // Find the insertion point (based on priority only).
        while( pInsert->Params.Priority > pVoice->Params.Priority )
        {
            pInsert = pInsert->Link.pNext;
        }

        // Volume is secondary key.
        while( (pInsert->Params.Priority == pVoice->Params.Priority) && (pInsert->Volume > pVoice->Volume) )
        {
            pInsert = pInsert->Link.pNext;
        }

        // Insert it into the used list.
        InsertVoiceIntoList( pVoice, pInsert );

        VOICE_PRIORITY_MISS++;
    }
    else
    {
        VOICE_PRIORITY_HIT++;
    }

}

//==============================================================================

xbool audio_voice_mgr::GetVoiceDescriptorCached( voice_id VoiceID, char* pName, s32 NameSize ) const
{
    if( !pName || NameSize <= 0 )
    {
        return FALSE;
    }

    pName[0]  = 0;
    s32 Index = ( ( VoiceID & 0xffff ) - 1 );

    if( !VoiceID || Index < 0 || Index >= m_NumVoices )
    {
        return FALSE;
    }

    s32 Size = MIN( NameSize - 1, AUDIO_PACKAGE_FILENAME_LENGTH - 1 );

    for( s32 Offset = 0; Offset < Size; Offset += 4 )
    {
        u64 Entry = x_AtomicLoadAcquire( &s_VoiceNames[Index][Offset / 4] );

        if( (u32)( Entry >> 32 ) != (u32)VoiceID )
        {
            pName[0] = 0;
            return FALSE;
        }

        u32 Bytes = (u32)Entry;
        x_memcpy( pName + Offset, &Bytes, MIN( 4, Size - Offset ) );
    }

    pName[Size] = 0;

    if( !IsValidVoiceIdCached( VoiceID ) )
    {
        pName[0] = 0;
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

f32 audio_voice_mgr::GetVolumeCached( voice_id VoiceID ) const
{
    s32 Index = ( ( VoiceID & 0xffff ) - 1 );

    if( !VoiceID || Index < 0 || Index >= m_NumVoices )
    {
        return 0.0f;
    }

    u64 Entry = x_AtomicLoadAcquire( &s_VoiceVolumes[Index] );

    if( (u32)( Entry >> 32 ) != (u32)VoiceID || !IsValidVoiceIdCached( VoiceID ) )
    {
        return 0.0f;
    }

    u32 Bits = (u32)Entry;
    f32 Volume;
    x_memcpy( &Volume, &Bits, sizeof( Bits ) );
    return Volume;
}

//==============================================================================

s32 audio_voice_mgr::GetPriorityCached( voice_id VoiceID ) const
{
    s32 Index = ( ( VoiceID & 0xffff ) - 1 );

    if( !VoiceID || Index < 0 || Index >= m_NumVoices )
    {
        return 0;
    }

    u64 Entry = x_AtomicLoadAcquire( &s_VoicePriorities[Index] );

    if( (u32)( Entry >> 32 ) != (u32)VoiceID || !IsValidVoiceIdCached( VoiceID ) )
    {
        return 0;
    }

    return (s32)(u32)Entry;
}

//==============================================================================

xbool audio_voice_mgr::GetVoiceStateCached( voice_id VoiceID, audio_voice_state& State ) const
{
    x_memset( &State, 0, sizeof( State ) );
    s32 Index = ( ( VoiceID & 0xffff ) - 1 );

    if( !VoiceID || Index < 0 || Index >= m_NumVoices )
    {
        return FALSE;
    }

    x_AtomicStoreRelease( &s_VoiceStateRequests[Index], (u32)VoiceID );
    u64 Length   = x_AtomicLoadAcquire( &s_VoiceLengths[Index] );
    u64 PlayTime = x_AtomicLoadAcquire( &s_VoicePlayTimes[Index] );

    if( (u32)( Length >> 32 ) != (u32)VoiceID || (u32)( PlayTime >> 32 ) != (u32)VoiceID ||
        !IsValidVoiceIdCached( VoiceID ) )
    {
        return FALSE;
    }

    u32 LengthBits = (u32)Length;
    u32 TimeBits   = (u32)PlayTime;
    x_memcpy( &State.Length, &LengthBits, sizeof( LengthBits ) );
    x_memcpy( &State.PlayTime, &TimeBits, sizeof( TimeBits ) );
    State.Valid = TRUE;
    State.Ready = x_AtomicLoadAcquire( &s_StateReadyVoiceIDs[Index] ) == (u32)VoiceID;
    return TRUE;
}

//==============================================================================

xbool audio_voice_mgr::IsValidVoiceIdCached( voice_id VoiceID ) const
{
    s32 Index = ( ( VoiceID & 0xffff ) - 1 );

    if( ( VoiceID == 0 ) || ( Index < 0 ) || ( Index >= m_NumVoices ) )
    {
        return FALSE;
    }

    return x_AtomicLoadAcquire( &s_PublishedVoiceIDs[Index] ) == (u32)VoiceID;
}

//==============================================================================

xbool audio_voice_mgr::IsVoiceReadyCached( voice_id VoiceID ) const
{
    s32 Index = ( ( VoiceID & 0xffff ) - 1 );

    if( ( VoiceID == 0 ) || ( Index < 0 ) || ( Index >= m_NumVoices ) )
    {
        return FALSE;
    }

    return x_AtomicLoadAcquire( &s_ReadyVoiceIDs[Index] ) == (u32)VoiceID;
}

//==============================================================================

void audio_voice_mgr::PublishVoiceReadiness( void )
{
    for( s32 i = 0; i < m_NumVoices; ++i )
    {
        u32 VoiceID = x_AtomicLoadAcquire( &s_PublishedVoiceIDs[i] );

        if( VoiceID && x_AtomicLoadAcquire( &s_VoiceStateRequests[i] ) == VoiceID )
        {
            x_AtomicStoreRelease( &s_VoiceLengths[i], PackVoiceTime( VoiceID, GetVoiceTime( &m_FirstVoice[i] ) ) );
            x_AtomicStoreRelease( &s_VoicePlayTimes[i], PackVoiceTime( VoiceID, GetCurrentPlayTime( &m_FirstVoice[i] ) ) );
        }

        if( VoiceID )
        {
            x_AtomicStoreRelease( &s_VoiceVolumes[i], PackVoiceTime( VoiceID, GetVoiceUserVolume( &m_FirstVoice[i] ) ) );
            x_AtomicStoreRelease( &s_VoicePriorities[i], ( (u64)VoiceID << 32 ) | (u32)m_FirstVoice[i].Params.Priority );
        }

        u32 ReadyID = ( VoiceID && IsVoiceReady( &m_FirstVoice[i] ) ) ? VoiceID : 0;
        x_AtomicStoreRelease( &s_ReadyVoiceIDs[i], ReadyID );
        x_AtomicStoreRelease( &s_StateReadyVoiceIDs[i], VoiceID && GetIsReady( &m_FirstVoice[i] ) ? VoiceID : 0 );
    }
}

//==============================================================================

void audio_voice_mgr::PublishVoiceId( voice* pVoice, voice_id VoiceID )
{
    ASSERT( IsValidVoice( pVoice ) );
    char Name[AUDIO_PACKAGE_FILENAME_LENGTH] = {};
    x_strncpy( Name, pVoice->pDescriptorName ? pVoice->pDescriptorName : "NULL", sizeof( Name ) - 1 );

    for( s32 Offset = 0; Offset < (s32)sizeof( Name ); Offset += 4 )
    {
        u32 Bytes;
        x_memcpy( &Bytes, Name + Offset, sizeof( Bytes ) );
        x_AtomicStoreRelease( &s_VoiceNames[pVoice - m_FirstVoice][Offset / 4], ( (u64)(u32)VoiceID << 32 ) | Bytes );
    }

    x_AtomicStoreRelease( &s_VoiceVolumes[pVoice - m_FirstVoice], PackVoiceTime( (u32)VoiceID, GetVoiceUserVolume( pVoice ) ) );
    x_AtomicStoreRelease( &s_VoicePriorities[pVoice - m_FirstVoice], ( (u64)(u32)VoiceID << 32 ) | (u32)pVoice->Params.Priority );
    x_AtomicStoreRelease( &s_VoiceStateRequests[pVoice - m_FirstVoice], 0 );
    x_AtomicStoreRelease( &s_VoiceLengths[pVoice - m_FirstVoice], PackVoiceTime( (u32)VoiceID, GetVoiceTime( pVoice ) ) );
    x_AtomicStoreRelease( &s_VoicePlayTimes[pVoice - m_FirstVoice], PackVoiceTime( (u32)VoiceID, GetCurrentPlayTime( pVoice ) ) );
    x_AtomicStoreRelease( &s_ReadyVoiceIDs[pVoice - m_FirstVoice], IsVoiceReady( pVoice ) ? (u32)VoiceID : 0 );
    x_AtomicStoreRelease( &s_StateReadyVoiceIDs[pVoice - m_FirstVoice], GetIsReady( pVoice ) ? (u32)VoiceID : 0 );
    x_AtomicStoreRelease( &s_PublishedVoiceIDs[pVoice - m_FirstVoice], (u32)VoiceID );
}

//==============================================================================

xbool audio_voice_mgr::FreeVoice( voice* pVoice, xbool PutInFreeList )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "audio_voice_mgr::FreeVoice" );

    element* pElement;
    element* pHead;
    u32      Sequence;

    // Error check.
    ASSERT( IsValidVoice(pVoice) );

    // Free up any elements in the voice.
    pHead    = (element*)&pVoice->Elements;
    pElement = pHead->Link.pNext;
    ASSERT( pElement );

    // For every element...
    while( pElement != pHead )
    {
        element* pNextElement = pElement->Link.pNext;
    
        // Release this element and the elements channel.
        if( !ReleaseElement( pElement, TRUE ) )
        {
            return FALSE;
        }

        // Walk the list
        pElement = pNextElement;
    }

    x_AtomicStoreRelease( &s_ReadyVoiceIDs[pVoice - m_FirstVoice], 0 );
    x_AtomicStoreRelease( &s_PublishedVoiceIDs[pVoice - m_FirstVoice], 0 );

    // Remove voice from the used list
    RemoveVoiceFromList( pVoice );

    // Bump sequence.
    Sequence = pVoice->Sequence+1;

    if( Sequence >= 32768 )
    {
        Sequence = 1;
    }

    // Nuke it.
    x_memset( pVoice, 0, sizeof(voice) );

    // Set sequence.
    pVoice->Sequence = Sequence;

    // Put it into the free list?
    if( PutInFreeList )
    {
        // I'm free!!!!
        InsertVoiceIntoList( pVoice, FreeVoices() );
    }

    return TRUE;
}

//==============================================================================

void audio_voice_mgr::SetPackageVoicesDirty( audio_package* pPackage, u32 Bits )
{
    voice*   pHeadVoice;
    voice*   pVoice;

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Get next voice.
        ASSERT( IsValidVoice(pVoice) );
        
        // Does this voice belong to the package?
        if( pVoice->pPackage == pPackage )
        {
            // Set the dirty bit.
            pVoice->Dirty |= Bits;
        }

        // Walk the list.
        pVoice = pVoice->Link.pNext;
    }
}

//==============================================================================

void audio_voice_mgr::UpdateVoiceVolume( audio_package* pPackage )
{
    SetPackageVoicesDirty( pPackage, VOICE_DB_VOLUME_CHANGE );
}

//==============================================================================

void audio_voice_mgr::UpdateVoicePitch( audio_package* pPackage )
{
    SetPackageVoicesDirty( pPackage, VOICE_DB_PITCH_CHANGE );
}

//==============================================================================

void audio_voice_mgr::UpdateVoiceEffectSend( audio_package* pPackage )
{
    SetPackageVoicesDirty( pPackage, VOICE_DB_EFFECTSEND_CHANGE );
}

//==============================================================================

void audio_voice_mgr::AppendElementToVoice( element* pElement, voice* pVoice )
{
    element* pHead;
    
    pHead = (element*)&pVoice->Elements;

    // Put it at end of list...
    pElement->Link.pNext          = pHead;
    pElement->Link.pPrev          = pHead->Link.pPrev;
    pHead->Link.pPrev->Link.pNext = pElement;
    pHead->Link.pPrev             = pElement;

    // Set elements voice pointer.
    pElement->pVoice = pVoice;

    // Element has changed.
    pVoice->Dirty |= VOICE_DB_ELEMENT_CHANGE;
}

//==============================================================================

void audio_voice_mgr::InitSingleVoice( voice* pVoice, audio_package* pPackage )
{
    ASSERT( pVoice );
    ASSERT( pPackage );

    // Set the voices package.
    pVoice->pPackage = pPackage;
    pVoice->bPitchLock = IsPitchLockedDescriptor( pPackage->m_LookupName, pVoice->pDescriptorName );

    // Empty the element list.
    pVoice->Elements.pNext =
    pVoice->Elements.pPrev = (element*)&pVoice->Elements;

    // No pending element, reset state.
    pVoice->pPendingElement = NULL;
    pVoice->State           = STATE_NOT_STARTED; 
    pVoice->CursorTime      = 0.0f;
    pVoice->IsReleasing     = FALSE;
    pVoice->UseReservedStream = FALSE;
    pVoice->DeltaVolume     = 0.0f;

    // Init the recursion depth
    pVoice->RecursionDepth = 0;

    // Clear the segue.
    pVoice->pSegueVoiceNext =
    pVoice->pSegueVoicePrev = NULL;
    pVoice->StartQ          = 0;
    pVoice->ReleaseTime     = 0.0f;

    // Calculate the voices parameters.
    pVoice->NearFalloff = CalculateVoiceNearFalloff( pVoice );
    pVoice->FarFalloff  = CalculateVoiceFarFalloff( pVoice );
    pVoice->NearDiffuse = CalculateVoiceNearDiffuse( pVoice );
    pVoice->FarDiffuse  = CalculateVoiceFarDiffuse( pVoice );
    pVoice->Volume      = CalculateVoiceVolume( pVoice, Runtime().AudioDuckLevel > 0 );
    pVoice->Pitch       = CalculateVoicePitch( pVoice );
    pVoice->EffectSend  = CalculateVoiceEffectSend( pVoice );

    // Now set the voices dirty bits.
    pVoice->Dirty = VOICE_DB_ELEMENT_CHANGE +
                    VOICE_DB_PAN_CHANGE +
                    VOICE_DB_VOLUME_CHANGE +
                    VOICE_DB_PITCH_CHANGE +
                    VOICE_DB_EFFECTSEND_CHANGE +
                    VOICE_DB_ELEMENT_CHANGE;

}

//==============================================================================

void audio_voice_mgr::InitSingleElement( element* pElement )
{
    s32 n;
    f32 Sign;

    // Calculate the elements parameters.
    pElement->PositionalVolume = 1.0f;
    
    // Volume variance defined?
    if( pElement->Params.VolumeVariance )
    {
        ASSERT( pElement->Params.VolumeVariance  > 0.0f );
        ASSERT( pElement->Params.VolumeVariance <= 1.0f );

        // Positive or negative?
        n = x_rand();

        if( n & 1 )
        {
            // Negative...
            Sign = -1.0f;
        }
        else
        {
            // Positive...
            Sign = 1.0f;
        }

        // Set the volume variance.
        pElement->VolumeVariance = 1.0f + (Sign * ((f32)(n % 100)) / 100.0f * pElement->Params.VolumeVariance);
    }
    else
    {
        pElement->VolumeVariance = 1.0f;
    }

    // Pitch variance defined?
    if( pElement->Params.PitchVariance )
    {
        ASSERT( pElement->Params.PitchVariance  > 0.0f );
        ASSERT( pElement->Params.PitchVariance <= 1.0f );

        // Positive or negative?
        n = x_rand();

        if( n & 1 )
        {
            // Negative...
            Sign = -1.0f;
        }
        else
        {
            // Positive...
            Sign = 1.0f;
        }

        // Set the pitch variance.
        pElement->PitchVariance = 1.0f + (Sign * ((f32)(n % 100)) / 100.0f * pElement->Params.PitchVariance);
    }
    else
    {
        pElement->PitchVariance = 1.0f;
    }

    pElement->Volume           = CalculateElementVolume( pElement, Runtime().AudioDuckLevel > 0 );
    pElement->Pitch            = CalculateElementPitch( pElement );
    pElement->EffectSend       = CalculateElementEffectSend( pElement );

}

//==============================================================================

void audio_voice_mgr::UpdateAllVoiceVolumes( void )
{
    voice*   pHeadVoice;
    voice*   pVoice;

    // Get head/tail of active voices, first active voice
    pHeadVoice = UsedVoices();
    pVoice     = pHeadVoice->Link.pNext;

    // For each active voice...
    while( pVoice != pHeadVoice )
    {
        // Gotta be valid...
        ASSERT( IsValidVoice(pVoice) );
        
        // Set the dirty bit.
        pVoice->Dirty |= VOICE_DB_VOLUME_CHANGE;

        // Walk the list. NEXT!
        pVoice = pVoice->Link.pNext;
    }

}

//==============================================================================

void audio_voice_mgr::SetPitchLock( voice* pVoice, xbool bPitchLock )
{

    // Lock or unlock the pitch (as the case may be...)
    pVoice->bPitchLock = bPitchLock;

    element* pHead    = (element*)&pVoice->Elements;
    element* pElement = pHead->Link.pNext;

    while( pElement != pHead )
    {
        if( pElement->pChannel )
        {
            pElement->pChannel->Dirty |= CHANNEL_DB_PITCH;
        }

        pElement = pElement->Link.pNext;
    }
}
