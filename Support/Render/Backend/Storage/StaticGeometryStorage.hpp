//=========================================================================
//
//  StaticGeometryStorage.hpp
//
//=========================================================================

#ifndef STATIC_GEOMETRY_STORAGE_HPP
#define STATIC_GEOMETRY_STORAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "x_array.hpp"
#include "e_RenderBuffer.hpp"

namespace render
{

class FrameGraphBuilder;

//=========================================================================
//  CLASS
//=========================================================================

class StaticGeometryStorage
{
public:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct PreparedMesh
    {

        //-------------------------------------------------------------------------
        //  FUNCTIONS
        //-------------------------------------------------------------------------

        PreparedMesh( void );

        xbool Allocate( s32 nVertices, s32 nIndices, s32 VertexStride );
        void  Clear( void );
        xbool IsValid( void ) const;

        void*       GetVertexData( void );
        const void* GetVertexData( void ) const;
        u16*        GetIndexData( void );
        const u16*  GetIndexData( void ) const;

        void        SetLocalBounds( const bbox& Bounds );
        const bbox& GetLocalBounds( void ) const;
        xbool       HasLocalBounds( void ) const;

        s32 GetVertexCount( void ) const;
        s32 GetIndexCount( void ) const;
        s32 GetVertexStride( void ) const;

    private:

        //-------------------------------------------------------------------------
        //  DATA
        //-------------------------------------------------------------------------

        xarray<byte> m_Vertices;
        xarray<u16>  m_Indices;
        s32          m_nVertices;
        s32          m_VertexStride;
        bbox         m_LocalBounds;
        xbool        m_hasLocalBounds;
    };

    //-------------------------------------------------------------------------

    // Pool IDs and draw offsets select mesh ranges without rebinding buffers.
    struct mesh_range
    {
        s32   FirstIndex;
        s32   IndexCount;
        s32   BaseVertex;
        s32   VertexPool;
        s32   IndexPool;
        bbox  LocalBounds;
        xbool HasLocalBounds;

        mesh_range( void );
    };

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    StaticGeometryStorage( void );
    ~StaticGeometryStorage( void );

    void  Init( s32 VertexStride );
    void  Kill( void );
    xbool IsInitialized( void ) const;

    static xbool PrepareMesh( PreparedMesh& Prepared,
                              const void*   pVertex,
                              s32           nVertices,
                              const u16*    pIndex,
                              s32           nIndices,
                              s32           VertexStride );

    xhandle AddMesh( const PreparedMesh& Prepared );
    void    RemoveMesh( xhandle hMesh );

    xbool BindMesh( xhandle hMesh ) const;
    xbool BindPools( const mesh_range& Range ) const;
    void DeclareResources( FrameGraphBuilder& Graph, u32 Pass, const mesh_range& Range ) const;
    xbool BindVertexIndices( const mesh_range& Range, u32 SlotIndex ) const;
    xbool GetMeshDrawRange( xhandle hMesh, mesh_range& Range ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum
    {
        DEFAULT_VERTEX_POOL_CAPACITY = 65536,
        DEFAULT_INDEX_POOL_CAPACITY  = 65536,
        MAX_MESH_VERTICES            = 65536,
        MAX_BUFFER_BYTES             = 256 * 1024 * 1024
    };

    //-------------------------------------------------------------------------

    struct FreeRange
    {
        s32 Offset;
        s32 Count;

        FreeRange( void );
    };

    //-------------------------------------------------------------------------

    struct Pool
    {
        rbuffer           Buffer;
        rbuffer           VertexIndexBuffer;
        s32               Capacity;
        s32               Stride;
        xarray<FreeRange> FreeRanges;

        Pool( void );
        ~Pool( void );

        Pool( const Pool& )            = delete;
        Pool& operator=( const Pool& ) = delete;
    };

    // Pools keep stable addresses because render-buffer backends retain owner pointers.
    using PoolArray = xarray<Pool*>;

    //-------------------------------------------------------------------------

    struct Allocation
    {
        s32 PoolIndex;
        s32 Offset;
        s32 Count;

        Allocation( void );

        xbool IsValid( void ) const;
    };

    //-------------------------------------------------------------------------

    struct Mesh
    {
        Allocation VertexAllocation;
        Allocation IndexAllocation;
        bbox       LocalBounds;
        xbool      HasLocalBounds;
    };

    //-------------------------------------------------------------------------

    struct RetiredMesh
    {
        Allocation VertexAllocation;
        Allocation IndexAllocation;
        u64        LastFrameSerial;
    };

private:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool AllocateRange( PoolArray&  Pools,
                         s32         Count,
                         s32         Stride,
                         u32         UsageFlags,
                         s32         DefaultCapacity,
                         const char* pDebugName,
                         Allocation& AllocationData );
    void  ReleaseRange( PoolArray& Pools, const Allocation& AllocationData );
    void  CollectRetiredRanges( void );
    xbool CreatePool( PoolArray& Pools, s32 Capacity, s32 Stride, u32 UsageFlags, const char* pDebugName );

private:

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xharray<Mesh>       m_Meshes;
    xarray<RetiredMesh> m_RetiredMeshes;
    xarray<u32>         m_LocalVertexIndices;
    PoolArray           m_VertexPools;
    PoolArray           m_IndexPools;
    s32                 m_VertexStride;
    xbool               m_isInitialized;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
void StaticGeometryStorage::PreparedMesh::SetLocalBounds( const bbox& Bounds )
{
    m_LocalBounds    = Bounds;
    m_hasLocalBounds = TRUE;
}

//=========================================================================

inline
const bbox& StaticGeometryStorage::PreparedMesh::GetLocalBounds( void ) const
{
    return m_LocalBounds;
}

//=========================================================================

inline
xbool StaticGeometryStorage::PreparedMesh::HasLocalBounds( void ) const
{
    return m_hasLocalBounds;
}

//=========================================================================

inline
StaticGeometryStorage::mesh_range::mesh_range( void )
    : FirstIndex( 0 ),
      IndexCount( 0 ),
      BaseVertex( 0 ),
      VertexPool( -1 ),
      IndexPool( -1 ),
      LocalBounds( vector3( 0.0f, 0.0f, 0.0f ) ),
      HasLocalBounds( FALSE )
{
}

//=========================================================================

inline
xbool StaticGeometryStorage::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
StaticGeometryStorage::FreeRange::FreeRange( void )
    : Offset( 0 ),
      Count( 0 )
{
}

//=========================================================================

inline
StaticGeometryStorage::Allocation::Allocation( void )
    : PoolIndex( -1 ),
      Offset( 0 ),
      Count( 0 )
{
}

//=========================================================================

inline
xbool StaticGeometryStorage::Allocation::IsValid( void ) const
{
    return ( PoolIndex >= 0 ) && ( Offset >= 0 ) && ( Count > 0 );
}

} // namespace render

#endif // STATIC_GEOMETRY_STORAGE_HPP
//=========================================================================
