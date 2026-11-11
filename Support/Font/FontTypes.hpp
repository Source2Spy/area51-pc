//=========================================================================
//
//  FontTypes.hpp
//
//=========================================================================

#ifndef FONT_TYPES_HPP
#define FONT_TYPES_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "x_ref.hpp"

//=========================================================================
//  TYPES
//=========================================================================

class Font;
class FontFace;

typedef xref<Font> FontHandle;

enum class FontHinting
{
    None,
    Light,
    Normal
};

//=========================================================================
//  STRUCTURES
//=========================================================================

struct FontDefinition
{
    xstring     Name;
    xstring     FileName;
    s32         FaceIndex     = 0;
    f32         Size          = 18.0f;
    f32         Weight        = 400.0f; // Variable-font weight axis.
    f32         WidthScale    = 1.0f;
    f32         HeightScale   = 1.0f;
    f32         LineSpacing   = 0.0f;
    f32         LetterSpacing = 0.0f;
    f32         SpaceSpacing  = 0.0f;
    xbool       Kerning       = TRUE;
    xbool       Antialias     = TRUE;
    FontHinting Hinting       = FontHinting::Light;
    xcolor      Color         = XCOLOR_WHITE;

    f32         OutlineWidth  = 0.0f;
    xcolor      OutlineColor  = xcolor( 0, 0, 0, 0 );

    vector2     ShadowOffset  = vector2( 0.0f, 0.0f );
    f32         ShadowBlur    = 0.0f;
    xcolor      ShadowColor   = xcolor( 0, 0, 0, 0 );
};

//-------------------------------------------------------------------------

struct FontFallback
{
    xstring FontName;
    xstring FileName;
    s32     FaceIndex = 0;
};

//-------------------------------------------------------------------------

struct FontMetrics
{
    f32 Ascender   = 0.0f;
    f32 Descender  = 0.0f;
    f32 LineHeight = 0.0f;
};

//-------------------------------------------------------------------------

struct GlyphBitmap
{
    xarray<u8> Pixels;
    s32        Width  = 0;
    s32        Height = 0;
    s32        Left   = 0;
    s32        Top    = 0;
};

//=========================================================================
#endif // FONT_TYPES_HPP
//=========================================================================
