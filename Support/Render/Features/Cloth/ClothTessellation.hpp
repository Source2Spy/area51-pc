//=========================================================================
//
//  ClothTessellation.hpp
//
//=========================================================================

#ifndef CLOTH_TESSELLATION_HPP
#define CLOTH_TESSELLATION_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Render/GeomDraw.hpp"

namespace render
{

class FrameGraphBuilder;
class RenderPassContext;

class ClothTessellation
{
public:
                    ClothTessellation( void );
                    ~ClothTessellation( void );

    xbool           Init( s32 VertexCount, const u16* pIndices, s32 IndexCount );
    void            Kill( void );
    xbool           Declare( FrameGraphBuilder& Graph, const dynamic_geom_vertex* pVertices, s32 VertexCount,
                             const bbox& Bounds, u32 Revision );
    const dynamic_geom_gpu_mesh* GetMesh( void ) const;

private:
    struct PassData;

    static xbool Execute( RenderPassContext& Context, const PassData& Data );

    shader                m_Shader;
    rbuffer               m_SourceVertices;
    rbuffer               m_SourceTriangles;
    rbuffer               m_Vertices;
    rbuffer               m_Indices;
    dynamic_geom_gpu_mesh m_Mesh;
    u32                   m_SourceVertexCount;
    u32                   m_TriangleCount;
    u32                   m_Subdivisions;
    u32                   m_Revision;
    u64                   m_FrameSerial;
    xbool                 m_isReady;
};

} // namespace render

#endif // CLOTH_TESSELLATION_HPP
