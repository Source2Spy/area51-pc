//=========================================================================
//
//  TextureFile.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "TextureFile.hpp"
#include "Bitmap/aux_Bitmap.hpp"
#include "x_files.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>

//=========================================================================
//  TYPES
//=========================================================================

struct texture_stream
{
    X_FILE* pFile;
    s32     Length;
};

//-------------------------------------------------------------------------

struct texture_fallback
{
    char const* pExtension;
    xbool     (*pLoad)( xbitmap& Bitmap, X_FILE* pFile );
};

//=========================================================================
//  PNG DECODING
//=========================================================================

static
s64 SDLCALL SeekTextureFile( void* pUserData, s64 Offset, SDL_IOWhence Origin )
{
    texture_stream const& Stream = *static_cast<texture_stream const*>( pUserData );
    s64 Base;

    switch( Origin )
    {
        case SDL_IO_SEEK_SET:
        {
            Base = 0;
        }
        break;

        case SDL_IO_SEEK_CUR:
        {
            Base = x_ftell( Stream.pFile );
        }
        break;

        case SDL_IO_SEEK_END:
        {
            Base = Stream.Length;
        }
        break;

        default:
        {
            return -1;
        }
        break;
    }

    // DFS permits seeking past EOF, but its read path expects an in-range position.
    if( ( Base < 0 ) || ( Offset < -Base ) || ( Offset > Stream.Length - Base ) )
    {
        return -1;
    }

    s32 const Position = static_cast<s32>( Base + Offset );
    return ( x_fseek( Stream.pFile, Position, X_SEEK_SET ) == 0 ) ? Position : -1;
}

//=========================================================================

static
usize SDLCALL ReadTextureFile( void* pUserData, void* pBuffer, usize Size, SDL_IOStatus* pStatus )
{
    texture_stream const& Stream = *static_cast<texture_stream const*>( pUserData );
    s32 const BytesRequested = static_cast<s32>( MIN( Size, static_cast<usize>( S32_MAX ) ) );
    s32 const BytesRead      = x_fread( pBuffer, 1, BytesRequested, Stream.pFile );

    if( BytesRead < BytesRequested )
    {
        *pStatus = x_feof( Stream.pFile ) ? SDL_IO_STATUS_EOF : SDL_IO_STATUS_ERROR;
    }

    return static_cast<usize>( MAX( 0, BytesRead ) );
}

//=========================================================================

static
xbool LoadPNG( xbitmap& Bitmap, X_FILE* pFile )
{
    texture_stream Stream{ pFile, x_flength( pFile ) };
    if( Stream.Length <= 0 )
    {
        return FALSE;
    }

    // SDL owns the adapter, not the X_FILE used by the resource manager.
    SDL_IOStreamInterface Interface;
    SDL_INIT_INTERFACE( &Interface );

    Interface.seek = SeekTextureFile;
    Interface.read = ReadTextureFile;

    SDL_IOStream* pStream = SDL_OpenIO( &Interface, &Stream );
    if( !pStream )
    {
        return FALSE;
    }

    SDL_Surface* pSurface = SDL_LoadPNG_IO( pStream, true );
    if( !pSurface )
    {
        return FALSE;
    }

    s32 const Width  = pSurface->w;
    s32 const Height = pSurface->h;

    // xbitmap stores signed 16-bit dimensions and Setup multiplies in bits.
    if( !IN_RANGE( 1, Width, S16_MAX ) || !IN_RANGE( 1, Height, S16_MAX ) ||
        ( static_cast<s64>( Width ) * Height * 32 > S32_MAX ) )
    {
        SDL_DestroySurface( pSurface );
        return FALSE;
    }

    // Convert palette transparency and color keys to alpha as well as RGBA pixels.
    SDL_Surface* pConverted = SDL_ConvertSurface( pSurface, SDL_PIXELFORMAT_ARGB8888 );
    SDL_DestroySurface( pSurface );
    if( !pConverted )
    {
        return FALSE;
    }

    s32 const RowBytes = Width * static_cast<s32>( sizeof( u32 ) );
    byte*     pData    = static_cast<byte*>( x_malloc( RowBytes * Height ) );
    if( !pData )
    {
        SDL_DestroySurface( pConverted );
        return FALSE;
    }

    byte const* pPixels = static_cast<byte const*>( pConverted->pixels );
    for( s32 Y = 0; Y < Height; ++Y )
    {
        x_memcpy( pData + Y * RowBytes, pPixels + Y * pConverted->pitch, RowBytes );
    }

    SDL_DestroySurface( pConverted );
    Bitmap.Setup( xbitmap::FMT_32_ARGB_8888, Width, Height, TRUE, pData );
    return TRUE;
}

