//=========================================================================
//
//  StaticGeometryStorage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "StaticGeometryStorage.hpp"
#include "../../FrameGraph/FrameGraph.hpp"
#include "e_Engine.hpp"

namespace render
{

//=========================================================================
//  PREPARED MESH
//=========================================================================

StaticGeometryStorage::PreparedMesh::PreparedMesh( void )
    : m_Vertices(),
      m_Indices(),
      m_nVertices( 0 ),
      m_VertexStride( 0 ),
      m_LocalBounds( vector3( 0.0f, 0.0f, 0.0f ) ),
      m_hasLocalBounds( FALSE )
{
}

//=========================================================================

xbool StaticGeometryStorage::PreparedMesh::Allocate( s32 nVertices, s32 nIndices, s32 VertexStride )
{
    Clear();

    if( ( nVertices <= 0 ) || ( nIndices <= 0 ) || ( VertexStride <= 0 ) || ( nVertices > MAX_MESH_VERTICES ) ||
        ( nVertices > ( 0x7fffffff / VertexStride ) ) ||
        ( static_cast<u64>( nVertices ) * static_cast<u64>( VertexStride ) > MAX_BUFFER_BYTES ) ||
        ( static_cast<u64>( nIndices ) * sizeof( u16 ) > MAX_BUFFER_BYTES ) )
    {
        return FALSE;
    }

    m_Vertices.SetCount( nVertices * VertexStride );
    m_Indices.SetCount( nIndices );
    m_nVertices    = nVertices;
    m_VertexStride = VertexStride;

    return TRUE;
}

//=========================================================================

void StaticGeometryStorage::PreparedMesh::Clear( void )
{
    m_Vertices.Clear();
    m_Indices.Clear();
    m_nVertices      = 0;
    m_VertexStride   = 0;
    m_hasLocalBounds = FALSE;
}

//=========================================================================

xbool StaticGeometryStorage::PreparedMesh::IsValid( void ) const
{
    if( ( m_nVertices <= 0 ) || ( m_nVertices > MAX_MESH_VERTICES ) || ( m_VertexStride <= 0 ) ||
        ( m_nVertices > ( 0x7fffffff / m_VertexStride ) ) ||
        ( m_Vertices.GetCount() != ( m_nVertices * m_VertexStride ) ) || ( m_Indices.GetCount() <= 0 ) ||
        ( static_cast<u64>( m_Vertices.GetCount() ) > MAX_BUFFER_BYTES ) ||
        ( static_cast<u64>( m_Indices.GetCount() ) * sizeof( u16 ) > MAX_BUFFER_BYTES ) ||
        ( ( m_Indices.GetCount() % 3 ) != 0 ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < m_Indices.GetCount(); ++i )
    {
        if( static_cast<s32>( m_Indices[i] ) >= m_nVertices )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void* StaticGeometryStorage::PreparedMesh::GetVertexData( void )
{
    return m_Vertices.GetPtr();
}

//=========================================================================

const void* StaticGeometryStorage::PreparedMesh::GetVertexData( void ) const
{
    return m_Vertices.GetPtr();
}

//=========================================================================

u16* StaticGeometryStorage::PreparedMesh::GetIndexData( void )
{
    return m_Indices.GetPtr();
}

//=========================================================================

const u16* StaticGeometryStorage::PreparedMesh::GetIndexData( void ) const
{
    return m_Indices.GetPtr();
}

//=========================================================================

s32 StaticGeometryStorage::PreparedMesh::GetVertexCount( void ) const
{
    return m_nVertices;
}

//=========================================================================

s32 StaticGeometryStorage::PreparedMesh::GetIndexCount( void ) const
{
    return m_Indices.GetCount();
}

//=========================================================================

s32 StaticGeometryStorage::PreparedMesh::GetVertexStride( void ) const
{
    return m_VertexStride;
}

//=========================================================================
//  POOL
//=========================================================================

StaticGeometryStorage::Pool::Pool( void )
    : Buffer(),
      VertexIndexBuffer(),
      Capacity( 0 ),
      Stride( 0 ),
      FreeRanges()
{
}

//=========================================================================

StaticGeometryStorage::Pool::~Pool( void )
{
    rbuffer_Destroy( VertexIndexBuffer );
    rbuffer_Destroy( Buffer );
}

//=========================================================================
//  STATIC GEOMETRY STORAGE
//=========================================================================

StaticGeometryStorage::StaticGeometryStorage( void )
    : m_Meshes(),
      m_RetiredMeshes(),
      m_LocalVertexIndices(),
      m_VertexPools(),
      m_IndexPools(),
      m_VertexStride( 0 ),
      m_isInitialized( FALSE )
{
}

//=========================================================================

StaticGeometryStorage::~StaticGeometryStorage( void )
{
    Kill();
}

//=========================================================================

void StaticGeometryStorage::Init( s32 VertexStride )
{
    ASSERT( VertexStride > 0 );

    if( m_isInitialized )
    {
        Kill();
    }

    if( VertexStride <= 0 )
    {
        return;
    }

    m_VertexStride  = VertexStride;
    m_isInitialized = TRUE;
}

//=========================================================================

void StaticGeometryStorage::Kill( void )
{
    m_Meshes.Clear();
    m_RetiredMeshes.Clear();
    m_LocalVertexIndices.Clear();

    for( s32 i = 0; i < m_IndexPools.GetCount(); ++i )
    {
        delete m_IndexPools[i];
    }

    for( s32 i = 0; i < m_VertexPools.GetCount(); ++i )
    {
        delete m_VertexPools[i];
    }

    m_IndexPools.Clear();
    m_VertexPools.Clear();

    m_VertexStride  = 0;
    m_isInitialized = FALSE;
}

//=========================================================================

xbool StaticGeometryStorage::PrepareMesh( PreparedMesh& Prepared,
                                          const void*   pVertex,
                                          s32           nVertices,
                                          const u16*    pIndex,
                                          s32           nIndices,
                                          s32           VertexStride )
{
    Prepared.Clear();

    if( !pVertex || !pIndex )
    {
        return FALSE;
    }

    if( !Prepared.Allocate( nVertices, nIndices, VertexStride ) )
    {
        return FALSE;
    }

    x_memcpy( Prepared.GetVertexData(), pVertex, nVertices * VertexStride );
    x_memcpy( Prepared.GetIndexData(), pIndex, sizeof( u16 ) * nIndices );

    if( !Prepared.IsValid() )
    {
        Prepared.Clear();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool StaticGeometryStorage::CreatePool( PoolArray&  Pools,
                                         s32         Capacity,
                                         s32         Stride,
                                         u32         UsageFlags,
                                         const char* pDebugName )
{
    if( ( Capacity <= 0 ) || ( Stride <= 0 ) ||
        ( static_cast<u32>( Capacity ) > ( 0xffffffffu / static_cast<u32>( Stride ) ) ) ||
        ( static_cast<u64>( Capacity ) * static_cast<u64>( Stride ) > MAX_BUFFER_BYTES ) ||
        ( ( UsageFlags & RBUFFER_USAGE_VERTEX ) &&
          ( static_cast<u64>( Capacity ) * sizeof( u32 ) > MAX_BUFFER_BYTES ) ) )
    {
        return FALSE;
    }

    Pool* pPool = new Pool;
    ASSERT( pPool );
    if( !pPool )
    {
        return FALSE;
    }

    rbuffer_desc Desc;
    Desc.Size       = static_cast<u32>( Capacity ) * static_cast<u32>( Stride );
    Desc.Stride     = static_cast<u32>( Stride );
    Desc.UsageFlags = UsageFlags;
    Desc.pDebugName = pDebugName;

    if( !rbuffer_Create( pPool->Buffer, Desc ) )
    {
        delete pPool;
        return FALSE;
    }

    if( UsageFlags & RBUFFER_USAGE_VERTEX )
    {
        rbuffer_desc IndexDesc;
        IndexDesc.Size       = static_cast<u32>( Capacity ) * sizeof( u32 );
        IndexDesc.Stride     = sizeof( u32 );
        IndexDesc.UsageFlags = RBUFFER_USAGE_VERTEX;
        IndexDesc.pDebugName = "StaticVertexPoolIndices";

        if( !rbuffer_Create( pPool->VertexIndexBuffer, IndexDesc ) )
        {
            rbuffer_Destroy( pPool->Buffer );
            delete pPool;
            return FALSE;
        }
    }

    pPool->Capacity = Capacity;
    pPool->Stride   = Stride;

    FreeRange& Range = pPool->FreeRanges.Append();
    Range.Offset     = 0;
    Range.Count      = Capacity;

    Pools.Append() = pPool;

    return TRUE;
}

//=========================================================================

xbool StaticGeometryStorage::AllocateRange( PoolArray&  Pools,
                                            s32         Count,
                                            s32         Stride,
                                            u32         UsageFlags,
                                            s32         DefaultCapacity,
                                            const char* pDebugName,
                                            Allocation& OutAllocation )
{
    CollectRetiredRanges();
    OutAllocation = Allocation();

    if( ( Count <= 0 ) || ( Stride <= 0 ) || ( DefaultCapacity <= 0 ) )
    {
        return FALSE;
    }

    s32 BestPool  = -1;
    s32 BestRange = -1;
    s32 BestWaste = 0x7fffffff;

    for( s32 iPool = 0; iPool < Pools.GetCount(); ++iPool )
    {
        Pool& PoolData = *Pools[iPool];

        if( PoolData.Stride != Stride )
        {
            continue;
        }

        for( s32 iRange = 0; iRange < PoolData.FreeRanges.GetCount(); ++iRange )
        {
            const FreeRange& Range = PoolData.FreeRanges[iRange];

            if( Range.Count < Count )
            {
                continue;
            }

            const s32 Waste = Range.Count - Count;

            if( Waste < BestWaste )
            {
                BestPool  = iPool;
                BestRange = iRange;
                BestWaste = Waste;
            }
        }
    }

    if( BestPool < 0 )
    {
        const s32 Capacity = MAX( DefaultCapacity, Count );

        if( !CreatePool( Pools, Capacity, Stride, UsageFlags, pDebugName ) )
        {
            return FALSE;
        }

        BestPool  = Pools.GetCount() - 1;
        BestRange = 0;
    }

    Pool&      PoolData = *Pools[BestPool];
    FreeRange& Range    = PoolData.FreeRanges[BestRange];

    OutAllocation.PoolIndex = BestPool;
    OutAllocation.Offset    = Range.Offset;
    OutAllocation.Count     = Count;

    if( Range.Count == Count )
    {
        PoolData.FreeRanges.Delete( BestRange );
    }
    else
    {
        Range.Offset += Count;
        Range.Count -= Count;
    }

    return TRUE;
}

//=========================================================================

void StaticGeometryStorage::CollectRetiredRanges( void )
{
    for( s32 i = m_RetiredMeshes.GetCount() - 1; i >= 0; --i )
    {
        const RetiredMesh& Retired = m_RetiredMeshes[i];

        if( !eng_IsFrameComplete( Retired.LastFrameSerial ) )
        {
            continue;
        }

        ReleaseRange( m_IndexPools, Retired.IndexAllocation );
        ReleaseRange( m_VertexPools, Retired.VertexAllocation );
        m_RetiredMeshes.Delete( i );
    }
}

//=========================================================================

void StaticGeometryStorage::ReleaseRange( PoolArray& Pools, const Allocation& AllocationData )
{
    if( !AllocationData.IsValid() )
    {
        return;
    }

    ASSERT( AllocationData.PoolIndex < Pools.GetCount() );
    if( ( AllocationData.PoolIndex < 0 ) || ( AllocationData.PoolIndex >= Pools.GetCount() ) )
    {
        return;
    }

    Pool& PoolData = *Pools[AllocationData.PoolIndex];

    if( ( AllocationData.Count <= 0 ) || ( AllocationData.Offset < 0 ) ||
        ( AllocationData.Offset > ( PoolData.Capacity - AllocationData.Count ) ) )
    {
        return;
    }

    s32 InsertAt = 0;

    while( ( InsertAt < PoolData.FreeRanges.GetCount() ) &&
           ( PoolData.FreeRanges[InsertAt].Offset < AllocationData.Offset ) )
    {
        ++InsertAt;
    }

    if( InsertAt > 0 )
    {
        const FreeRange& Previous = PoolData.FreeRanges[InsertAt - 1];
        ASSERT( ( Previous.Offset + Previous.Count ) <= AllocationData.Offset );
    }

    if( InsertAt < PoolData.FreeRanges.GetCount() )
    {
        const FreeRange& Next = PoolData.FreeRanges[InsertAt];
        ASSERT( ( AllocationData.Offset + AllocationData.Count ) <= Next.Offset );
    }

    FreeRange& Range = PoolData.FreeRanges.Insert( InsertAt );
    Range.Offset     = AllocationData.Offset;
    Range.Count      = AllocationData.Count;

    if( InsertAt > 0 )
    {
        FreeRange& Previous = PoolData.FreeRanges[InsertAt - 1];

        if( ( Previous.Offset + Previous.Count ) == Range.Offset )
        {
            Previous.Count += Range.Count;
            PoolData.FreeRanges.Delete( InsertAt );
            --InsertAt;
        }
    }

    if( ( InsertAt + 1 ) < PoolData.FreeRanges.GetCount() )
    {
        FreeRange&       Current = PoolData.FreeRanges[InsertAt];
        const FreeRange& Next    = PoolData.FreeRanges[InsertAt + 1];

        if( ( Current.Offset + Current.Count ) == Next.Offset )
        {
            Current.Count += Next.Count;
            PoolData.FreeRanges.Delete( InsertAt + 1 );
        }
    }
}

//=========================================================================

xhandle StaticGeometryStorage::AddMesh( const PreparedMesh& Prepared )
{
    ASSERT( m_isInitialized );
    ASSERT( Prepared.GetVertexStride() == m_VertexStride );
    ASSERT( Prepared.IsValid() );

    if( !m_isInitialized || ( Prepared.GetVertexStride() != m_VertexStride ) || !Prepared.IsValid() )
    {
        x_throw( "Invalid prepared static geometry" );
    }

    Allocation VertexAllocation;
    Allocation IndexAllocation;
    xhandle    hMesh;
    hMesh.Handle = HNULL;

    xbool isUploadReferenced = FALSE;

    x_try;

        if( !AllocateRange( m_VertexPools, Prepared.GetVertexCount(), m_VertexStride, RBUFFER_USAGE_VERTEX,
                            DEFAULT_VERTEX_POOL_CAPACITY, "StaticGeomVertexPool", VertexAllocation ) )
        {
            x_throw( "Unable to allocate static vertex range" );
        }

        if( !AllocateRange( m_IndexPools, Prepared.GetIndexCount(), sizeof( u16 ), RBUFFER_USAGE_INDEX,
                            DEFAULT_INDEX_POOL_CAPACITY, "StaticGeomIndexPool", IndexAllocation ) )
        {
            x_throw( "Unable to allocate static index range" );
        }

        Pool& VertexPool = *m_VertexPools[VertexAllocation.PoolIndex];
        Pool& IndexPool  = *m_IndexPools[IndexAllocation.PoolIndex];

        const u32 VertexUploadOffset = static_cast<u32>( VertexAllocation.Offset ) * static_cast<u32>( m_VertexStride );
        const u32 VertexBytes        = static_cast<u32>( VertexAllocation.Count ) * static_cast<u32>( m_VertexStride );
        const u32 IndexUploadOffset  = static_cast<u32>( IndexAllocation.Offset ) * static_cast<u32>( sizeof( u16 ) );
        const u32 IndexBytes         = static_cast<u32>( IndexAllocation.Count ) * static_cast<u32>( sizeof( u16 ) );

        m_LocalVertexIndices.SetCount( VertexAllocation.Count );

        for( s32 i = 0; i < VertexAllocation.Count; ++i )
        {
            m_LocalVertexIndices[i] = i;
        }

        rbuffer_upload_batch_item Uploads[3];
        Uploads[0].pBuffer       = &VertexPool.Buffer;
        Uploads[0].Upload.pData  = Prepared.GetVertexData();
        Uploads[0].Upload.Size   = VertexBytes;
        Uploads[0].Upload.Offset = VertexUploadOffset;
        Uploads[0].Upload.bCycle = FALSE;

        Uploads[1].pBuffer       = &VertexPool.VertexIndexBuffer;
        Uploads[1].Upload.pData  = m_LocalVertexIndices.GetPtr();
        Uploads[1].Upload.Size   = VertexAllocation.Count * sizeof( u32 );
        Uploads[1].Upload.Offset = VertexAllocation.Offset * sizeof( u32 );
        Uploads[1].Upload.bCycle = FALSE;

        Uploads[2].pBuffer       = &IndexPool.Buffer;
        Uploads[2].Upload.pData  = Prepared.GetIndexData();
        Uploads[2].Upload.Size   = IndexBytes;
        Uploads[2].Upload.Offset = IndexUploadOffset;
        Uploads[2].Upload.bCycle = FALSE;

        if( !rbuffer_UploadBatch( Uploads, 3 ) )
        {
            x_throw( "Unable to upload static geometry data" );
        }

        isUploadReferenced = TRUE;

        Mesh& MeshData            = m_Meshes.Add( hMesh );
        MeshData.VertexAllocation = VertexAllocation;
        MeshData.IndexAllocation  = IndexAllocation;
        MeshData.LocalBounds      = Prepared.GetLocalBounds();
        MeshData.HasLocalBounds   = Prepared.HasLocalBounds();

    x_catch_begin;

        if( hMesh.IsNonNull() )
        {
            m_Meshes.DeleteByHandle( hMesh );
        }

        const u64 Serial = eng_GetFrameLifetimeSerial();

        if( isUploadReferenced && Serial && !eng_IsFrameComplete( Serial ) )
        {
            RetiredMesh& Retired     = m_RetiredMeshes.Append();
            Retired.IndexAllocation  = IndexAllocation;
            Retired.VertexAllocation = VertexAllocation;
            Retired.LastFrameSerial  = Serial;
        }
        else
        {
            if( isUploadReferenced )
            {
                eng_Sync();
            }

            ReleaseRange( m_IndexPools, IndexAllocation );
            ReleaseRange( m_VertexPools, VertexAllocation );
        }

    x_catch_end_ret;

    return hMesh;
}

//=========================================================================

void StaticGeometryStorage::RemoveMesh( xhandle hMesh )
{
    const Mesh MeshData = m_Meshes( hMesh );
    const u64  Serial   = eng_GetFrameLifetimeSerial();

    if( Serial && !eng_IsFrameComplete( Serial ) )
    {
        RetiredMesh& Retired     = m_RetiredMeshes.Append();
        Retired.IndexAllocation  = MeshData.IndexAllocation;
        Retired.VertexAllocation = MeshData.VertexAllocation;
        Retired.LastFrameSerial  = Serial;
    }
    else
    {
        ReleaseRange( m_IndexPools, MeshData.IndexAllocation );
        ReleaseRange( m_VertexPools, MeshData.VertexAllocation );
    }

    m_Meshes.DeleteByHandle( hMesh );
}

//=========================================================================

xbool StaticGeometryStorage::BindMesh( xhandle hMesh ) const
{
    mesh_range Range;

    if( !GetMeshDrawRange( hMesh, Range ) )
    {
        return FALSE;
    }

    return BindPools( Range );
}

//=========================================================================

void StaticGeometryStorage::DeclareResources( FrameGraphBuilder& Graph, u32 Pass, const mesh_range& Range ) const
{
    if( Range.VertexPool >= 0 && Range.VertexPool < m_VertexPools.GetCount() &&
        Range.IndexPool >= 0 && Range.IndexPool < m_IndexPools.GetCount() )
    {
        const Pool& Vertices = *m_VertexPools[Range.VertexPool];
        const Pool& Indices  = *m_IndexPools[Range.IndexPool];
        Graph.Read( Pass, Vertices.Buffer, RENDER_RESOURCE_ACCESS_VERTEX_READ );
        Graph.Read( Pass, Indices.Buffer, RENDER_RESOURCE_ACCESS_INDEX_READ );
        if( Vertices.VertexIndexBuffer )
        {
            Graph.Read( Pass, Vertices.VertexIndexBuffer, RENDER_RESOURCE_ACCESS_VERTEX_READ );
        }
    }
}

//=========================================================================

xbool StaticGeometryStorage::BindPools( const mesh_range& Range ) const
{
    if( ( Range.VertexPool < 0 ) || ( Range.VertexPool >= m_VertexPools.GetCount() ) || ( Range.IndexPool < 0 ) ||
        ( Range.IndexPool >= m_IndexPools.GetCount() ) )
    {
        return FALSE;
    }

    const Pool& VertexPool = *m_VertexPools[Range.VertexPool];
    const Pool& IndexPool  = *m_IndexPools[Range.IndexPool];

    return rbuffer_BindVertex( VertexPool.Buffer, 0, 0 ) &&
           rbuffer_BindIndex( IndexPool.Buffer, RBUFFER_INDEX_FORMAT_U16, 0 );
}

//=========================================================================

xbool StaticGeometryStorage::BindVertexIndices( const mesh_range& Range, u32 SlotIndex ) const
{
    if( ( Range.VertexPool < 0 ) || ( Range.VertexPool >= m_VertexPools.GetCount() ) )
    {
        return FALSE;
    }

    const Pool& VertexPool = *m_VertexPools[Range.VertexPool];
    return rbuffer_BindVertex( VertexPool.VertexIndexBuffer, SlotIndex, 0 );
}

//=========================================================================

xbool StaticGeometryStorage::GetMeshDrawRange( xhandle hMesh, mesh_range& Range ) const
{
    Range = mesh_range();

    const Mesh& MeshData = m_Meshes( hMesh );

    if( !MeshData.VertexAllocation.IsValid() || !MeshData.IndexAllocation.IsValid() ||
        ( MeshData.VertexAllocation.PoolIndex >= m_VertexPools.GetCount() ) ||
        ( MeshData.IndexAllocation.PoolIndex >= m_IndexPools.GetCount() ) )
    {
        return FALSE;
    }

    Range.FirstIndex     = MeshData.IndexAllocation.Offset;
    Range.IndexCount     = MeshData.IndexAllocation.Count;
    Range.BaseVertex     = MeshData.VertexAllocation.Offset;
    Range.VertexPool     = MeshData.VertexAllocation.PoolIndex;
    Range.IndexPool      = MeshData.IndexAllocation.PoolIndex;
    Range.LocalBounds    = MeshData.LocalBounds;
    Range.HasLocalBounds = MeshData.HasLocalBounds;

    return TRUE;
}

//=========================================================================

} // namespace render
