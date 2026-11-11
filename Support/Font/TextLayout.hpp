//=========================================================================
//
//  TextLayout.hpp
//
//=========================================================================

#ifndef TEXT_LAYOUT_HPP
#define TEXT_LAYOUT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontTypes.hpp"

//=========================================================================
//  STRUCTURES
//=========================================================================

struct TextStyleRun
{
    s32     Start         = 0;
    s32     Length        = 0;
    xcolor  Color         = XCOLOR_WHITE;
    s32     InlineId      = -1;
    vector2 InlineSize    = vector2( 0.0f, 0.0f );
    f32     InlineSpacing = 0.0f;
};

//-------------------------------------------------------------------------

struct TextLayoutOptions
{
    f32                     Width     = 0.0f;
    xstring                 Language  = "en";
    s32                     Direction = 0; // 0 auto, 1 LTR, 2 RTL.
    xbool                   Wrap      = FALSE;
    xbool                   Ellipsis  = FALSE;

    xarray<TextStyleRun>     Runs;
};

//-------------------------------------------------------------------------

struct TextGlyph
{
    xref<FontFace> Face;
    u32            GlyphIndex    = 0;
    s32            Cluster       = 0;
    s32            Line          = 0;
    xbool          UnsafeToBreak = FALSE;
    s32            InlineId      = -1;
    vector2        Position      = vector2( 0.0f, 0.0f );
    vector2        InlineSize    = vector2( 0.0f, 0.0f );
    f32            Advance       = 0.0f;
    xcolor         Color         = XCOLOR_WHITE;
};

//-------------------------------------------------------------------------

struct TextLine
{
    s32 Start      = 0;
    s32 End        = 0;
    s32 FirstGlyph = 0;
    s32 GlyphCount = 0;
    f32 Width      = 0.0f;
    f32 Baseline   = 0.0f;
};

//=========================================================================
//  CLASS
//=========================================================================

class TextLayout
{
public:

    void                    Clear       ( void );

public:

    FontHandle              Owner;
    xwstring                Text;
    xarray<TextGlyph>        Glyphs;
    xarray<TextLine>         Lines;
    vector2                 Size = vector2( 0.0f, 0.0f );
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool DecodeFontText ( const char* pText, xwstring& Result );
xbool BuildTextLayout( Font& Source,
                       const xwchar* pText,
                       const TextLayoutOptions& Options,
                       TextLayout& Result );

//=========================================================================
#endif // TEXT_LAYOUT_HPP
//=========================================================================
