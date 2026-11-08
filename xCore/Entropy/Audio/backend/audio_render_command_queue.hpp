//==============================================================================
//
//  audio_render_command_queue.hpp
//
//==============================================================================

#ifndef AUDIO_RENDER_COMMAND_QUEUE_HPP
#define AUDIO_RENDER_COMMAND_QUEUE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/backend/audio_render_command.hpp"

//==============================================================================
//  AUDIO RENDER COMMAND QUEUE
//==============================================================================

// Single producer / single consumer queue. Producer is audio_backend::QueueRenderCommand()
// on the audio service thread; consumer is the SDL render callback.
template<s32 MAX_COMMANDS>
class audio_render_command_queue
{
public:
                    audio_render_command_queue ( void );

    void            Clear                      ( void );
    xbool           Push                       ( const audio_render_command& Command );
    xbool           Pop                        ( audio_render_command& Command );

private:
                    audio_render_command_queue ( const audio_render_command_queue& );
    audio_render_command_queue& operator=      ( const audio_render_command_queue& );

    u32             NextIndex                  ( u32 Index ) const;

    audio_render_command m_Commands[MAX_COMMANDS];
    x_atomic_u32         m_Read;
    x_atomic_u32         m_Write;
};

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

template<s32 MAX_COMMANDS>
audio_render_command_queue<MAX_COMMANDS>::audio_render_command_queue( void )
{
    ASSERT( MAX_COMMANDS > 1 );
    Clear();
}

//==============================================================================

template<s32 MAX_COMMANDS>
void audio_render_command_queue<MAX_COMMANDS>::Clear( void )
{
    x_AtomicStoreRelaxed( &m_Read,  0 );
    x_AtomicStoreRelaxed( &m_Write, 0 );
}

//==============================================================================

template<s32 MAX_COMMANDS>
u32 audio_render_command_queue<MAX_COMMANDS>::NextIndex( u32 Index ) const
{
    Index++;
    if( Index >= (u32)MAX_COMMANDS )
    {
        Index = 0;
    }

    return Index;
}

//==============================================================================

template<s32 MAX_COMMANDS>
xbool audio_render_command_queue<MAX_COMMANDS>::Push( const audio_render_command& Command )
{
    u32 Write = x_AtomicLoadRelaxed( &m_Write );
    u32 Next  = NextIndex( Write );
    u32 Read  = x_AtomicLoadAcquire( &m_Read );

    if( Next == Read )
    {
        return FALSE;
    }

    m_Commands[Write] = Command;
    x_AtomicStoreRelease( &m_Write, Next );

    return TRUE;
}

//==============================================================================

template<s32 MAX_COMMANDS>
xbool audio_render_command_queue<MAX_COMMANDS>::Pop( audio_render_command& Command )
{
    u32 Read  = x_AtomicLoadRelaxed( &m_Read );
    u32 Write = x_AtomicLoadAcquire( &m_Write );

    if( Read == Write )
    {
        return FALSE;
    }

    Command = m_Commands[Read];
    x_AtomicStoreRelease( &m_Read, NextIndex( Read ) );

    return TRUE;
}

//==============================================================================
#endif // AUDIO_RENDER_COMMAND_QUEUE_HPP
//==============================================================================
