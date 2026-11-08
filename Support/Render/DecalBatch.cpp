//=========================================================================
//
//  DecalBatch.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "DecalBatch.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

render::decal_vertex::decal_vertex( void )
    : Position( 0.0f, 0.0f, 0.0f ), Color( 0xffffffffu ), UV( 0.0f, 0.0f )
{
}

//=========================================================================

render::decal_vertex::decal_vertex( vector3 const& position, vector2 const& uV, xcolor const& color )
    : Position( position ), Color( ( static_cast<u32>( color.A ) << 24 ) | ( static_cast<u32>( color.R ) << 16 ) |
                                  ( static_cast<u32>( color.G ) << 8 ) | static_cast<u32>( color.B ) ),
      UV( uV )
{
}

//=========================================================================

render::decal_draw_desc::decal_draw_desc( void )
    : pTexture( NULL ), Blend( DECAL_BLEND_ALPHA ), Flags( DECAL_DRAW_FLAG_NONE ),
      Bounds( vector3( 0.0f, 0.0f, 0.0f ) ), GeometricNormal( 0.0f, 1.0f, 0.0f ), Ambient( 64, 64, 64, 255 )
{
}