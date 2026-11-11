//==============================================================================
//  
//  e_VRAM.hpp
//
//  Texture resource API for explicit PSO render backends.
//
//==============================================================================

#ifndef E_VRAM_HPP
#define E_VRAM_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_bitmap.hpp"

struct shader_resource;

//==============================================================================
//  TYPES
//==============================================================================

enum vram_texture_type
{
    VRAM_TEXTURE_TYPE_2D = 0,
    VRAM_TEXTURE_TYPE_2D_ARRAY,
    VRAM_TEXTURE_TYPE_3D,
    VRAM_TEXTURE_TYPE_CUBE,
    VRAM_TEXTURE_TYPE_CUBE_ARRAY
};

//------------------------------------------------------------------------------

enum vram_texture_format
{
    VRAM_TEXTURE_FORMAT_RGBA8 = 0,
    VRAM_TEXTURE_FORMAT_BGRA8,
    VRAM_TEXTURE_FORMAT_RGB10A2,
    VRAM_TEXTURE_FORMAT_RGBA16F,
    VRAM_TEXTURE_FORMAT_RGBA32F,
    VRAM_TEXTURE_FORMAT_R8,
    VRAM_TEXTURE_FORMAT_RG16F,
    VRAM_TEXTURE_FORMAT_R32F,
    VRAM_TEXTURE_FORMAT_B5G6R5,
    VRAM_TEXTURE_FORMAT_B5G5R5A1,
    VRAM_TEXTURE_FORMAT_B4G4R4A4,
    VRAM_TEXTURE_FORMAT_BC1_RGBA,
    VRAM_TEXTURE_FORMAT_BC2_RGBA,
    VRAM_TEXTURE_FORMAT_BC3_RGBA,
    VRAM_TEXTURE_FORMAT_DEPTH_STENCIL,
    VRAM_TEXTURE_FORMAT_DEPTH32F,
    VRAM_TEXTURE_FORMAT_ASTC_4X4,
    VRAM_TEXTURE_FORMAT_ASTC_5X4,
    VRAM_TEXTURE_FORMAT_ASTC_5X5,
    VRAM_TEXTURE_FORMAT_ASTC_6X5,
    VRAM_TEXTURE_FORMAT_ASTC_6X6,
    VRAM_TEXTURE_FORMAT_ASTC_8X5,
    VRAM_TEXTURE_FORMAT_ASTC_8X6,
    VRAM_TEXTURE_FORMAT_ASTC_8X8,
    VRAM_TEXTURE_FORMAT_ASTC_10X5,
    VRAM_TEXTURE_FORMAT_ASTC_10X6,
    VRAM_TEXTURE_FORMAT_ASTC_10X8,
    VRAM_TEXTURE_FORMAT_ASTC_10X10,
    VRAM_TEXTURE_FORMAT_ASTC_12X10,
    VRAM_TEXTURE_FORMAT_ASTC_12X12,
    VRAM_TEXTURE_FORMAT_ASTC_4X4_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_5X4_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_5X5_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_6X5_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_6X6_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_8X5_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_8X6_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_8X8_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_10X5_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_10X6_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_10X8_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_10X10_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_12X10_SRGB,
    VRAM_TEXTURE_FORMAT_ASTC_12X12_SRGB,
    VRAM_TEXTURE_FORMAT_COUNT
};

//------------------------------------------------------------------------------

enum vram_texture_usage_flags
{
    VRAM_TEXTURE_USAGE_SAMPLED                         = (1 << 0),
    VRAM_TEXTURE_USAGE_COLOR_TARGET                    = (1 << 1),
    VRAM_TEXTURE_USAGE_DEPTH_STENCIL_TARGET            = (1 << 2),
    VRAM_TEXTURE_USAGE_GRAPHICS_STORAGE_READ           = (1 << 3),
    VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ            = (1 << 4),
    VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE           = (1 << 5),
    VRAM_TEXTURE_USAGE_COMPUTE_STORAGE_READ_WRITE      = (1 << 6)
};

//------------------------------------------------------------------------------

struct vram_texture_backend;
struct vram_texture;

//------------------------------------------------------------------------------

struct vram_texture_desc
{
    vram_texture_type   Type;
    u32                 Width;
    u32                 Height;
    u32                 Depth;
    u32                 LayerCount;
    u32                 MipCount;
    vram_texture_format Format;
    u32                 UsageFlags;
    u32                 SampleCount;
    const char*         pDebugName;

