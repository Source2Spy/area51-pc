//=========================================================================
//
//  PrimitiveBatch.hpp
//
//  Explicit CPU-side geometry builder for renderer-owned world primitives.
//
//=========================================================================

#ifndef PRIMITIVE_BATCH_HPP
#define PRIMITIVE_BATCH_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Texture.hpp"

#include "x_array.hpp"
#include "x_color.hpp"

//=========================================================================

namespace render
{
enum
{
    MAX_PRIMITIVE_VERTICES = 65535
};

//-------------------------------------------------------------------------

enum primitive_topology
{
    PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 0,
    PRIMITIVE_TOPOLOGY_LINE_LIST,
    PRIMITIVE_TOPOLOGY_COUNT
};

//-------------------------------------------------------------------------

enum primitive_blend_mode
{
    PRIMITIVE_BLEND_OPAQUE = 0,
    PRIMITIVE_BLEND_ALPHA,
    PRIMITIVE_BLEND_ADDITIVE,
    PRIMITIVE_BLEND_SUBTRACTIVE,
    PRIMITIVE_BLEND_INTENSITY,
    PRIMITIVE_BLEND_COUNT
};

//-------------------------------------------------------------------------

enum primitive_depth_mode
{
    PRIMITIVE_DEPTH_DISABLED = 0,
    PRIMITIVE_DEPTH_READ_ONLY,
    PRIMITIVE_DEPTH_READ_WRITE,
    PRIMITIVE_DEPTH_COUNT
};

//-------------------------------------------------------------------------

enum primitive_raster_mode
{
    PRIMITIVE_RASTER_SOLID = 0,
    PRIMITIVE_RASTER_SOLID_NO_CULL,
    PRIMITIVE_RASTER_WIREFRAME,
    PRIMITIVE_RASTER_WIREFRAME_NO_CULL,
    PRIMITIVE_RASTER_COLLISION_BIASED,
    PRIMITIVE_RASTER_COUNT
};

//-------------------------------------------------------------------------

enum primitive_sampler_mode
{
    PRIMITIVE_SAMPLER_LINEAR_WRAP = 0,
    PRIMITIVE_SAMPLER_LINEAR_CLAMP,
    PRIMITIVE_SAMPLER_LINEAR_WRAP_U_CLAMP_V,
    PRIMITIVE_SAMPLER_LINEAR_CLAMP_U_WRAP_V,
    PRIMITIVE_SAMPLER_ANISOTROPIC_WRAP,
    PRIMITIVE_SAMPLER_ANISOTROPIC_CLAMP,
    PRIMITIVE_SAMPLER_COUNT
};

//-------------------------------------------------------------------------

enum primitive_output_mode
{
    PRIMITIVE_OUTPUT_COLOR = 0,
    PRIMITIVE_OUTPUT_GLOW,
    PRIMITIVE_OUTPUT_DISTORTION,
    PRIMITIVE_OUTPUT_COUNT
};

//-------------------------------------------------------------------------

struct primitive_vertex
{
    primitive_vertex ( void );
    primitive_vertex ( vector3 const& position, vector2 const& uV, xcolor const& color );

    vector3p Position;
    u32      Color;
    vector2  UV;
};

//-------------------------------------------------------------------------

struct primitive_draw_desc
{
    primitive_draw_desc ( void );
    primitive_draw_desc ( texture const* pTexture, primitive_topology topology, primitive_blend_mode blend,
                          primitive_depth_mode depth, primitive_raster_mode raster, primitive_sampler_mode sampler,
                          primitive_output_mode OutputMode = PRIMITIVE_OUTPUT_COLOR,
                          f32 DistortionScalePixels = 12.0f );

    texture const*          pTexture;
    primitive_topology      Topology;
    primitive_blend_mode    Blend;
    primitive_depth_mode    Depth;
    primitive_raster_mode   Raster;
    primitive_sampler_mode  Sampler;
    primitive_output_mode   Output;
    f32                     DistortionScale;
};

//=========================================================================

struct primitive_sprite_instance
{
    vector3p Position;
    u32      Color;
    vector3p Right;
    vector3p Up;
    f32      UVRect[4];
};

//=========================================================================

xbool SubmitPrimitives ( primitive_draw_desc const& desc, matrix4 const& LocalToWorld,
                         primitive_vertex const* pVertices, s32 nVertices, u16 const* pIndices, s32 nIndices );

xbool SubmitPrimitiveSprite ( primitive_draw_desc const& desc, vector3 const& position, vector2 const& size,
                              vector2 const& uV0, vector2 const& uV1, xcolor const& color, radian rotation );

xbool SubmitPrimitiveBillboards ( primitive_draw_desc const& desc, s32 nSprites, f32 UniformScale,
                                  matrix4 const* pLocalToWorld, vector4 const* pPositions,
                                  vector2 const* pRotScales, u32 const* pColors,
                                  s32 const* pSpriteIndices = NULL );

xbool SubmitPrimitiveVelocityBillboards ( primitive_draw_desc const& desc, s32 nSprites, f32 UniformScale,
                                          matrix4 const* pLocalToWorld, matrix4 const* pVelocityMatrix,
                                          vector4 const* pPositions,
                                          vector4 const* pVelocities, u32 const* pColors,
                                          s32 const* pSpriteIndices = NULL );

//=========================================================================

class PrimitiveBatch
{
public:

    PrimitiveBatch ( primitive_draw_desc const& desc );

    void  Reserve               ( s32 nVertices, s32 nIndices );
    void  Clear                 ( void );
    xbool AppendIndexed         ( primitive_vertex const* pVertices, s32 nVertices, u16 const* pIndices, s32 nIndices );
    xbool AddTriangle           ( primitive_vertex const& vertex0, primitive_vertex const& vertex1,
                                  primitive_vertex const& vertex2 );
    xbool AddLine               ( primitive_vertex const& vertex0, primitive_vertex const& vertex1 );
    xbool AddLine               ( vector3 const& position0, vector3 const& position1, xcolor const& color0,
                                  xcolor const& color1 );
    xbool AddQuad               ( vector3 const* pPositions, vector2 const* pUVs, xcolor const* pColors );
    xbool AddTriangleStripQuad  ( vector3 const* pPositions, vector2 const* pUVs, xcolor const* pColors );
    xbool AddViewOrientedQuad   ( vector3 const& position0, vector3 const& position1, vector3 const& ViewPosition,
                                  vector2 const& uV0, vector2 const& uV1, xcolor const& color0, xcolor const& color1,
                                  f32 radius0, f32 radius1 );
    xbool AddViewOrientedStrand ( vector3 const* pPositions, s32 nPositions, vector3 const& ViewPosition,
                                  vector2 const& uV0, vector2 const& uV1, xcolor const& color0, xcolor const& color1,
                                  f32 radius );
    xbool AddPlaneAlignedQuad   ( vector3 const& center, vector3 const& normal, f32 radius, xcolor const& color );
    xbool Submit                ( matrix4 const& LocalToWorld ) const;
    // Optional geometry can report rejection without aborting the render frame.
    xbool TrySubmit             ( matrix4 const& LocalToWorld ) const;

    primitive_draw_desc const& GetDesc      ( void ) const;
    s32                        GetVertexCount( void ) const;
    s32                        GetIndexCount ( void ) const;

private:

    xbool CanAppend ( s32 nVertices, s32 nIndices ) const;

    primitive_draw_desc       m_Desc;
    xarray<primitive_vertex>  m_Vertices;
    xarray<u16>               m_Indices;
};
} // namespace render

//=========================================================================
#endif // PRIMITIVE_BATCH_HPP
//=========================================================================
