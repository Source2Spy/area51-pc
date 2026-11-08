//=========================================================================
//
//  FontFace.hpp
//
//=========================================================================

#ifndef FONT_FACE_HPP
#define FONT_FACE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontTypes.hpp"

//=========================================================================
//  CLASS
//=========================================================================

class FontFace : public xref_counted
{
public:

                        FontFace        ( void );
    virtual            ~FontFace        ( void );

    xbool               Load            ( const char* pFileName, s32 FaceIndex, void* pLibrary,
                                          const f32* pWeight = NULL );
    u32                 GetGlyphIndex   ( u32 Codepoint ) const;
    FontMetrics         GetMetrics      ( f32 Size ) const;
    xbool               Rasterize       ( u32 GlyphIndex,
                                          u32 PixelSize,
                                          const FontDefinition& Definition,
                                          f32 Outline,
                                          f32 Blur,
                                          GlyphBitmap& Bitmap );

    struct Data;

    Data*               GetData         ( void ) const;

public:

    xstring             FileName;
    s32                 FaceIndex = 0;
    f32                 Weight    = 400.0f;
    xbool               HasWeight = FALSE;

private:

    Data*               m_pData;
};

//=========================================================================
#endif // FONT_FACE_HPP
//=========================================================================
