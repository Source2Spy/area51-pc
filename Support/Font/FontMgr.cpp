//=========================================================================
//
//  FontMgr.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontMgr.hpp"
#include "GlyphAtlas.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <unicode/udata.h>
#include <unicode/uclean.h>

//=========================================================================
//  STRUCTURES
//=========================================================================

struct FontMgr::Data
{
    FT_Library              Library = NULL;
    xarray<u8>              UnicodeData;
    xarray<xref<FontFace>>   Faces;
    GlyphAtlas              Atlas;
};

//=========================================================================
//  STORAGE
//=========================================================================

FontMgr g_FontMgr;

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

FontMgr::FontMgr( void ) :
    m_pData( NULL )
{
}

//=========================================================================

FontMgr::~FontMgr( void )
{
    ASSERT( !m_pData );
}

//=========================================================================

xbool FontMgr::Init( const char* pUnicodeDataFile )
{
    if( m_pData )
    {
        return TRUE;
    }

    Data*   pData = new Data;
    X_FILE* pFile = x_fopen( pUnicodeDataFile, "rb" );

    if( !pFile )
    {
        x_DebugMsg( "FontMgr: cannot open Unicode data '%s'\n", pUnicodeDataFile );
        delete pData;
        return FALSE;
    }

    s32 Length = x_flength( pFile );

    if( Length <= 0 )
    {
        x_DebugMsg( "FontMgr: empty Unicode data '%s'\n", pUnicodeDataFile );
        x_fclose( pFile );
        delete pData;
        return FALSE;
    }

    pData->UnicodeData.SetCount( Length );

    xbool Read = x_fread( pData->UnicodeData.GetPtr(), 1, Length, pFile ) == Length;
    x_fclose( pFile );

    UErrorCode Error = U_ZERO_ERROR;

    if( Read )
    {
        udata_setCommonData( pData->UnicodeData.GetPtr(), &Error );
    }

    if( !Read || U_FAILURE( Error ) )
    {
        x_DebugMsg( "FontMgr: Unicode data '%s', read=%d, ICU=%s\n",
                      pUnicodeDataFile, Read, u_errorName( Error ) );
        u_cleanup();
        delete pData;
        return FALSE;
    }

    FT_Error FreeTypeError = FT_Init_FreeType( &pData->Library );

    if( FreeTypeError )
    {
        x_DebugMsg( "FontMgr: FreeType initialization failed (%d)\n", FreeTypeError );
        u_cleanup();
        delete pData;
        return FALSE;
    }

    u_init( &Error );

    if( U_FAILURE( Error ) )
    {
        x_DebugMsg( "FontMgr: Unicode initialization failed (%s)\n", u_errorName( Error ) );
        FT_Done_FreeType( pData->Library );
        u_cleanup();
        delete pData;
        return FALSE;
    }

    m_pData = pData;
    return TRUE;
}

//=========================================================================

void FontMgr::Kill( void )
{
    if( !m_pData )
    {
        return;
    }

    m_pData->Atlas.Kill();

    for( s32 I = 0; I < m_pData->Faces.GetCount(); I++ )
    {
        ASSERT( m_pData->Faces[I]->GetReferenceCount() == 1 );
    }

    m_pData->Faces.Clear();
    FT_Done_FreeType( m_pData->Library );
    u_cleanup();

    delete m_pData;
    m_pData = NULL;
}

//=========================================================================

xbool FontMgr::IsInitialized( void ) const
{
    return m_pData != NULL;
}

//=========================================================================

xref<FontFace> FontMgr::LoadFace( const char* pPath, s32 Index, const f32* pWeight )
{
    for( s32 I = 0; I < m_pData->Faces.GetCount(); I++ )
    {
        if( m_pData->Faces[I]->FileName == pPath && m_pData->Faces[I]->FaceIndex == Index &&
            m_pData->Faces[I]->HasWeight == (pWeight != NULL) &&
            (!pWeight || m_pData->Faces[I]->Weight == *pWeight) )
        {
            return m_pData->Faces[I];
        }
    }

    xref<FontFace> Face = new FontFace;

    if( !Face->Load( pPath, Index, m_pData->Library, pWeight ) )
    {
        return xref<FontFace>();
    }

    m_pData->Faces.Append() = Face;
    return Face;
}

//=========================================================================

FontHandle FontMgr::FindFont( const FontCollection& Collection, const char* pName )
{
    if( !m_pData )
    {
        return FontHandle();
    }

    for( s32 I = 0; I < Collection.Fonts.GetCount(); I++ )
    {
        const FontDefinition& Definition = Collection.Fonts[I];

        if( Definition.Name != pName )
        {
            continue;
        }

        xref<FontFace> Face = LoadFace( Definition.FileName, Definition.FaceIndex, &Definition.Weight );

        if( !Face.Get() )
        {
            return FontHandle();
        }

        FontHandle Result = new Font( Definition, Face );

        for( s32 J = 0; J < Collection.Fallbacks.GetCount(); J++ )
        {
            const FontFallback& Fallback = Collection.Fallbacks[J];

            if( Fallback.FontName != pName )
            {
                continue;
            }
            Face = LoadFace( Fallback.FileName, Fallback.FaceIndex );

            if( !Face.Get() )
            {
                return FontHandle();
            }

            Result->AddFallback( Face );
        }

        return Result;
    }

    return FontHandle();
}

//=========================================================================

void FontMgr::BeginFrame( u64 Serial )
{
    if( m_pData )
    {
        m_pData->Atlas.BeginFrame( Serial );
    }
}

//=========================================================================

xbool FontMgr::Prepare( void )
{
    return !m_pData || m_pData->Atlas.Prepare();
}

//=========================================================================

GlyphAtlas& FontMgr::GetAtlas( void )
{
    ASSERT( m_pData );
    return m_pData->Atlas;
}
