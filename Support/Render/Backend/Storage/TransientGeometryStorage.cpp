//=========================================================================
//
//  TransientGeometryStorage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "TransientGeometryStorage.hpp"
#include "../../FrameGraph/FrameGraph.hpp"
#include "../FrameStorage.hpp"
#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
s32 NextCapacity( s32 Current, s32 Required, s32 Maximum )
{
    if( ( Required <= 0 ) || ( Required > Maximum ) )
    {
        return 0;
    }

    s32 Capacity = Current;

    if( Capacity < 1 )
    {
        Capacity = 1;
    }

    while( Capacity < Required )
    {
        if( Capacity > ( Maximum / 2 ) )
        {
            return Maximum;
        }

        Capacity *= 2;
    }

    return Capacity;
}

//=========================================================================

static
xbool CalculateBytes( s32 Count, s32 Stride, u32& Bytes )
{
    if( ( Count <= 0 ) || ( Stride <= 0 ) )
    {
        Bytes = 0;
        return FALSE;
    }

    const u64 Size = static_cast<u64>( Count ) * static_cast<u64>( Stride );

    if( Size > FrameStorage::MAX_STREAM_BYTES )
    {
        Bytes = 0;
        return FALSE;
    }

    Bytes = static_cast<u32>( Size );

    return TRUE;
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

TransientGeometryStorage::TransientGeometryStorage( void )
    : m_VertexBuffer(),
      m_IndexBuffer(),
      m_VertexStride( 0 ),
      m_MaxVertices( 0 ),
      m_MaxIndices( 0 ),
      m_UploadedVertices( 0 ),
      m_UploadedIndices( 0 ),
      m_isInitialized( FALSE )
{
}

//=========================================================================

xbool TransientGeometryStorage::Init( s32 VertexStride, s32 InitialVertices, s32 InitialIndices )
{
    ASSERT( VertexStride > 0 );
    ASSERT( InitialVertices > 0 );
    ASSERT( InitialIndices > 0 );

    if( ( VertexStride <= 0 ) || ( InitialVertices <= 0 ) || ( InitialIndices <= 0 ) )
    {
        return FALSE;
    }

    if( m_isInitialized )
    {
        Kill();
    }

    m_VertexStride = VertexStride;

    if( !CreateBuffers( InitialVertices, InitialIndices ) )
    {
        DestroyBuffers();
        m_VertexStride = 0;
        return FALSE;
    }

    m_isInitialized = TRUE;

    return TRUE;
}

//=========================================================================

void TransientGeometryStorage::Kill( void )
{
    DestroyBuffers();

    m_VertexStride     = 0;
    m_MaxVertices      = 0;
    m_MaxIndices       = 0;
    m_UploadedVertices = 0;
    m_UploadedIndices  = 0;
    m_isInitialized    = FALSE;
}

//=========================================================================

xbool TransientGeometryStorage::Reserve( s32 nVertices, s32 nIndices )
{
    if( ( nVertices <= 0 ) || ( nIndices <= 0 ) )
    {
        return FALSE;
    }

    return EnsureCapacity( nVertices, nIndices );
}

//=========================================================================

xbool TransientGeometryStorage::CreateBuffers( s32 MaxVertices, s32 MaxIndices )
{
    ASSERT( m_VertexStride > 0 );
    ASSERT( MaxVertices > 0 );
    ASSERT( MaxIndices > 0 );

    if( ( m_VertexStride <= 0 ) || ( MaxVertices <= 0 ) || ( MaxIndices <= 0 ) )
    {
        return FALSE;
    }

    u32 VertexBytes = 0;
    u32 IndexBytes  = 0;

    if( !CalculateBytes( MaxVertices, m_VertexStride, VertexBytes ) ||
        !CalculateBytes( MaxIndices, sizeof( u16 ), IndexBytes ) )
    {
        return FALSE;
    }

    rbuffer_desc VertexDesc;
    VertexDesc.Size       = VertexBytes;
    VertexDesc.Stride     = static_cast<u32>( m_VertexStride );
    VertexDesc.UsageFlags = RBUFFER_USAGE_VERTEX;
    VertexDesc.pDebugName = "TransientGeometryStorageVertex";

    rbuffer_desc IndexDesc;
    IndexDesc.Size       = IndexBytes;
    IndexDesc.Stride     = sizeof( u16 );
    IndexDesc.UsageFlags = RBUFFER_USAGE_INDEX;
    IndexDesc.pDebugName = "TransientGeometryStorageIndex";

    rbuffer NewVertexBuffer;
    rbuffer NewIndexBuffer;

    if( !rbuffer_Create( NewVertexBuffer, VertexDesc ) )
    {
        return FALSE;
    }

    if( !rbuffer_Create( NewIndexBuffer, IndexDesc ) )
    {
        rbuffer_Destroy( NewVertexBuffer );
        return FALSE;
    }

    rbuffer_Swap( m_VertexBuffer, NewVertexBuffer );
    rbuffer_Swap( m_IndexBuffer, NewIndexBuffer );
    rbuffer_Destroy( NewVertexBuffer );
    rbuffer_Destroy( NewIndexBuffer );

    m_MaxVertices      = MaxVertices;
    m_MaxIndices       = MaxIndices;
    m_UploadedVertices = 0;
    m_UploadedIndices  = 0;

    return TRUE;
}

//=========================================================================

void TransientGeometryStorage::DestroyBuffers( void )
{
    rbuffer_Destroy( m_IndexBuffer );
    rbuffer_Destroy( m_VertexBuffer );

    m_MaxVertices      = 0;
    m_MaxIndices       = 0;
    m_UploadedVertices = 0;
    m_UploadedIndices  = 0;
}

//=========================================================================

xbool TransientGeometryStorage::EnsureCapacity( s32 RequiredVertices, s32 RequiredIndices )
{
    if( !m_isInitialized )
    {
        return FALSE;
    }

    if( ( RequiredVertices <= 0 ) || ( RequiredIndices <= 0 ) )
    {
        return FALSE;
    }

    if( ( RequiredVertices <= m_MaxVertices ) && ( RequiredIndices <= m_MaxIndices ) )
    {
        return TRUE;
    }

    const s32 MaximumVertices = static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / m_VertexStride );
    const s32 MaximumIndices  = static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( u16 ) );

    if( ( RequiredVertices > MaximumVertices ) || ( RequiredIndices > MaximumIndices ) )
    {
        return FALSE;
    }

    const s32 NewMaxVertices = NextCapacity( m_MaxVertices, RequiredVertices, MaximumVertices );
    const s32 NewMaxIndices  = NextCapacity( m_MaxIndices, RequiredIndices, MaximumIndices );

    if( ( NewMaxVertices <= 0 ) || ( NewMaxIndices <= 0 ) )
    {
        return FALSE;
    }

    return CreateBuffers( NewMaxVertices, NewMaxIndices );
}

