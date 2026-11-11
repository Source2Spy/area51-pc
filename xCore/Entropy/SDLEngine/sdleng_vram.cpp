//==============================================================================
//
//  sdleng_vram.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    SDLENG_MAX_TEXTURE_UPLOAD_BATCH = 128
};

//==============================================================================
//  TYPES
//==============================================================================

struct sdlvram_prepared_upload
{
    vram_texture_region Region;
    u32                 RowPitch;
    u32                 SlicePitch;
    u32                 SourceRowPitch;
    u32                 SourceSlicePitch;
    u32                 RowBytes;
    u32                 RowCount;
    u32                 UploadSize;
    u32                 Offset;
    xbool               bGenerateMips;

    sdlvram_prepared_upload( void ) :
        Region          (),
        RowPitch        ( 0 ),
        SlicePitch      ( 0 ),
        SourceRowPitch  ( 0 ),
        SourceSlicePitch( 0 ),
        RowBytes        ( 0 ),
        RowCount        ( 0 ),
        UploadSize      ( 0 ),
        Offset          ( 0 ),
        bGenerateMips   ( FALSE )
    {
    }
};

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static vram_texture_backend* s_pTextureList = NULL;
static u32                   s_TextureCount = 0;

//==============================================================================
//  HELPERS
//==============================================================================

static
u32 sdlvram_MaxU32( u32 A, u32 B )
{
    return ( A > B ) ? A : B;
}

//==============================================================================

static
u32 sdlvram_MipExtent( u32 Extent, u32 MipLevel )
{
    Extent >>= MipLevel;

    return Extent ? Extent : 1;
}

//==============================================================================

