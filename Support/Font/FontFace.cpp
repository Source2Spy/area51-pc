//=========================================================================
//
//  FontFace.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FontFace_Private.hpp"

#include "x_math.hpp"
#include <vector>

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
xbool SetVariations( FontFace::Data& Data, FT_Library Library, const f32* pWeight )
{
    if( !FT_HAS_MULTIPLE_MASTERS( Data.Face ) )
    {
        return TRUE;
    }

    FT_MM_Var* pMaster = NULL;

    if( FT_Get_MM_Var( Data.Face, &pMaster ) )
    {
        return FALSE;
    }

    s32 WeightAxis = -1;

    for( FT_UInt I = 0; I < pMaster->num_axis; I++ )
    {
        if( pMaster->axis[I].tag == FT_MAKE_TAG( 'w', 'g', 'h', 't' ) )
        {
            WeightAxis = (s32)I;
            break;
        }
    }

    if( WeightAxis < 0 )
    {
        FT_Done_MM_Var( Library, pMaster );
        return TRUE;
    }

    // Preserve the selected instance's other design coordinates.
    xarray<FT_Fixed> Coordinates;
    Coordinates.SetCount( (s32)pMaster->num_axis );

    FT_Error Error = FT_Get_Var_Design_Coordinates( Data.Face, pMaster->num_axis,
                                                   Coordinates.GetPtr() );

    if( !Error && pWeight )
    {
        FT_Fixed Value = (FT_Fixed)(*pWeight * 65536.0f + 0.5f);
        Coordinates[WeightAxis] = MAX( pMaster->axis[WeightAxis].minimum,
                                       MIN( pMaster->axis[WeightAxis].maximum, Value ) );
        Error = FT_Set_Var_Design_Coordinates( Data.Face, pMaster->num_axis,
                                               Coordinates.GetPtr() );
    }

    if( !Error )
    {
        // Rasterization and shaping share the same variation coordinates.
        Data.ShapeCoordinates.SetCount( Coordinates.GetCount() );

        for( s32 I = 0; I < Coordinates.GetCount(); I++ )
        {
            Data.ShapeCoordinates[I] = (f32)Coordinates[I] / 65536.0f;
        }
    }

    FT_Done_MM_Var( Library, pMaster );
    return Error == 0;
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

hb_font_t* FontFace::Data::CreateShapeFont( f32 Size ) const
{
    hb_font_t* pFont = hb_font_create( ShapeFace );
    s32        Scale = (s32)(Size * 64.0f + 0.5f);

    hb_ot_font_set_funcs( pFont );
    hb_font_set_scale( pFont, Scale, Scale );

    if( ShapeCoordinates.GetCount() )
    {
        hb_font_set_var_coords_design( pFont, ShapeCoordinates.GetPtr(), ShapeCoordinates.GetCount() );
    }

    return pFont;
}

//=========================================================================

FontFace::Data* FontFace::GetData( void ) const
{
    return m_pData;
}

//=========================================================================

FontFace::FontFace( void ) :
    m_pData( new Data )
{
}

//=========================================================================

FontFace::~FontFace( void )
{
    if( m_pData->ShapeFace )
    {
        hb_face_destroy( m_pData->ShapeFace );
    }

    if( m_pData->Blob )
    {
        hb_blob_destroy( m_pData->Blob );
    }

    if( m_pData->Face )
    {
        FT_Done_Face( m_pData->Face );
    }

    delete m_pData;
}

//=========================================================================

xbool FontFace::Load( const char* pFileName, s32 Index, void* pLibrary, const f32* pWeight )
{
    if( pWeight && (!x_isvalid( *pWeight ) || *pWeight < 1 || *pWeight > 1000) )
    {
        x_DebugMsg( "FontFace: invalid weight for '%s'\n", pFileName );
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );

    if( !pFile )
    {
        return FALSE;
    }

    s32 Length = x_flength( pFile );

    if( Length <= 0 )
    {
        x_fclose( pFile );
        return FALSE;
    }

    m_pData->Bytes.SetCount( Length );

    xbool Read = x_fread( m_pData->Bytes.GetPtr(), 1, Length, pFile ) == Length;
    x_fclose( pFile );

    if( !Read || FT_New_Memory_Face( (FT_Library)pLibrary,
                                     m_pData->Bytes.GetPtr(),
                                     Length,
                                     Index,
                                     &m_pData->Face ) )
    {
        return FALSE;
    }

    if( FT_Select_Charmap( m_pData->Face, FT_ENCODING_UNICODE ) )
    {
        return FALSE;
    }

    if( !SetVariations( *m_pData, (FT_Library)pLibrary, pWeight ) )
    {
        x_DebugMsg( "FontFace: cannot initialize variations for '%s'\n", pFileName );
        return FALSE;
    }

    m_pData->Blob = hb_blob_create( (const char*)m_pData->Bytes.GetPtr(),
                                    Length,
                                    HB_MEMORY_MODE_READONLY,
                                    NULL,
                                    NULL );
    m_pData->ShapeFace = hb_face_create( m_pData->Blob, Index );

    FileName  = pFileName;
    FaceIndex = Index;
    HasWeight = pWeight != NULL;

    if( pWeight )
    {
        Weight = *pWeight;
    }

    return hb_face_get_glyph_count( m_pData->ShapeFace ) > 0;
}

//=========================================================================

u32 FontFace::GetGlyphIndex( u32 Codepoint ) const
{
    return FT_Get_Char_Index( m_pData->Face, Codepoint );
}

//=========================================================================

FontMetrics FontFace::GetMetrics( f32 Size ) const
{
    FT_Face     Face  = m_pData->Face;
    f32         Scale = Size / Face->units_per_EM;
    FontMetrics Result;

    Result.Ascender   = Face->ascender * Scale;
    Result.Descender  = Face->descender * Scale;
    Result.LineHeight = Face->height * Scale;
    return Result;
}

//=========================================================================

xbool FontFace::Rasterize( u32 Index,
                            u32 PixelSize,
                            const FontDefinition& Definition,
                            f32 Outline,
                            f32 Blur,
                            GlyphBitmap& Result )
{
    FT_Face Face = m_pData->Face;

    // Scale outlines before rasterization to preserve coverage at the target size.
    FT_Size_RequestRec Request;

    Request.type           = FT_SIZE_REQUEST_TYPE_NOMINAL;
    Request.width          = (FT_Long)MAX( 1.0f, x_floor( PixelSize * Definition.WidthScale * 64.0f + 0.5f ) );
    Request.height         = (FT_Long)MAX( 1.0f, x_floor( PixelSize * Definition.HeightScale * 64.0f + 0.5f ) );
    Request.horiResolution = 0;
    Request.vertResolution = 0;

    if( FT_Request_Size( Face, &Request ) )
    {
        return FALSE;
    }

    FT_Int32 Flags = FT_LOAD_NO_BITMAP;

    if( Definition.Hinting == FontHinting::None )
    {
        Flags |= FT_LOAD_NO_HINTING;
    }
    else if( Definition.Hinting == FontHinting::Light )
    {
        Flags |= FT_LOAD_TARGET_LIGHT;
    }

    if( !Definition.Antialias )
    {
        Flags |= FT_LOAD_TARGET_MONO;
    }

    if( FT_Load_Glyph( Face, Index, Flags ) )
    {
        return FALSE;
    }

    FT_Glyph Glyph = NULL;

    if( FT_Get_Glyph( Face->glyph, &Glyph ) )
    {
        return FALSE;
    }

    if( Outline > 0.0f )
    {
        FT_Stroker Stroker = NULL;

        if( FT_Stroker_New( Face->glyph->library, &Stroker ) )
        {
            FT_Done_Glyph( Glyph );
            return FALSE;
        }
        FT_Stroker_Set( Stroker,
                        (FT_Fixed)(Outline * 64.0f),
                        FT_STROKER_LINECAP_ROUND,
                        FT_STROKER_LINEJOIN_ROUND,
                        0 );

        FT_Error Error = FT_Glyph_StrokeBorder( &Glyph, Stroker, 0, 1 );
        FT_Stroker_Done( Stroker );

        if( Error )
        {
            FT_Done_Glyph( Glyph );
            return FALSE;
        }
    }

    if( FT_Glyph_To_Bitmap( &Glyph, Definition.Antialias ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, NULL, 1 ) )
    {
        FT_Done_Glyph( Glyph );
        return FALSE;
    }

    FT_BitmapGlyph  Rendered = (FT_BitmapGlyph)Glyph;
    const FT_Bitmap& Bitmap  = Rendered->bitmap;
    s32             Padding = Blur > 0.0f ? MAX( 1, (s32)x_ceil( Blur * 3.0f ) ) : 1;

    Result.Width  = Bitmap.width ? (s32)Bitmap.width + Padding * 2 : 0;
    Result.Height = Bitmap.rows ? (s32)Bitmap.rows + Padding * 2 : 0;
    Result.Left   = Rendered->left - Padding;
    Result.Top    = Rendered->top + Padding;

    if( Result.Width > 1022 || Result.Height > 1022 )
    {
        FT_Done_Glyph( Glyph );
        return FALSE;
    }

    Result.Pixels.SetCount( Result.Width * Result.Height );

    if( Result.Pixels.GetCount() )
    {
        x_memset( Result.Pixels.GetPtr(), 0, Result.Pixels.GetCount() );
    }

    for( s32 Y = 0; Y < (s32)Bitmap.rows; Y++ )
    {
        const u8* pRow = Bitmap.buffer + Y * Bitmap.pitch;

        for( s32 X = 0; X < (s32)Bitmap.width; X++ )
        {
            u8 Value;

            if( Bitmap.pixel_mode == FT_PIXEL_MODE_MONO )
            {
                Value = pRow[X >> 3] & (0x80 >> (X & 7)) ? 255 : 0;
            }
            else if( Bitmap.pixel_mode == FT_PIXEL_MODE_GRAY )
            {
                Value = pRow[X];
            }
            else
            {
                FT_Done_Glyph( Glyph );
                return FALSE;
            }

            Result.Pixels[(Y + Padding) * Result.Width + X + Padding] = Value;
        }
    }
    FT_Done_Glyph( Glyph );

    if( Blur > 0.0f && Result.Pixels.GetCount() )
    {
        std::vector<f32> Kernel( Padding * 2 + 1 );
        f32              Sum = 0.0f;

        for( s32 I = -Padding; I <= Padding; I++ )
        {
            Sum += Kernel[I + Padding] = x_exp( -( I * I ) / ( 2.0f * Blur * Blur ) );
        }

        for( f32& Value : Kernel )
        {
            Value /= Sum;
        }

        std::vector<f32> Temporary( Result.Pixels.GetCount() );

        for( s32 Y = 0; Y < Result.Height; Y++ )
        {
            for( s32 X = 0; X < Result.Width; X++ )
            {
                for( s32 I = -Padding; I <= Padding; I++ )
                {
                    if( X + I >= 0 && X + I < Result.Width )
                    {
                        Temporary[Y * Result.Width + X] +=
                            Result.Pixels[Y * Result.Width + X + I] * Kernel[I + Padding];
                    }
                }
            }
        }

        for( s32 Y = 0; Y < Result.Height; Y++ )
        {
            for( s32 X = 0; X < Result.Width; X++ )
            {
                f32 Value = 0.0f;

                for( s32 I = -Padding; I <= Padding; I++ )
                {
                    if( Y + I >= 0 && Y + I < Result.Height )
                    {
                        Value += Temporary[( Y + I ) * Result.Width + X] * Kernel[I + Padding];
                    }
                }

                Result.Pixels[Y * Result.Width + X] = (u8)MIN( 255.0f, Value + 0.5f );
            }
        }
    }

    return TRUE;
}