//=========================================================================

xbool TransientGeometryStorage::UploadPrimitive( const Primitive& PrimitiveData, FrameGraphBuilder& Graph )
{
    if( !PrimitiveData.pVertex || !PrimitiveData.pIndex )
    {
        return FALSE;
    }

    if( ( PrimitiveData.nVertices <= 0 ) || ( PrimitiveData.nIndices <= 0 ) )
    {
        return FALSE;
    }

    if( !EnsureCapacity( PrimitiveData.nVertices, PrimitiveData.nIndices ) )
    {
        return FALSE;
    }

    u32 VertexBytes = 0;
    u32 IndexBytes  = 0;

    if( !CalculateBytes( PrimitiveData.nVertices, m_VertexStride, VertexBytes ) ||
        !CalculateBytes( PrimitiveData.nIndices, sizeof( u16 ), IndexBytes ) )
    {
        return FALSE;
    }

    m_UploadedVertices = 0;
    m_UploadedIndices  = 0;

    if( !Graph.QueueUpload( m_VertexBuffer, PrimitiveData.pVertex, VertexBytes, 0, TRUE ) )
    {
        return FALSE;
    }

    if( !Graph.QueueUpload( m_IndexBuffer, PrimitiveData.pIndex, IndexBytes, 0, TRUE ) )
    {
        return FALSE;
    }

    m_UploadedVertices = PrimitiveData.nVertices;
    m_UploadedIndices  = PrimitiveData.nIndices;

    return TRUE;
}

//=========================================================================

void TransientGeometryStorage::DeclareResources( FrameGraphBuilder& Graph, u32 Pass ) const
{
    if( m_VertexBuffer && m_IndexBuffer )
    {
        Graph.Read( Pass, m_VertexBuffer, RENDER_RESOURCE_ACCESS_VERTEX_READ );
        Graph.Read( Pass, m_IndexBuffer, RENDER_RESOURCE_ACCESS_INDEX_READ );
    }
}

//=========================================================================

xbool TransientGeometryStorage::BindBuffers( void ) const
{
    if( !m_isInitialized || ( m_UploadedVertices <= 0 ) || ( m_UploadedIndices <= 0 ) )
    {
        return FALSE;
    }

    if( !rbuffer_BindVertex( m_VertexBuffer, 0 ) )
    {
        return FALSE;
    }

    return rbuffer_BindIndex( m_IndexBuffer, RBUFFER_INDEX_FORMAT_U16 );
}

//=========================================================================

xbool TransientGeometryStorage::DrawIndexed( s32 IndexCount, s32 StartIndex, s32 BaseVertex ) const
{
    if( !m_isInitialized || ( IndexCount <= 0 ) || ( StartIndex < 0 ) || ( BaseVertex < 0 ) ||
        ( StartIndex > ( m_UploadedIndices - IndexCount ) ) || ( BaseVertex >= m_UploadedVertices ) )
    {
        return FALSE;
    }

    return rdraw_DrawIndexed( IndexCount, StartIndex, BaseVertex );
}

//=========================================================================

} // namespace render
