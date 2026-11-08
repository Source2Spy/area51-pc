//=========================================================================
//
//  Font.hpp
//
//=========================================================================

#ifndef FONT_HPP
#define FONT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontFace.hpp"
#include "TextLayout.hpp"
#include "GlyphAtlas.hpp"

//=========================================================================
//  CLASS
//=========================================================================

class Font : public xref_counted
{
public:

                                    Font            ( const FontDefinition& Definition,
                                                      const xref<FontFace>& Face );
    virtual                        ~Font            ( void );

    const FontDefinition&           GetDefinition   ( void ) const;
    FontMetrics                     GetMetrics      ( void ) const;

    xbool                           BuildLayout     ( const xwchar* pText,
                                                      const TextLayoutOptions& Options,
                                                      TextLayout& Result );
    xbool                           BuildLayout     ( const char* pText,
                                                      const TextLayoutOptions& Options,
                                                      TextLayout& Result );
    xbool                           PrepareText     ( const TextLayout& Layout,
                                                      f32 Scale,
                                                      PreparedText& Result );

    void                            AddFallback     ( const xref<FontFace>& Face );
    const xarray<xref<FontFace>>&    GetFaces        ( void ) const;

private:

    FontDefinition                  m_Definition;
    xarray<xref<FontFace>>           m_Faces;
};

//=========================================================================
#endif // FONT_HPP
//=========================================================================
