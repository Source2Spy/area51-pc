//=========================================================================
//
//  GlyphAtlas.hpp
//
//=========================================================================

#ifndef GLYPH_ATLAS_HPP
#define GLYPH_ATLAS_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontFace.hpp"
#include "TextLayout.hpp"
#include "e_VRAM.hpp"
#include "e_Shader.hpp"

//=========================================================================
//  STRUCTURES
//=========================================================================

struct GlyphImage
{
    shader_resource Texture;
    vector2         UV0    = vector2( 0.0f, 0.0f );
    vector2         UV1    = vector2( 0.0f, 0.0f );
    vector2         Offset = vector2( 0.0f, 0.0f );
    vector2         Size   = vector2( 0.0f, 0.0f );
};

//-------------------------------------------------------------------------

struct PreparedGlyph
{
    GlyphImage Image;
    vector2    Position = vector2( 0.0f, 0.0f );
    xcolor     Color    = XCOLOR_WHITE;
    s32        Cluster  = 0;
    s32        InlineId = -1;
    s32        Layer    = 2;
};

//-------------------------------------------------------------------------

struct PreparedText
{
    FontHandle              Owner;
    xarray<PreparedGlyph>    Glyphs;
};

//=========================================================================
//  CLASS
//=========================================================================

class GlyphAtlas
{
public:

                        GlyphAtlas      ( void );
                       ~GlyphAtlas      ( void );

    void                BeginFrame      ( u64 Serial );
    xbool               RequestGlyph    ( const TextGlyph& Glyph,
                                          const FontDefinition& Definition,
                                          f32 Scale,
                                          f32 Outline,
                                          f32 Blur,
                                          GlyphImage& Image );
    xbool               Prepare         ( void );
    void                Kill            ( void );

    u32                 GetPageCount    ( void ) const;
    u64                 GetRasterCount  ( void ) const;

private:

    struct Data;

    Data*               m_pData;
};

//=========================================================================
#endif // GLYPH_ATLAS_HPP
//=========================================================================
