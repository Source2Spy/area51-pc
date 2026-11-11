//=========================================================================
//
//  GeomDraw.hpp
//
//  Frame geometry submitted by the common render layer to the renderer.
//
//=========================================================================

#ifndef GEOM_DRAW_HPP
#define GEOM_DRAW_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "x_types.hpp"
#include "x_color.hpp"
#include "RenderLighting.hpp"
#include "e_RenderBuffer.hpp"

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class material;
class texture;
struct shader_resource;
struct vram_texture;
struct rigid_geom;
struct skin_geom;

//=========================================================================
//  TYPES
//=========================================================================

// Shared capacity for geometry collection and backend upload.
static s32 const RENDER_MAX_GEOM_SUBMISSIONS = 32768;

enum geom_draw_type
{
    GEOM_DRAW_RIGID = 0,
    GEOM_DRAW_SKIN
};

//-------------------------------------------------------------------------

enum geom_render_pass
{
    GEOM_PASS_FORWARD_OPAQUE = 0,
    GEOM_PASS_TRANSPARENT,
    GEOM_PASS_ADDITIVE,
    GEOM_PASS_FORCE_LAST,
    GEOM_PASS_ZPRIME,
    GEOM_PASS_FADING,
    GEOM_PASS_DISTORTION,
    GEOM_PASS_SHADOW_CAST
};

namespace render
{
class ClothTessellation;

enum RenderContributionPolicy
{
    RENDER_CONTRIBUTION_DEFAULT,
    RENDER_CONTRIBUTION_SUPPRESS_BLOOM,
};

// Derived per-instance features. These are resolved by the submission layer
// and are never part of the public render option mask.
enum geom_inst_feature_bits : u32
{
    GEOM_INST_FORCED_GLOW  = ( 1u << 0 ),
    GEOM_INST_FILTER_LIGHT = ( 1u << 1 ),
    GEOM_INST_PROJ_LIGHT   = ( 1u << 2 ),
    GEOM_INST_PROJ_SHADOW  = ( 1u << 3 ),
};

using geom_inst_features = u32;
}

//-------------------------------------------------------------------------

struct rigid_geom_draw
{
    rigid_geom*    pGeom;
    matrix4 const* pL2W;
    u32 const*     pColorInfo;
};

//-------------------------------------------------------------------------

struct skin_geom_draw
{
    skin_geom*     pGeom;
    matrix4 const* pBones;
    s32            BoneCount;
};

//-------------------------------------------------------------------------

struct dynamic_geom_vertex
{
    vector3 Position;
    vector3 Normal;
    vector2 UV;
    xcolor  Color;
};

//-------------------------------------------------------------------------

struct dynamic_geom_gpu_mesh
{
    const rbuffer* pVertices;
    const rbuffer* pIndices;
    s32            VertexCount;
    s32            IndexCount;

    dynamic_geom_gpu_mesh( void ) :
        pVertices  ( NULL ),
        pIndices   ( NULL ),
        VertexCount( 0 ),
        IndexCount ( 0 )
    {
    }
};

//-------------------------------------------------------------------------

inline xbool geom_IsGpuMeshValid( const dynamic_geom_gpu_mesh& Mesh )
{
    return Mesh.pVertices && Mesh.pIndices && *Mesh.pVertices && *Mesh.pIndices &&
           Mesh.VertexCount > 0 && Mesh.VertexCount <= 65535 && Mesh.IndexCount > 0 &&
           ( Mesh.IndexCount % 3 ) == 0 &&
           ( Mesh.pVertices->Desc.UsageFlags & RBUFFER_USAGE_VERTEX ) &&
           ( Mesh.pIndices->Desc.UsageFlags & RBUFFER_USAGE_INDEX ) &&
           Mesh.pVertices->Desc.Stride == sizeof( dynamic_geom_vertex ) &&
           static_cast<u64>( Mesh.VertexCount ) * sizeof( dynamic_geom_vertex ) <= Mesh.pVertices->Desc.Size &&
           static_cast<u64>( Mesh.IndexCount ) * sizeof( u16 ) <= Mesh.pIndices->Desc.Size;
}

//-------------------------------------------------------------------------

struct dynamic_geom_draw
{
    render::ClothTessellation* pClothTessellation;
    u32                       ClothRevision;
    const dynamic_geom_gpu_mesh*    pGpuMesh;
    dynamic_geom_vertex const* pVertices;
    u16 const*                     pIndices;
    texture const*                 pDiffuseTexture;
    xbool                          CoverageUsesTextureAlpha;
    shader_resource const*         pDamageMask;
    vram_texture*                  pDamageTexture;
    u8 const*                      pDamageUpload;
    xbool*                         pDamageUploadPending;
    s32                            DamageUploadX;
    s32                            DamageUploadY;
    s32                            DamageUploadWidth;
    s32                            DamageUploadHeight;
    s32                            VertexCount;
    s32                            IndexCount;
    bbox                           Bounds;
    xcolor                         Ambient;
    RenderLightingHandle           Lighting;
    u32                            RenderFlags;
    render::geom_inst_features InstFeatures;
    u32                            Sequence;

    dynamic_geom_draw( void );
};

