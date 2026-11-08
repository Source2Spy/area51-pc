//=========================================================================
//
//  FrameStorage.hpp
//
//  Fence-owned GPU buffers for the render backend's typed upload streams.
//
//=========================================================================

#ifndef RENDER_FRAME_STORAGE_HPP
#define RENDER_FRAME_STORAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_array.hpp"
#include "e_Engine.hpp"
#include "e_RenderBuffer.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class FrameStorage
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum Stream
    {
        STREAM_RIGID_INSTANCES = 0,
        STREAM_RIGID_INSTANCE_INDICES,
        STREAM_RIGID_COLORS,
        STREAM_RIGID_INDIRECT,
        STREAM_SKIN_INSTANCES,
        STREAM_SKIN_BONES,
        STREAM_SKIN_DRAW_INSTANCES,
        STREAM_SKIN_INDIRECT,
        STREAM_DYNAMIC_VERTICES,
        STREAM_DYNAMIC_INDICES,
        STREAM_LIGHTING,
        STREAM_PROJECTION_LIGHTING,
        STREAM_AMBIENT_LIGHTING,
        STREAM_COUNT
    };

    static const u32 MAX_STREAM_BYTES = 256u * 1024u * 1024u;

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    FrameStorage( void );
    ~FrameStorage( void );

    xbool Init( u32 FrameCount = 0 );
    void  Kill( void );

    xbool BeginFrame( u32 FrameSlot, u64 FrameSerial );
    xbool BeginReadOnlyFrame( u32 FrameSlot, u64 FrameSerial );
    xbool Seal( void );
    xbool EndFrame( void );
    void  AbortFrame( void );

    rbuffer&       GetGpuBuffer( Stream StreamId );
    const rbuffer& GetGpuBuffer( Stream StreamId ) const;
    u32            GetGpuCapacity( Stream StreamId ) const;
    void           SetGpuCapacity( Stream StreamId, u32 Capacity );

    xbool IsInitialized( void ) const;
    xbool IsCollecting( void ) const;
    xbool IsSealed( void ) const;
    u32   GetFrameSlot( void ) const;
    u64   GetFrameSerial( void ) const;
    u32   GetFrameCount( void ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum State
    {
        STATE_IDLE = 0,
        STATE_COLLECTING,
        STATE_SEALED
    };

    //-------------------------------------------------------------------------

    struct Slot
    {
        rbuffer GpuBuffers[STREAM_COUNT];
        u32     GpuCapacities[STREAM_COUNT];
        u64     LastSerial;

        Slot( void );
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    static xbool IsValidStream( Stream StreamId );
    xbool        BeginFrameInternal( u32 FrameSlot, u64 FrameSerial, xbool AllowSameSerialReuse );

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<Slot> m_Slots;
    u32          m_ActiveSlot;
    u64          m_ActiveSerial;
    State        m_State;
    xbool        m_isInitialized;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
xbool FrameStorage::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
xbool FrameStorage::IsCollecting( void ) const
{
    return m_State == STATE_COLLECTING;
}

//=========================================================================

inline
xbool FrameStorage::IsSealed( void ) const
{
    return m_State == STATE_SEALED;
}

//=========================================================================

inline
u32 FrameStorage::GetFrameSlot( void ) const
{
    return m_ActiveSlot;
}

//=========================================================================

inline
u64 FrameStorage::GetFrameSerial( void ) const
{
    return m_ActiveSerial;
}

//=========================================================================

inline
u32 FrameStorage::GetFrameCount( void ) const
{
    return static_cast<u32>( m_Slots.GetCount() );
}

//=========================================================================

inline
FrameStorage::Slot::Slot( void )
    : GpuBuffers(),
      GpuCapacities(),
      LastSerial( 0 )
{
}

} // namespace render

#endif // RENDER_FRAME_STORAGE_HPP