    vram_texture_desc( void ) :
        Type       ( VRAM_TEXTURE_TYPE_2D ),
        Width      ( 0 ),
        Height     ( 0 ),
        Depth      ( 1 ),
        LayerCount ( 1 ),
        MipCount   ( 1 ),
        Format     ( VRAM_TEXTURE_FORMAT_RGBA8 ),
        UsageFlags ( VRAM_TEXTURE_USAGE_SAMPLED ),
        SampleCount( 1 ),
        pDebugName ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct vram_texture_region
{
    u32 MipLevel;
    u32 Layer;
    u32 X;
    u32 Y;
    u32 Z;
    u32 Width;
    u32 Height;
    u32 Depth;

    vram_texture_region( void ) :
        MipLevel( 0 ),
        Layer   ( 0 ),
        X       ( 0 ),
        Y       ( 0 ),
        Z       ( 0 ),
        Width   ( 0 ),
        Height  ( 0 ),
        Depth   ( 1 )
    {
    }
};

//------------------------------------------------------------------------------

struct vram_texture_upload_desc
{
    vram_texture_region Region;
    const void*         pData;
    u32                 Size;
    u32                 RowPitch;
    u32                 SlicePitch;
    xbool               bCycle;
    xbool               bGenerateMips;

    vram_texture_upload_desc( void ) :
        Region       (),
        pData        ( NULL ),
        Size         ( 0 ),
        RowPitch     ( 0 ),
        SlicePitch   ( 0 ),
        bCycle       ( FALSE ),
        bGenerateMips( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct vram_texture_upload_batch_item
{
    vram_texture*           pTexture;
    vram_texture_upload_desc Upload;

    vram_texture_upload_batch_item( void ) :
        pTexture( NULL ),
        Upload  ()
    {
    }
};

//------------------------------------------------------------------------------

struct vram_texture
{
    vram_texture_desc     Desc;
    vram_texture_backend* pBackend;

    vram_texture( void ) :
        Desc    (),
        pBackend( NULL )
    {
    }

    operator xbool( void ) const { return pBackend != NULL; }
};

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void                    vram_Init               ( void );
void                    vram_Kill               ( void );

//==============================================================================
//  TEXTURE OBJECTS
//==============================================================================

xbool                   vram_CreateTexture      ( vram_texture&            Texture,
                                                  const vram_texture_desc& Desc );
xbool                   vram_CreateTexture      ( vram_texture& Texture,
                                                  const xbitmap& Bitmap,
                                                  xbool bGenerateMips = FALSE,
                                                  const char* pDebugName = NULL );
xbool                   vram_CreateTextureCube  ( vram_texture& Texture,
                                                  const xbitmap* pFaces,
                                                  s32 nFaces,
                                                  xbool bGenerateMips = FALSE,
                                                  const char* pDebugName = NULL );
void                    vram_DestroyTexture     ( vram_texture& Texture );

//==============================================================================
//  UPLOADS
//==============================================================================

xbool                   vram_UploadTexture      ( vram_texture& Texture,
                                                  const vram_texture_upload_desc& Upload );
xbool                   vram_UploadTextureBatch ( const vram_texture_upload_batch_item* pItems,
                                                  u32 Count );
xbool                   vram_UploadTexture      ( vram_texture& Texture,
                                                  const xbitmap& Bitmap,
                                                  xbool bGenerateMips = FALSE );
xbool                   vram_UploadTextureCube  ( vram_texture& Texture,
                                                  const xbitmap* pFaces,
                                                  s32 nFaces,
                                                  xbool bGenerateMips = FALSE );
xbool                   vram_GenerateMipmaps    ( vram_texture& Texture );

//==============================================================================
//  RESOURCE ACCESS
//==============================================================================

xbool                   vram_IsValid            ( const vram_texture& Texture );
const vram_texture_desc*
                        vram_GetDesc            ( const vram_texture& Texture );
const shader_resource*  vram_GetShaderResource  ( const vram_texture& Texture );

//==============================================================================
//  UTILITY
//==============================================================================

u32                     vram_CalcMipCount       ( u32 Width,
                                                  u32 Height,
                                                  u32 Depth = 1 );
u32                     vram_GetNTextures       ( void );
void                    vram_PrintStats         ( void );
void                    vram_SanityCheck        ( void );

//==============================================================================
#endif // E_VRAM_HPP
//==============================================================================
