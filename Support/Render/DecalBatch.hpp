//=========================================================================
//
//  DecalBatch.hpp
//
//  Internal surface-decal submission types.
//
//=========================================================================

#ifndef DECAL_BATCH_HPP
#define DECAL_BATCH_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Texture.hpp"

#include "x_color.hpp"
#include "x_math.hpp"

//=========================================================================

namespace render
{
enum decal_blend_mode
{
    DECAL_BLEND_ALPHA = 0,
    DECAL_BLEND_ADDITIVE,
    DECAL_BLEND_SUBTRACTIVE,
    DECAL_BLEND_INTENSITY,
    DECAL_BLEND_COUNT
};

//-------------------------------------------------------------------------

enum decal_draw_flags
{
    DECAL_DRAW_FLAG_NONE       = 0,
    DECAL_DRAW_FLAG_GLOW       = ( 1u << 0 ),
    DECAL_DRAW_FLAG_ENV_MAPPED = ( 1u << 1 )
};

//-------------------------------------------------------------------------

struct decal_vertex
{
    decal_vertex ( void );
    decal_vertex ( vector3 const& position, vector2 const& uV, xcolor const& color );

    vector3p Position;
    u32      Color;
    vector2  UV;
};

//-------------------------------------------------------------------------

struct decal_draw_desc
{
    decal_draw_desc ( void );

    texture const*   pTexture;
    decal_blend_mode Blend;
    u32              Flags;
    bbox             Bounds;
    vector3          GeometricNormal;
    xcolor           Ambient;
};

//=========================================================================

xbool SubmitDecalBatch ( decal_draw_desc const& desc, decal_vertex const* pVertices, s32 nVertices,
                         u16 const* pIndices, s32 nIndices );
void ReserveDecalSubmissionCapacity ( s32 nVertices, s32 nIndices, s32 nDraws );
} // namespace render

//=========================================================================
#endif // DECAL_BATCH_HPP
//=========================================================================
