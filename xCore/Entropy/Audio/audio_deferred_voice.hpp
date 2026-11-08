//==============================================================================
//
//  audio_deferred_voice.hpp
//
//==============================================================================

#ifndef AUDIO_DEFERRED_VOICE_HPP
#define AUDIO_DEFERRED_VOICE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Audio.hpp"
#include "x_atomic.hpp"

//==============================================================================
//  CLASSES
//==============================================================================

class audio_deferred_voice_table
{
public:

    // Deferred slot indices must stay outside the physical voice range.
    enum
    {
        CAPACITY    = 512,
        FIRST_INDEX = 0x8000
    };

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    audio_deferred_voice_table( void ) : m_NextSlot( 0 )
    {
        for( s32 i = 0; i < CAPACITY; i++ )
        {
            x_AtomicInit( &m_Entries[i], 0 );
        }
    }

    void Reset( void )
    {
        for( s32 i = 0; i < CAPACITY; i++ )
        {
            u64 Entry = x_AtomicLoadAcquire( &m_Entries[i] );
            x_AtomicStoreRelease( &m_Entries[i], Entry & 0xffffffff00000000ull );
        }

        m_NextSlot = 0;
    }

    //-------------------------------------------------------------------------
    //  HANDLE RESERVATION AND RESOLUTION
    //-------------------------------------------------------------------------

    static xbool IsDeferred( voice_id VoiceID )
    {
        u32 Index = (u32)VoiceID & 0xffff;
        return Index >= FIRST_INDEX && Index < FIRST_INDEX + CAPACITY;
    }

    template <class T>
    voice_id Reserve( const T& Voices )
    {
        m_Lock.Acquire();

        for( s32 n = 0; n < CAPACITY; n++ )
        {
            s32 Slot   = ( m_NextSlot + n ) % CAPACITY;
            u64 Entry  = x_AtomicLoadAcquire( &m_Entries[Slot] );
            u32 Actual = (u32)Entry;
            if( Actual == 0xffffffffu || ( Actual && Voices.IsValidVoiceIdCached( (voice_id)Actual ) ) )
            {
                continue;
            }

            u32 Generation = ( ( (u32)( Entry >> 48 ) ) + 1 ) & 0xffff;
            if( !Generation )
            {
                Generation = 1;
            }
            u32 ID = ( Generation << 16 ) | ( FIRST_INDEX + Slot );

            x_AtomicStoreRelease( &m_Entries[Slot], ( (u64)ID << 32 ) | 0xffffffffu );
            m_NextSlot = ( Slot + 1 ) % CAPACITY;
            m_Lock.Release();
            return (voice_id)ID;
        }

        m_Lock.Release();
        return 0;
    }

    xbool Complete( voice_id DeferredID, voice_id ActualID )
    {
        s32 Slot    = ( (u32)DeferredID & 0xffff ) - FIRST_INDEX;
        u64 Pending = ( (u64)(u32)DeferredID << 32 ) | 0xffffffffu;
        return x_AtomicCompareExchange( &m_Entries[Slot], ( (u64)(u32)DeferredID << 32 ) | (u32)ActualID, Pending ) == Pending;
    }

    voice_id Resolve( voice_id VoiceID, xbool& Pending ) const
    {
        Pending = FALSE;
        if( !IsDeferred( VoiceID ) )
        {
            return VoiceID;
        }

        s32 Slot  = ( (u32)VoiceID & 0xffff ) - FIRST_INDEX;
        u64 Entry = x_AtomicLoadAcquire( &m_Entries[Slot] );
        if( (u32)( Entry >> 32 ) != (u32)VoiceID )
        {
            return 0;
        }
        Pending = (u32)Entry == 0xffffffffu;
        return Pending ? 0 : (voice_id)(u32)Entry;
    }

private:

    xmutex       m_Lock;
    x_atomic_u64 m_Entries[CAPACITY];
    s32          m_NextSlot;
};

//==============================================================================
#endif // AUDIO_DEFERRED_VOICE_HPP
//==============================================================================