//=========================================================================
//  TGA DECODING
//=========================================================================

static
xbool LoadTGA( xbitmap& Bitmap, X_FILE* pFile )
{
    if( !auxbmp_LoadTGA( Bitmap, pFile ) )
    {
        return FALSE;
    }

    // Use the same upload format as PNG; the TGA reader also returns 15/16-
    // and 24-bit bitmaps that may not have a matching GPU format.
    if( Bitmap.GetFormat() != xbitmap::FMT_32_ARGB_8888 )
    {
        Bitmap.ConvertFormat( xbitmap::FMT_32_ARGB_8888 );
    }
    return TRUE;
}

//=========================================================================
//  TEXTURE FILE LOADING
//=========================================================================

xbool texture_file::LoadXBMP( xbitmap& Bitmap, X_FILE* pFile )
{
    if( !Bitmap.Load( pFile ) )
    {
        return FALSE;
    }

    switch( Bitmap.GetFormat() )
    {
        case xbitmap::FMT_32_ARGB_8888:
        case xbitmap::FMT_32_URGB_8888:
        case xbitmap::FMT_16_ARGB_4444:
        case xbitmap::FMT_16_ARGB_1555:
        case xbitmap::FMT_16_URGB_1555:
        case xbitmap::FMT_16_RGB_565:
#ifdef TARGET_DESKTOP
        case xbitmap::FMT_DXT1:
        case xbitmap::FMT_DXT3:
        case xbitmap::FMT_DXT5:
#endif
#ifdef TARGET_MOBILE
        case xbitmap::FMT_ASTC_4X4:
        case xbitmap::FMT_ASTC_5X4:
        case xbitmap::FMT_ASTC_5X5:
        case xbitmap::FMT_ASTC_6X5:
        case xbitmap::FMT_ASTC_6X6:
        case xbitmap::FMT_ASTC_8X5:
        case xbitmap::FMT_ASTC_8X6:
        case xbitmap::FMT_ASTC_8X8:
        case xbitmap::FMT_ASTC_10X5:
        case xbitmap::FMT_ASTC_10X6:
        case xbitmap::FMT_ASTC_10X8:
        case xbitmap::FMT_ASTC_10X10:
        case xbitmap::FMT_ASTC_12X10:
        case xbitmap::FMT_ASTC_12X12:
#endif
        {
            return TRUE;
        }
        break;

        default:
        {
            Bitmap.Kill();
            return FALSE;
        }
        break;
    }
}

//=========================================================================

X_FILE* texture_file::LoadWithFallback( xbitmap& Bitmap, char const* pFileName )
{
    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( pFile )
    {
        if( LoadXBMP( Bitmap, pFile ) )
        {
            return pFile;
        }
        x_fclose( pFile );
    }

    char const* pExtension = x_strrchr( pFileName, '.' );
    if( !pExtension || ( x_stricmp( pExtension, ".xbmp" ) != 0 ) )
    {
        return NULL;
    }

    static texture_fallback const s_Fallbacks[] =
    {
        { ".png", LoadPNG },
        { ".tga", LoadTGA },
    };

    xstring const Stem( pFileName, static_cast<s32>( pExtension - pFileName ) );
    for( texture_fallback const& Fallback : s_Fallbacks )
    {
        xstring const Path = Stem + Fallback.pExtension;
        pFile = x_fopen( Path, "rb" );
        if( !pFile )
        {
            continue;
        }

        if( Fallback.pLoad( Bitmap, pFile ) )
        {
            x_DebugMsg( "texture_file::LoadWithFallback: Using fallback '%s' for '%s'\n",
                        static_cast<char const*>( Path ), pFileName );
            return pFile;
        }
        x_fclose( pFile );
    }

    return NULL;
}