static
xbool sdlvram_ToSDLTextureType( vram_texture_type Type, SDL_GPUTextureType& SDLType )
{
    switch( Type )
    {
        case VRAM_TEXTURE_TYPE_2D:
        {
            SDLType = SDL_GPU_TEXTURETYPE_2D;
            return TRUE;
        }
        break;

        case VRAM_TEXTURE_TYPE_2D_ARRAY:
        {
            SDLType = SDL_GPU_TEXTURETYPE_2D_ARRAY;
            return TRUE;
        }
        break;

        case VRAM_TEXTURE_TYPE_3D:
        {
            SDLType = SDL_GPU_TEXTURETYPE_3D;
            return TRUE;
        }
        break;

        case VRAM_TEXTURE_TYPE_CUBE:
        {
            SDLType = SDL_GPU_TEXTURETYPE_CUBE;
            return TRUE;
        }
        break;

        case VRAM_TEXTURE_TYPE_CUBE_ARRAY:
        {
            SDLType = SDL_GPU_TEXTURETYPE_CUBE_ARRAY;
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
        break;
    }
}

//==============================================================================

static
SDL_GPUTextureUsageFlags sdlvram_ToSDLUsageFlags( u32 UsageFlags )
{
    SDL_GPUTextureUsageFlags SDLFlags = 0;

    if( UsageFlags & VRAM_TEXTURE_USAGE_SAMPLED )
    {
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    }

    if( UsageFlags & VRAM_TEXTURE_USAGE_COLOR_TARGET )
    {
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    }

    if( UsageFlags & VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET )
    {
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    }

    if( UsageFlags & VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ )
    {
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ;
    }

    if( UsageFlags & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ )
    {
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    }

    if( UsageFlags & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE )
    {
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
    }

    if( UsageFlags & VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE )
    {
        SDLFlags |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;
    }

    return SDLFlags;
}

//==============================================================================

static
xbool sdlvram_IsASTCFormat( vram_texture_format Format )
{
    return ( Format >= VRAM_TEXTURE_FORMAT_ASTC_4X4 ) &&
           ( Format <= VRAM_TEXTURE_FORMAT_ASTC_12X12_SRGB );
}

//==============================================================================

static
xbitmap::format sdlvram_ASTCBitmapFormat( vram_texture_format Format )
{
    const s32 First = ( Format >= VRAM_TEXTURE_FORMAT_ASTC_4X4_SRGB ) ? VRAM_TEXTURE_FORMAT_ASTC_4X4_SRGB : VRAM_TEXTURE_FORMAT_ASTC_4X4;

    return (xbitmap::format)( xbitmap::FMT_ASTC_4X4 + Format - First );
}

//==============================================================================

static
u32 sdlvram_GetBlockWidth( vram_texture_format Format )
{
    return sdlvram_IsASTCFormat( Format ) ? xbitmap::GetBlockWidth( sdlvram_ASTCBitmapFormat( Format ) ) : 4;
}

//==============================================================================

static
u32 sdlvram_GetBlockHeight( vram_texture_format Format )
{
    return sdlvram_IsASTCFormat( Format ) ? xbitmap::GetBlockHeight( sdlvram_ASTCBitmapFormat( Format ) ) : 4;
}

//==============================================================================

static
xbool sdlvram_IsCompressedFormat( vram_texture_format Format )
{
    return ( Format == VRAM_TEXTURE_FORMAT_BC1_RGBA ) ||
           ( Format == VRAM_TEXTURE_FORMAT_BC2_RGBA ) ||
           ( Format == VRAM_TEXTURE_FORMAT_BC3_RGBA ) ||
           sdlvram_IsASTCFormat( Format );
}

//==============================================================================

static
xbool sdlvram_CanGenerateMipmaps( const vram_texture_desc& Desc )
{
    return ( ( Desc.UsageFlags & VRAM_TEXTURE_USAGE_COLOR_TARGET ) != 0 ) &&
           !sdlvram_IsCompressedFormat( Desc.Format ) &&
           !sdleng_IsDepthFormat( Desc.Format );
}

//==============================================================================

static
u32 sdlvram_GetBlockSize( vram_texture_format Format )
{
    if( sdlvram_IsASTCFormat( Format ) )
    {
        return 16;
    }

    switch( Format )
    {
        case VRAM_TEXTURE_FORMAT_BC1_RGBA: return 8;  break;
        case VRAM_TEXTURE_FORMAT_BC2_RGBA: return 16; break;
        case VRAM_TEXTURE_FORMAT_BC3_RGBA: return 16; break;
        default:                           return 0;  break;
    }
}

//==============================================================================

static
u32 sdlvram_GetBytesPerPixel( vram_texture_format Format )
{
    switch( Format )
    {
        case VRAM_TEXTURE_FORMAT_R8:       return 1;  break;
        case VRAM_TEXTURE_FORMAT_B5G6R5:   return 2;  break;
        case VRAM_TEXTURE_FORMAT_B5G5R5A1: return 2;  break;
        case VRAM_TEXTURE_FORMAT_B4G4R4A4: return 2;  break;
        case VRAM_TEXTURE_FORMAT_RG16F:    return 4;  break;
        case VRAM_TEXTURE_FORMAT_R32F:     return 4;  break;
        case VRAM_TEXTURE_FORMAT_RGBA8:    return 4;  break;
        case VRAM_TEXTURE_FORMAT_BGRA8:    return 4;  break;
        case VRAM_TEXTURE_FORMAT_RGB10A2:  return 4;  break;
        case VRAM_TEXTURE_FORMAT_RGBA16F:  return 8;  break;
        case VRAM_TEXTURE_FORMAT_RGBA32F:  return 16; break;
        default:                           return 0;  break;
    }
}

//==============================================================================

static
u32 sdlvram_CalcRowPitch( vram_texture_format Format, u32 Width )
{
    u64 Pitch;

    if( sdlvram_IsCompressedFormat( Format ) )
    {
        const u64 BlocksWide = ( (u64)Width + sdlvram_GetBlockWidth( Format ) - 1 ) / sdlvram_GetBlockWidth( Format );
        Pitch                = BlocksWide * sdlvram_GetBlockSize( Format );
    }
    else
    {
        Pitch = (u64)Width * sdlvram_GetBytesPerPixel( Format );
    }

    return ( Pitch <= 0xffffffffULL ) ? (u32)Pitch : 0;
}

//==============================================================================

static
u32 sdlvram_CalcSlicePitch( vram_texture_format Format, u32 Width, u32 Height )
{
    const u64 RowPitch = sdlvram_CalcRowPitch( Format, Width );
    u64       SlicePitch;

    if( sdlvram_IsCompressedFormat( Format ) )
    {
        SlicePitch = RowPitch * ( ( (u64)Height + sdlvram_GetBlockHeight( Format ) - 1 ) / sdlvram_GetBlockHeight( Format ) );
    }
    else
    {
        SlicePitch = RowPitch * Height;
    }

    return ( SlicePitch <= 0xffffffffULL ) ? (u32)SlicePitch : 0;
}

//==============================================================================

static
xbool sdlvram_CalcBitmapUploadLayout( const xbitmap& Bitmap,
                                      s32            Mip,
                                      u32&           RowPitch,
                                      u32&           UploadSize )
{
    const s32 PWidth = Bitmap.GetPWidth( Mip );
    const s32 Height = Bitmap.GetHeight( Mip );
    const u32 BPP    = Bitmap.GetFormatInfo().BPP;

    if( ( PWidth <= 0 ) || ( Height <= 0 ) || ( BPP == 0 ) || ( ( BPP & 7 ) != 0 ) )
    {
        return FALSE;
    }

    const u64 RowPitch64   = (u64)(u32)PWidth * (u64)BPP / 8;
    const u64 UploadSize64 = RowPitch64 * (u64)(u32)Height;

    if( ( RowPitch64 == 0 ) || ( UploadSize64 == 0 ) ||
        ( RowPitch64 > 0xffffffffULL ) || ( UploadSize64 > 0xffffffffULL ) )
    {
        return FALSE;
    }

    RowPitch   = (u32)RowPitch64;
    UploadSize = (u32)UploadSize64;

    return TRUE;
}

//==============================================================================

static
xbool sdlvram_ValidateTextureDesc( const vram_texture_desc&  Desc,
                                   SDL_GPUTextureType&       TextureType,
                                   SDL_GPUTextureFormat&     Format,
                                   SDL_GPUSampleCount&       SampleCount,
                                   SDL_GPUTextureUsageFlags& UsageFlags )
{
    if( !g_pSDLGPUDevice || ( Desc.Width == 0 ) || ( Desc.Height == 0 ) ||
        ( Desc.Depth == 0 ) || ( Desc.LayerCount == 0 ) || ( Desc.MipCount == 0 ) )
    {
        return FALSE;
    }

    if( !sdlvram_ToSDLTextureType( Desc.Type, TextureType ) ||
        !sdleng_ToSDLSampleCount( Desc.SampleCount, SampleCount ) )
    {
        return FALSE;
    }

    Format = sdleng_ToSDLTextureFormat( Desc.Format );
    if( Format == SDL_GPU_TEXTUREFORMAT_INVALID )
    {
        return FALSE;
    }

    const u32 KnownUsageFlags = VRAM_TEXTURE_USAGE_SAMPLED |
                                VRAM_TEXTURE_USAGE_COLOR_TARGET |
                                VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET |
                                VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ |
                                VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ |
                                VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE |
                                VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE;

    if( ( Desc.UsageFlags == 0 ) || ( Desc.UsageFlags & ~KnownUsageFlags ) )
    {
        return FALSE;
    }

    if( ( Desc.UsageFlags & VRAM_TEXTURE_USAGE_SAMPLED ) &&
        ( Desc.UsageFlags & VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ ) )
    {
        return FALSE;
    }

    if( sdleng_IsDepthFormat( Desc.Format ) )
    {
        const u32 AllowedDepthUsage = VRAM_TEXTURE_USAGE_SAMPLED |
                                      VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET;

        if( Desc.UsageFlags & ~AllowedDepthUsage )
        {
            return FALSE;
        }
    }
    else if( Desc.UsageFlags & VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET )
    {
        return FALSE;
    }

    switch( Desc.Type )
    {
        case VRAM_TEXTURE_TYPE_2D:
        {
            if( ( Desc.Width > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
                ( Desc.Height > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
                ( Desc.Depth != 1 ) || ( Desc.LayerCount != 1 ) )
            {
                return FALSE;
            }
        }

        break;

        case VRAM_TEXTURE_TYPE_2D_ARRAY:
        {
            if( ( Desc.Width > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
                ( Desc.Height > SDLENG_MAX_2D_TEXTURE_DIMENSION ) || ( Desc.Depth != 1 ) )
            {
                return FALSE;
            }
        }

        break;

        case VRAM_TEXTURE_TYPE_3D:
        {
            if( ( Desc.Width > SDLENG_MAX_3D_TEXTURE_DIMENSION ) ||
                ( Desc.Height > SDLENG_MAX_3D_TEXTURE_DIMENSION ) ||
                ( Desc.Depth > SDLENG_MAX_3D_TEXTURE_DIMENSION ) ||
                ( Desc.LayerCount != 1 ) ||
                ( Desc.UsageFlags & VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET ) )
            {
                return FALSE;
            }
        }

        break;

        case VRAM_TEXTURE_TYPE_CUBE:
        {
            if( ( Desc.Width > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
                ( Desc.Height > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
                ( Desc.Width != Desc.Height ) || ( Desc.Depth != 1 ) || ( Desc.LayerCount != 6 ) )
            {
                return FALSE;
            }
        }

        break;

        case VRAM_TEXTURE_TYPE_CUBE_ARRAY:
        {
            if( ( Desc.Width > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
                ( Desc.Height > SDLENG_MAX_2D_TEXTURE_DIMENSION ) ||
                ( Desc.Width != Desc.Height ) || ( Desc.Depth != 1 ) || ( ( Desc.LayerCount % 6 ) != 0 ) )
            {
                return FALSE;
            }
        }

        break;

        default:
        {
            return FALSE;
        }

        break;
    }

    const u32 MaxMipCount = vram_CalcMipCount( Desc.Width,
                                               Desc.Height,
                                               ( Desc.Type == VRAM_TEXTURE_TYPE_3D ) ? Desc.Depth : 1 );

    if( Desc.MipCount > MaxMipCount )
    {
        return FALSE;
    }

    if( Desc.SampleCount > 1 )
    {
        const u32 InvalidMSAAUsage = VRAM_TEXTURE_USAGE_SAMPLED |
                                     VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE |
                                     VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE;

        if( ( Desc.Type != VRAM_TEXTURE_TYPE_2D ) || ( Desc.MipCount != 1 ) ||
            ( Desc.UsageFlags & InvalidMSAAUsage ) ||
            !( Desc.UsageFlags & ( VRAM_TEXTURE_USAGE_COLOR_TARGET |
                                   VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET ) ) )
        {
            return FALSE;
        }
    }

    UsageFlags = sdlvram_ToSDLUsageFlags( Desc.UsageFlags );
    if( !UsageFlags ||
        !SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice, Format, TextureType, UsageFlags ) )
    {
        return FALSE;
    }

    if( !SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, Format, SampleCount ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
vram_texture_format sdlvram_FromBitmapFormat( const xbitmap& Bitmap )
{
    const xbitmap::format Format = Bitmap.GetFormat();
    if( Bitmap.IsASTC() )
    {
        const s32 First = Bitmap.IsSRGB() ? VRAM_TEXTURE_FORMAT_ASTC_4X4_SRGB : VRAM_TEXTURE_FORMAT_ASTC_4X4;
        return (vram_texture_format)( First + Format - xbitmap::FMT_ASTC_4X4 );
    }

    switch( Format )
    {
        case xbitmap::FMT_32_ARGB_8888: return VRAM_TEXTURE_FORMAT_BGRA8;    break;
        case xbitmap::FMT_32_URGB_8888: return VRAM_TEXTURE_FORMAT_BGRA8;    break;
        case xbitmap::FMT_16_ARGB_4444: return VRAM_TEXTURE_FORMAT_B4G4R4A4; break;
        case xbitmap::FMT_16_ARGB_1555: return VRAM_TEXTURE_FORMAT_B5G5R5A1; break;
        case xbitmap::FMT_16_URGB_1555: return VRAM_TEXTURE_FORMAT_B5G5R5A1; break;
        case xbitmap::FMT_16_RGB_565:   return VRAM_TEXTURE_FORMAT_B5G6R5;   break;
        case xbitmap::FMT_DXT1:         return VRAM_TEXTURE_FORMAT_BC1_RGBA; break;
        case xbitmap::FMT_DXT3:         return VRAM_TEXTURE_FORMAT_BC2_RGBA; break;
        case xbitmap::FMT_DXT5:         return VRAM_TEXTURE_FORMAT_BC3_RGBA; break;
        default:                        return VRAM_TEXTURE_FORMAT_COUNT;    break;
    }
}

//==============================================================================

static
void sdlvram_ReleaseBackend( vram_texture_backend* pBackend )
{
    if( !pBackend )
    {
        return;
    }

    if( pBackend->pTexture && g_pSDLGPUDevice )
    {
        SDL_ReleaseGPUTexture( g_pSDLGPUDevice, pBackend->pTexture );
    }

    pBackend->pTexture                 = NULL;
    pBackend->Resource.pBackend        = NULL;
    pBackend->ResourceBackend.Kind     = SDLENG_SHADER_RESOURCE_NONE;
    pBackend->ResourceBackend.pTexture = NULL;
    pBackend->ResourceBackend.pBuffer  = NULL;
}

//==============================================================================

static
void sdlvram_AdoptTexture( vram_texture& Destination, vram_texture& Source )
{
    vram_texture_backend* pNewBackend = Source.pBackend;
    vram_texture_backend* pOldBackend = Destination.pBackend;
    ASSERT( pNewBackend != NULL );

    if( pOldBackend )
    {
        sdleng_ResetShaderBindings();
        sdleng_UnlinkBackend( s_pTextureList, pOldBackend, s_TextureCount );
        sdlvram_ReleaseBackend( pOldBackend );
        delete pOldBackend;
    }

    Destination.Desc                           = Source.Desc;
    Destination.pBackend                       = pNewBackend;
    pNewBackend->pOwner                        = &Destination;
    pNewBackend->ResourceBackend.pTextureOwner = &Destination;
    Source.Desc                                = vram_texture_desc();
    Source.pBackend                            = NULL;
}

//==============================================================================

static
xbool sdlvram_FillRegionFromTexture( const vram_texture&  Texture,
                                     vram_texture_region  Region,
                                     vram_texture_region& OutRegion )
{
    if( !Texture.pBackend )
    {
        return FALSE;
    }

    if( Region.MipLevel >= Texture.Desc.MipCount )
    {
        return FALSE;
    }

    OutRegion = Region;
    if( OutRegion.Width == 0 )
    {
        OutRegion.Width = sdlvram_MipExtent( Texture.Desc.Width, OutRegion.MipLevel );
    }

    if( OutRegion.Height == 0 )
    {
        OutRegion.Height = sdlvram_MipExtent( Texture.Desc.Height, OutRegion.MipLevel );
    }

    if( OutRegion.Depth == 0 )
    {
        OutRegion.Depth = ( Texture.Desc.Type == VRAM_TEXTURE_TYPE_3D )
                              ? sdlvram_MipExtent( Texture.Desc.Depth, OutRegion.MipLevel )
                              : 1;
    }

    const u32 MipWidth  = sdlvram_MipExtent( Texture.Desc.Width, OutRegion.MipLevel );
    const u32 MipHeight = sdlvram_MipExtent( Texture.Desc.Height, OutRegion.MipLevel );
    const u32 MipDepth  = ( Texture.Desc.Type == VRAM_TEXTURE_TYPE_3D )
                              ? sdlvram_MipExtent( Texture.Desc.Depth, OutRegion.MipLevel )
                              : 1;

    if( ( OutRegion.Width == 0 ) || ( OutRegion.Height == 0 ) || ( OutRegion.Depth == 0 ) ||
        ( OutRegion.X >= MipWidth ) || ( OutRegion.Y >= MipHeight ) || ( OutRegion.Z >= MipDepth ) ||
        ( OutRegion.Width > ( MipWidth - OutRegion.X ) ) ||
        ( OutRegion.Height > ( MipHeight - OutRegion.Y ) ) ||
        ( OutRegion.Depth > ( MipDepth - OutRegion.Z ) ) )
    {
        return FALSE;
    }

    if( Texture.Desc.Type == VRAM_TEXTURE_TYPE_3D )
    {
        if( OutRegion.Layer != 0 )
        {
            return FALSE;
        }
    }
    else if( OutRegion.Layer >= Texture.Desc.LayerCount )
    {
        return FALSE;
    }

    if( sdlvram_IsCompressedFormat( Texture.Desc.Format ) )
    {
        const xbool bWidthReachesEdge  = ( OutRegion.X + OutRegion.Width ) == MipWidth;
        const xbool bHeightReachesEdge = ( OutRegion.Y + OutRegion.Height ) == MipHeight;

        if( ( OutRegion.X % sdlvram_GetBlockWidth( Texture.Desc.Format ) ) || ( OutRegion.Y % sdlvram_GetBlockHeight( Texture.Desc.Format ) ) ||
            ( !bWidthReachesEdge && ( OutRegion.Width % sdlvram_GetBlockWidth( Texture.Desc.Format ) ) ) ||
            ( !bHeightReachesEdge && ( OutRegion.Height % sdlvram_GetBlockHeight( Texture.Desc.Format ) ) ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static
u32 sdlvram_AlignUploadOffset( u32 Offset )
{
    const u32 Alignment = 512;

    if( Offset > ( 0xffffffffu - ( Alignment - 1 ) ) )
    {
        return 0xffffffffu;
    }

    return ( Offset + ( Alignment - 1 ) ) & ~( Alignment - 1 );
}

//==============================================================================

static
xbool sdlvram_PrepareUpload( const vram_texture&             Texture,
                             const vram_texture_upload_desc& Upload,
                             sdlvram_prepared_upload&        Prepared )
{
    if( !Texture.pBackend ||
        !shader_ValidateResourceAccess( Texture.pBackend->Resource, SHADER_RESOURCE_ACCESS_UPLOAD ) )
    {
        return FALSE;
    }

    if( !Texture.pBackend || !Texture.pBackend->pTexture || !Upload.pData )
    {
        return FALSE;
    }

    if( !sdlvram_FillRegionFromTexture( Texture, Upload.Region, Prepared.Region ) )
    {
        return FALSE;
    }

    const xbool bCompressed = sdlvram_IsCompressedFormat( Texture.Desc.Format );
    Prepared.bGenerateMips  = Upload.bGenerateMips && ( Texture.Desc.MipCount > 1 );

    if( Prepared.bGenerateMips && !sdlvram_CanGenerateMipmaps( Texture.Desc ) )
    {
        x_DebugMsg( "SDLVRAM: texture was not created for GPU mip generation\n" );
        return FALSE;
    }

    Prepared.RowBytes = sdlvram_CalcRowPitch( Texture.Desc.Format, Prepared.Region.Width );
    Prepared.RowCount = bCompressed ? ( Prepared.Region.Height + sdlvram_GetBlockHeight( Texture.Desc.Format ) - 1 ) / sdlvram_GetBlockHeight( Texture.Desc.Format ) : Prepared.Region.Height;
    Prepared.RowPitch = Upload.RowPitch ? Upload.RowPitch : Prepared.RowBytes;

    if( Upload.SlicePitch )
    {
        Prepared.SlicePitch = Upload.SlicePitch;
    }
    else if( Upload.RowPitch )
    {
        const u64 SlicePitch = static_cast<u64>( Prepared.RowPitch ) * Prepared.RowCount;
        Prepared.SlicePitch  = ( SlicePitch <= 0xffffffffULL ) ? static_cast<u32>( SlicePitch ) : 0;
    }
    else
    {
        Prepared.SlicePitch = sdlvram_CalcSlicePitch( Texture.Desc.Format, Prepared.Region.Width, Prepared.Region.Height );
    }

    const u64 RequiredSize = (u64)Prepared.SlicePitch * Prepared.Region.Depth;

    if( RequiredSize > 0xffffffffULL )
    {
        return FALSE;
    }

    Prepared.UploadSize = (u32)RequiredSize;

    if( ( Prepared.RowPitch == 0 ) || ( Prepared.SlicePitch == 0 ) ||
        ( Upload.Size && ( Upload.Size < RequiredSize ) ) )
    {
        return FALSE;
    }

    const u32 BytesPerUnit = bCompressed ? sdlvram_GetBlockSize( Texture.Desc.Format ) : sdlvram_GetBytesPerPixel( Texture.Desc.Format );

    if( !BytesPerUnit || ( Prepared.RowPitch < Prepared.RowBytes ) ||
        ( Prepared.RowPitch % BytesPerUnit ) ||
        ( Prepared.SlicePitch < (u64)Prepared.RowPitch * Prepared.RowCount ) ||
        ( Prepared.SlicePitch % Prepared.RowPitch ) )
    {
        return FALSE;
    }

    Prepared.SourceRowPitch   = Prepared.RowPitch;
    Prepared.SourceSlicePitch = Prepared.SlicePitch;
    const char* pDriver       = SDL_GetGPUDeviceDriver( g_pSDLGPUDevice );

    if( pDriver && ( x_strcmp( pDriver, "direct3d12" ) == 0 ) )
    {
        const u64 RowPitch   = ( (u64)Prepared.RowBytes + 255 ) & ~255ULL;
        const u64 SlicePitch = RowPitch * Prepared.RowCount;
        const u64 UploadSize = SlicePitch * Prepared.Region.Depth;

        if( UploadSize > 0xffffffffULL )
        {
            return FALSE;
        }

        Prepared.RowPitch   = (u32)RowPitch;
        Prepared.SlicePitch = (u32)SlicePitch;
        Prepared.UploadSize = (u32)UploadSize;
    }

    return TRUE;
}

//==============================================================================

static
xbool sdlvram_SubmitUploadBatch( const vram_texture_upload_batch_item* pItems, u32 Count )
{
    if( Count == 0 )
    {
        return TRUE;
    }

    if( !pItems || ( Count > SDLENG_MAX_TEXTURE_UPLOAD_BATCH ) ||
        !g_pSDLGPUDevice || sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        return FALSE;
    }

    sdlvram_prepared_upload PreparedUploads[SDLENG_MAX_TEXTURE_UPLOAD_BATCH];

    u64 TotalSize64 = 0;
    u32 UploadBytes = 0;

    for( u32 i = 0; i < Count; ++i )
    {
        if( !pItems[i].pTexture ||
            !sdlvram_PrepareUpload( *pItems[i].pTexture, pItems[i].Upload, PreparedUploads[i] ) )
        {
            return FALSE;
        }

        if( TotalSize64 > 0xffffffffULL )
        {
            return FALSE;
        }

        const u32 Offset = sdlvram_AlignUploadOffset( (u32)TotalSize64 );

        if( ( Offset == 0xffffffffu ) ||
            ( PreparedUploads[i].UploadSize > ( 0xffffffffu - Offset ) ) )
        {
            return FALSE;
        }

        TotalSize64 = (u64)Offset + PreparedUploads[i].UploadSize;

        if( UploadBytes > ( 0xffffffffu - PreparedUploads[i].UploadSize ) )
        {
            return FALSE;
        }

        UploadBytes += PreparedUploads[i].UploadSize;
    }

    if( ( TotalSize64 == 0 ) || ( TotalSize64 > 0xffffffffULL ) )
    {
        return FALSE;
    }

    if( !sdleng_BeginTransfer( (u32)TotalSize64, 512, "SDLVRAM" ) )
    {
        return FALSE;
    }

    for( u32 i = 0; i < Count; ++i )
    {
        if( !sdleng_ReserveTransfer( PreparedUploads[i].UploadSize,
                                     512,
                                     PreparedUploads[i].Offset ) )
        {
            sdleng_CancelTransfer();
            return FALSE;
        }
    }

    void* pMapped = NULL;

    if( !sdleng_MapTransferBuffer( pMapped, "SDLVRAM" ) )
    {
        sdleng_CancelTransfer();
        return FALSE;
    }

    SDL_GPUTransferBuffer* pTransferBuffer = sdleng_GetTransferBuffer();

    for( u32 i = 0; i < Count; ++i )
    {
        const sdlvram_prepared_upload& Prepared     = PreparedUploads[i];
        u8*                            pDestination = (u8*)pMapped + Prepared.Offset;
        const u8*                      pSource      = (const u8*)pItems[i].Upload.pData;

        if( ( Prepared.RowPitch == Prepared.SourceRowPitch ) &&
            ( Prepared.SlicePitch == Prepared.SourceSlicePitch ) )
        {
            x_memcpy( pDestination, pSource, Prepared.UploadSize );
        }
        else
        {
            for( u32 z = 0; z < Prepared.Region.Depth; ++z )
            {
                for( u32 y = 0; y < Prepared.RowCount; ++y )
                {
                    x_memcpy( pDestination + z * Prepared.SlicePitch + y * Prepared.RowPitch,
                              pSource + z * Prepared.SourceSlicePitch + y * Prepared.SourceRowPitch,
                              Prepared.RowBytes );
                }
            }
        }
    }

    sdleng_UnmapTransferBuffer();

    SDL_GPUCopyPass* pCopyPass = sdleng_GetTransferCopyPass();

    if( !pTransferBuffer || !pCopyPass )
    {
        sdleng_CancelTransfer();
        return FALSE;
    }

    for( u32 i = 0; i < Count; ++i )
    {
        SDL_GPUTextureTransferInfo Source;
        x_memset( &Source, 0, sizeof( Source ) );
        Source.transfer_buffer = pTransferBuffer;
        Source.offset          = PreparedUploads[i].Offset;

        const xbool                    bCompressed = sdlvram_IsCompressedFormat( pItems[i].pTexture->Desc.Format );
        const sdlvram_prepared_upload& Prepared    = PreparedUploads[i];

        if( Prepared.RowPitch != Prepared.RowBytes )
        {
            Source.pixels_per_row = bCompressed ? Prepared.RowPitch / sdlvram_GetBlockSize( pItems[i].pTexture->Desc.Format ) * sdlvram_GetBlockWidth( pItems[i].pTexture->Desc.Format ) : Prepared.RowPitch / sdlvram_GetBytesPerPixel( pItems[i].pTexture->Desc.Format );
        }

        if( Prepared.SlicePitch != Prepared.RowPitch * Prepared.RowCount )
        {
            Source.rows_per_layer = Prepared.SlicePitch / Prepared.RowPitch * ( bCompressed ? sdlvram_GetBlockHeight( pItems[i].pTexture->Desc.Format ) : 1 );
        }

        SDL_GPUTextureRegion Destination;
        x_memset( &Destination, 0, sizeof( Destination ) );
        Destination.texture   = pItems[i].pTexture->pBackend->pTexture;
        Destination.mip_level = PreparedUploads[i].Region.MipLevel;
        Destination.layer     = PreparedUploads[i].Region.Layer;
        Destination.x         = PreparedUploads[i].Region.X;
        Destination.y         = PreparedUploads[i].Region.Y;
        Destination.z         = PreparedUploads[i].Region.Z;
        Destination.w         = PreparedUploads[i].Region.Width;
        Destination.h         = PreparedUploads[i].Region.Height;
        Destination.d         = PreparedUploads[i].Region.Depth;

        xbool bCycle       = FALSE;
        xbool bFirstUpload = TRUE;

        for( u32 j = 0; j < Count; ++j )
        {
            if( pItems[j].pTexture == pItems[i].pTexture )
            {
                bCycle |= pItems[j].Upload.bCycle;

                if( j < i )
                {
                    bFirstUpload = FALSE;
                }
            }
        }

        SDL_UploadToGPUTexture( pCopyPass, &Source, &Destination, bFirstUpload && bCycle );
    }

    xbool bGenerateMips = FALSE;

    for( u32 i = 0; i < Count; ++i )
    {
        if( PreparedUploads[i].bGenerateMips )
        {
            bGenerateMips = TRUE;
            break;
        }
    }

    if( bGenerateMips )
    {
        sdleng_EndTransferPass();

        for( u32 i = 0; i < Count; ++i )
        {
            xbool bAlreadyGenerated = FALSE;

            for( u32 j = 0; j < i; ++j )
            {
                if( PreparedUploads[j].bGenerateMips && ( pItems[j].pTexture == pItems[i].pTexture ) )
                {
                    bAlreadyGenerated = TRUE;
                }
            }

            if( PreparedUploads[i].bGenerateMips && !bAlreadyGenerated )
            {
                SDL_GenerateMipmapsForGPUTexture( sdleng_GetTransferCommandBuffer(),
                                                  pItems[i].pTexture->pBackend->pTexture );
            }
        }
    }

    static xprofile_counter TextureUploadRequests =
        x_GetProfiler().RegisterCounter( "TextureUploadRequests", "RenderCounter" );

    static xprofile_counter TextureUploadBytes =
        x_GetProfiler().RegisterCounter( "TextureUploadBytes", "RenderCounter" );

    TextureUploadRequests.Add( Count );
    TextureUploadBytes.Add( UploadBytes );

    return sdleng_CommitTransfer();
}

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void vram_Init( void )
{
    s_pTextureList = NULL;
    s_TextureCount = 0;
}

//==============================================================================

void vram_Kill( void )
{
    while( s_pTextureList )
    {
        vram_texture_backend* pBackend = s_pTextureList;
        sdleng_UnlinkBackend( s_pTextureList, pBackend, s_TextureCount );
        sdlvram_ReleaseBackend( pBackend );

        if( pBackend->pOwner )
        {
            pBackend->pOwner->Desc     = vram_texture_desc();
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        delete pBackend;
    }
}

//==============================================================================
//  TEXTURE OBJECTS
//==============================================================================

xbool vram_CreateTexture( vram_texture& Texture, const vram_texture_desc& Desc )
{
    if( Texture.pBackend && ( sdleng_InRenderPass() || sdleng_InComputePass() ) )
    {
        return FALSE;
    }

    SDL_GPUTextureType       TextureType;
    SDL_GPUTextureFormat     Format;
    SDL_GPUSampleCount       SampleCount;
    SDL_GPUTextureUsageFlags UsageFlags;
    if( !sdlvram_ValidateTextureDesc( Desc, TextureType, Format, SampleCount, UsageFlags ) )
    {
        x_DebugMsg( "SDLVRAM: invalid or unsupported texture descriptor for '%s'\n",
                    Desc.pDebugName ? Desc.pDebugName : "unnamed" );

        return FALSE;
    }

    SDL_GPUTextureCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof( CreateInfo ) );
    CreateInfo.type                 = TextureType;
    CreateInfo.format               = Format;
    CreateInfo.usage                = UsageFlags;
    CreateInfo.width                = Desc.Width;
    CreateInfo.height               = Desc.Height;
    CreateInfo.layer_count_or_depth = ( Desc.Type == VRAM_TEXTURE_TYPE_3D ) ? Desc.Depth : Desc.LayerCount;
    CreateInfo.num_levels           = Desc.MipCount;
    CreateInfo.sample_count         = SampleCount;

    SDL_GPUTexture* pTexture = SDL_CreateGPUTexture( g_pSDLGPUDevice, &CreateInfo );
    if( !pTexture )
    {
        sdleng_LogError( "SDLVRAM", "SDL_CreateGPUTexture" );
        return FALSE;
    }

    if( Desc.pDebugName )
    {
        SDL_SetGPUTextureName( g_pSDLGPUDevice, pTexture, Desc.pDebugName );
    }

    vram_texture_backend* pBackend = new vram_texture_backend;
    if( !pBackend )
    {
        SDL_ReleaseGPUTexture( g_pSDLGPUDevice, pTexture );
        return FALSE;
    }

    vram_texture_backend* pOldBackend       = Texture.pBackend;
    pBackend->pOwner                        = &Texture;
    pBackend->pTexture                      = pTexture;
    pBackend->Resource.pBackend             = &pBackend->ResourceBackend;
    pBackend->ResourceBackend.Kind          = SDLENG_SHADER_RESOURCE_TEXTURE;
    pBackend->ResourceBackend.pTexture      = pTexture;
    pBackend->ResourceBackend.UsageFlags    = UsageFlags;
    pBackend->ResourceBackend.MipCount      = Desc.MipCount;
    pBackend->ResourceBackend.LayerCount    = Desc.LayerCount;
    pBackend->ResourceBackend.pTextureOwner = &Texture;
    pBackend->ResourceBackend.pBuffer       = NULL;

    Texture.Desc     = Desc;
    Texture.pBackend = pBackend;

    if( pOldBackend )
    {
        sdleng_ResetShaderBindings();
        sdleng_UnlinkBackend( s_pTextureList, pOldBackend, s_TextureCount );
        sdlvram_ReleaseBackend( pOldBackend );
        delete pOldBackend;
    }

    sdleng_LinkBackend( s_pTextureList, pBackend, s_TextureCount );

    return TRUE;
}

//==============================================================================

xbool vram_CreateTexture( vram_texture& Texture, const xbitmap& Bitmap, xbool bGenerateMips, const char* pDebugName )
{
    if( Texture.pBackend && ( sdleng_InRenderPass() || sdleng_InComputePass() ) )
    {
        return FALSE;
    }

    vram_texture_format Format = sdlvram_FromBitmapFormat( Bitmap );
    if( Format == VRAM_TEXTURE_FORMAT_COUNT )
    {
        return FALSE;
    }

    bGenerateMips = bGenerateMips && ( Bitmap.GetNMips() == 0 ) && !sdlvram_IsCompressedFormat( Format );

    vram_texture_desc Desc;
    Desc.Type        = VRAM_TEXTURE_TYPE_2D;
    Desc.Width       = Bitmap.GetWidth();
    Desc.Height      = Bitmap.GetHeight();
    Desc.Depth       = 1;
    Desc.LayerCount  = 1;
    Desc.MipCount    = bGenerateMips ? vram_CalcMipCount( Desc.Width, Desc.Height ) : Bitmap.GetNMips() + 1;
    Desc.Format      = Format;
    Desc.UsageFlags  = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.SampleCount = 1;
    Desc.pDebugName  = pDebugName;

    if( bGenerateMips )
    {
        Desc.UsageFlags |= VRAM_TEXTURE_USAGE_COLOR_TARGET;
    }

    vram_texture Replacement;
    if( !vram_CreateTexture( Replacement, Desc ) )
    {
        return FALSE;
    }

    if( !vram_UploadTexture( Replacement, Bitmap, bGenerateMips ) )
    {
        vram_DestroyTexture( Replacement );
        return FALSE;
    }

    sdlvram_AdoptTexture( Texture, Replacement );

    return TRUE;
}

//==============================================================================

xbool vram_CreateTextureCube( vram_texture& Texture, const xbitmap* pFaces, s32 nFaces, xbool bGenerateMips, const char* pDebugName )
{
    if( Texture.pBackend && ( sdleng_InRenderPass() || sdleng_InComputePass() ) )
    {
        return FALSE;
    }

    if( !pFaces || ( nFaces != 6 ) )
    {
        return FALSE;
    }

    vram_texture_format Format = sdlvram_FromBitmapFormat( pFaces[0] );
    if( Format == VRAM_TEXTURE_FORMAT_COUNT )
    {
        return FALSE;
    }

    bGenerateMips = bGenerateMips && ( pFaces[0].GetNMips() == 0 ) && !sdlvram_IsCompressedFormat( Format );

    for( s32 i = 1; i < nFaces; i++ )
    {
        if( ( pFaces[i].GetWidth() != pFaces[0].GetWidth() ) ||
            ( pFaces[i].GetHeight() != pFaces[0].GetHeight() ) ||
            ( sdlvram_FromBitmapFormat( pFaces[i] ) != Format ) ||
            ( pFaces[i].GetNMips() != pFaces[0].GetNMips() ) )
        {
            return FALSE;
        }
    }

    vram_texture_desc Desc;
    Desc.Type        = VRAM_TEXTURE_TYPE_CUBE;
    Desc.Width       = pFaces[0].GetWidth();
    Desc.Height      = pFaces[0].GetHeight();
    Desc.Depth       = 1;
    Desc.LayerCount  = 6;
    Desc.MipCount    = bGenerateMips ? vram_CalcMipCount( Desc.Width, Desc.Height ) : pFaces[0].GetNMips() + 1;
    Desc.Format      = Format;
    Desc.UsageFlags  = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.SampleCount = 1;
    Desc.pDebugName  = pDebugName;

    if( bGenerateMips )
    {
        Desc.UsageFlags |= VRAM_TEXTURE_USAGE_COLOR_TARGET;
    }

    vram_texture Replacement;
    if( !vram_CreateTexture( Replacement, Desc ) )
    {
        return FALSE;
    }

    if( !vram_UploadTextureCube( Replacement, pFaces, nFaces, bGenerateMips ) )
    {
        vram_DestroyTexture( Replacement );
        return FALSE;
    }

    sdlvram_AdoptTexture( Texture, Replacement );

    return TRUE;
}

//==============================================================================

void vram_DestroyTexture( vram_texture& Texture )
{
    vram_texture_backend* pBackend = Texture.pBackend;
    if( !pBackend )
    {
        return;
    }

    if( sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        ASSERTS( FALSE, "Cannot destroy a texture during an active GPU pass" );
        return;
    }

    sdleng_ResetShaderBindings();
    sdleng_UnlinkBackend( s_pTextureList, pBackend, s_TextureCount );
    sdlvram_ReleaseBackend( pBackend );
    delete pBackend;

    Texture.Desc     = vram_texture_desc();
    Texture.pBackend = NULL;
}

//==============================================================================
//  UPLOADS
//==============================================================================

xbool vram_UploadTexture( vram_texture& Texture, const vram_texture_upload_desc& Upload )
{
    vram_texture_upload_batch_item Item;
    Item.pTexture = &Texture;
    Item.Upload   = Upload;

    return sdlvram_SubmitUploadBatch( &Item, 1 );
}

//==============================================================================

xbool vram_UploadTextureBatch( const vram_texture_upload_batch_item* pItems, u32 Count )
{
    return sdlvram_SubmitUploadBatch( pItems, Count );
}

//==============================================================================

xbool vram_UploadTexture( vram_texture& Texture, const xbitmap& Bitmap, xbool bGenerateMips )
{
    if( !Texture.pBackend || ( Texture.Desc.Type != VRAM_TEXTURE_TYPE_2D ) ||
        ( sdlvram_FromBitmapFormat( Bitmap ) != Texture.Desc.Format ) ||
        ( Bitmap.GetWidth() != (s32)Texture.Desc.Width ) ||
        ( Bitmap.GetHeight() != (s32)Texture.Desc.Height ) )
    {
        return FALSE;
    }

    const u32 MipCount = Bitmap.GetNMips() + 1;
    if( ( MipCount > Texture.Desc.MipCount ) || ( MipCount > SDLENG_MAX_TEXTURE_UPLOAD_BATCH ) )
    {
        return FALSE;
    }

    vram_texture_upload_batch_item Uploads[SDLENG_MAX_TEXTURE_UPLOAD_BATCH];
    for( u32 i = 0; i < MipCount; ++i )
    {
        vram_texture_upload_desc& Upload = Uploads[i].Upload;
        Uploads[i].pTexture              = &Texture;
        Upload.Region.MipLevel           = i;
        Upload.Region.Width              = Bitmap.GetWidth( i );
        Upload.Region.Height             = Bitmap.GetHeight( i );
        Upload.Region.Depth              = 1;
        Upload.pData                     = Bitmap.GetPixelData( i );
        Upload.bGenerateMips             = bGenerateMips && ( MipCount == 1 );

        if( sdlvram_IsCompressedFormat( Texture.Desc.Format ) )
        {
            Upload.Size = sdlvram_CalcSlicePitch( Texture.Desc.Format, Upload.Region.Width, Upload.Region.Height );
        }
        else if( !sdlvram_CalcBitmapUploadLayout( Bitmap, i, Upload.RowPitch, Upload.Size ) )
        {
            return FALSE;
        }

        if( Upload.Size > (u32)Bitmap.GetMipDataSize( i ) )
        {
            return FALSE;
        }
    }

    return vram_UploadTextureBatch( Uploads, MipCount );
}

//==============================================================================

xbool vram_UploadTextureCube( vram_texture& Texture, const xbitmap* pFaces, s32 nFaces, xbool bGenerateMips )
{
    if( !Texture.pBackend || !pFaces || ( nFaces != 6 ) || ( Texture.Desc.Type != VRAM_TEXTURE_TYPE_CUBE ) )
    {
        return FALSE;
    }

    const u32 MipCount = pFaces[0].GetNMips() + 1;
    if( ( MipCount > Texture.Desc.MipCount ) || ( MipCount > SDLENG_MAX_TEXTURE_UPLOAD_BATCH / 6 ) )
    {
        return FALSE;
    }

    vram_texture_upload_batch_item Uploads[SDLENG_MAX_TEXTURE_UPLOAD_BATCH];
    for( s32 i = 0; i < nFaces; i++ )
    {
        if( ( sdlvram_FromBitmapFormat( pFaces[i] ) != Texture.Desc.Format ) ||
            ( pFaces[i].GetWidth() != (s32)Texture.Desc.Width ) ||
            ( pFaces[i].GetHeight() != (s32)Texture.Desc.Height ) ||
            ( pFaces[i].GetNMips() + 1 != (s32)MipCount ) )
        {
            return FALSE;
        }

        for( u32 j = 0; j < MipCount; ++j )
        {
            vram_texture_upload_batch_item& Item   = Uploads[i * MipCount + j];
            vram_texture_upload_desc&       Upload = Item.Upload;
            Item.pTexture                          = &Texture;
            Upload.Region.Layer                    = i;
            Upload.Region.MipLevel                 = j;
            Upload.Region.Width                    = pFaces[i].GetWidth( j );
            Upload.Region.Height                   = pFaces[i].GetHeight( j );
            Upload.Region.Depth                    = 1;
            Upload.pData                           = pFaces[i].GetPixelData( j );

            if( sdlvram_IsCompressedFormat( Texture.Desc.Format ) )
            {
                Upload.Size = sdlvram_CalcSlicePitch( Texture.Desc.Format, Upload.Region.Width, Upload.Region.Height );
            }
            else if( !sdlvram_CalcBitmapUploadLayout( pFaces[i], j, Upload.RowPitch, Upload.Size ) )
            {
                return FALSE;
            }

            if( Upload.Size > (u32)pFaces[i].GetMipDataSize( j ) )
            {
                return FALSE;
            }
        }
    }

    Uploads[0].Upload.bGenerateMips = bGenerateMips && ( MipCount == 1 );

    if( !vram_UploadTextureBatch( Uploads, 6 * MipCount ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool vram_GenerateMipmaps( vram_texture& Texture )
{
    if( !g_pSDLGPUDevice || !Texture.pBackend || !Texture.pBackend->pTexture )
    {
        return FALSE;
    }

    if( !shader_ValidateResourceAccess( Texture.pBackend->Resource, SHADER_RESOURCE_ACCESS_UPLOAD ) )
    {
        return FALSE;
    }

    if( Texture.Desc.MipCount <= 1 )
    {
        return TRUE;
    }

    if( !sdlvram_CanGenerateMipmaps( Texture.Desc ) )
    {
        x_DebugMsg( "SDLVRAM: texture was not created for GPU mip generation\n" );
        return FALSE;
    }

    if( sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        x_DebugMsg( "SDLVRAM: mip generation cannot run inside a render pass\n" );
        return FALSE;
    }

    if( !sdleng_BeginTransferCommands( "SDLVRAM" ) )
    {
        return FALSE;
    }

    SDL_GenerateMipmapsForGPUTexture( sdleng_GetTransferCommandBuffer(),
                                      Texture.pBackend->pTexture );

    return sdleng_FlushTransferCommands();
}

//==============================================================================
//  RESOURCE ACCESS
//==============================================================================

xbool vram_IsValid( const vram_texture& Texture )
{
    return ( Texture.pBackend != NULL ) && ( Texture.pBackend->pTexture != NULL );
}

//==============================================================================

const vram_texture_desc* vram_GetDesc( const vram_texture& Texture )
{
    return vram_IsValid( Texture ) ? &Texture.Desc : NULL;
}

//==============================================================================

const shader_resource* vram_GetShaderResource( const vram_texture& Texture )
{
    if( !vram_IsValid( Texture ) )
    {
        return NULL;
    }

    return &Texture.pBackend->Resource;
}

//==============================================================================
//  UTILITY
//==============================================================================

u32 vram_CalcMipCount( u32 Width, u32 Height, u32 Depth )
{
    u32 MipCount = 1;
    u32 Extent   = sdlvram_MaxU32( Width, sdlvram_MaxU32( Height, Depth ) );

    while( Extent > 1 )
    {
        Extent >>= 1;
        MipCount++;
    }

    return MipCount;
}

//==============================================================================

u32 vram_GetNTextures( void )
{
    return s_TextureCount;
}

//==============================================================================

void vram_PrintStats( void )
{
    x_printfxy( 0, 7, "NTextures:   %d", s_TextureCount );
}

//==============================================================================

void vram_SanityCheck( void )
{
}

//==============================================================================
//  SDL BACKEND ACCESSORS
//==============================================================================

SDL_GPUTexture* sdleng_GetGPUTexture( const shader_resource* pResource )
{
    if( !pResource || !pResource->pBackend )
    {
        return NULL;
    }

    if( pResource->pBackend->Kind != SDLENG_SHADER_RESOURCE_TEXTURE )
    {
        return NULL;
    }

    return pResource->pBackend->pTexture;
}

//==============================================================================

SDL_GPUBuffer* sdleng_GetGPUBuffer( const shader_resource* pResource )
{
    if( !pResource || !pResource->pBackend )
    {
        return NULL;
    }

    if( pResource->pBackend->Kind != SDLENG_SHADER_RESOURCE_BUFFER )
    {
        return NULL;
    }

    return pResource->pBackend->pBuffer;
}

//==============================================================================
#endif  // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
