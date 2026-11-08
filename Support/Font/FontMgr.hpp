//=========================================================================
//
//  FontMgr.hpp
//
//=========================================================================

#ifndef FONT_MGR_HPP
#define FONT_MGR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Font.hpp"
#include "FontCollection.hpp"

//=========================================================================
//  CLASS
//=========================================================================

class GlyphAtlas;

class FontMgr
{
public:

                        FontMgr         ( void );
                       ~FontMgr         ( void );

    xbool               Init            ( const char* pUnicodeDataFile );
    void                Kill            ( void );
    xbool               IsInitialized   ( void ) const;

    FontHandle          FindFont        ( const FontCollection& Collection,
                                          const char* pName );

    void                BeginFrame      ( u64 Serial );
    xbool               Prepare         ( void );
    GlyphAtlas&         GetAtlas        ( void );

private:

    xref<FontFace>       LoadFace        ( const char* pPath, s32 Index,
                                          const f32* pWeight = NULL );

private:

    struct Data;

    Data*               m_pData;
};

//=========================================================================
//  STORAGE
//=========================================================================

extern FontMgr g_FontMgr;

//=========================================================================
#endif // FONT_MGR_HPP
//=========================================================================