// Packed dynamic geometry remains owned by the frame submission until the backend finishes the frame.
struct dynamic_geom_storage
{
    dynamic_geom_vertex const* pVertices;
    s32                        VertexCount;
    u16 const*                 pIndices;
    s32                        IndexCount;

    dynamic_geom_storage( void )
        : pVertices( NULL ), VertexCount( 0 ), pIndices( NULL ), IndexCount( 0 )
    {
    }
};

//-------------------------------------------------------------------------

struct dynamic_geom_shadow_draw
{
    render::ClothTessellation* pClothTessellation;
    u32                       ClothRevision;
    bbox                      Bounds;
    const dynamic_geom_gpu_mesh*    pGpuMesh;
    dynamic_geom_vertex const* pVertices;
    u16 const*                     pIndices;
    shader_resource const*         pDiffuse;
    xbool                          CoverageUsesTextureAlpha;
    shader_resource const*         pDamageMask;
    vram_texture*                  pDamageTexture;
    u8 const*                      pDamageUpload;
    xbool*                         pDamageUploadPending;
    s32                            DamageUploadX;
    s32                            DamageUploadY;
    s32                            DamageUploadWidth;
    s32                            DamageUploadHeight;
    s32                            VertexCount;
    s32                            IndexCount;
    u64                            ShadowSourceMask;

    dynamic_geom_shadow_draw( void );
};

// Damage textures are single-byte uploads; reject dimensions before computing their byte size.
inline xbool geom_CalculateDamageUploadSize( s32 Width, s32 Height, u32& Size )
{
    if( ( Width <= 0 ) || ( Height <= 0 ) || ( Width > ( 0x7fffffff / Height ) ) )
    {
        Size = 0;
        return FALSE;
    }

    Size = static_cast<u32>( Width ) * static_cast<u32>( Height );
    return TRUE;
}

//-------------------------------------------------------------------------

union geom_draw_data
{
    rigid_geom_draw Rigid;
    skin_geom_draw  Skin;
};

//-------------------------------------------------------------------------

struct geom_draw_item
{
    geom_render_pass Pass;
    geom_draw_type   Type;
    xhandle              hRenderGeom;
    material const*      pMaterial;
    u32                  MaterialOrder;
    s32                  iSurface;
    u32                  RenderFlags;
    render::geom_inst_features InstFeatures;
    RenderLightingHandle Lighting;
    geom_draw_data   Data;
    radian3              DistortionNormalRot;
    f32                  SortDepth;
    u32                  Sequence;
    s32                  ShadowSourceIndex;
    u8                   UOffset;
    u8                   VOffset;
    u8                   Alpha;
    u8                   MaterialOverride;
    render::RenderContributionPolicy ContributionPolicy;

    geom_draw_item( void );
};

//=========================================================================

inline geom_draw_item::geom_draw_item( void )
    : Pass( GEOM_PASS_FORWARD_OPAQUE ), Type( GEOM_DRAW_RIGID ), hRenderGeom(), pMaterial( NULL ), MaterialOrder( 0 ),
      iSurface( -1 ), RenderFlags( 0 ), InstFeatures( 0 ), Lighting(), Data(), DistortionNormalRot(), SortDepth( 0.0f ), Sequence( 0 ),
      ShadowSourceIndex( -1 ), UOffset( 0 ), VOffset( 0 ), Alpha( 255 ), MaterialOverride( FALSE ),
      ContributionPolicy( render::RENDER_CONTRIBUTION_DEFAULT )
{
    hRenderGeom.Handle = HNULL;
    DistortionNormalRot.Zero();
    x_memset( &Data, 0, sizeof( Data ) );
}

//=========================================================================

inline dynamic_geom_draw::dynamic_geom_draw( void )
    : pClothTessellation( NULL ), ClothRevision( 0 ),
      pGpuMesh( NULL ), pVertices( NULL ), pIndices( NULL ), pDiffuseTexture( NULL ), CoverageUsesTextureAlpha( FALSE ),
      pDamageMask( NULL ), pDamageTexture( NULL ),
      pDamageUpload( NULL ), pDamageUploadPending( NULL ), DamageUploadX( 0 ), DamageUploadY( 0 ),
      DamageUploadWidth( 0 ), DamageUploadHeight( 0 ), VertexCount( 0 ), IndexCount( 0 ), Bounds(),
      Ambient( XCOLOR_BLACK ), Lighting(),
      RenderFlags( 0 ), InstFeatures( 0 ), Sequence( 0 )
{
}

//=========================================================================

inline dynamic_geom_shadow_draw::dynamic_geom_shadow_draw( void )
    : pClothTessellation( NULL ), ClothRevision( 0 ), Bounds(),
      pGpuMesh( NULL ), pVertices( NULL ), pIndices( NULL ), pDiffuse( NULL ), CoverageUsesTextureAlpha( FALSE ),
      pDamageMask( NULL ), pDamageTexture( NULL ),
      pDamageUpload( NULL ), pDamageUploadPending( NULL ), DamageUploadX( 0 ), DamageUploadY( 0 ),
      DamageUploadWidth( 0 ), DamageUploadHeight( 0 ), VertexCount( 0 ), IndexCount( 0 ), ShadowSourceMask( 0 )
{
}

#endif // GEOM_DRAW_HPP
//=========================================================================
