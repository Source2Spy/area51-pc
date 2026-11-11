//=========================================================================
//
//  ClothTessellation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ClothTessellation.hpp"
#include "e_Engine.hpp"
#include "../../FrameGraph/FrameGraph.hpp"
#include "a51_compute_cloth_tessellate.hpp"
#include <cstddef>

namespace render
{

namespace
{

struct ClothTessellationConstants
{
    u32 TriangleCount;
    u32 Subdivisions;
    u32 VertexStride;
    u32 ColorOffset;
    u32 NormalOffset;
    u32 UVOffset;
    u32 Padding[2];
    vector4 BoundsMin;
    vector4 BoundsMax;
};

static_assert( sizeof( ClothTessellationConstants ) == 64, "Cloth tessellation constants must match HLSL" );
static_assert( offsetof( dynamic_geom_vertex, Position ) == 0, "Cloth positions must begin the vertex" );
static_assert( sizeof( dynamic_geom_vertex ) % 4 == 0, "Cloth vertex stride must allow byte address stores" );

//=========================================================================

u32 TriangleRowStart( u32 Row, u32 Subdivisions )
{
    return Row * ( 2 * Subdivisions + 3 - Row ) / 2;
}

//=========================================================================

xbool CreateClothBuffer( rbuffer& Buffer, u32 Size, u32 Stride, u32 Usage, const char* pName )
{
    rbuffer_desc Desc;
    Desc.Size       = Size;
    Desc.Stride     = Stride;
    Desc.UsageFlags = Usage;
    Desc.pDebugName = pName;
    return rbuffer_Create( Buffer, Desc );
}

} // namespace

//=========================================================================

ClothTessellation::ClothTessellation( void ) :
    m_SourceVertexCount( 0 ),
    m_TriangleCount    ( 0 ),
    m_Subdivisions     ( 0 ),
    m_Revision         ( 0 ),
    m_FrameSerial      ( 0 ),
    m_isReady          ( FALSE )
{
}

//=========================================================================

ClothTessellation::~ClothTessellation( void )
{
    Kill();
}

//=========================================================================

xbool ClothTessellation::Init( s32 VertexCount, const u16* pIndices, s32 IndexCount )
{
    Kill();
    if( VertexCount <= 0 || VertexCount > 65535 || !pIndices || IndexCount <= 0 || IndexCount % 3 )
    {
        return FALSE;
    }

    m_SourceVertexCount = static_cast<u32>( VertexCount );
    m_TriangleCount     = static_cast<u32>( IndexCount / 3 );
    m_Subdivisions      = 8;
    while( m_Subdivisions > 1 &&
           static_cast<u64>( m_TriangleCount ) * ( m_Subdivisions + 1 ) * ( m_Subdivisions + 2 ) / 2 > 65535 )
    {
        m_Subdivisions /= 2;
    }
    if( m_Subdivisions == 1 )
    {
        return TRUE;
    }

    const u32 VerticesPerTriangle = ( m_Subdivisions + 1 ) * ( m_Subdivisions + 2 ) / 2;
    m_Mesh.VertexCount = m_TriangleCount * VerticesPerTriangle;
    m_Mesh.IndexCount  = m_TriangleCount * m_Subdivisions * m_Subdivisions * 3;
    xarray<u32> Triangles;
    Triangles.SetCount( IndexCount );
    for( s32 i = 0; i < IndexCount; ++i )
    {
        if( pIndices[i] >= VertexCount )
        {
            Kill();
            return FALSE;
        }
        Triangles[i] = pIndices[i];
    }

    xarray<u16> Indices;
    Indices.SetCount( m_Mesh.IndexCount );
    s32 Cursor = 0;
    for( u32 Triangle = 0; Triangle < m_TriangleCount; ++Triangle )
    {
        const u32 Base = Triangle * VerticesPerTriangle;
        for( u32 Row = 0; Row < m_Subdivisions; ++Row )
        {
            for( u32 Column = 0; Column < m_Subdivisions - Row; ++Column )
            {
                const u32 A = Base + TriangleRowStart( Row, m_Subdivisions ) + Column;
                const u32 B = Base + TriangleRowStart( Row + 1, m_Subdivisions ) + Column;
                Indices[Cursor++] = static_cast<u16>( A );
                Indices[Cursor++] = static_cast<u16>( B );
                Indices[Cursor++] = static_cast<u16>( A + 1 );
                if( Column + 1 < m_Subdivisions - Row )
                {
                    Indices[Cursor++] = static_cast<u16>( A + 1 );
                    Indices[Cursor++] = static_cast<u16>( B );
                    Indices[Cursor++] = static_cast<u16>( B + 1 );
                }
            }
        }
    }
    ASSERT( Cursor == m_Mesh.IndexCount );

    shader_desc Shader;
    if( !A51Compute_cloth_tessellate_GetDesc( Shader, shader_GetPreferredFormat() ) )
    {
        Kill();
        return FALSE;
    }
    Shader.pDebugName = "ClothTessellation";
    if( !shader_Create( m_Shader, Shader ) ||
        !CreateClothBuffer( m_SourceVertices, VertexCount * sizeof( dynamic_geom_vertex ),
                            sizeof( dynamic_geom_vertex ), RBUFFER_USAGE_COMPUTE_STORAGE_READ, "ClothSourceVertices" ) ||
        !CreateClothBuffer( m_SourceTriangles, IndexCount * sizeof( u32 ), sizeof( u32 ) * 3,
                            RBUFFER_USAGE_COMPUTE_STORAGE_READ, "ClothSourceTriangles" ) ||
        !CreateClothBuffer( m_Vertices, m_Mesh.VertexCount * sizeof( dynamic_geom_vertex ),
                            sizeof( dynamic_geom_vertex ), RBUFFER_USAGE_VERTEX | RBUFFER_USAGE_COMPUTE_STORAGE_WRITE,
                            "ClothTessellatedVertices" ) ||
        !CreateClothBuffer( m_Indices, m_Mesh.IndexCount * sizeof( u16 ), sizeof( u16 ),
                            RBUFFER_USAGE_INDEX, "ClothTessellatedIndices" ) ||
        !rbuffer_Upload( m_SourceTriangles, Triangles.GetPtr(), IndexCount * sizeof( u32 ) ) ||
        !rbuffer_Upload( m_Indices, Indices.GetPtr(), m_Mesh.IndexCount * sizeof( u16 ) ) )
    {
        Kill();
        return FALSE;
    }

    m_Mesh.pVertices = &m_Vertices;
    m_Mesh.pIndices  = &m_Indices;
    return TRUE;
}

//=========================================================================

void ClothTessellation::Kill( void )
{
    shader_Destroy( m_Shader );
    rbuffer_Destroy( m_SourceVertices );
    rbuffer_Destroy( m_SourceTriangles );
    rbuffer_Destroy( m_Vertices );
    rbuffer_Destroy( m_Indices );
    m_Mesh              = dynamic_geom_gpu_mesh();
    m_SourceVertexCount = 0;
    m_TriangleCount     = 0;
    m_Subdivisions      = 0;
    m_Revision          = 0;
    m_FrameSerial       = 0;
    m_isReady           = FALSE;
}

//=========================================================================

struct ClothTessellation::PassData
{
    ClothTessellation*          pOwner;
    ClothTessellationConstants Constants;
    RenderResourceHandle       Vertices;
    RenderResourceHandle       Triangles;
    u64                        FrameSerial;
    u32                        Revision;
};

//=========================================================================

xbool ClothTessellation::Declare( FrameGraphBuilder& Graph, const dynamic_geom_vertex* pVertices, s32 VertexCount,
                                  const bbox& Bounds, u32 Revision )
{
    const u64 FrameSerial = eng_GetCurrentFrameSerial();
    if( !m_Shader || !pVertices || !FrameSerial || VertexCount <= 0 ||
        static_cast<u32>( VertexCount ) != m_SourceVertexCount )
    {
        return FALSE;
    }

    const xbool Current = m_isReady && m_FrameSerial == FrameSerial && m_Revision == Revision;
    const RenderResourceHandle Output = Graph.ImportExternal( m_Vertices, "ClothTessellatedVertices", Current );
    if( !Output.IsValid() )
    {
        return FALSE;
    }
    if( Current )
    {
        return TRUE;
    }

    rbuffer_upload_desc Upload;
    Upload.pData  = pVertices;
    Upload.Size   = m_SourceVertexCount * sizeof( dynamic_geom_vertex );
    Upload.bCycle = TRUE;
    if( !Graph.QueueUpload( m_SourceVertices, Upload ) )
    {
        return FALSE;
    }

    PassData Data = {};
    Data.pOwner                  = this;
    Data.FrameSerial             = FrameSerial;
    Data.Revision                = Revision;
    Data.Vertices                = Graph.ImportExternal( m_SourceVertices, "ClothSourceVertices" );
    Data.Triangles               = Graph.ImportExternal( m_SourceTriangles, "ClothSourceTriangles" );
    Data.Constants.TriangleCount = m_TriangleCount;
    Data.Constants.Subdivisions  = m_Subdivisions;
    Data.Constants.VertexStride  = sizeof( dynamic_geom_vertex );
    Data.Constants.ColorOffset   = offsetof( dynamic_geom_vertex, Color );
    Data.Constants.NormalOffset  = offsetof( dynamic_geom_vertex, Normal );
    Data.Constants.UVOffset      = offsetof( dynamic_geom_vertex, UV );
    Data.Constants.BoundsMin.Set( Bounds.Min.GetX(), Bounds.Min.GetY(), Bounds.Min.GetZ(), 0 );
    Data.Constants.BoundsMax.Set( Bounds.Max.GetX(), Bounds.Max.GetY(), Bounds.Max.GetZ(), 0 );

    RenderPassDesc Desc;
    Desc.pDebugName                 = "ClothTessellation";
    Desc.Type                       = RENDER_PASS_COMPUTE;
    Desc.pComputeShader             = &m_Shader;
    Desc.ComputeBufferCount         = 1;
    Desc.ComputeBuffers[0].Resource = Output;
    Desc.ComputeBuffers[0].Cycle    = TRUE;

    const RenderPassId Pass = Graph.AddPass( Desc, Execute, Data );
    if( Pass == INVALID_RENDER_PASS || !Data.Vertices.IsValid() || !Data.Triangles.IsValid() )
    {
        return FALSE;
    }

    Graph.Read( Pass, Data.Vertices, RENDER_RESOURCE_ACCESS_STORAGE_READ );
    Graph.Read( Pass, Data.Triangles, RENDER_RESOURCE_ACCESS_STORAGE_READ );
    return TRUE;
}

//=========================================================================

xbool ClothTessellation::Execute( RenderPassContext& Context, const PassData& Data )
{
    ClothTessellation& Owner = *Data.pOwner;
    Owner.m_isReady = FALSE;
    const rbuffer* pVertices = Context.ReadBuffer( Data.Vertices );
    const rbuffer* pTriangles = Context.ReadBuffer( Data.Triangles );
    if( !pVertices || !pTriangles )
    {
        return FALSE;
    }

    const shader_storage_buffer_binding Inputs[] =
    {
        shader_storage_buffer_binding( SHADER_STAGE_COMPUTE, 0, rbuffer_GetResource( *pVertices ) ),
        shader_storage_buffer_binding( SHADER_STAGE_COMPUTE, 1, rbuffer_GetResource( *pTriangles ) )
    };
    if( !shader_PushUniformData( SHADER_STAGE_COMPUTE, 0, &Data.Constants, sizeof( Data.Constants ) ) ||
        !shader_BindStorageBuffers( Inputs, 2 ) || !Context.Dispatch( ( Owner.m_Mesh.VertexCount + 63 ) / 64 ) )
    {
        return FALSE;
    }

    Owner.m_FrameSerial = Data.FrameSerial;
    Owner.m_Revision    = Data.Revision;
    Owner.m_isReady     = TRUE;
    return TRUE;
}

//=========================================================================

const dynamic_geom_gpu_mesh* ClothTessellation::GetMesh( void ) const
{
    return m_Shader ? &m_Mesh : NULL;
}

} // namespace render
