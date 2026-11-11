//=========================================================================
//
//  FrameStorage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameStorage.hpp"

#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

FrameStorage::FrameStorage( void )
    : m_Slots(),
      m_ActiveSlot( 0 ),
      m_ActiveSerial( 0 ),
      m_State( STATE_IDLE ),
      m_isInitialized( FALSE )
{
}

//=========================================================================

FrameStorage::~FrameStorage( void )
{
    Kill();
}

//=========================================================================

xbool FrameStorage::Init( u32 FrameCount )
{
    if( m_isInitialized )
    {
        return TRUE;
    }

    if( FrameCount == 0 )
    {
        FrameCount = eng_GetFramesInFlight();
    }

    if( FrameCount == 0 )
    {
        FrameCount = 3;
    }

    if( FrameCount > 16 )
    {
        return FALSE;
    }

    m_Slots.SetCount( static_cast<s32>( FrameCount ) );
    m_ActiveSlot    = 0;
    m_ActiveSerial  = 0;
    m_State         = STATE_IDLE;
    m_isInitialized = TRUE;

    return TRUE;
}

//=========================================================================

void FrameStorage::Kill( void )
{
    AbortFrame();

    for( s32 i = 0; i < m_Slots.GetCount(); ++i )
    {
        for( s32 StreamId = 0; StreamId < STREAM_COUNT; ++StreamId )
        {
            rbuffer_Destroy( m_Slots[i].GpuBuffers[StreamId] );
            m_Slots[i].GpuCapacities[StreamId] = 0;
        }
    }

    m_Slots.Clear();
    m_ActiveSlot    = 0;
    m_ActiveSerial  = 0;
    m_State         = STATE_IDLE;
    m_isInitialized = FALSE;
}

//=========================================================================

xbool FrameStorage::IsValidStream( Stream StreamId )
{
    return StreamId >= STREAM_RIGID_INSTANCES && StreamId < STREAM_COUNT;
}

//=========================================================================

xbool FrameStorage::BeginFrame( u32 FrameSlot, u64 FrameSerial )
{
    return BeginFrameInternal( FrameSlot, FrameSerial, TRUE );
}

//=========================================================================

xbool FrameStorage::BeginReadOnlyFrame( u32 FrameSlot, u64 FrameSerial )
{
    return BeginFrameInternal( FrameSlot, FrameSerial, TRUE );
}

//=========================================================================

xbool FrameStorage::BeginFrameInternal( u32 FrameSlot, u64 FrameSerial, xbool AllowSameSerialReuse )
{
    if( !m_isInitialized || m_State != STATE_IDLE || FrameSlot >= static_cast<u32>( m_Slots.GetCount() ) )
    {
        return FALSE;
    }

    Slot& SlotState = m_Slots[static_cast<s32>( FrameSlot )];

    if( SlotState.LastSerial != 0 && !eng_IsFrameComplete( SlotState.LastSerial ) &&
        !( AllowSameSerialReuse && ( SlotState.LastSerial == FrameSerial ) ) )
    {
        return FALSE;
    }

    m_ActiveSlot   = FrameSlot;
    m_ActiveSerial = FrameSerial;
    m_State        = STATE_COLLECTING;

    return TRUE;
}

//=========================================================================

xbool FrameStorage::Seal( void )
{
    if( m_State != STATE_COLLECTING )
    {
        return FALSE;
    }

    m_State = STATE_SEALED;
    return TRUE;
}

//=========================================================================

xbool FrameStorage::EndFrame( void )
{
    if( m_State != STATE_SEALED )
    {
        return FALSE;
    }

    m_Slots[static_cast<s32>( m_ActiveSlot )].LastSerial = m_ActiveSerial;

    m_ActiveSerial = 0;
    m_State        = STATE_IDLE;

    return TRUE;
}

//=========================================================================

void FrameStorage::AbortFrame( void )
{
    if( m_State == STATE_IDLE )
    {
        return;
    }

    Slot& SlotState = m_Slots[static_cast<s32>( m_ActiveSlot )];
    // Uploads may already have reached the GPU when graph declaration fails.
    // Retain the serial so this slot cannot be overwritten prematurely.
    SlotState.LastSerial = m_ActiveSerial;

    m_ActiveSerial = 0;
    m_State        = STATE_IDLE;
}

//=========================================================================

rbuffer& FrameStorage::GetGpuBuffer( Stream StreamId )
{
    ASSERT( m_ActiveSlot < static_cast<u32>( m_Slots.GetCount() ) && IsValidStream( StreamId ) );
    return m_Slots[static_cast<s32>( m_ActiveSlot )].GpuBuffers[StreamId];
}

//=========================================================================

const rbuffer& FrameStorage::GetGpuBuffer( Stream StreamId ) const
{
    ASSERT( m_ActiveSlot < static_cast<u32>( m_Slots.GetCount() ) && IsValidStream( StreamId ) );
    return m_Slots[static_cast<s32>( m_ActiveSlot )].GpuBuffers[StreamId];
}

//=========================================================================

u32 FrameStorage::GetGpuCapacity( Stream StreamId ) const
{
    if( m_ActiveSlot >= static_cast<u32>( m_Slots.GetCount() ) || !IsValidStream( StreamId ) )
    {
        return 0;
    }

    return m_Slots[static_cast<s32>( m_ActiveSlot )].GpuCapacities[StreamId];
}

//=========================================================================

void FrameStorage::SetGpuCapacity( Stream StreamId, u32 Capacity )
{
    if( m_ActiveSlot >= static_cast<u32>( m_Slots.GetCount() ) || !IsValidStream( StreamId ) )
    {
        return;
    }

    m_Slots[static_cast<s32>( m_ActiveSlot )].GpuCapacities[StreamId] = Capacity;
}

} // namespace render
