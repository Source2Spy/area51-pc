//=========================================================================
//
//  FontCollection.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontCollection.hpp"
#include "TextIn.hpp"

#include "x_math.hpp"

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
xbool ReadColor( text_in& Input, const char* pName, xcolor& Color )
{
    f32 R, G, B, A;

    if( !Input.GetField( xfs( "%s:ffff", pName ), &R, &G, &B, &A ) )
    {
        return FALSE;
    }

    if( !x_isvalid( R + G + B + A ) || R < 0 || R > 1 || G < 0 || G > 1 || B < 0 || B > 1 || A < 0 || A > 1 )
    {
        return FALSE;
    }

    Color = xcolor( (u8)(R * 255), (u8)(G * 255), (u8)(B * 255), (u8)(A * 255) );
    return TRUE;
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool FontCollection::Load( X_FILE* pFile )
{
    Fonts.Clear();
    Fallbacks.Clear();
    Error = "Invalid font descriptor";

    if( !pFile )
    {
        return FALSE;
    }

    s32 Length = x_flength( pFile );

    if( Length <= 0 || Length > 1024 * 1024 )
    {
        return FALSE;
    }

    xarray<char> Bytes;
    Bytes.SetCount( Length + 1 );

    if( x_fread( Bytes.GetPtr(), 1, Length, pFile ) != Length )
    {
        return FALSE;
    }

    Bytes[Length] = 0;

    text_in Input;
    Input.OpenText( Bytes.GetPtr() );

    s32 Version = 0;

    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "FontTable" ) || Input.GetFieldCount() != 1 ||
        !Input.ReadFields() || !Input.GetField( "Version:d", &Version ) || Version != 1 )
    {
        return FALSE;
    }

    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "Fonts" ) )
    {
        return FALSE;
    }

    s32 Count = Input.GetHeaderCount();

    if( Input.GetFieldCount() != 19 || Count <= 0 || Count > 256 )
    {
        return FALSE;
    }

    for( s32 I = 0; I < Count; I++ )
    {
        if( !Input.ReadFields() )
        {
            return FALSE;
        }

        FontDefinition  Definition;
        char            Name[TEXTFILE_MAX_STRLENGTH];
        char            Path[TEXTFILE_MAX_STRLENGTH];
        char            Hinting[TEXTFILE_MAX_STRLENGTH];
        s32             Antialias;
        s32             Kerning;

        if( !Input.GetField( "Name:s",          Name )                         ||
            !Input.GetField( "FileName:s",      Path )                         ||
            !Input.GetField( "FaceIndex:d",     &Definition.FaceIndex )        ||
            !Input.GetField( "Size:f",          &Definition.Size )             ||
            !Input.GetField( "Weight:f",        &Definition.Weight )           ||
            !Input.GetField( "WidthScale:f",    &Definition.WidthScale )       ||
            !Input.GetField( "HeightScale:f",   &Definition.HeightScale )      ||
            !Input.GetField( "LineSpacing:f",   &Definition.LineSpacing )      ||
            !Input.GetField( "LetterSpacing:f", &Definition.LetterSpacing )    ||
            !Input.GetField( "SpaceSpacing:f",  &Definition.SpaceSpacing )     ||
            !Input.GetField( "Kerning:d",       &Kerning )                     ||
            !Input.GetField( "Antialias:d",     &Antialias )                   ||
            !Input.GetField( "Hinting:s",       Hinting )                      ||
            !ReadColor( Input, "Color", Definition.Color )                     ||
            !Input.GetField( "OutlineWidth:f",  &Definition.OutlineWidth )     ||
            !ReadColor( Input, "OutlineColor", Definition.OutlineColor )       ||
            !Input.GetField( "ShadowOffset:ff", &Definition.ShadowOffset.X, &Definition.ShadowOffset.Y ) ||
            !Input.GetField( "ShadowBlur:f",    &Definition.ShadowBlur )        ||
            !ReadColor( Input, "ShadowColor", Definition.ShadowColor ) )
        {
            return FALSE;
        }

        if( !Name[0] || !Path[0] || Definition.FaceIndex < 0 || Definition.Size <= 0 || Definition.Size > 512 ||
            !x_isvalid( Definition.Weight ) || Definition.Weight < 1 || Definition.Weight > 1000 ||
            !x_isvalid( Definition.WidthScale ) || Definition.WidthScale <= 0 ||
            !x_isvalid( Definition.HeightScale ) || Definition.HeightScale <= 0 ||
            Definition.Size * Definition.WidthScale > 512 || Definition.Size * Definition.HeightScale > 512 ||
            Definition.OutlineWidth < 0 || Definition.ShadowBlur < 0 ||
            !x_isvalid( Definition.Size + Definition.LineSpacing + Definition.LetterSpacing +
                            Definition.SpaceSpacing + Definition.OutlineWidth + Definition.ShadowBlur +
                            Definition.ShadowOffset.X + Definition.ShadowOffset.Y ) ||
            (Antialias != 0 && Antialias != 1) || (Kerning != 0 && Kerning != 1) )
        {
            return FALSE;
        }

        if( !x_strcmp( Hinting, "none" ) )
        {
            Definition.Hinting = FontHinting::None;
        }
        else if( !x_strcmp( Hinting, "light" ) )
        {
            Definition.Hinting = FontHinting::Light;
        }
        else if( !x_strcmp( Hinting, "normal" ) )
        {
            Definition.Hinting = FontHinting::Normal;
        }
        else
        {
            return FALSE;
        }

        Definition.Name      = Name;
        Definition.FileName  = Path;
        Definition.Antialias = Antialias;
        Definition.Kerning   = Kerning;

        for( s32 J = 0; J < Fonts.GetCount(); J++ )
        {
            if( Fonts[J].Name == Definition.Name )
            {
                return FALSE;
            }
        }

        Fonts.Append() = Definition;
    }

    if( !Input.ReadHeader() || x_strcmp( Input.GetHeaderName(), "Fallbacks" ) )
    {
        return FALSE;
    }

    Count = Input.GetHeaderCount();

    if( Input.GetFieldCount() != 3 || Count < 0 || Count > 1024 )
    {
        return FALSE;
    }

    for( s32 I = 0; I < Count; I++ )
    {
        FontFallback    Fallback;
        char            Name[TEXTFILE_MAX_STRLENGTH];
        char            Path[TEXTFILE_MAX_STRLENGTH];

        if( !Input.ReadFields() || !Input.GetField( "FontName:s", Name ) || !Input.GetField( "FileName:s", Path ) ||
            !Input.GetField( "FaceIndex:d", &Fallback.FaceIndex ) || !Name[0] || !Path[0] || Fallback.FaceIndex < 0 )
        {
            return FALSE;
        }

        xbool Found = FALSE;

        for( s32 J = 0; J < Fonts.GetCount(); J++ )
        {
            Found |= Fonts[J].Name == Name;
        }

        if( !Found )
        {
            return FALSE;
        }

        Fallback.FontName  = Name;
        Fallback.FileName  = Path;
        Fallbacks.Append() = Fallback;
    }

    if( Input.ReadHeader() || Input.HasError() || !Input.IsEOF() )
    {
        return FALSE;
    }

    Error.Clear();
    return TRUE;
}
