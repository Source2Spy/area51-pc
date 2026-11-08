//=========================================================================
//
//  ShadowRenderer.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ShadowRenderer.hpp"
#include "a51_shader_catalog.hpp"
#include "../../ShaderContract.hpp"
#include "../../ShadowCasterCulling.hpp"
#include "../../Pipeline/RenderPipeline.hpp"

#include "../../Backend/Resource/GeometryRepository.hpp"
#include "../../Backend/Frame/FrameCompiler.hpp"
#include "../../GeomDraw.hpp"
#include "../../RigidGeom.hpp"
#include "../../SkinGeom.hpp"
#include "../../Backend/Storage/SkinnedGeometryStorage.hpp"
#include "../../Backend/Storage/StaticGeometryStorage.hpp"


namespace render
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{
enum
{
    SHADOW_CASTER_NONE    = -1,
    SHADOW_CASTER_RIGID   = 0,
    SHADOW_CASTER_SKIN    = 1,
    SHADOW_CASTER_DYNAMIC = 8,
};

//-------------------------------------------------------------------------

enum class ShadowPipeline : u64
{
    RigidOpaqueSingleSided = 0,
    SkinOpaqueSingleSided,
    RigidAlphaSingleSided,
    SkinAlphaSingleSided,
    RigidOpaqueTwoSided,
    SkinOpaqueTwoSided,
    RigidAlphaTwoSided,
    SkinAlphaTwoSided,
    DynamicAlphaTwoSided,
    TileClear,
};

//-------------------------------------------------------------------------

enum class ShadowEVSMPipeline : u64
{
    Convert = 0,
    BlurHorizontal,
    BlurVertical,
};

static const f32 kShadowDepthBias               = 2.0f;
static const f32 kShadowDepthBiasClamp          = 0.0f;
static const f32 kShadowEVSMPositiveExponent    = 40.0f;
static const f32 kShadowEVSMNegativeExponent    = 5.0f;
static const f32 kShadowEVSMLightBleedReduction = 0.1f;
static const s32 kMaxShadowAtlasDimension       = 16384;
static const u64 kShadowAtlasFeatureKey         = 0x5348444f5741544cull;

//=========================================================================

static u64 GetShadowAtlasIdentity( const RenderFrameContext& Frame )
{
    const RenderView* pView = Frame.GetView( 0 );
    return RenderHistoryKeyForIdentity( kShadowAtlasFeatureKey, Frame.Desc.ScopeId, pView ? pView->ViewId : 0 );
}

//=========================================================================

static xbool FlushDamageTextureUploads( FrameGraphBuilder& Graph, vram_texture_upload_batch_item* pUploads,
                                        xbool**                         ppPendingFlags,
                                        u32&                            UploadCount,
                                        u64&                            UploadedBytes )
{
    if( UploadCount == 0 )
    {
        return TRUE;
    }

    if( !Graph.QueueUploadBatch( pUploads, UploadCount, ppPendingFlags ) )
    {
        return FALSE;
    }

    for( u32 i = 0; i < UploadCount; ++i )
    {
        UploadedBytes += pUploads[i].Upload.Size;
    }

    UploadCount = 0;

    return TRUE;
}

//-------------------------------------------------------------------------

struct PreparedShadowDraw
{
    geom_draw_type Type;
    xhandle        hMesh;
    s32            RegistrationHandle;
    s32            SurfaceIndex;
    const matrix4* pBones;
    s32            BoneCount;
    const texture* pAlphaTexture;
    s32            SourceIndex;
    u32            RigidInstanceIndex;
    u32            SkinPaletteBase;
    s32            SkinSectionCount;
    u8             UOffset;
    u8             VOffset;
    xbool          isPunchThru;
    xbool          isTwoSided;

    PreparedShadowDraw( void )
        : Type( GEOM_DRAW_RIGID ),
          hMesh(),
          RegistrationHandle( HNULL ),
          SurfaceIndex( -1 ),
          pBones( NULL ),
          BoneCount( 0 ),
          pAlphaTexture( NULL ),
          SourceIndex( -1 ),
          RigidInstanceIndex( 0 ),
          SkinPaletteBase( 0 ),
          SkinSectionCount( 0 ),
          UOffset( 0 ),
          VOffset( 0 ),
          isPunchThru( FALSE ),
          isTwoSided( FALSE )
    {
        hMesh.Handle = HNULL;
    }
};

//-------------------------------------------------------------------------

struct SkinPaletteRef
{
    const matrix4* pBones;
    s32            BoneCount;
    s32            MeshHandle;
    u32            DrawIndex;
};

//-------------------------------------------------------------------------

struct ShadowDrawPacket
{
    geom_draw_type Type;
    xhandle        hMesh;
    const texture* pAlphaTexture;
    s32            SourceIndex;
    u32            FirstDraw;
    u32            FirstInstance;
    u32            InstanceCount;
    u32            FirstSkinDrawInstance;
    s32            SkinSectionCount;
    u8             UOffset;
    u8             VOffset;
    xbool          isPunchThru;
    xbool          isTwoSided;

    //=========================================================================

    ShadowDrawPacket( void )
        : Type( GEOM_DRAW_RIGID ),
          hMesh(),
          pAlphaTexture( NULL ),
          SourceIndex( -1 ),
          FirstDraw( 0 ),
          FirstInstance( 0 ),
          InstanceCount( 0 ),
          FirstSkinDrawInstance( 0 ),
          SkinSectionCount( 0 ),
          UOffset( 0 ),
          VOffset( 0 ),
          isPunchThru( FALSE ),
          isTwoSided( FALSE )
    {
        hMesh.Handle = HNULL;
    }
};

//-------------------------------------------------------------------------

struct ShadowIndirectItem
{
    u32                               PacketIndex;
    s32                               SourceIndex;
    u32                               FirstInstance;
    xbool                             isTwoSided;
    StaticGeometryStorage::mesh_range Range;
};

//-------------------------------------------------------------------------

struct ShadowIndirectRun
{
    u32   FirstItem;
    u32   FirstCommand;
    u32   CommandCount;
    s32   SourceIndex;
    s32   VertexPool;
    s32   IndexPool;
    xbool isTwoSided;

    //=========================================================================

    ShadowIndirectRun( void )
        : FirstItem( 0 ),
          FirstCommand( 0 ),
          CommandCount( 0 ),
          SourceIndex( -1 ),
          VertexPool( -1 ),
          IndexPool( -1 ),
          isTwoSided( FALSE )
    {
    }
};

//-------------------------------------------------------------------------

struct PreparedDynamicShadowDraw
{
    const dynamic_geom_gpu_mesh* pGpuMesh;
    const shader_resource* pDiffuse;
    const shader_resource* pDamageMask;
    xbool                  CoverageUsesTextureAlpha;
    u32                    FirstVertex;
    u32                    FirstIndex;
    u32                    VertexCount;
    u32                    IndexCount;
    u64                    ShadowSourceMask;

    //=========================================================================

    PreparedDynamicShadowDraw( void )
        : pGpuMesh( NULL ),
          pDiffuse( NULL ),
          pDamageMask( NULL ),
          CoverageUsesTextureAlpha( FALSE ),
          FirstVertex( 0 ),
          FirstIndex( 0 ),
          VertexCount( 0 ),
          IndexCount( 0 ),
          ShadowSourceMask( 0 )
    {
    }
};

} // namespace

//-------------------------------------------------------------------------

struct ShadowCasterSubmission
{

    //=========================================================================

    ShadowCasterSubmission( void )
        : Ready( FALSE )
    {
        const s32 MaxDraws = RENDER_MAX_GEOM_SUBMISSIONS;
        Packets.SetCapacity( MaxDraws );
        RigidIndirectItems.SetCapacity( MaxDraws );
        RigidIndirectRuns.SetCapacity( MaxDraws );
        RigidIndirectCommands.SetCapacity( MaxDraws );
        SkinIndirectItems.SetCapacity( MaxDraws );
        SkinIndirectRuns.SetCapacity( MaxDraws );
        SkinIndirectCommands.SetCapacity( MaxDraws );
        DynamicDraws.SetCapacity( MaxDraws );
        PreparedDraws.SetCapacity( MaxDraws );
        SkinPaletteRefs.SetCapacity( MaxDraws );
        RigidInstances.SetCapacity( MaxDraws );
        SkinBoneBases.SetCapacity( MaxDraws );
        DamageTextureUploads.SetCapacity( MaxDraws );
        DamageUploadPendingFlags.SetCapacity( MaxDraws );
    }

    //=========================================================================

    void Reset( void )
    {
        Ready = FALSE;
        Packets.SetCount( 0 );
        RigidIndirectItems.SetCount( 0 );
        RigidIndirectRuns.SetCount( 0 );
        RigidIndirectCommands.SetCount( 0 );
        SkinIndirectItems.SetCount( 0 );
        SkinIndirectRuns.SetCount( 0 );
        SkinIndirectCommands.SetCount( 0 );
        DynamicDraws.SetCount( 0 );
        PreparedDraws.SetCount( 0 );
        SkinPaletteRefs.SetCount( 0 );
        RigidInstances.SetCount( 0 );
        SkinBoneMatrices.SetCount( 0 );
        SkinUploadMatrices.SetCount( 0 );
        SkinPaletteRemap.SetCount( 0 );
        DamageUploadBytes.SetCount( 0 );
        DamageUploadOffsets.SetCount( 0 );
        SkinBoneBases.SetCount( 0 );
        DynamicVertices.SetCount( 0 );
        DynamicIndices.SetCount( 0 );
        DamageTextureUploads.SetCount( 0 );
        DamageUploadPendingFlags.SetCount( 0 );
    }

    xbool                                  Ready;
    xarray<ShadowDrawPacket>               Packets;
    xarray<ShadowIndirectItem>             RigidIndirectItems;
    xarray<ShadowIndirectRun>              RigidIndirectRuns;
    xarray<rdraw_indexed_indirect_command> RigidIndirectCommands;
    xarray<ShadowIndirectItem>             SkinIndirectItems;
    xarray<ShadowIndirectRun>              SkinIndirectRuns;
    xarray<rdraw_indexed_indirect_command> SkinIndirectCommands;
    xarray<PreparedDynamicShadowDraw>      DynamicDraws;
    xarray<PreparedShadowDraw>             PreparedDraws;
    xarray<SkinPaletteRef>                 SkinPaletteRefs;
    xarray<matrix4>                        RigidInstances;
    xarray<matrix4>                        SkinBoneMatrices;
    xarray<matrix4>                        SkinUploadMatrices;
    xarray<u32>                            SkinPaletteRemap;
    xarray<u8>                             DamageUploadBytes;
    xarray<u32>                            DamageUploadOffsets;
    xarray<u32>                            SkinBoneBases;
    xarray<dynamic_geom_vertex>            DynamicVertices;
    xarray<u16>                            DynamicIndices;
    xarray<vram_texture_upload_batch_item> DamageTextureUploads;
    xarray<xbool*>                         DamageUploadPendingFlags;
};

namespace
{

//=========================================================================

static s32 GetShadowCasterVariant( s32 CasterShader, xbool isAlphaTest, xbool isTwoSided )
{
    ASSERT( ( CasterShader == SHADOW_CASTER_RIGID ) || ( CasterShader == SHADOW_CASTER_SKIN ) );
    return CasterShader + ( isAlphaTest ? 2 : 0 ) + ( isTwoSided ? 4 : 0 );
}

//=========================================================================

static void ConfigureShadowRasterizer( rstate_raster_desc& Rasterizer, rstate_raster_preset Preset )
{
    Rasterizer                      = rstate_GetRasterDesc( Preset );
    Rasterizer.DepthBias            = kShadowDepthBias;
    Rasterizer.DepthBiasClamp       = kShadowDepthBiasClamp;
    Rasterizer.SlopeScaledDepthBias = 0.0f;
    Rasterizer.bDepthBiasEnable     = TRUE;
}

//=========================================================================

static s32 CompareShadowIndirectItems( const void* pA, const void* pB )
{
    const ShadowIndirectItem& A = *static_cast<const ShadowIndirectItem*>( pA );
    const ShadowIndirectItem& B = *static_cast<const ShadowIndirectItem*>( pB );

    if( A.SourceIndex < B.SourceIndex )
    {
        return -1;
    }

    if( A.SourceIndex > B.SourceIndex )
    {
        return 1;
    }

    if( A.isTwoSided != B.isTwoSided )
    {
        return A.isTwoSided ? 1 : -1;
    }

    if( A.Range.VertexPool < B.Range.VertexPool )
    {
        return -1;
    }

    if( A.Range.VertexPool > B.Range.VertexPool )
    {
        return 1;
    }

    if( A.Range.IndexPool < B.Range.IndexPool )
    {
        return -1;
    }

    if( A.Range.IndexPool > B.Range.IndexPool )
    {
        return 1;
    }

    if( A.PacketIndex < B.PacketIndex )
    {
        return -1;
    }

    if( A.PacketIndex > B.PacketIndex )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

static s32 CompareSkinPaletteRefs( const void* pA, const void* pB )
{
    const SkinPaletteRef& A = *static_cast<const SkinPaletteRef*>( pA );
    const SkinPaletteRef& B = *static_cast<const SkinPaletteRef*>( pB );

    if( A.MeshHandle < B.MeshHandle )
    {
        return -1;
    }

    if( A.MeshHandle > B.MeshHandle )
    {
        return 1;
    }

    const uaddr BonesA = reinterpret_cast<uaddr>( A.pBones );
    const uaddr BonesB = reinterpret_cast<uaddr>( B.pBones );

    if( BonesA < BonesB )
    {
        return -1;
    }

    if( BonesA > BonesB )
    {
        return 1;
    }

    if( A.BoneCount < B.BoneCount )
    {
        return -1;
    }

    if( A.BoneCount > B.BoneCount )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

// Indices address filtered preparedDraws, not the input list; reject stale palette references.
static xbool IsPreparedDrawIndexValid( const xarray<PreparedShadowDraw>& PreparedDraws, u32 DrawIndex )
{
    return DrawIndex < static_cast<u32>( PreparedDraws.GetCount() );
}

//=========================================================================

static u32 GrowCasterBufferCapacity( u32 CurrentCapacity, u32 RequiredCount )
{
    u32 Capacity = MAX( CurrentCapacity, 1u );

    while( Capacity < RequiredCount )
    {
        if( Capacity > ( 0xffffffffu / 2u ) )
        {
            Capacity = RequiredCount;
            break;
        }

        Capacity *= 2u;
    }

    return Capacity;
}

//=========================================================================

static xbool ShadowStagingFits( u64 Count, u32 ElementSize )
{
    return ( Count > 0 ) && ( Count <= 0x7fffffffu ) && ( ElementSize > 0 ) &&
           ( Count * static_cast<u64>( ElementSize ) <= FrameStorage::MAX_STREAM_BYTES );
}

//=========================================================================

template <typename T>
static xbool ShadowReserveArrayCapacity( xarray<T>& Storage, u64 RequiredCount, const char* pName )
{
    if( RequiredCount == 0 )
    {
        return TRUE;
    }

    if( !ShadowStagingFits( RequiredCount, sizeof( T ) ) )
    {
        x_DebugMsg( "ShadowRenderer: %s exceeds bounded staging storage\n", pName );
        return FALSE;
    }

    const s32 Required = static_cast<s32>( RequiredCount );

    if( Storage.GetCapacity() < Required )
    {
        const s32 Maximum  = static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( T ) );
        s32       Capacity = MAX( Storage.GetCapacity(), 256 );

        while( Capacity < Required )
        {
            if( Capacity > ( 0x7fffffff / 2 ) )
            {
                Capacity = Required;
                break;
            }

            Capacity *= 2;
        }

        Capacity = MIN( Capacity, Maximum );

        if( Capacity < Required )
        {
            return FALSE;
        }

        Storage.SetCapacity( Capacity );
    }

    return TRUE;
}

//=========================================================================

static s32 ShadowNextStagingCapacity( s32 Current, s32 Required, s32 Maximum )
{
    s32 Capacity = MAX( Current, 1 );

    while( Capacity < Required )
    {
        if( Capacity > ( 0x7fffffff / 2 ) )
        {
            return Required;
        }

        Capacity *= 2;
    }

    return MIN( Capacity, Maximum );
}

//=========================================================================

static xbool EnsureCasterBuffer( rbuffer&    Buffer,
                                 u32&        Capacity,
                                 u32         RequiredCount,
                                 u32         ElementStride,
                                 const char* pDebugName,
                                 u32         UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ )
{
    if( ( ElementStride == 0 ) || ( RequiredCount > ( 0xffffffffu / ElementStride ) ) )
    {
        return FALSE;
    }

    const u32 MaxCapacity = FrameStorage::MAX_STREAM_BYTES / ElementStride;

    if( ( MaxCapacity == 0 ) || ( RequiredCount > MaxCapacity ) )
    {
        return FALSE;
    }

    u32 DesiredCapacity = GrowCasterBufferCapacity( Capacity, MAX( RequiredCount, 1u ) );
    DesiredCapacity     = MIN( DesiredCapacity, MaxCapacity );

    if( Buffer && ( Capacity >= DesiredCapacity ) )
    {
        return TRUE;
    }

    rbuffer_desc Desc;
    Desc.Size       = DesiredCapacity * ElementStride;
    Desc.Stride     = ElementStride;
    Desc.UsageFlags = UsageFlags;
    Desc.pDebugName = pDebugName;

    if( !rbuffer_Create( Buffer, Desc ) )
    {
        return FALSE;
    }

    Capacity = DesiredCapacity;

    return TRUE;
}

//=========================================================================

static xbool EnsureInstanceIndexBuffer( rbuffer&     Buffer,
                                        u32&         Capacity,
                                        u32          RequiredCount,
                                        const char*  pDebugName,
                                        xarray<u32>& Scratch )
{
    const u32 MaxCapacity = FrameStorage::MAX_STREAM_BYTES / sizeof( u32 );

    if( ( MaxCapacity == 0 ) || ( RequiredCount > MaxCapacity ) )
    {
        return FALSE;
    }

    u32 DesiredCapacity = GrowCasterBufferCapacity( Capacity, MAX( RequiredCount, 1u ) );
    DesiredCapacity     = MIN( DesiredCapacity, MaxCapacity );

    if( DesiredCapacity > ( 0xffffffffu / sizeof( u32 ) ) )
    {
        return FALSE;
    }

    if( Buffer && ( Capacity >= DesiredCapacity ) )
    {
        return TRUE;
    }

    Scratch.SetCount( DesiredCapacity );

    for( u32 i = 0; i < DesiredCapacity; ++i )
    {
        Scratch[i] = i;
    }

    rbuffer_desc Desc;
    Desc.Size       = DesiredCapacity * sizeof( u32 );
    Desc.Stride     = sizeof( u32 );
    Desc.UsageFlags = RBUFFER_USAGE_VERTEX;
    Desc.pDebugName = pDebugName;

    if( !rbuffer_Create( Buffer, Desc ) )
    {
        return FALSE;
    }

    Capacity = DesiredCapacity;

    return TRUE;
}

//=========================================================================

static const texture* GetShadowAlphaTexture( const material* pMaterial, xbool& BPunchThru )
{
    BPunchThru = FALSE;

    if( !pMaterial )
    {
        return NULL;
    }

    BPunchThru = pMaterial->IsPunchThrough();

    if( !pMaterial->UsesShadowAlpha() )
    {
        return NULL;
    }

    const texture* pDiffuse = pMaterial->m_DiffuseMap.GetPointer();

    if( !pDiffuse || !pDiffuse->GetShaderResource() )
    {
        return NULL;
    }

    return pDiffuse;
}

} // namespace

//=========================================================================
//  RENDERER LIFETIME
//=========================================================================

ShadowRenderer::ShadowRenderer( void )
    : m_isInitialized( FALSE ),
      m_CurrentSource( -1 ),
      m_CurrentCasterVariant( SHADOW_CASTER_NONE ),
      m_ShadowAtlasSize( 0 ),
      m_ShadowMomentAtlasSize( 0 ),
      m_LastCasterStats(),
      m_pSubmission( NULL ),
      m_ScopeId( -1 ),
      m_SubmissionScopeId( -1 ),
      m_ShadowAtlas(),
      m_ShadowMomentAtlas(),
      m_ShadowMomentTempAtlas(),
      m_TileCache(),
      m_isTileUpdatesExecuted( FALSE ),
      m_RigidVertexShader(),
      m_SkinVertexShader(),
      m_DynamicVertexShader(),
      m_DynamicPixelShader(),
      m_OpaqueDepthPixelShader(),
      m_AlphaDepthPixelShader(),
      m_EvsmVertexShader(),
      m_EvsmConvertPixelShader(),
      m_EvsmBlurHorizontalPixelShader(),
      m_EvsmBlurVerticalPixelShader(),
      m_CasterPipelines(),
      m_EvsmPipelines(),
      m_DiffuseSampler(),
      m_EvsmPointSampler(),
      m_EvsmLinearSampler(),
      m_ShadowSingleSidedRasterizerDesc(),
      m_ShadowTwoSidedRasterizerDesc(),
      m_RigidInstanceBuffer(),
      m_RigidInstanceIndexBuffer(),
      m_RigidIndirectBuffer(),
      m_SkinBoneBuffer(),
      m_SkinBoneBaseBuffer(),
      m_SkinIndirectBuffer(),
      m_DynamicVertexBuffer(),
      m_DynamicIndexBuffer(),
      m_InstanceIndexScratch(),
      m_RigidInstanceCapacity( 0 ),
      m_RigidInstanceIndexCapacity( 0 ),
      m_RigidIndirectCapacity( 0 ),
      m_SkinBoneCapacity( 0 ),
      m_SkinBoneBaseCapacity( 0 ),
      m_SkinIndirectCapacity( 0 ),
      m_DynamicVertexCapacity( 0 ),
      m_DynamicIndexCapacity( 0 ),
      m_RigidCastConstantsSlot( 0 ),
      m_RigidInstanceSlot( 0 ),
      m_SkinCastConstantsSlot( 0 ),
      m_SkinBoneSlot( 0 ),
      m_DynamicCastConstantsSlot( 0 ),
      m_DynamicDiffuseTextureSlot( 0 ),
      m_DynamicDamageTextureSlot( 0 ),
      m_AlphaConstantsSlot( 0 ),
      m_AlphaTextureSlot( 0 ),
      m_EvsmConvertConstantsSlot( 0 ),
      m_EvsmConvertTextureSlot( 0 ),
      m_EvsmBlurHorizontalConstantsSlot( 0 ),
      m_EvsmBlurHorizontalTextureSlot( 0 ),
      m_EvsmBlurVerticalConstantsSlot( 0 ),
      m_EvsmBlurVerticalTextureSlot( 0 )
{
}

//=========================================================================

ShadowRenderer::~ShadowRenderer( void )
{
    Kill();
}

//=========================================================================

void ShadowRenderer::Init( void )
{
    if( m_isInitialized )
    {
        return;
    }

    if( !m_pSubmission )
    {
        m_pSubmission = new ShadowCasterSubmission;

        if( !m_pSubmission )
        {
            Kill();
            RENDER_THROW( "ShadowRenderer", "failed to allocate shadow submission storage" );
            return;
        }
    }

    auto LoadCaster = [&]( VertexContract Vertex, xbool Masked, shader& VertexShader, shader& PixelShader ) -> xbool
    {
        const ShaderProgramKey      Key     = ResolveShadowProgram( Vertex, Masked );
        const A51RecipeProgramDesc* pRecipe = A51FindRecipeProgram( Key.Hash() );

        if( !pRecipe )
        {
            RENDER_THROW( "ShaderCatalog", "shadow program %016llx is absent from catalog",
                          ( unsigned long long )Key.Hash() );
            return FALSE;
        }

        if( !shader_LoadComboStage( VertexShader, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                    SHADER_STAGE_VERTEX ) )
        {
            return FALSE;
        }

        if( !PixelShader && !shader_LoadComboStage( PixelShader, pRecipe->PackageFile, SHADER_COMBO_KEYED, pRecipe->Key,
                                                    SHADER_STAGE_PIXEL ) )
        {
            return FALSE;
        }

        return PixelShader.ProgramId == pRecipe->PixelId;
    };

    if( !LoadCaster( VertexContract::Rigid, FALSE, m_RigidVertexShader, m_OpaqueDepthPixelShader ) ||
        !LoadCaster( VertexContract::Skin, FALSE, m_SkinVertexShader, m_OpaqueDepthPixelShader ) ||
        !LoadCaster( VertexContract::Rigid, TRUE, m_RigidMaskedVertexShader, m_AlphaDepthPixelShader ) ||
        !LoadCaster( VertexContract::Skin, TRUE, m_SkinMaskedVertexShader, m_AlphaDepthPixelShader ) )
    {
        Kill();
        return;
    }

    shader_LoadComboStage( m_DynamicVertexShader, A51FixedPrograms[A51FixedProgram_shadow_cast_dynamic].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_shadow_cast_dynamic].Key,
                           SHADER_STAGE_VERTEX );
    shader_LoadComboStage( m_DynamicPixelShader, A51FixedPrograms[A51FixedProgram_shadow_cast_dynamic].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_shadow_cast_dynamic].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage( m_EvsmVertexShader, A51FixedPrograms[A51FixedProgram_shadow_evsm_convert].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_shadow_evsm_convert].Key,
                           SHADER_STAGE_VERTEX );
    shader_LoadComboStage( m_EvsmConvertPixelShader, A51FixedPrograms[A51FixedProgram_shadow_evsm_convert].PackageFile,
                           SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_shadow_evsm_convert].Key,
                           SHADER_STAGE_PIXEL );
    shader_LoadComboStage(
        m_EvsmBlurHorizontalPixelShader, A51FixedPrograms[A51FixedProgram_shadow_evsm_blur_horizontal].PackageFile,
        SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_shadow_evsm_blur_horizontal].Key, SHADER_STAGE_PIXEL );
    shader_LoadComboStage( m_EvsmBlurVerticalPixelShader,
                           A51FixedPrograms[A51FixedProgram_shadow_evsm_blur_vertical].PackageFile, SHADER_COMBO_FIXED,
                           A51FixedPrograms[A51FixedProgram_shadow_evsm_blur_vertical].Key, SHADER_STAGE_PIXEL );
    rstate_CreateSampler( m_DiffuseSampler, RSTATE_SAMPLER_PRESET_MATERIAL_WRAP, "ShadowDiffuse" );
    rstate_CreateSampler( m_EvsmPointSampler, RSTATE_SAMPLER_PRESET_POINT_CLAMP, "ShadowEVSMDepth" );
    rstate_CreateSampler( m_EvsmLinearSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "ShadowEVSMMoments" );

    ConfigureShadowRasterizer( m_ShadowSingleSidedRasterizerDesc, RSTATE_RASTER_PRESET_SOLID );
    ConfigureShadowRasterizer( m_ShadowTwoSidedRasterizerDesc, RSTATE_RASTER_PRESET_SOLID_NO_CULL );

    const xbool BHasRigidCaster   = m_RigidVertexShader && m_RigidMaskedVertexShader;
    const xbool BHasSkinCaster    = m_SkinVertexShader && m_SkinMaskedVertexShader;
    const xbool BHasDynamicCaster = m_DynamicVertexShader && m_DynamicPixelShader;

    if( !BHasRigidCaster )
    {
        shader_Destroy( m_RigidVertexShader );
        shader_Destroy( m_RigidMaskedVertexShader );
    }

    if( !BHasSkinCaster )
    {
        shader_Destroy( m_SkinVertexShader );
        shader_Destroy( m_SkinMaskedVertexShader );
    }

    if( !BHasDynamicCaster )
    {
        shader_Destroy( m_DynamicVertexShader );
        shader_Destroy( m_DynamicPixelShader );
    }

    if( !m_OpaqueDepthPixelShader || !m_AlphaDepthPixelShader || !m_EvsmVertexShader || !m_EvsmConvertPixelShader ||
        !m_EvsmBlurHorizontalPixelShader || !m_EvsmBlurVerticalPixelShader || !m_DiffuseSampler ||
        !m_EvsmPointSampler || !m_EvsmLinearSampler || !( BHasRigidCaster || BHasSkinCaster || BHasDynamicCaster ) )
    {
        Kill();
        RENDER_THROW( "ShadowRenderer", "initialization failed: required shadow shader or sampler is unavailable" );
        return;
    }

    if( ( BHasRigidCaster &&
          ( !shader_GetResourceSlot( m_RigidVertexShader, A51Resource_cbShadowCast, m_RigidCastConstantsSlot ) ||
            !shader_GetResourceSlot( m_RigidVertexShader, A51Resource_ShadowRigidWorld, m_RigidInstanceSlot ) ) ) ||
        ( BHasSkinCaster &&
          ( !shader_GetResourceSlot( m_SkinVertexShader, A51Resource_cbShadowCast, m_SkinCastConstantsSlot ) ||
            !shader_GetResourceSlot( m_SkinVertexShader, A51Resource_ShadowSkinBones, m_SkinBoneSlot ) ) ) ||
        ( BHasDynamicCaster &&
          ( !shader_GetResourceSlot( m_DynamicVertexShader, A51Resource_cbShadowCast, m_DynamicCastConstantsSlot ) ||
            !shader_GetResourceSlot( m_DynamicPixelShader, A51Resource_txDiffuse, m_DynamicDiffuseTextureSlot ) ||
            !shader_GetResourceSlot( m_DynamicPixelShader, A51Resource_txDamage, m_DynamicDamageTextureSlot ) ) ) ||
        !shader_GetResourceSlot( m_AlphaDepthPixelShader, A51Resource_cbShadowAlpha, m_AlphaConstantsSlot ) ||
        !shader_GetResourceSlot( m_AlphaDepthPixelShader, A51Resource_txDiffuse, m_AlphaTextureSlot ) ||
        !shader_GetResourceSlot( m_EvsmConvertPixelShader, A51Resource_cbShadowFilter, m_EvsmConvertConstantsSlot ) ||
        !shader_GetResourceSlot( m_EvsmConvertPixelShader, A51Resource_txShadowDepth, m_EvsmConvertTextureSlot ) ||
        !shader_GetResourceSlot( m_EvsmBlurHorizontalPixelShader, A51Resource_cbShadowFilter,
                                 m_EvsmBlurHorizontalConstantsSlot ) ||
        !shader_GetResourceSlot( m_EvsmBlurHorizontalPixelShader, A51Resource_txShadowMoments,
                                 m_EvsmBlurHorizontalTextureSlot ) ||
        !shader_GetResourceSlot( m_EvsmBlurVerticalPixelShader, A51Resource_cbShadowFilter,
                                 m_EvsmBlurVerticalConstantsSlot ) ||
        !shader_GetResourceSlot( m_EvsmBlurVerticalPixelShader, A51Resource_txShadowMoments,
                                 m_EvsmBlurVerticalTextureSlot ) )
    {
        Kill();
        RENDER_THROW( "ShadowRenderer", "failed to resolve shadow shader bindings" );
        return;
    }

    if( !CreateClearGeometry() )
    {
        Kill();
        RENDER_THROW( "ShadowRenderer", "failed to initialize tile clear geometry" );
        return;
    }

    m_isInitialized = TRUE;
}

//=========================================================================
//  PIPELINE STATE
//=========================================================================

xbool ShadowRenderer::QueueGeometryPipeline( xbool Skin, xbool Alpha, xbool TwoSided )
{
    return RequestPipelines( Skin ? SHADOW_CASTER_SKIN : SHADOW_CASTER_RIGID, Alpha, TwoSided, TRUE );
}

//=========================================================================

xbool ShadowRenderer::QueueDynamicPipeline( void )
{
    return RequestPipelines( SHADOW_CASTER_DYNAMIC, TRUE, TRUE, TRUE );
}

//=========================================================================

xbool ShadowRenderer::QueueFilterPipelines( void )
{
    return RequestPipelines( SHADOW_CASTER_NONE, FALSE, FALSE, TRUE );
}

//=========================================================================

static render_pipeline* StoreShadowPipeline( GraphicsPipelineCache&      Cache,
                                             render_pipeline*&           Handle,
                                             const render_pipeline_desc& Desc,
                                             xbool                       IsPrewarm )
{
    Handle = IsPrewarm ? Cache.QueuePrewarm( Desc ) : Cache.GetOrCreate( Desc );
    return Handle;
}

//=========================================================================

static xbool StoreShadowCasterPipelines( GraphicsPipelineCache&      Cache,
                                         render_pipeline*&           PointHandle,
                                         render_pipeline*&           SpotHandle,
                                         const render_pipeline_desc& Desc,
                                         xbool                       IsPrewarm )
{
    render_pipeline_desc PointDesc = Desc;
    PointDesc.Raster.FrontFace = Desc.Raster.FrontFace == RSTATE_FRONT_FACE_CW
                                    ? RSTATE_FRONT_FACE_CCW : RSTATE_FRONT_FACE_CW;

    if( !StoreShadowPipeline( Cache, PointHandle, PointDesc, IsPrewarm ) )
    {
        return FALSE;
    }

    return StoreShadowPipeline( Cache, SpotHandle, Desc, IsPrewarm ) != NULL;
}

//=========================================================================

xbool ShadowRenderer::RequestPipelines( s32 Caster, xbool Alpha, xbool isTwoSided, xbool IsPrewarm )
{
    if( !m_isInitialized )
    {
        return FALSE;
    }

    static shader_vertex_element RigidLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, 0 ),
                                                   shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT3, 12 ),
                                                   shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, 24 ),
                                                   shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ) };

    static shader_vertex_element SkinLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT4, 0 ),
                                                  shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT4, 16 ),
                                                  shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT4, 32 ),
                                                  shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ) };

    static shader_vertex_buffer_desc RigidVertexBuffers[2];
    RigidVertexBuffers[0].Slot   = 0;
    RigidVertexBuffers[0].Stride = sizeof( rigid_geom::vertex );

    RigidVertexBuffers[1].Slot         = 1;
    RigidVertexBuffers[1].Stride       = sizeof( u32 );
    RigidVertexBuffers[1].bPerInstance = TRUE;

    static shader_vertex_buffer_desc SkinVertexBuffers[2];
    SkinVertexBuffers[0].Slot   = 0;
    SkinVertexBuffers[0].Stride = sizeof( SkinGpuVertex );

    SkinVertexBuffers[1].Slot         = 1;
    SkinVertexBuffers[1].Stride       = sizeof( u32 );
    SkinVertexBuffers[1].bPerInstance = TRUE;

    static shader_vertex_element DynamicLayout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( dynamic_geom_vertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( dynamic_geom_vertex, UV ) )
    };

    static shader_vertex_buffer_desc DynamicVertexBuffer;
    DynamicVertexBuffer.Slot   = 0;
    DynamicVertexBuffer.Stride = sizeof( dynamic_geom_vertex );

    if( Caster == SHADOW_CASTER_RIGID )
    {
        render_pipeline_desc Desc;
        Desc.Shader.pVertexShader     = &m_RigidVertexShader;
        Desc.Shader.pPixelShader      = &m_OpaqueDepthPixelShader;
        Desc.Shader.pVertexBuffers    = RigidVertexBuffers;
        Desc.Shader.VertexBufferCount = ARRAYSIZE( RigidVertexBuffers );
        Desc.Shader.pInputElements    = RigidLayout;
        Desc.Shader.InputElementCount = ARRAYSIZE( RigidLayout );
        Desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
        Desc.Depth                    = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_NORMAL );
        Desc.ColorCount               = 0;
        Desc.DepthFormat              = RTARGET_FORMAT_DEPTH32F;

        {
            Desc.Raster              = isTwoSided ? m_ShadowTwoSidedRasterizerDesc : m_ShadowSingleSidedRasterizerDesc;
            Desc.Shader.pPixelShader = &m_OpaqueDepthPixelShader;
            Desc.pDebugName = isTwoSided ? "ShadowRigidOpaqueTwoSidedCaster" : "ShadowRigidOpaqueSingleSidedCaster";

            const ShadowPipeline OpaquePipeline =
                isTwoSided ? ShadowPipeline::RigidOpaqueTwoSided : ShadowPipeline::RigidOpaqueSingleSided;

            if( !Alpha &&
                !( StoreShadowCasterPipelines( m_CasterPipelines,
                                               m_pCasterHandles[static_cast<u32>( OpaquePipeline )],
                                               m_pSpotCasterHandles[static_cast<u32>( OpaquePipeline )],
                                               Desc, IsPrewarm ) ) )
            {
                RENDER_THROW( "ShadowRenderer", "failed to prewarm rigid opaque %s-sided caster pipeline",
                              isTwoSided ? "two" : "single" );
                return FALSE;
            }

            Desc.Shader.pVertexShader =
                Caster == SHADOW_CASTER_RIGID ? &m_RigidMaskedVertexShader : &m_SkinMaskedVertexShader;
            Desc.Shader.pPixelShader = &m_AlphaDepthPixelShader;
            Desc.pDebugName = isTwoSided ? "ShadowRigidAlphaTwoSidedCaster" : "ShadowRigidAlphaSingleSidedCaster";

            const ShadowPipeline AlphaPipeline =
                isTwoSided ? ShadowPipeline::RigidAlphaTwoSided : ShadowPipeline::RigidAlphaSingleSided;

            if( Alpha && !( StoreShadowCasterPipelines( m_CasterPipelines,
                                                       m_pCasterHandles[static_cast<u32>( AlphaPipeline )],
                                                       m_pSpotCasterHandles[static_cast<u32>( AlphaPipeline )],
                                                       Desc, IsPrewarm ) ) )
            {
                RENDER_THROW( "ShadowRenderer", "failed to prewarm rigid alpha %s-sided caster pipeline",
                              isTwoSided ? "two" : "single" );
                return FALSE;
            }
        }
    }

    if( Caster == SHADOW_CASTER_SKIN )
    {
        render_pipeline_desc Desc;
        Desc.Shader.pVertexShader     = &m_SkinVertexShader;
        Desc.Shader.pPixelShader      = &m_OpaqueDepthPixelShader;
        Desc.Shader.pVertexBuffers    = SkinVertexBuffers;
        Desc.Shader.VertexBufferCount = ARRAYSIZE( SkinVertexBuffers );
        Desc.Shader.pInputElements    = SkinLayout;
        Desc.Shader.InputElementCount = ARRAYSIZE( SkinLayout );
        Desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
        Desc.Depth                    = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_NORMAL );
        Desc.ColorCount               = 0;
        Desc.DepthFormat              = RTARGET_FORMAT_DEPTH32F;

        {
            Desc.Raster              = isTwoSided ? m_ShadowTwoSidedRasterizerDesc : m_ShadowSingleSidedRasterizerDesc;
            Desc.Shader.pPixelShader = &m_OpaqueDepthPixelShader;
            Desc.pDebugName = isTwoSided ? "ShadowSkinOpaqueTwoSidedCaster" : "ShadowSkinOpaqueSingleSidedCaster";

            const ShadowPipeline OpaquePipeline =
                isTwoSided ? ShadowPipeline::SkinOpaqueTwoSided : ShadowPipeline::SkinOpaqueSingleSided;

            if( !Alpha &&
                !( StoreShadowCasterPipelines( m_CasterPipelines,
                                               m_pCasterHandles[static_cast<u32>( OpaquePipeline )],
                                               m_pSpotCasterHandles[static_cast<u32>( OpaquePipeline )],
                                               Desc, IsPrewarm ) ) )
            {
                RENDER_THROW( "ShadowRenderer", "failed to prewarm skin opaque %s-sided caster pipeline",
                              isTwoSided ? "two" : "single" );
                return FALSE;
            }

            Desc.Shader.pVertexShader =
                Caster == SHADOW_CASTER_RIGID ? &m_RigidMaskedVertexShader : &m_SkinMaskedVertexShader;
            Desc.Shader.pPixelShader = &m_AlphaDepthPixelShader;
            Desc.pDebugName = isTwoSided ? "ShadowSkinAlphaTwoSidedCaster" : "ShadowSkinAlphaSingleSidedCaster";

            const ShadowPipeline AlphaPipeline =
                isTwoSided ? ShadowPipeline::SkinAlphaTwoSided : ShadowPipeline::SkinAlphaSingleSided;

            if( Alpha && !( StoreShadowCasterPipelines( m_CasterPipelines,
                                                       m_pCasterHandles[static_cast<u32>( AlphaPipeline )],
                                                       m_pSpotCasterHandles[static_cast<u32>( AlphaPipeline )],
                                                       Desc, IsPrewarm ) ) )
            {
                RENDER_THROW( "ShadowRenderer", "failed to prewarm skin alpha %s-sided caster pipeline",
                              isTwoSided ? "two" : "single" );
                return FALSE;
            }
        }
    }

    if( Caster == SHADOW_CASTER_DYNAMIC )
    {
        render_pipeline_desc Desc;
        Desc.Shader.pVertexShader     = &m_DynamicVertexShader;
        Desc.Shader.pPixelShader      = &m_DynamicPixelShader;
        Desc.Shader.pVertexBuffers    = &DynamicVertexBuffer;
        Desc.Shader.VertexBufferCount = 1;
        Desc.Shader.pInputElements    = DynamicLayout;
        Desc.Shader.InputElementCount = ARRAYSIZE( DynamicLayout );
        Desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
        Desc.Depth                    = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_NORMAL );
        Desc.Raster                   = m_ShadowTwoSidedRasterizerDesc;
        Desc.ColorCount               = 0;
        Desc.DepthFormat              = RTARGET_FORMAT_DEPTH32F;
        Desc.pDebugName               = "ShadowDynamicAlphaTwoSidedCaster";

        if( !( StoreShadowCasterPipelines( m_CasterPipelines,
                                           m_pCasterHandles[static_cast<u32>( ShadowPipeline::DynamicAlphaTwoSided )],
                                           m_pSpotCasterHandles[static_cast<u32>( ShadowPipeline::DynamicAlphaTwoSided )],
                                           Desc, IsPrewarm ) ) )
        {
            RENDER_THROW( "ShadowRenderer", "failed to prewarm dynamic alpha two-sided caster pipeline" );
            return FALSE;
        }
    }

    if( Caster == SHADOW_CASTER_NONE )
    {
        shader_vertex_element     ClearLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, 0 ),
                                                    shader_vertex_element( 3, 0, SHADER_VERTEX_FORMAT_UINT1, 12 ) };
        shader_vertex_buffer_desc ClearBuffer;
        ClearBuffer.Slot   = 0;
        ClearBuffer.Stride = 16;

        render_pipeline_desc Clear;
        Clear.Shader.pVertexShader     = &m_RigidVertexShader;
        Clear.Shader.pPixelShader      = &m_OpaqueDepthPixelShader;
        Clear.Shader.pVertexBuffers    = &ClearBuffer;
        Clear.Shader.VertexBufferCount = 1;
        Clear.Shader.pInputElements    = ClearLayout;
        Clear.Shader.InputElementCount = ARRAYSIZE( ClearLayout );
        Clear.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
        Clear.Raster                   = rstate_GetRasterDesc( RSTATE_RASTER_PRESET_SOLID_NO_CULL );
        Clear.Depth                    = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_WRITE_ALWAYS );
        Clear.ColorCount               = 0;
        Clear.DepthFormat              = RTARGET_FORMAT_DEPTH32F;
        Clear.pDebugName               = "ShadowTileClear";

        if( !StoreShadowPipeline( m_CasterPipelines, m_pCasterHandles[static_cast<u32>( ShadowPipeline::TileClear )],
                                  Clear, IsPrewarm ) )
        {
            return FALSE;
        }

        const shader* PixelShaders[] = {
            &m_EvsmConvertPixelShader,
            &m_EvsmBlurHorizontalPixelShader,
            &m_EvsmBlurVerticalPixelShader,
        };
        const char* DebugNames[] = {
            "ShadowEVSMConvert",
            "ShadowEVSMBlurHorizontal",
            "ShadowEVSMBlurVertical",
        };

        for( u32 i = 0; i < ARRAYSIZE( PixelShaders ); ++i )
        {
            render_pipeline_desc Desc;
            Desc.Shader.pVertexShader   = &m_EvsmVertexShader;
            Desc.Shader.pPixelShader    = PixelShaders[i];
            Desc.Shader.Topology        = SHADER_TOPOLOGY_TRIANGLE_LIST;
            Desc.Raster                 = rstate_GetRasterDesc( RSTATE_RASTER_PRESET_SOLID_NO_CULL );
            Desc.Depth                  = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE );
            Desc.ColorTargets[0].Format = RTARGET_FORMAT_RGBA32F;
            Desc.ColorTargets[0].Blend  = rstate_GetBlendDesc( RSTATE_BLEND_PRESET_NONE );
            Desc.ColorCount             = 1;
            Desc.DepthFormat            = RTARGET_FORMAT_COUNT;
            Desc.pDebugName             = DebugNames[i];

            const ShadowEVSMPipeline Pipeline = static_cast<ShadowEVSMPipeline>( i );

            if( !( StoreShadowPipeline( m_EvsmPipelines, m_pEvsmHandles[static_cast<u32>( Pipeline )], Desc,
                                        IsPrewarm ) ) )
            {
                RENDER_THROW( "ShadowRenderer", "failed to prewarm %s pipeline", DebugNames[i] );
                return FALSE;
            }
        }
    }

    return TRUE;
}

//=========================================================================

void ShadowRenderer::ReleaseLevelPipelines( void )
{
    m_TileCache.Clear();
    m_isTileUpdatesExecuted = FALSE;
    m_CasterPipelines.Reset();
    m_EvsmPipelines.Reset();
    x_memset( m_pCasterHandles, 0, sizeof( m_pCasterHandles ) );
    x_memset( m_pSpotCasterHandles, 0, sizeof( m_pSpotCasterHandles ) );
    x_memset( m_pEvsmHandles, 0, sizeof( m_pEvsmHandles ) );
}

//=========================================================================

void ShadowRenderer::Kill( void )
{
    AbortFrame();
    m_TileCache.Clear();

    delete m_pSubmission;
    m_pSubmission = NULL;

    m_CasterPipelines.Reset();
    m_EvsmPipelines.Reset();
    x_memset( m_pCasterHandles, 0, sizeof( m_pCasterHandles ) );
    x_memset( m_pSpotCasterHandles, 0, sizeof( m_pSpotCasterHandles ) );
    x_memset( m_pEvsmHandles, 0, sizeof( m_pEvsmHandles ) );

    rbuffer_Destroy( m_ClearVertices );
    rbuffer_Destroy( m_ClearWorld );
    rbuffer_Destroy( m_RigidInstanceBuffer );
    rbuffer_Destroy( m_RigidInstanceIndexBuffer );
    rbuffer_Destroy( m_RigidIndirectBuffer );
    rbuffer_Destroy( m_SkinBoneBuffer );
    rbuffer_Destroy( m_SkinBoneBaseBuffer );
    rbuffer_Destroy( m_SkinIndirectBuffer );
    rbuffer_Destroy( m_DynamicVertexBuffer );
    rbuffer_Destroy( m_DynamicIndexBuffer );
    m_InstanceIndexScratch.Clear();

    rstate_DestroySampler( m_DiffuseSampler );
    rstate_DestroySampler( m_EvsmPointSampler );
    rstate_DestroySampler( m_EvsmLinearSampler );
    shader_Destroy( m_EvsmBlurVerticalPixelShader );
    shader_Destroy( m_EvsmBlurHorizontalPixelShader );
    shader_Destroy( m_EvsmConvertPixelShader );
    shader_Destroy( m_EvsmVertexShader );
    shader_Destroy( m_AlphaDepthPixelShader );
    shader_Destroy( m_OpaqueDepthPixelShader );
    shader_Destroy( m_RigidVertexShader );
    shader_Destroy( m_RigidMaskedVertexShader );
    shader_Destroy( m_SkinVertexShader );
    shader_Destroy( m_SkinMaskedVertexShader );
    shader_Destroy( m_DynamicVertexShader );
    shader_Destroy( m_DynamicPixelShader );

    m_CurrentSource              = -1;
    m_CurrentCasterVariant       = SHADOW_CASTER_NONE;
    m_ShadowAtlasSize            = 0;
    m_ShadowMomentAtlasSize      = 0;
    m_RigidInstanceCapacity      = 0;
    m_RigidInstanceIndexCapacity = 0;
    m_RigidIndirectCapacity      = 0;
    m_SkinBoneCapacity           = 0;
    m_SkinBoneBaseCapacity       = 0;
    m_SkinIndirectCapacity       = 0;
    m_DynamicVertexCapacity      = 0;
    m_DynamicIndexCapacity       = 0;
    m_isInitialized              = FALSE;
}

//=========================================================================
//  FRAME LIFECYCLE
//=========================================================================

void ShadowRenderer::BeginFrame( s32 ScopeId )
{
    m_isTileUpdatesExecuted = FALSE;

    // Casters are collected before the graph begins; retain only submissions for this scope.
    const xbool HasSubmissionForScope = m_pSubmission && ( m_SubmissionScopeId == ScopeId );

    if( !HasSubmissionForScope )
    {
        if( m_pSubmission )
        {
            m_pSubmission->Reset();
        }

        m_SubmissionScopeId = -1;
        m_LastCasterStats   = CasterStats();
    }

    m_ScopeId               = ScopeId;
    m_ShadowAtlas           = RenderResourceHandle();
    m_ShadowMomentAtlas     = RenderResourceHandle();
    m_ShadowMomentTempAtlas = RenderResourceHandle();
    m_CurrentSource         = -1;
    m_CurrentCasterVariant  = SHADOW_CASTER_NONE;
}

//=========================================================================

void ShadowRenderer::AbortFrame( void )
{
    m_TileCache.Invalidate();
    m_isTileUpdatesExecuted = FALSE;

    if( m_pSubmission )
    {
        m_pSubmission->Reset();
    }

    m_SubmissionScopeId     = -1;
    m_ShadowAtlas           = RenderResourceHandle();
    m_ShadowMomentAtlas     = RenderResourceHandle();
    m_ShadowMomentTempAtlas = RenderResourceHandle();
    m_CurrentSource         = -1;
    m_CurrentCasterVariant  = SHADOW_CASTER_NONE;
}

//=========================================================================

void ShadowRenderer::EndFrame( void )
{
    if( m_isTileUpdatesExecuted )
    {
        m_TileCache.Commit();
    }

    // Prevent reuse if a later frame has the same ScopeId but no new collection.
    if( m_pSubmission )
    {
        m_pSubmission->Reset();
    }

    m_SubmissionScopeId     = -1;
    m_ScopeId               = -1;
    m_ShadowAtlas           = RenderResourceHandle();
    m_ShadowMomentAtlas     = RenderResourceHandle();
    m_ShadowMomentTempAtlas = RenderResourceHandle();
    m_CurrentSource         = -1;
    m_CurrentCasterVariant  = SHADOW_CASTER_NONE;
}

//=========================================================================
//  SOURCE MANAGEMENT
//=========================================================================

xbool ShadowRenderer::CreateClearGeometry( void )
{
    struct ClearVertex
    {
        f32 X, Y, Z;
        u32 Instance;
    };

    const ClearVertex Vertices[] = { { -1, -1, 1, 0 }, { 3, -1, 1, 0 }, { -1, 3, 1, 0 } };
    matrix4           World;
    World.Identity();

    rbuffer_desc VertexDesc;
    VertexDesc.Size       = sizeof( Vertices );
    VertexDesc.Stride     = sizeof( ClearVertex );
    VertexDesc.UsageFlags = RBUFFER_USAGE_VERTEX;
    VertexDesc.pDebugName = "ShadowTileClearVertices";

    rbuffer_desc WorldDesc;
    WorldDesc.Size       = sizeof( World );
    WorldDesc.Stride     = sizeof( World );
    WorldDesc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    WorldDesc.pDebugName = "ShadowTileClearWorld";

    if( !rbuffer_Create( m_ClearVertices, VertexDesc, Vertices ) || !rbuffer_Create( m_ClearWorld, WorldDesc, &World ) )
    {
        rbuffer_Destroy( m_ClearVertices );
        rbuffer_Destroy( m_ClearWorld );
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool ShadowRenderer::CreateAtlasResources( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    ( void )Frame;

    s32 ShadowAtlasSize = GetRenderPipeline().GetShadowMapAtlas().GetAtlasSize();

    if( ShadowAtlasSize <= 0 )
    {
        ShadowAtlasSize = MAX_SHADOW_ATLAS_SIZE;
    }

    ASSERT( ShadowAtlasSize > 0 );
    ASSERT( ( ShadowAtlasSize & ( ShadowAtlasSize - 1 ) ) == 0 );
    ASSERT( ShadowAtlasSize <= kMaxShadowAtlasDimension );
    if( ShadowAtlasSize <= 0 )
    {
        RENDER_THROW( "ShadowRenderer", "invalid shadow atlas size %d", ShadowAtlasSize );
        return FALSE;
    }

    if( ( ShadowAtlasSize & ( ShadowAtlasSize - 1 ) ) != 0 )
    {
        RENDER_THROW( "ShadowRenderer", "shadow atlas size %d is not a power of two", ShadowAtlasSize );
        return FALSE;
    }

    if( ShadowAtlasSize > kMaxShadowAtlasDimension )
    {
        RENDER_THROW( "ShadowRenderer", "shadow atlas size %d exceeds max %d", ShadowAtlasSize,
                      kMaxShadowAtlasDimension );
        return FALSE;
    }

    const xbool UseEVSM = GetRenderPipeline().GetShadowMapAtlas().GetShadowFilterType() == ShadowFilterType::Evsm;
    m_ShadowAtlasSize   = ShadowAtlasSize;
    m_ShadowMomentAtlasSize =
        UseEVSM ? MIN( MAX( ShadowAtlasSize / 2, 1 ), static_cast<s32>( MAX_SHADOW_MOMENT_ATLAS_SIZE ) ) : 0;

    RenderResourceDesc Depth;
    Depth.Extent   = RenderExtent( m_ShadowAtlasSize, m_ShadowAtlasSize );
    Depth.Format   = RTARGET_FORMAT_DEPTH32F;
    Depth.Usage    = RENDER_RESOURCE_USAGE_DEPTH | RENDER_RESOURCE_USAGE_SAMPLED;
    Depth.Lifetime = RENDER_RESOURCE_PERSISTENT;

    const u64 ScopeKey  = GetShadowAtlasIdentity( Frame );
    Depth.PersistentKey = ScopeKey;

    if( !Frame.pResources->HasPersistentResource( ScopeKey ) || m_HistoryResetToken != Frame.Desc.TemporalResetToken )
    {
        m_TileCache.Invalidate();
    }

    m_HistoryResetToken    = Frame.Desc.TemporalResetToken;
    Depth.PreserveContents = TRUE;
    Depth.pDebugName       = "ShadowDepthAtlas";
    m_ShadowAtlas          = Graph.CreateResource( Depth );

    if( !m_ShadowAtlas.IsValid() )
    {
        return FALSE;
    }

    if( UseEVSM )
    {
        RenderResourceDesc Moments;
        Moments.Extent           = RenderExtent( m_ShadowMomentAtlasSize, m_ShadowMomentAtlasSize );
        Moments.Format           = RTARGET_FORMAT_RGBA32F;
        Moments.Usage            = RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_SAMPLED;
        Moments.Lifetime         = RENDER_RESOURCE_PERSISTENT;
        Moments.PersistentKey    = ScopeKey ^ 0x534844574d4f4d31ull;
        Moments.PreserveContents = TRUE;
        Moments.pDebugName       = "ShadowEVSMMomentAtlas";
        m_ShadowMomentAtlas      = Graph.CreateResource( Moments );
        Moments.Lifetime         = RENDER_RESOURCE_TRANSIENT;
        Moments.PersistentKey    = 0;
        Moments.PreserveContents = FALSE;
        Moments.pDebugName       = "ShadowEVSMMomentTempAtlas";
        m_ShadowMomentTempAtlas  = Graph.CreateResource( Moments );

        if( !m_ShadowMomentAtlas.IsValid() || !m_ShadowMomentTempAtlas.IsValid() )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void ShadowRenderer::PrepareTileUpdates( const RenderFrameContext& Frame )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ResolveTileUpdates" );

    const ShadowMapAtlas& Atlas = GetRenderPipeline().GetShadowMapAtlas();
    const s32             Count = Atlas.GetSourceCount();
    m_isTileUpdatesExecuted     = FALSE;
    m_TileCache.Begin( GetShadowAtlasIdentity( Frame ), Frame.pResources->GetResetEpoch(), m_ShadowAtlasSize,
                       m_ShadowMomentAtlasSize, Count );

    for( s32 i = 0; i < Count; ++i )
    {
        const ShadowMapAtlas::ShadowSource& Source = Atlas.GetSource( i );
        // Compare ownership and depth/conversion inputs; camera motion alone is irrelevant.
        m_TileCache.Append( i, Frame.Desc.ScopeId );
        m_TileCache.Append( i, GetRenderPipeline().IsTessellationEnabled() );
        m_TileCache.Append( i, Frame.GetView( 0 )->ViewId );
        m_TileCache.Append( i, Source.WorldToClip );
        m_TileCache.Append( i, Source.AtlasX );
        m_TileCache.Append( i, Source.AtlasY );
        m_TileCache.Append( i, Source.AtlasWidth );
        m_TileCache.Append( i, Source.AtlasHeight );
        m_TileCache.Append( i, Source.AtlasTileX );
        m_TileCache.Append( i, Source.AtlasTileY );
        m_TileCache.Append( i, Source.AtlasTileWidth );
        m_TileCache.Append( i, Source.AtlasTileHeight );
        m_TileCache.Append( i, Source.Type );
        m_TileCache.Append( i, static_cast<s32>( Atlas.GetShadowFilterType() ) );
        m_TileCache.Append( i, Source.NearZ );
        m_TileCache.Append( i, Source.FarZ );
        m_TileCache.Append( i, Source.LightPosRadius );
        m_TileCache.Append( i, Source.FaceLightData.GetX() );
    }

    u64 MutableCoverage = 0;

    for( s32 i = 0; i < m_pSubmission->PreparedDraws.GetCount(); ++i )
    {
        if( m_pSubmission->PreparedDraws[i].pAlphaTexture )
        {
            MutableCoverage |= u64( 1 ) << m_pSubmission->PreparedDraws[i].SourceIndex;
        }
    }

    // Cloth damage and streamed coverage can change without a resource-handle change.
    for( s32 i = 0; i < m_pSubmission->DynamicDraws.GetCount(); ++i )
    {
        MutableCoverage |= m_pSubmission->DynamicDraws[i].ShadowSourceMask;
    }

    for( s32 i = 0; i < m_pSubmission->PreparedDraws.GetCount(); ++i )
    {
        const PreparedShadowDraw& Draw = m_pSubmission->PreparedDraws[i];
        const s32                 Tile = Draw.SourceIndex;

        if( MutableCoverage & ( u64( 1 ) << Tile ) )
        {
            continue;
        }

        m_TileCache.Append( Tile, static_cast<u32>( Draw.Type ) );
        m_TileCache.Append( Tile, Draw.hMesh.Handle );
        m_TileCache.Append( Tile, Draw.RegistrationHandle );
        m_TileCache.Append( Tile, Draw.SurfaceIndex );
        m_TileCache.Append( Tile, Draw.SkinSectionCount );
        m_TileCache.Append( Tile, static_cast<u32>( Draw.isTwoSided ) );

        if( Draw.Type == GEOM_DRAW_RIGID )
        {
            m_TileCache.Append( Tile, m_pSubmission->RigidInstances[Draw.RigidInstanceIndex] );
        }
        else
        {
            m_TileCache.Append( Tile, &m_pSubmission->SkinBoneMatrices[Draw.SkinPaletteBase],
                                Draw.SkinSectionCount * SkinnedGeometryStorage::MAX_BONE_PALETTE * sizeof( matrix4 ) );
        }
    }

    for( s32 i = 0; i < Count; ++i )
    {
        m_TileCache.Resolve( i, ( MutableCoverage & ( u64( 1 ) << i ) ) != 0 );

        if( m_TileCache.IsDirty( i ) )
        {
            ++m_LastCasterStats.UpdatedTileCount;
        }
        else
        {
            ++m_LastCasterStats.ReusedTileCount;
        }
    }
}

//=========================================================================

xbool ShadowRenderer::ClearUpdatedTiles( void )
{
    if( !RequestPipelines( SHADOW_CASTER_NONE, FALSE, FALSE, FALSE ) ||
        !render_BindPipeline( *m_pCasterHandles[static_cast<u32>( ShadowPipeline::TileClear )] ) ||
        !rbuffer_BindVertex( m_ClearVertices, 0, 0 ) ||
        !shader_BindStorageBuffer( shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_RigidInstanceSlot,
                                                                  rbuffer_GetResource( m_ClearWorld ) ) ) )
    {
        return FALSE;
    }

    cb_shadow_cast Constants;
    x_memset( &Constants, 0, sizeof( Constants ) );
    Constants.ShadowViewProjection.Identity();

    if( !shader_PushUniformData( SHADER_STAGE_VERTEX, m_RigidCastConstantsSlot, &Constants, sizeof( Constants ) ) )
    {
        return FALSE;
    }

    const ShadowMapAtlas& Atlas = GetRenderPipeline().GetShadowMapAtlas();

    for( s32 i = 0; i < Atlas.GetSourceCount(); ++i )
    {
        if( !m_TileCache.IsDirty( i ) )
        {
            continue;
        }

        const ShadowMapAtlas::ShadowSource& Source = Atlas.GetSource( i );
        rdraw_viewport                      Viewport;
        Viewport.TopLeftX = static_cast<f32>( Source.AtlasTileX );
        Viewport.TopLeftY = static_cast<f32>( Source.AtlasTileY );
        Viewport.Width    = static_cast<f32>( Source.AtlasTileWidth );
        Viewport.Height   = static_cast<f32>( Source.AtlasTileHeight );
        Viewport.MinDepth = 0;
        Viewport.MaxDepth = 1;

        if( !rdraw_SetViewport( Viewport ) || !rdraw_Draw( 3, 0 ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================
//  GRAPH DECLARATION
//=========================================================================

xbool ShadowRenderer::DeclareGraph( FrameGraphBuilder& Graph, const RenderFrameContext& Frame )
{
    if( !GetRenderPipeline().GetShadowMapAtlas().HasActiveSources() ||
        ( GetRenderPipeline().GetShadowMapAtlas().GetAtlasSourceCount() == 0 ) )
    {
        return TRUE;
    }

    if( !m_isInitialized || !m_pSubmission || !m_pSubmission->Ready || ( m_SubmissionScopeId != Frame.Desc.ScopeId ) ||
        !m_OpaqueDepthPixelShader || !m_AlphaDepthPixelShader || !m_DiffuseSampler ||
        !CreateAtlasResources( Graph, Frame ) )
    {
        return FALSE;
    }

    PrepareTileUpdates( Frame );

    if( !m_TileCache.HasUpdates() )
    {
        return TRUE;
    }

    if( !BuildCasterSubmission( Graph ) )
    {
        return FALSE;
    }

    RenderPassDesc Caster;
    Caster.pDebugName  = "ShadowCaster";
    Caster.Depth       = m_ShadowAtlas;
    Caster.DepthLoadOp = m_TileCache.IsFullRefresh() ? RTARGET_LOAD_CLEAR : RTARGET_LOAD_LOAD;

    const RenderPassId CasterPass = Graph.AddPass( Caster, ExecuteCasterPassThunk, this );

    if( CasterPass == INVALID_RENDER_PASS )
    {
        return FALSE;
    }

    const rbuffer* Buffers[] =
    {
        &m_RigidInstanceBuffer, &m_ClearVertices, &m_ClearWorld, &m_RigidInstanceIndexBuffer,
        &m_RigidIndirectBuffer, &m_SkinBoneBuffer, &m_SkinBoneBaseBuffer, &m_SkinIndirectBuffer,
        &m_DynamicVertexBuffer, &m_DynamicIndexBuffer
    };
    for( u32 i = 0; i < ARRAYSIZE( Buffers ); ++i )
    {
        const rbuffer& Buffer = *Buffers[i];
        if( !Buffer )
        {
            continue;
        }
        u32 Access = 0;
        if( Buffer.Desc.UsageFlags & RBUFFER_USAGE_VERTEX )
        {
            Access |= RENDER_RESOURCE_ACCESS_VERTEX_READ;
        }
        if( Buffer.Desc.UsageFlags & RBUFFER_USAGE_INDEX )
        {
            Access |= RENDER_RESOURCE_ACCESS_INDEX_READ;
        }
        if( Buffer.Desc.UsageFlags & RBUFFER_USAGE_INDIRECT )
        {
            Access |= RENDER_RESOURCE_ACCESS_INDIRECT_READ;
        }
        if( Buffer.Desc.UsageFlags & RBUFFER_USAGE_GRAPHICS_STORAGE_READ )
        {
            Access |= RENDER_RESOURCE_ACCESS_STORAGE_READ;
        }
        Graph.Read( CasterPass, Buffer, Access );
    }

    GeometryRepository& Geometry = GetRenderPipeline().GetGeometryRepository();
    for( s32 i = 0; i < m_pSubmission->Packets.GetCount(); ++i )
    {
        const ShadowDrawPacket& Packet = m_pSubmission->Packets[i];
        Graph.Read( CasterPass, Packet.pAlphaTexture ? Packet.pAlphaTexture->GetShaderResource() : NULL );
        if( Packet.Type == GEOM_DRAW_RIGID )
        {
            GeometryRepository::RigidMeshView View;
            if( !Geometry.GetRigidMeshView( Packet.hMesh, View ) )
            {
                return FALSE;
            }
            Geometry.DeclareRigidResources( Graph, CasterPass, View );
        }
        else
        {
            GeometryRepository::SkinMeshView View;
            if( !Geometry.GetSkinMeshView( Packet.hMesh, View ) )
            {
                return FALSE;
            }
            Geometry.DeclareSkinResources( Graph, CasterPass, View.Geom );
        }
    }

    for( s32 i = 0; i < m_pSubmission->DynamicDraws.GetCount(); ++i )
    {
        const PreparedDynamicShadowDraw& Draw = m_pSubmission->DynamicDraws[i];
        Graph.Read( CasterPass, Draw.pDiffuse );
        Graph.Read( CasterPass, Draw.pDamageMask );
        const dynamic_geom_gpu_mesh* pMesh = Draw.pGpuMesh;
        if( pMesh )
        {
            Graph.Read( CasterPass, *pMesh->pVertices, RENDER_RESOURCE_ACCESS_VERTEX_READ );
            Graph.Read( CasterPass, *pMesh->pIndices, RENDER_RESOURCE_ACCESS_INDEX_READ );
        }
    }

    if( GetRenderPipeline().GetShadowMapAtlas().GetShadowFilterType() != ShadowFilterType::Evsm )
    {
        return TRUE;
    }

    RenderPassDesc Convert;
    Convert.pDebugName  = "ShadowAtlasConvert";
    Convert.Color       = m_ShadowMomentAtlas;
    Convert.ColorLoadOp = m_TileCache.IsFullRefresh() ? RTARGET_LOAD_CLEAR : RTARGET_LOAD_LOAD;

    const RenderPassId ConvertPass = Graph.AddPass( Convert, ExecuteEVSMConvertPassThunk, this );
    Graph.Read( ConvertPass, m_ShadowAtlas );

    // Skip redundant blur passes while the EVSM blur scale is zero.
    if( SHADOW_EVSM_BLUR_SCALE <= 0.0f )
    {
        return ConvertPass != INVALID_RENDER_PASS;
    }

    RenderPassDesc BlurHorizontal;
    BlurHorizontal.pDebugName  = "ShadowEVSMBlurHorizontal";
    BlurHorizontal.Color       = m_ShadowMomentTempAtlas;
    BlurHorizontal.ColorLoadOp = RTARGET_LOAD_DONT_CARE;

    const RenderPassId HorizontalPass = Graph.AddPass( BlurHorizontal, ExecuteEVSMBlurHorizontalPassThunk, this );
    Graph.Read( HorizontalPass, m_ShadowMomentAtlas );

    RenderPassDesc BlurVertical;
    BlurVertical.pDebugName  = "ShadowEVSMBlurVertical";
    BlurVertical.Color       = m_ShadowMomentAtlas;
    BlurVertical.ColorLoadOp = m_TileCache.IsFullRefresh() ? RTARGET_LOAD_DONT_CARE : RTARGET_LOAD_LOAD;

    const RenderPassId VerticalPass = Graph.AddPass( BlurVertical, ExecuteEVSMBlurVerticalPassThunk, this );
    Graph.Read( VerticalPass, m_ShadowMomentTempAtlas );

    return ( ConvertPass != INVALID_RENDER_PASS ) && ( HorizontalPass != INVALID_RENDER_PASS ) &&
           ( VerticalPass != INVALID_RENDER_PASS );
}

//=========================================================================

void ShadowRenderer::DeclareConsumer( FrameGraphBuilder& Graph, RenderPassId PassData ) const
{
    if( m_ShadowAtlas.IsValid() )
    {
        Graph.Read( PassData, m_ShadowAtlas );
    }

    if( m_ShadowMomentAtlas.IsValid() )
    {
        Graph.Read( PassData, m_ShadowMomentAtlas );
    }
}

//=========================================================================
//  CASTER EXECUTION
//=========================================================================

xbool ShadowRenderer::ExecuteCasterPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend )
{
    return pBackend && pBackend->ExecuteCasterPass( Context );
}

//=========================================================================
//  EVSM FILTERING
//=========================================================================

xbool ShadowRenderer::ExecuteEVSMConvertPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend )
{
    return pBackend && pBackend->ExecuteEVSMConvertPass( Context );
}

//=========================================================================

xbool ShadowRenderer::ExecuteEVSMBlurHorizontalPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend )
{
    return pBackend && pBackend->ExecuteEVSMBlurPass( Context, TRUE );
}

//=========================================================================

xbool ShadowRenderer::ExecuteEVSMBlurVerticalPassThunk( RenderPassContext& Context, ShadowRenderer* pBackend )
{
    return pBackend && pBackend->ExecuteEVSMBlurPass( Context, FALSE );
}

//=========================================================================

xbool ShadowRenderer::ExecuteEVSMConvertPass( RenderPassContext& Context )
{
    const rtarget* pDepthAtlas  = Context.Read( m_ShadowAtlas );
    const rtarget* pMomentAtlas = Context.Write( m_ShadowMomentAtlas );

    if( !pDepthAtlas || !pMomentAtlas )
    {
        return FALSE;
    }

    render_pipeline* pConvertPipeline = m_pEvsmHandles[static_cast<u32>( ShadowEVSMPipeline::Convert )];

    if( !RequestPipelines( SHADOW_CASTER_NONE, FALSE, FALSE, FALSE ) )
    {
        return FALSE;
    }

    pConvertPipeline = m_pEvsmHandles[static_cast<u32>( ShadowEVSMPipeline::Convert )];

    const shader_resource* pDepthResource = rtarget_GetShaderResource( *pDepthAtlas );

    if( !pConvertPipeline || !pDepthResource )
    {
        return FALSE;
    }

    if( !render_BindPipeline( *pConvertPipeline ) ||
        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_EvsmConvertTextureSlot, pDepthResource,
                                                     &m_EvsmPointSampler ) ) )
    {
        return FALSE;
    }

    const f32 DepthTexelSize  = 1.0f / static_cast<f32>( m_ShadowAtlasSize );
    const f32 MomentTexelSize = 1.0f / static_cast<f32>( m_ShadowMomentAtlasSize );
    const s32 nSources        = GetRenderPipeline().GetShadowMapAtlas().GetSourceCount();

    for( s32 i = 0; i < nSources; ++i )
    {
        if( !m_TileCache.IsDirty( i ) )
        {
            continue;
        }

        const ShadowMapAtlas::ShadowSource& Source  = GetRenderPipeline().GetShadowMapAtlas().GetSource( i );
        const s32                           TargetX = Source.AtlasTileX * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;
        const s32                           TargetY = Source.AtlasTileY * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;
        const s32 TargetWidth  = Source.AtlasTileWidth * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;
        const s32 TargetHeight = Source.AtlasTileHeight * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;

        if( ( TargetWidth <= 0 ) || ( TargetHeight <= 0 ) )
        {
            continue;
        }

        cb_shadow_filter Constants;
        Constants.TextureParams.Set( DepthTexelSize, DepthTexelSize, MomentTexelSize, MomentTexelSize );
        Constants.SourceClampRect.Set(
            ( static_cast<f32>( Source.AtlasX ) + 0.5f ) * DepthTexelSize,
            ( static_cast<f32>( Source.AtlasY ) + 0.5f ) * DepthTexelSize,
            ( static_cast<f32>( Source.AtlasX + Source.AtlasWidth ) - 0.5f ) * DepthTexelSize,
            ( static_cast<f32>( Source.AtlasY + Source.AtlasHeight ) - 0.5f ) * DepthTexelSize );
        Constants.FilterParams.Set( kShadowEVSMPositiveExponent, kShadowEVSMNegativeExponent, SHADOW_EVSM_BLUR_SCALE,
                                    0.0f );

        Constants.DepthParams.Set( Source.NearZ, Source.FarZ, Source.LightPosRadius.GetW(), 0.0f );

        const f32 TanHalfFov = 1.0f / Source.ProjectionScale;
        Constants.SourceProjectionParams.Set(
            ( static_cast<f32>( Source.AtlasX ) + static_cast<f32>( Source.AtlasWidth ) * 0.5f ) * DepthTexelSize,
            ( static_cast<f32>( Source.AtlasY ) + static_cast<f32>( Source.AtlasHeight ) * 0.5f ) * DepthTexelSize,
            2.0f / MAX( static_cast<f32>( Source.AtlasWidth ) * DepthTexelSize, 1.0e-8f ), TanHalfFov );

        rdraw_viewport Viewport;
        Viewport.TopLeftX = static_cast<f32>( TargetX );
        Viewport.TopLeftY = static_cast<f32>( TargetY );
        Viewport.Width    = static_cast<f32>( TargetWidth );
        Viewport.Height   = static_cast<f32>( TargetHeight );

        rdraw_scissor Scissor;
        Scissor.X      = TargetX;
        Scissor.Y      = TargetY;
        Scissor.Width  = TargetWidth;
        Scissor.Height = TargetHeight;

        if( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_EvsmConvertConstantsSlot, &Constants,
                                     sizeof( Constants ) ) ||
            !rdraw_SetViewport( Viewport ) || !rdraw_SetScissor( Scissor ) || !rdraw_Draw( 3 ) )
        {
            return FALSE;
        }
    }

    if( SHADOW_EVSM_BLUR_SCALE <= 0.0f )
    {
        m_isTileUpdatesExecuted = TRUE;
    }

    return TRUE;
}

//=========================================================================

xbool ShadowRenderer::ExecuteEVSMBlurPass( RenderPassContext& Context, xbool Horizontal )
{
    const RenderResourceHandle Source       = Horizontal ? m_ShadowMomentAtlas : m_ShadowMomentTempAtlas;
    const RenderResourceHandle Destination  = Horizontal ? m_ShadowMomentTempAtlas : m_ShadowMomentAtlas;
    const rtarget*             pSource      = Context.Read( Source );
    const rtarget*             pDestination = Context.Write( Destination );
    const ShadowEVSMPipeline   Pipeline =
        Horizontal ? ShadowEVSMPipeline::BlurHorizontal : ShadowEVSMPipeline::BlurVertical;
    render_pipeline* pPipeline = m_pEvsmHandles[static_cast<u32>( Pipeline )];

    if( !RequestPipelines( SHADOW_CASTER_NONE, FALSE, FALSE, FALSE ) )
    {
        return FALSE;
    }

    pPipeline = m_pEvsmHandles[static_cast<u32>( Pipeline )];

    const shader_resource* pSourceResource = pSource ? rtarget_GetShaderResource( *pSource ) : NULL;
    const u32 ConstantsSlot = Horizontal ? m_EvsmBlurHorizontalConstantsSlot : m_EvsmBlurVerticalConstantsSlot;
    const u32 TextureSlot   = Horizontal ? m_EvsmBlurHorizontalTextureSlot : m_EvsmBlurVerticalTextureSlot;

    if( !pPipeline || !pSourceResource || !pDestination )
    {
        return FALSE;
    }

    if( !render_BindPipeline( *pPipeline ) ||
        !shader_BindSampler(
            shader_sampler_binding( SHADER_STAGE_PIXEL, TextureSlot, pSourceResource, &m_EvsmLinearSampler ) ) )
    {
        return FALSE;
    }

    const f32 MomentTexelSize = 1.0f / static_cast<f32>( m_ShadowMomentAtlasSize );
    const s32 nSources        = GetRenderPipeline().GetShadowMapAtlas().GetSourceCount();

    for( s32 i = 0; i < nSources; ++i )
    {
        if( !m_TileCache.IsDirty( i ) )
        {
            continue;
        }

        const ShadowMapAtlas::ShadowSource& Source  = GetRenderPipeline().GetShadowMapAtlas().GetSource( i );
        const s32                           TargetX = Source.AtlasTileX * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;
        const s32                           TargetY = Source.AtlasTileY * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;
        const s32 TargetWidth  = Source.AtlasTileWidth * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;
        const s32 TargetHeight = Source.AtlasTileHeight * m_ShadowMomentAtlasSize / m_ShadowAtlasSize;

        if( ( TargetWidth <= 0 ) || ( TargetHeight <= 0 ) )
        {
            continue;
        }

        cb_shadow_filter Constants;
        Constants.TextureParams.Set( MomentTexelSize, MomentTexelSize, MomentTexelSize, MomentTexelSize );
        Constants.SourceClampRect.Set( ( static_cast<f32>( TargetX ) + 0.5f ) * MomentTexelSize,
                                       ( static_cast<f32>( TargetY ) + 0.5f ) * MomentTexelSize,
                                       ( static_cast<f32>( TargetX + TargetWidth ) - 0.5f ) * MomentTexelSize,
                                       ( static_cast<f32>( TargetY + TargetHeight ) - 0.5f ) * MomentTexelSize );
        Constants.FilterParams.Set( kShadowEVSMPositiveExponent, kShadowEVSMNegativeExponent, SHADOW_EVSM_BLUR_SCALE,
                                    0.0f );
        Constants.DepthParams.Zero();
        Constants.SourceProjectionParams.Zero();

        rdraw_viewport Viewport;
        Viewport.TopLeftX = static_cast<f32>( TargetX );
        Viewport.TopLeftY = static_cast<f32>( TargetY );
        Viewport.Width    = static_cast<f32>( TargetWidth );
        Viewport.Height   = static_cast<f32>( TargetHeight );
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;

        rdraw_scissor Scissor;
        Scissor.X      = TargetX;
        Scissor.Y      = TargetY;
        Scissor.Width  = TargetWidth;
        Scissor.Height = TargetHeight;

        if( !shader_PushUniformData( SHADER_STAGE_PIXEL, ConstantsSlot, &Constants, sizeof( Constants ) ) ||
            !rdraw_SetViewport( Viewport ) || !rdraw_SetScissor( Scissor ) || !rdraw_Draw( 3 ) )
        {
            return FALSE;
        }
    }

    if( !Horizontal )
    {
        m_isTileUpdatesExecuted = TRUE;
    }

    return TRUE;
}

//=========================================================================
//  CASTER STATE
//=========================================================================

xbool ShadowRenderer::SetShadowCastConstants( u32 UniformSlot, const ShadowMapAtlas::ShadowSource& Source,
                                             xbool CoverageUsesTextureAlpha )
{
    cb_shadow_cast CbData;
    x_memset( &CbData, 0, sizeof( CbData ) );
    CbData.ShadowViewProjection = Source.WorldToClip;

    CbData.ShadowOrigin = Source.LightPosRadius;
    CbData.ShadowOrigin.GetW() = 0.0f;
    CbData.ShadowViewProjection( 3, 0 ) = 0.0f;
    CbData.ShadowViewProjection( 3, 1 ) = 0.0f;
    CbData.ShadowViewProjection( 3, 2 ) = -( Source.NearZ * Source.FarZ ) / ( Source.FarZ - Source.NearZ );
    CbData.ShadowViewProjection( 3, 3 ) = 0.0f;

    CbData.CoverageUsesTextureAlpha = CoverageUsesTextureAlpha;

    return shader_PushUniformData( SHADER_STAGE_VERTEX, UniformSlot, &CbData, sizeof( CbData ) );
}

//=========================================================================

xbool ShadowRenderer::SetShadowAlphaConstants( const texture& DiffuseTexture,
                                               xbool          isPunchThru,
                                               u8             UOffset,
                                               u8             VOffset )
{
    if( !m_AlphaDepthPixelShader || !DiffuseTexture.GetShaderResource() )
    {
        return FALSE;
    }

    cb_shadow_alpha CbData;
    x_memset( &CbData, 0, sizeof( CbData ) );
    CbData.AlphaRef = isPunchThru ? 0.5f : ( 4.0f / 255.0f );

    const f32 kInvByte = 1.0f / 255.0f;
    CbData.UVOffset[0] = static_cast<f32>( UOffset ) * kInvByte;
    CbData.UVOffset[1] = static_cast<f32>( VOffset ) * kInvByte;

    if( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_AlphaConstantsSlot, &CbData, sizeof( CbData ) ) )
    {
        return FALSE;
    }

    if( !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_AlphaTextureSlot,
                                                     DiffuseTexture.GetShaderResource(), &m_DiffuseSampler ) ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void ShadowRenderer::ApplySource( s32 SourceIndex, s32 CasterShader, xbool isAlphaTest, xbool isTwoSided )
{
    ASSERT( SourceIndex >= 0 );
    ASSERT( SourceIndex < GetRenderPipeline().GetShadowMapAtlas().GetSourceCount() );

    const s32 CasterVariant = GetShadowCasterVariant( CasterShader, isAlphaTest, isTwoSided );

    if( ( SourceIndex == m_CurrentSource ) && ( CasterVariant == m_CurrentCasterVariant ) )
    {
        return;
    }

    const ShadowMapAtlas::ShadowSource& Source = GetRenderPipeline().GetShadowMapAtlas().GetSource( SourceIndex );

    render_pipeline** pCasterHandles = Source.Type == ShadowMapAtlas::SHADOW_SOURCE_POINT_FACE
                                          ? m_pCasterHandles : m_pSpotCasterHandles;
    render_pipeline* pPipeline     = NULL;
    const shader*    pVertexShader = NULL;

    if( CasterShader == SHADOW_CASTER_RIGID )
    {
        const ShadowPipeline Pipeline = static_cast<ShadowPipeline>( CasterVariant );
        pPipeline                     = pCasterHandles[static_cast<u32>( Pipeline )];
        pVertexShader                 = isAlphaTest ? &m_RigidMaskedVertexShader : &m_RigidVertexShader;
    }
    else if( CasterShader == SHADOW_CASTER_SKIN )
    {
        const ShadowPipeline Pipeline = static_cast<ShadowPipeline>( CasterVariant );
        pPipeline                     = pCasterHandles[static_cast<u32>( Pipeline )];
        pVertexShader                 = isAlphaTest ? &m_SkinMaskedVertexShader : &m_SkinVertexShader;
    }

    if( !RequestPipelines( CasterShader, isAlphaTest, isTwoSided, FALSE ) )
    {
        return;
    }

    pPipeline = pCasterHandles[static_cast<u32>( CasterVariant )];

    if( !pPipeline || !( *pPipeline ) || !pVertexShader || !( *pVertexShader ) )
    {
        return;
    }

    rdraw_viewport Viewport;
    x_memset( &Viewport, 0, sizeof( Viewport ) );
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    Viewport.TopLeftX = static_cast<f32>( Source.AtlasX );
    Viewport.TopLeftY = static_cast<f32>( Source.AtlasY );
    Viewport.Width    = static_cast<f32>( Source.AtlasWidth );
    Viewport.Height   = static_cast<f32>( Source.AtlasHeight );
    rdraw_SetViewport( Viewport );

    if( !render_BindPipeline( *pPipeline ) )
    {
        return;
    }

    if( CasterShader == SHADOW_CASTER_RIGID )
    {
        const shader_resource* pInstances = rbuffer_GetResource( m_RigidInstanceBuffer );

        if( !pInstances || !shader_BindStorageBuffer(
                               shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_RigidInstanceSlot, pInstances ) ) )
        {
            return;
        }
    }
    else
    {
        const shader_resource* pBones = rbuffer_GetResource( m_SkinBoneBuffer );

        if( !pBones )
        {
            return;
        }

        if( !shader_BindStorageBuffer( shader_storage_buffer_binding( SHADER_STAGE_VERTEX, m_SkinBoneSlot, pBones ) ) )
        {
            return;
        }
    }

    m_CurrentSource        = SourceIndex;
    m_CurrentCasterVariant = CasterVariant;
}

//=========================================================================

void ShadowRenderer::ApplyDynamicSource( s32 SourceIndex )
{
    ASSERT( SourceIndex >= 0 );
    ASSERT( SourceIndex < GetRenderPipeline().GetShadowMapAtlas().GetSourceCount() );

    const s32 CasterVariant = SHADOW_CASTER_DYNAMIC;

    if( ( SourceIndex == m_CurrentSource ) && ( CasterVariant == m_CurrentCasterVariant ) )
    {
        return;
    }

    const ShadowMapAtlas::ShadowSource& Source = GetRenderPipeline().GetShadowMapAtlas().GetSource( SourceIndex );
    render_pipeline** pCasterHandles = Source.Type == ShadowMapAtlas::SHADOW_SOURCE_POINT_FACE
                                          ? m_pCasterHandles : m_pSpotCasterHandles;

    if( !RequestPipelines( SHADOW_CASTER_DYNAMIC, TRUE, TRUE, FALSE ) )
    {
        return;
    }

    render_pipeline* pPipeline = pCasterHandles[static_cast<u32>( ShadowPipeline::DynamicAlphaTwoSided )];

    if( !pPipeline || !( *pPipeline ) || !m_DynamicVertexShader || !m_DynamicPixelShader )
    {
        return;
    }

    rdraw_viewport Viewport;
    x_memset( &Viewport, 0, sizeof( Viewport ) );
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Viewport.TopLeftX = static_cast<f32>( Source.AtlasX );
    Viewport.TopLeftY = static_cast<f32>( Source.AtlasY );
    Viewport.Width    = static_cast<f32>( Source.AtlasWidth );
    Viewport.Height   = static_cast<f32>( Source.AtlasHeight );
    rdraw_SetViewport( Viewport );

    if( !render_BindPipeline( *pPipeline ) )
    {
        return;
    }

    m_CurrentSource        = SourceIndex;
    m_CurrentCasterVariant = CasterVariant;
}

//=========================================================================
//  CASTER PREPARATION
//=========================================================================

xbool ShadowRenderer::PrepareCasters( const xarray<const geom_draw_item*>&    Draws,
                                      const xarray<dynamic_geom_shadow_draw>& DynamicDraws,
                                      s32                                     ScopeId )
{
    if( !m_pSubmission )
    {
        return FALSE;
    }

    m_pSubmission->Reset();
    m_ScopeId                        = ScopeId;
    m_SubmissionScopeId              = -1;
    m_LastCasterStats                = CasterStats();
    m_LastCasterStats.InputDrawCount = Draws.GetCount() + DynamicDraws.GetCount();

    if( !m_isInitialized )
    {
        return FALSE;
    }

    if( !GetRenderPipeline().GetShadowMapAtlas().HasActiveSources() )
    {
        return TRUE;
    }

    m_SubmissionScopeId = m_ScopeId;

    if( ( Draws.GetCount() == 0 ) && ( DynamicDraws.GetCount() == 0 ) )
    {
        m_pSubmission->Ready = TRUE;
        return TRUE;
    }

    xarray<PreparedShadowDraw>&             PreparedDraws         = m_pSubmission->PreparedDraws;
    xarray<SkinPaletteRef>&                 SkinPaletteRefs       = m_pSubmission->SkinPaletteRefs;
    xarray<matrix4>&                        RigidInstances        = m_pSubmission->RigidInstances;
    xarray<matrix4>&                        SkinBoneMatrices      = m_pSubmission->SkinBoneMatrices;
    xarray<u32>&                            SkinBoneBases         = m_pSubmission->SkinBoneBases;
    xarray<ShadowIndirectItem>&             RigidIndirectItems    = m_pSubmission->RigidIndirectItems;
    xarray<ShadowIndirectRun>&              RigidIndirectRuns     = m_pSubmission->RigidIndirectRuns;
    xarray<rdraw_indexed_indirect_command>& RigidIndirectCommands = m_pSubmission->RigidIndirectCommands;
    xarray<ShadowIndirectItem>&             SkinIndirectItems     = m_pSubmission->SkinIndirectItems;
    xarray<ShadowIndirectRun>&              SkinIndirectRuns      = m_pSubmission->SkinIndirectRuns;
    xarray<rdraw_indexed_indirect_command>& SkinIndirectCommands  = m_pSubmission->SkinIndirectCommands;
    xarray<PreparedDynamicShadowDraw>&      PreparedDynamicDraws  = m_pSubmission->DynamicDraws;
    xarray<dynamic_geom_vertex>&            DynamicVertices       = m_pSubmission->DynamicVertices;
    xarray<u16>&                            DynamicIndices        = m_pSubmission->DynamicIndices;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ResolveCasters" );

        // Resolve surfaces before the pass; SDL GPU uploads are not allowed inside it.
        for( s32 i = 0; i < Draws.GetCount(); ++i )
        {
            const geom_draw_item* pItem = Draws[i];

            if( !pItem || ( pItem->ShadowSourceIndex < 0 ) ||
                ( pItem->ShadowSourceIndex >= GetRenderPipeline().GetShadowMapAtlas().GetSourceCount() ) )
            {
                continue;
            }

            PreparedShadowDraw Draw;
            Draw.Type               = pItem->Type;
            Draw.SourceIndex        = pItem->ShadowSourceIndex;
            Draw.RegistrationHandle = pItem->hRenderGeom.Handle;
            Draw.SurfaceIndex       = pItem->iSurface;

            if( Draw.Type == GEOM_DRAW_RIGID )
            {
                if( !pItem->Data.Rigid.pL2W )
                {
                    continue;
                }

                Draw.hMesh =
                    GetRenderPipeline().GetGeometryRepository().GetRigidSurface( pItem->hRenderGeom, pItem->iSurface );

                if( Draw.hMesh.IsNull() )
                {
                    continue;
                }

                GeometryRepository::RigidMeshView Mesh;

                if( !GetRenderPipeline().GetGeometryRepository().GetRigidMeshView( Draw.hMesh, Mesh ) ||
                    !Mesh.HasLocalBounds )
                {
                    RENDER_THROW( "ShadowRenderer", "rigid caster has no local bounds" );
                    return FALSE;
                }

                const matrix4 LocalToClip =
                    GetRenderPipeline().GetShadowMapAtlas().GetSource( Draw.SourceIndex ).WorldToClip *
                    *pItem->Data.Rigid.pL2W;

                if( !ShadowCasterIntersectsFrustum( Mesh.LocalBounds, LocalToClip ) )
                {
                    m_LastCasterStats.FrustumRejectedDrawCount++;
                    continue;
                }

                if( RigidInstances.GetCount() >= RigidInstances.GetCapacity() )
                {
                    x_DebugMsg( "ShadowRenderer: rigid instance staging capacity exhausted\n" );
                    m_pSubmission->Reset();
                    return FALSE;
                }

                Draw.RigidInstanceIndex = RigidInstances.GetCount();

                RigidInstances.Append() = *pItem->Data.Rigid.pL2W;
            }
            else
            {
                Draw.hMesh =
                    GetRenderPipeline().GetGeometryRepository().GetSkinSurface( pItem->hRenderGeom, pItem->iSurface );
                Draw.pBones    = pItem->Data.Skin.pBones;
                Draw.BoneCount = pItem->Data.Skin.BoneCount;

                if( Draw.hMesh.IsNull() || !Draw.pBones || ( Draw.BoneCount <= 0 ) )
                {
                    continue;
                }

                SkinnedGeometryStorage::MeshView View;

                if( !GetRenderPipeline().GetGeometryRepository().GetSkinMeshView( Draw.hMesh, View ) ||
                    ( View.SectionCount <= 0 ) )
                {
                    continue;
                }

                xbool ValidBoneRemaps = TRUE;

                for( s32 S = 0; ValidBoneRemaps && ( S < View.SectionCount ); ++S )
                {
                    const SkinnedGeometryStorage::SoftSection& Section = View.pSections[S];

                    for( s32 B = 0; B < SkinnedGeometryStorage::MAX_BONE_PALETTE; ++B )
                    {
                        const u16 BoneId = Section.BoneRemap[B];

                        if( ( BoneId != SkinnedGeometryStorage::INVALID_BONE_REMAP ) &&
                            ( static_cast<s32>( BoneId ) >= Draw.BoneCount ) )
                        {
                            ValidBoneRemaps = FALSE;
                            break;
                        }
                    }
                }

                ASSERTS( ValidBoneRemaps, "Skin shadow section references a bone outside the submitted pose" );
                if( !ValidBoneRemaps )
                {
                    continue;
                }

                Draw.SkinSectionCount = View.SectionCount;
            }

            Draw.pAlphaTexture = GetShadowAlphaTexture( pItem->pMaterial, Draw.isPunchThru );
            Draw.isTwoSided    = pItem->pMaterial && !pItem->pMaterial->CullsBackFaces();

            if( Draw.pAlphaTexture )
            {
                Draw.UOffset = pItem->UOffset;
                Draw.VOffset = pItem->VOffset;
            }

            if( PreparedDraws.GetCount() >= PreparedDraws.GetCapacity() )
            {
                x_DebugMsg( "ShadowRenderer: prepared draw staging capacity exhausted\n" );
                m_pSubmission->Reset();
                return FALSE;
            }

            const u32 DrawIndex    = PreparedDraws.GetCount();
            PreparedDraws.Append() = Draw;

            if( Draw.Type == GEOM_DRAW_RIGID )
            {
                m_LastCasterStats.RigidInstanceCount++;
            }
            else
            {
                m_LastCasterStats.SkinInstanceCount++;
            }

            if( Draw.Type == GEOM_DRAW_SKIN )
            {
                if( SkinPaletteRefs.GetCount() >= SkinPaletteRefs.GetCapacity() )
                {
                    x_DebugMsg( "ShadowRenderer: skin palette staging capacity exhausted\n" );
                    m_pSubmission->Reset();
                    return FALSE;
                }

                SkinPaletteRef& Ref = SkinPaletteRefs.Append();
                Ref.pBones          = Draw.pBones;
                Ref.BoneCount       = Draw.BoneCount;
                Ref.MeshHandle      = Draw.hMesh.Handle;
                Ref.DrawIndex       = DrawIndex;
            }
        }

        // Copy dynamic geometry and pending damage uploads.
        for( s32 i = 0; i < DynamicDraws.GetCount(); ++i )
        {
            const dynamic_geom_shadow_draw& SourceDraw = DynamicDraws[i];

            if( !SourceDraw.pVertices || !SourceDraw.pIndices || !SourceDraw.pDiffuse || !SourceDraw.pDamageMask ||
                !SourceDraw.pDamageTexture || !SourceDraw.pDamageUploadPending || ( SourceDraw.VertexCount <= 0 ) ||
                ( SourceDraw.VertexCount > 65535 ) || ( SourceDraw.IndexCount <= 0 ) ||
                ( ( SourceDraw.IndexCount % 3 ) != 0 ) || ( SourceDraw.ShadowSourceMask == 0 ) )
            {
                continue;
            }

            xbool ValidIndices = TRUE;

            for( s32 j = 0; j < SourceDraw.IndexCount; ++j )
            {
                if( SourceDraw.pIndices[j] >= SourceDraw.VertexCount )
                {
                    ValidIndices = FALSE;
                    break;
                }
            }

            if( !ValidIndices )
            {
                continue;
            }

            if( *SourceDraw.pDamageUploadPending )
            {
                u32 DamageSize = 0;

                if( !SourceDraw.pDamageUpload ||
                    !geom_CalculateDamageUploadSize( SourceDraw.DamageUploadWidth, SourceDraw.DamageUploadHeight,
                                                     DamageSize ) )
                {
                    continue;
                }

                vram_texture_upload_desc Upload;
                Upload.Region.X      = SourceDraw.DamageUploadX;
                Upload.Region.Y      = SourceDraw.DamageUploadY;
                Upload.Region.Width  = SourceDraw.DamageUploadWidth;
                Upload.Region.Height = SourceDraw.DamageUploadHeight;

                const u64 RequiredBytes = static_cast<u64>( m_pSubmission->DamageUploadBytes.GetCount() ) + DamageSize;

                if( !ShadowReserveArrayCapacity( m_pSubmission->DamageUploadBytes, RequiredBytes, "damage uploads" ) )
                {
                    return FALSE;
                }

                const u32 Offset                            = m_pSubmission->DamageUploadBytes.GetCount();
                m_pSubmission->DamageUploadOffsets.Append() = Offset;
                m_pSubmission->DamageUploadBytes.SetCount( static_cast<s32>( RequiredBytes ) );
                x_memcpy( m_pSubmission->DamageUploadBytes.GetPtr() + Offset, SourceDraw.pDamageUpload, DamageSize );
                Upload.Size       = DamageSize;
                Upload.RowPitch   = SourceDraw.DamageUploadWidth;
                Upload.SlicePitch = Upload.Size;

                if( m_pSubmission->DamageTextureUploads.GetCount() >=
                    m_pSubmission->DamageTextureUploads.GetCapacity() )
                {
                    x_DebugMsg( "ShadowRenderer: damage texture upload staging capacity exhausted\n" );
                    m_pSubmission->Reset();
                    return FALSE;
                }

                vram_texture_upload_batch_item& TextureUpload = m_pSubmission->DamageTextureUploads.Append();
                TextureUpload.pTexture                        = SourceDraw.pDamageTexture;
                TextureUpload.Upload                          = Upload;

                m_pSubmission->DamageUploadPendingFlags.Append() = SourceDraw.pDamageUploadPending;
            }

            const u64 RequiredVertexCount =
                static_cast<u64>( DynamicVertices.GetCount() ) + static_cast<u64>( SourceDraw.VertexCount );
            const u64 RequiredIndexCount =
                static_cast<u64>( DynamicIndices.GetCount() ) + static_cast<u64>( SourceDraw.IndexCount );

            if( !ShadowStagingFits( RequiredVertexCount, sizeof( dynamic_geom_vertex ) ) ||
                !ShadowStagingFits( RequiredIndexCount, sizeof( u16 ) ) )
            {
                m_pSubmission->Reset();
                return FALSE;
            }

            const s32 RequiredVertices = static_cast<s32>( RequiredVertexCount );
            const s32 RequiredIndices  = static_cast<s32>( RequiredIndexCount );

            if( DynamicVertices.GetCapacity() < RequiredVertices )
            {
                const s32 Maximum = static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( dynamic_geom_vertex ) );
                DynamicVertices.SetCapacity(
                    ShadowNextStagingCapacity( DynamicVertices.GetCapacity(), RequiredVertices, Maximum ) );
            }

            if( DynamicIndices.GetCapacity() < RequiredIndices )
            {
                const s32 Maximum = static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( u16 ) );
                DynamicIndices.SetCapacity(
                    ShadowNextStagingCapacity( DynamicIndices.GetCapacity(), RequiredIndices, Maximum ) );
            }

            if( PreparedDynamicDraws.GetCount() >= PreparedDynamicDraws.GetCapacity() )
            {
                x_DebugMsg( "ShadowRenderer: dynamic shadow staging capacity exhausted\n" );
                m_pSubmission->Reset();
                return FALSE;
            }

            PreparedDynamicShadowDraw& Draw = PreparedDynamicDraws.Append();
            Draw.pGpuMesh                   = SourceDraw.pGpuMesh;
            Draw.pDiffuse                   = SourceDraw.pDiffuse;
            Draw.pDamageMask                = SourceDraw.pDamageMask;
            Draw.CoverageUsesTextureAlpha    = SourceDraw.CoverageUsesTextureAlpha;
            Draw.FirstVertex                = DynamicVertices.GetCount();
            Draw.FirstIndex                 = DynamicIndices.GetCount();
            Draw.VertexCount                = SourceDraw.VertexCount;
            Draw.IndexCount                 = SourceDraw.IndexCount;
            Draw.ShadowSourceMask           = SourceDraw.ShadowSourceMask;

            const s32 OldVertexCount = DynamicVertices.GetCount();
            DynamicVertices.SetCount( RequiredVertices );
            x_memcpy( &DynamicVertices[OldVertexCount], SourceDraw.pVertices,
                      SourceDraw.VertexCount * sizeof( dynamic_geom_vertex ) );

            const s32 OldIndexCount = DynamicIndices.GetCount();
            DynamicIndices.SetCount( RequiredIndices );
            x_memcpy( &DynamicIndices[OldIndexCount], SourceDraw.pIndices, SourceDraw.IndexCount * sizeof( u16 ) );
        }
    }

    m_LastCasterStats.PreparedDrawCount = PreparedDraws.GetCount() + PreparedDynamicDraws.GetCount();

    if( ( PreparedDraws.GetCount() == 0 ) && ( PreparedDynamicDraws.GetCount() == 0 ) )
    {
        m_pSubmission->Ready = TRUE;
        return TRUE;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildSkinPalettes" );

        // Reuse each animated object's remapped palettes across point-light faces.
        if( SkinPaletteRefs.GetCount() > 1 )
        {
            x_qsort( SkinPaletteRefs.GetPtr(), SkinPaletteRefs.GetCount(), sizeof( SkinPaletteRef ),
                     CompareSkinPaletteRefs );
        }

        for( s32 i = 0; i < SkinPaletteRefs.GetCount(); ++i )
        {
            if( !IsPreparedDrawIndexValid( PreparedDraws, SkinPaletteRefs[i].DrawIndex ) )
            {
                x_DebugMsg( "ShadowRenderer: invalid prepared skin draw index=%u count=%d\n",
                            SkinPaletteRefs[i].DrawIndex, PreparedDraws.GetCount() );
                // Reject malformed caster data without aborting the frame.
                m_pSubmission->Reset();
                return FALSE;
            }
        }

        for( s32 Begin = 0; Begin < SkinPaletteRefs.GetCount(); )
        {
            s32 End = Begin + 1;

            while( ( End < SkinPaletteRefs.GetCount() ) &&
                   ( SkinPaletteRefs[End].MeshHandle == SkinPaletteRefs[Begin].MeshHandle ) &&
                   ( SkinPaletteRefs[End].pBones == SkinPaletteRefs[Begin].pBones ) &&
                   ( SkinPaletteRefs[End].BoneCount == SkinPaletteRefs[Begin].BoneCount ) )
            {
                ++End;
            }

            PreparedShadowDraw&              FirstDraw = PreparedDraws[SkinPaletteRefs[Begin].DrawIndex];
            SkinnedGeometryStorage::MeshView View;

            if( GetRenderPipeline().GetGeometryRepository().GetSkinMeshView( FirstDraw.hMesh, View ) )
            {
                const u64 RequiredBoneCount =
                    static_cast<u64>( SkinBoneMatrices.GetCount() ) +
                    static_cast<u64>( View.SectionCount ) * SkinnedGeometryStorage::MAX_BONE_PALETTE;

                if( !ShadowStagingFits( RequiredBoneCount, sizeof( matrix4 ) ) )
                {
                    m_pSubmission->Reset();
                    return FALSE;
                }

                const s32 RequiredBones = static_cast<s32>( RequiredBoneCount );

                if( SkinBoneMatrices.GetCapacity() < RequiredBones )
                {
                    const s32 Maximum = static_cast<s32>( FrameStorage::MAX_STREAM_BYTES / sizeof( matrix4 ) );
                    SkinBoneMatrices.SetCapacity(
                        ShadowNextStagingCapacity( SkinBoneMatrices.GetCapacity(), RequiredBones, Maximum ) );
                }

                const u32 PaletteBase = SkinBoneMatrices.GetCount();

                for( s32 S = 0; S < View.SectionCount; ++S )
                {
                    const SkinnedGeometryStorage::SoftSection& Section = View.pSections[S];

                    for( s32 B = 0; B < SkinnedGeometryStorage::MAX_BONE_PALETTE; ++B )
                    {
                        matrix4& Bone = SkinBoneMatrices.Append();
                        Bone.Identity();

                        const u16 BoneId = Section.BoneRemap[B];

                        if( FirstDraw.pBones && ( BoneId != SkinnedGeometryStorage::INVALID_BONE_REMAP ) &&
                            ( static_cast<s32>( BoneId ) < FirstDraw.BoneCount ) )
                        {
                            Bone = FirstDraw.pBones[BoneId];
                        }
                    }
                }

                for( s32 i = Begin; i < End; ++i )
                {
                    PreparedDraws[SkinPaletteRefs[i].DrawIndex].SkinPaletteBase = PaletteBase;
                }

                m_LastCasterStats.SkinPaletteCount++;
            }

            Begin = End;
        }
    }

    for( s32 i = 0; i < m_pSubmission->DamageTextureUploads.GetCount(); ++i )
    {
        m_pSubmission->DamageTextureUploads[i].Upload.pData =
            m_pSubmission->DamageUploadBytes.GetPtr() + m_pSubmission->DamageUploadOffsets[i];
    }

    m_pSubmission->Ready = TRUE;

    return TRUE;
}

//=========================================================================
//  PACKET PREPARATION
//=========================================================================

xbool ShadowRenderer::BuildCasterSubmission( FrameGraphBuilder& Graph )
{
    xarray<PreparedShadowDraw>&             PreparedDraws         = m_pSubmission->PreparedDraws;
    xarray<SkinPaletteRef>&                 SkinPaletteRefs       = m_pSubmission->SkinPaletteRefs;
    xarray<matrix4>&                        RigidInstances        = m_pSubmission->RigidInstances;
    xarray<matrix4>&                        SkinBoneMatrices      = m_pSubmission->SkinBoneMatrices;
    xarray<u32>&                            SkinBoneBases         = m_pSubmission->SkinBoneBases;
    xarray<ShadowDrawPacket>&               Packets               = m_pSubmission->Packets;
    xarray<ShadowIndirectItem>&             RigidIndirectItems    = m_pSubmission->RigidIndirectItems;
    xarray<ShadowIndirectRun>&              RigidIndirectRuns     = m_pSubmission->RigidIndirectRuns;
    xarray<rdraw_indexed_indirect_command>& RigidIndirectCommands = m_pSubmission->RigidIndirectCommands;
    xarray<ShadowIndirectItem>&             SkinIndirectItems     = m_pSubmission->SkinIndirectItems;
    xarray<ShadowIndirectRun>&              SkinIndirectRuns      = m_pSubmission->SkinIndirectRuns;
    xarray<rdraw_indexed_indirect_command>& SkinIndirectCommands  = m_pSubmission->SkinIndirectCommands;
    xarray<PreparedDynamicShadowDraw>&      PreparedDynamicDraws  = m_pSubmission->DynamicDraws;
    xarray<dynamic_geom_vertex>&            DynamicVertices       = m_pSubmission->DynamicVertices;
    xarray<u16>&                            DynamicIndices        = m_pSubmission->DynamicIndices;

    xarray<matrix4>& UploadMatrices = m_pSubmission->SkinUploadMatrices;
    xarray<u32>&     Remap          = m_pSubmission->SkinPaletteRemap;

    if( !ShadowReserveArrayCapacity( Remap, SkinBoneMatrices.GetCount(), "skin palette remap" ) )
    {
        return FALSE;
    }

    Remap.SetCount( SkinBoneMatrices.GetCount() );

    for( s32 i = 0; i < Remap.GetCount(); ++i )
    {
        Remap[i] = 0xffffffffu;
    }

    // Compact the casters for dirty atlas tiles.
    s32 DrawCount  = 0;
    s32 RigidCount = 0;

    for( s32 i = 0; i < PreparedDraws.GetCount(); ++i )
    {
        PreparedShadowDraw Draw = PreparedDraws[i];

        if( !m_TileCache.IsDirty( Draw.SourceIndex ) )
        {
            continue;
        }

        if( Draw.Type == GEOM_DRAW_RIGID )
        {
            RigidInstances[RigidCount] = RigidInstances[Draw.RigidInstanceIndex];
            Draw.RigidInstanceIndex    = RigidCount++;
        }
        else
        {
            u32& Base = Remap[Draw.SkinPaletteBase];

            if( Base == 0xffffffffu )
            {
                const u32 Count = Draw.SkinSectionCount * SkinnedGeometryStorage::MAX_BONE_PALETTE;

                if( !ShadowReserveArrayCapacity( UploadMatrices, static_cast<u64>( UploadMatrices.GetCount() ) + Count,
                                                 "skin upload palettes" ) )
                {
                    return FALSE;
                }

                Base = UploadMatrices.GetCount();

                for( u32 Bone = 0; Bone < Count; ++Bone )
                {
                    UploadMatrices.Append() = SkinBoneMatrices[Draw.SkinPaletteBase + Bone];
                }
            }

            Draw.SkinPaletteBase = Base;
        }

        PreparedDraws[DrawCount++] = Draw;
    }

    PreparedDraws.SetCount( DrawCount );
    RigidInstances.SetCount( RigidCount );
    SkinBoneMatrices = UploadMatrices;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildPackets" );

        // Batch adjacent compatible casters; keep source index in the key.
        for( s32 i = 0; i < PreparedDraws.GetCount(); ++i )
        {
            const PreparedShadowDraw& Draw     = PreparedDraws[i];
            xbool                     isAppend = FALSE;

            if( Packets.GetCount() > 0 )
            {
                const ShadowDrawPacket& Last = Packets[Packets.GetCount() - 1];
                isAppend = ( Last.Type == Draw.Type ) && ( Last.hMesh.Handle == Draw.hMesh.Handle ) &&
                           ( Last.SourceIndex == Draw.SourceIndex ) && ( Last.pAlphaTexture == Draw.pAlphaTexture ) &&
                           ( Last.isPunchThru == Draw.isPunchThru ) && ( Last.UOffset == Draw.UOffset ) &&
                           ( Last.VOffset == Draw.VOffset ) && ( Last.isTwoSided == Draw.isTwoSided );
            }

            if( !isAppend )
            {
                if( Packets.GetCount() >= Packets.GetCapacity() )
                {
                    x_DebugMsg( "ShadowRenderer: shadow packet staging capacity exhausted\n" );
                    m_pSubmission->Reset();
                    return FALSE;
                }

                ShadowDrawPacket& PacketData = Packets.Append();
                PacketData.Type              = Draw.Type;
                PacketData.hMesh             = Draw.hMesh;
                PacketData.pAlphaTexture     = Draw.pAlphaTexture;
                PacketData.SourceIndex       = Draw.SourceIndex;
                PacketData.FirstDraw         = i;
                PacketData.FirstInstance     = Draw.RigidInstanceIndex;
                // Reused xarray entries retain old fields after SetCount(0).
                PacketData.InstanceCount         = 0;
                PacketData.FirstSkinDrawInstance = 0;
                PacketData.SkinSectionCount      = Draw.SkinSectionCount;
                PacketData.UOffset               = Draw.UOffset;
                PacketData.VOffset               = Draw.VOffset;
                PacketData.isPunchThru           = Draw.isPunchThru;
                PacketData.isTwoSided            = Draw.isTwoSided;

                if( Draw.Type == GEOM_DRAW_RIGID )
                {
                    m_LastCasterStats.RigidPacketCount++;
                }
                else
                {
                    m_LastCasterStats.SkinPacketCount++;
                }

                if( Draw.pAlphaTexture )
                {
                    m_LastCasterStats.AlphaPacketCount++;
                }
            }

            ShadowDrawPacket& PacketData = Packets[Packets.GetCount() - 1];
            PacketData.InstanceCount++;
            m_LastCasterStats.MaxPacketInstanceCount =
                MAX( m_LastCasterStats.MaxPacketInstanceCount, PacketData.InstanceCount );
        }
    }

    {
        // Reserve section and indirect-command storage.
        u64 SkinBoneBaseCount      = 0;
        u64 SkinIndirectItemCount  = 0;
        u64 RigidIndirectItemCount = 0;

        for( s32 i = 0; i < Packets.GetCount(); ++i )
        {
            const ShadowDrawPacket& PacketData = Packets[i];

            if( PacketData.Type == GEOM_DRAW_RIGID )
            {
                if( !PacketData.pAlphaTexture )
                {
                    ++RigidIndirectItemCount;
                }

                continue;
            }

            if( PacketData.SkinSectionCount <= 0 )
            {
                x_DebugMsg( "ShadowRenderer: invalid skin section count in packet %d\n", i );
                m_pSubmission->Reset();
                return FALSE;
            }

            const u64 SectionCount  = static_cast<u64>( PacketData.SkinSectionCount );
            const u64 InstanceCount = static_cast<u64>( PacketData.InstanceCount );

            if( ( InstanceCount != 0 ) && ( SectionCount > ( ~static_cast<u64>( 0 ) / InstanceCount ) ) )
            {
                x_DebugMsg( "ShadowRenderer: skin bone base count overflow in packet %d\n", i );
                m_pSubmission->Reset();
                return FALSE;
            }

            const u64 BoneBaseCount = SectionCount * InstanceCount;

            if( SkinBoneBaseCount > ( ~static_cast<u64>( 0 ) - BoneBaseCount ) ||
                SkinIndirectItemCount > ( ~static_cast<u64>( 0 ) - SectionCount ) )
            {
                x_DebugMsg( "ShadowRenderer: skin staging count overflow in packet %d\n", i );
                m_pSubmission->Reset();
                return FALSE;
            }

            SkinBoneBaseCount += BoneBaseCount;

            if( !PacketData.pAlphaTexture )
            {
                SkinIndirectItemCount += SectionCount;
            }
        }

        if( !ShadowReserveArrayCapacity( SkinBoneBases, SkinBoneBaseCount, "skin bone bases" ) ||
            !ShadowReserveArrayCapacity( RigidIndirectItems, RigidIndirectItemCount, "rigid indirect items" ) ||
            !ShadowReserveArrayCapacity( RigidIndirectCommands, RigidIndirectItemCount, "rigid indirect commands" ) ||
            !ShadowReserveArrayCapacity( RigidIndirectRuns, RigidIndirectItemCount, "rigid indirect runs" ) ||
            !ShadowReserveArrayCapacity( SkinIndirectItems, SkinIndirectItemCount, "skin indirect items" ) ||
            !ShadowReserveArrayCapacity( SkinIndirectCommands, SkinIndirectItemCount, "skin indirect commands" ) ||
            !ShadowReserveArrayCapacity( SkinIndirectRuns, SkinIndirectItemCount, "skin indirect runs" ) )
        {
            m_pSubmission->Reset();
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildSkinOffsets" );

        // first_instance selects each skin section's bone palette base.
        for( s32 i = 0; i < Packets.GetCount(); ++i )
        {
            ShadowDrawPacket& PacketData = Packets[i];

            if( PacketData.Type != GEOM_DRAW_SKIN )
            {
                continue;
            }

            if( ( PacketData.FirstDraw >= static_cast<u32>( PreparedDraws.GetCount() ) ) ||
                ( PacketData.InstanceCount > static_cast<u32>( PreparedDraws.GetCount() ) - PacketData.FirstDraw ) )
            {
                x_DebugMsg( "ShadowRenderer: invalid skin packet range first=%u count=%u draws=%d\n",
                            PacketData.FirstDraw, PacketData.InstanceCount, PreparedDraws.GetCount() );
                m_pSubmission->Reset();
                return FALSE;
            }

            PacketData.FirstSkinDrawInstance = SkinBoneBases.GetCount();

            for( s32 S = 0; S < PacketData.SkinSectionCount; ++S )
            {
                for( u32 D = 0; D < PacketData.InstanceCount; ++D )
                {
                    const PreparedShadowDraw& DrawData = PreparedDraws[PacketData.FirstDraw + D];
                    SkinBoneBases.Append() = DrawData.SkinPaletteBase + S * SkinnedGeometryStorage::MAX_BONE_PALETTE;
                }
            }
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BuildIndirectRuns" );

        for( s32 i = 0; i < Packets.GetCount(); ++i )
        {
            const ShadowDrawPacket& PacketData = Packets[i];

            if( ( PacketData.Type != GEOM_DRAW_RIGID ) || PacketData.pAlphaTexture )
            {
                continue;
            }

            ShadowIndirectItem Item;
            Item.PacketIndex   = i;
            Item.SourceIndex   = PacketData.SourceIndex;
            Item.FirstInstance = PacketData.FirstInstance;
            Item.isTwoSided    = PacketData.isTwoSided;

            if( !GetRenderPipeline().GetGeometryRepository().GetRigidMeshView( PacketData.hMesh, Item.Range ) )
            {
                return FALSE;
            }

            RigidIndirectItems.Append() = Item;
        }

        if( RigidIndirectItems.GetCount() > 1 )
        {
            x_qsort( RigidIndirectItems.GetPtr(), RigidIndirectItems.GetCount(), sizeof( ShadowIndirectItem ),
                     CompareShadowIndirectItems );
        }

        for( s32 i = 0; i < RigidIndirectItems.GetCount(); ++i )
        {
            const ShadowIndirectItem& Item       = RigidIndirectItems[i];
            const ShadowDrawPacket&   PacketData = Packets[Item.PacketIndex];

            ShadowIndirectRun* pRun = NULL;

            if( RigidIndirectRuns.GetCount() > 0 )
            {
                ShadowIndirectRun& Last = RigidIndirectRuns[RigidIndirectRuns.GetCount() - 1];

                if( ( Last.SourceIndex == Item.SourceIndex ) && ( Last.VertexPool == Item.Range.VertexPool ) &&
                    ( Last.IndexPool == Item.Range.IndexPool ) && ( Last.isTwoSided == Item.isTwoSided ) )
                {
                    pRun = &Last;
                }
            }

            if( !pRun )
            {
                ShadowIndirectRun& Run = RigidIndirectRuns.Append();
                Run.FirstItem          = i;
                Run.FirstCommand       = RigidIndirectCommands.GetCount();
                Run.CommandCount       = 0;
                Run.SourceIndex        = Item.SourceIndex;
                Run.VertexPool         = Item.Range.VertexPool;
                Run.IndexPool          = Item.Range.IndexPool;
                Run.isTwoSided         = Item.isTwoSided;
                pRun                   = &Run;
            }

            rdraw_indexed_indirect_command& Command = RigidIndirectCommands.Append();
            Command.IndexCount                      = Item.Range.IndexCount;
            Command.InstanceCount                   = PacketData.InstanceCount;
            Command.FirstIndex                      = Item.Range.FirstIndex;
            Command.BaseVertex                      = Item.Range.BaseVertex;
            Command.FirstInstance                   = Item.FirstInstance;
            pRun->CommandCount++;
        }

        for( s32 i = 0; i < Packets.GetCount(); ++i )
        {
            const ShadowDrawPacket& PacketData = Packets[i];

            if( ( PacketData.Type != GEOM_DRAW_SKIN ) || PacketData.pAlphaTexture )
            {
                continue;
            }

            SkinnedGeometryStorage::MeshView View;

            if( !GetRenderPipeline().GetGeometryRepository().GetSkinMeshView( PacketData.hMesh, View ) )
            {
                return FALSE;
            }

            for( s32 S = 0; S < View.SectionCount; ++S )
            {
                const SkinnedGeometryStorage::SoftSection& Section = View.pSections[S];
                ShadowIndirectItem                         Item;
                Item.PacketIndex   = i;
                Item.SourceIndex   = PacketData.SourceIndex;
                Item.FirstInstance = PacketData.FirstSkinDrawInstance + S * PacketData.InstanceCount;
                Item.isTwoSided    = PacketData.isTwoSided;
                Item.Range         = View.Geom;
                Item.Range.FirstIndex += Section.FirstIndex;
                Item.Range.IndexCount = Section.IndexCount;

                SkinIndirectItems.Append() = Item;
            }
        }

        if( SkinIndirectItems.GetCount() > 1 )
        {
            x_qsort( SkinIndirectItems.GetPtr(), SkinIndirectItems.GetCount(), sizeof( ShadowIndirectItem ),
                     CompareShadowIndirectItems );
        }

        for( s32 i = 0; i < SkinIndirectItems.GetCount(); ++i )
        {
            const ShadowIndirectItem& Item       = SkinIndirectItems[i];
            const ShadowDrawPacket&   PacketData = Packets[Item.PacketIndex];

            ShadowIndirectRun* pRun = NULL;

            if( SkinIndirectRuns.GetCount() > 0 )
            {
                ShadowIndirectRun& Last = SkinIndirectRuns[SkinIndirectRuns.GetCount() - 1];

                if( ( Last.SourceIndex == Item.SourceIndex ) && ( Last.VertexPool == Item.Range.VertexPool ) &&
                    ( Last.IndexPool == Item.Range.IndexPool ) && ( Last.isTwoSided == Item.isTwoSided ) )
                {
                    pRun = &Last;
                }
            }

            if( !pRun )
            {
                ShadowIndirectRun& Run = SkinIndirectRuns.Append();
                Run.FirstItem          = i;
                Run.FirstCommand       = SkinIndirectCommands.GetCount();
                Run.CommandCount       = 0;
                Run.SourceIndex        = Item.SourceIndex;
                Run.VertexPool         = Item.Range.VertexPool;
                Run.IndexPool          = Item.Range.IndexPool;
                Run.isTwoSided         = Item.isTwoSided;
                pRun                   = &Run;
            }

            rdraw_indexed_indirect_command& Command = SkinIndirectCommands.Append();
            Command.IndexCount                      = Item.Range.IndexCount;
            Command.InstanceCount                   = PacketData.InstanceCount;
            Command.FirstIndex                      = Item.Range.FirstIndex;
            Command.BaseVertex                      = Item.Range.BaseVertex;
            Command.FirstInstance                   = Item.FirstInstance;
            pRun->CommandCount++;
        }
    }

    m_LastCasterStats.PacketCount = Packets.GetCount() + PreparedDynamicDraws.GetCount();
    m_LastCasterStats.AlphaPacketCount += PreparedDynamicDraws.GetCount();
    m_LastCasterStats.SkinPaletteMatrixCount    = SkinBoneMatrices.GetCount();
    m_LastCasterStats.RigidIndirectRunCount     = RigidIndirectRuns.GetCount();
    m_LastCasterStats.RigidIndirectCommandCount = RigidIndirectCommands.GetCount();
    m_LastCasterStats.SkinIndirectRunCount      = SkinIndirectRuns.GetCount();
    m_LastCasterStats.SkinIndirectCommandCount  = SkinIndirectCommands.GetCount();
    m_LastCasterStats.UploadBytes +=
        static_cast<u64>( RigidInstances.GetCount() ) * sizeof( matrix4 ) +
        static_cast<u64>( SkinBoneMatrices.GetCount() ) * sizeof( matrix4 ) +
        static_cast<u64>( SkinBoneBases.GetCount() ) * sizeof( u32 ) +
        static_cast<u64>( RigidIndirectCommands.GetCount() + SkinIndirectCommands.GetCount() ) *
            sizeof( rdraw_indexed_indirect_command ) +
        static_cast<u64>( DynamicVertices.GetCount() ) * sizeof( dynamic_geom_vertex ) +
        static_cast<u64>( DynamicIndices.GetCount() ) * sizeof( u16 );

    if( ( RigidInstances.GetCount() > 0 ) &&
        ( !m_RigidInstanceBuffer || ( m_RigidInstanceCapacity < static_cast<u32>( RigidInstances.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    if( ( RigidInstances.GetCount() > 0 ) &&
        ( !m_RigidInstanceIndexBuffer ||
          ( m_RigidInstanceIndexCapacity < static_cast<u32>( RigidInstances.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    if( ( SkinBoneMatrices.GetCount() > 0 ) &&
        ( !m_SkinBoneBuffer || ( m_SkinBoneCapacity < static_cast<u32>( SkinBoneMatrices.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    if( ( SkinBoneBases.GetCount() > 0 ) &&
        ( !m_SkinBoneBaseBuffer || ( m_SkinBoneBaseCapacity < static_cast<u32>( SkinBoneBases.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    if( ( RigidIndirectCommands.GetCount() > 0 ) &&
        ( !m_RigidIndirectBuffer ||
          ( m_RigidIndirectCapacity < static_cast<u32>( RigidIndirectCommands.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    if( ( SkinIndirectCommands.GetCount() > 0 ) &&
        ( !m_SkinIndirectBuffer || ( m_SkinIndirectCapacity < static_cast<u32>( SkinIndirectCommands.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    if( ( DynamicVertices.GetCount() > 0 ) &&
        ( !m_DynamicVertexBuffer || ( m_DynamicVertexCapacity < static_cast<u32>( DynamicVertices.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    if( ( DynamicIndices.GetCount() > 0 ) &&
        ( !m_DynamicIndexBuffer || ( m_DynamicIndexCapacity < static_cast<u32>( DynamicIndices.GetCount() ) ) ) )
    {
        m_LastCasterStats.BufferReallocationCount++;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/UploadBuffers" );

        rbuffer_upload_batch_item Uploads[8];
        u32                       UploadCount = 0;

        if( RigidInstances.GetCount() > 0 )
        {
            if( !EnsureCasterBuffer( m_RigidInstanceBuffer, m_RigidInstanceCapacity, RigidInstances.GetCount(),
                                     sizeof( matrix4 ), "ShadowRigidWorld" ) ||
                !EnsureInstanceIndexBuffer( m_RigidInstanceIndexBuffer, m_RigidInstanceIndexCapacity,
                                            RigidInstances.GetCount(), "ShadowRigidInstanceIndices",
                                            m_InstanceIndexScratch ) )
            {
                return FALSE;
            }

            if( !Graph.QueueUpload( m_RigidInstanceIndexBuffer, m_InstanceIndexScratch.GetPtr(),
                                    RigidInstances.GetCount() * sizeof( u32 ), 0, TRUE ) )
            {
                return FALSE;
            }

            Uploads[UploadCount].pBuffer       = &m_RigidInstanceBuffer;
            Uploads[UploadCount].Upload.pData  = &RigidInstances[0];
            Uploads[UploadCount].Upload.Size   = sizeof( matrix4 ) * RigidInstances.GetCount();
            Uploads[UploadCount].Upload.bCycle = TRUE;
            ++UploadCount;
        }

        if( SkinBoneMatrices.GetCount() > 0 )
        {
            if( !EnsureCasterBuffer( m_SkinBoneBuffer, m_SkinBoneCapacity, SkinBoneMatrices.GetCount(),
                                     sizeof( matrix4 ), "ShadowSkinBones" ) )
            {
                return FALSE;
            }

            Uploads[UploadCount].pBuffer       = &m_SkinBoneBuffer;
            Uploads[UploadCount].Upload.pData  = &SkinBoneMatrices[0];
            Uploads[UploadCount].Upload.Size   = sizeof( matrix4 ) * SkinBoneMatrices.GetCount();
            Uploads[UploadCount].Upload.bCycle = TRUE;
            ++UploadCount;
        }

        if( SkinBoneBases.GetCount() > 0 )
        {
            if( !EnsureCasterBuffer( m_SkinBoneBaseBuffer, m_SkinBoneBaseCapacity, SkinBoneBases.GetCount(),
                                     sizeof( u32 ), "ShadowSkinBoneBases", RBUFFER_USAGE_VERTEX ) )
            {
                return FALSE;
            }

            Uploads[UploadCount].pBuffer       = &m_SkinBoneBaseBuffer;
            Uploads[UploadCount].Upload.pData  = &SkinBoneBases[0];
            Uploads[UploadCount].Upload.Size   = sizeof( u32 ) * SkinBoneBases.GetCount();
            Uploads[UploadCount].Upload.bCycle = TRUE;
            ++UploadCount;
        }

        if( RigidIndirectCommands.GetCount() > 0 )
        {
            if( !EnsureCasterBuffer( m_RigidIndirectBuffer, m_RigidIndirectCapacity, RigidIndirectCommands.GetCount(),
                                     sizeof( rdraw_indexed_indirect_command ), "ShadowRigidIndirect",
                                     RBUFFER_USAGE_INDIRECT ) )
            {
                return FALSE;
            }

            Uploads[UploadCount].pBuffer      = &m_RigidIndirectBuffer;
            Uploads[UploadCount].Upload.pData = &RigidIndirectCommands[0];
            Uploads[UploadCount].Upload.Size =
                sizeof( rdraw_indexed_indirect_command ) * RigidIndirectCommands.GetCount();
            Uploads[UploadCount].Upload.bCycle = TRUE;
            ++UploadCount;
        }

        if( SkinIndirectCommands.GetCount() > 0 )
        {
            if( !EnsureCasterBuffer( m_SkinIndirectBuffer, m_SkinIndirectCapacity, SkinIndirectCommands.GetCount(),
                                     sizeof( rdraw_indexed_indirect_command ), "ShadowSkinIndirect",
                                     RBUFFER_USAGE_INDIRECT ) )
            {
                return FALSE;
            }

            Uploads[UploadCount].pBuffer      = &m_SkinIndirectBuffer;
            Uploads[UploadCount].Upload.pData = &SkinIndirectCommands[0];
            Uploads[UploadCount].Upload.Size =
                sizeof( rdraw_indexed_indirect_command ) * SkinIndirectCommands.GetCount();
            Uploads[UploadCount].Upload.bCycle = TRUE;
            ++UploadCount;
        }

        if( DynamicVertices.GetCount() > 0 )
        {
            if( !EnsureCasterBuffer( m_DynamicVertexBuffer, m_DynamicVertexCapacity, DynamicVertices.GetCount(),
                                     sizeof( dynamic_geom_vertex ), "ShadowDynamicVertices", RBUFFER_USAGE_VERTEX ) ||
                !EnsureCasterBuffer( m_DynamicIndexBuffer, m_DynamicIndexCapacity, DynamicIndices.GetCount(),
                                     sizeof( u16 ), "ShadowDynamicIndices", RBUFFER_USAGE_INDEX ) )
            {
                return FALSE;
            }

            Uploads[UploadCount].pBuffer       = &m_DynamicVertexBuffer;
            Uploads[UploadCount].Upload.pData  = &DynamicVertices[0];
            Uploads[UploadCount].Upload.Size   = sizeof( dynamic_geom_vertex ) * DynamicVertices.GetCount();
            Uploads[UploadCount].Upload.bCycle = TRUE;
            ++UploadCount;

            Uploads[UploadCount].pBuffer       = &m_DynamicIndexBuffer;
            Uploads[UploadCount].Upload.pData  = &DynamicIndices[0];
            Uploads[UploadCount].Upload.Size   = sizeof( u16 ) * DynamicIndices.GetCount();
            Uploads[UploadCount].Upload.bCycle = TRUE;
            ++UploadCount;
        }

        if( UploadCount > 0 && !Graph.QueueUploadBatch( Uploads, UploadCount ) )
        {
            return FALSE;
        }
    }

    {
        enum
        {
            MAX_TEXTURE_UPLOAD_BATCH = 64
        };

        vram_texture_upload_batch_item TextureUploads[MAX_TEXTURE_UPLOAD_BATCH];
        xbool*                         PendingFlags[MAX_TEXTURE_UPLOAD_BATCH];
        u32                            TextureUploadCount = 0;

        for( s32 i = 0; i < m_pSubmission->DamageTextureUploads.GetCount(); ++i )
        {
            if( TextureUploadCount == MAX_TEXTURE_UPLOAD_BATCH )
            {
                if( !FlushDamageTextureUploads( Graph, TextureUploads, PendingFlags, TextureUploadCount,
                                                m_LastCasterStats.UploadBytes ) )
                {
                    m_pSubmission->Reset();
                    return FALSE;
                }
            }

            TextureUploads[TextureUploadCount] = m_pSubmission->DamageTextureUploads[i];
            PendingFlags[TextureUploadCount]   = m_pSubmission->DamageUploadPendingFlags[i];
            ++TextureUploadCount;
        }

        if( !FlushDamageTextureUploads( Graph, TextureUploads, PendingFlags, TextureUploadCount,
                                        m_LastCasterStats.UploadBytes ) )
        {
            m_pSubmission->Reset();
            return FALSE;
        }
    }

    m_pSubmission->Ready = TRUE;

    return TRUE;
}

//=========================================================================

xbool ShadowRenderer::ExecuteCasterPass( RenderPassContext& Context )
{
    if( !m_pSubmission || !m_pSubmission->Ready || !Context.Write( m_ShadowAtlas ) )
    {
        return FALSE;
    }

    // Atlas coordinates are independent of the camera's viewport, including PIP.
    rdraw_scissor AtlasScissor;
    AtlasScissor.X = AtlasScissor.Y = 0;
    AtlasScissor.Width = AtlasScissor.Height = m_ShadowAtlasSize;

    if( !rdraw_SetScissor( AtlasScissor ) )
    {
        return FALSE;
    }

    if( !m_TileCache.IsFullRefresh() && !ClearUpdatedTiles() )
    {
        return FALSE;
    }

    const xarray<ShadowDrawPacket>&               PacketsData           = m_pSubmission->Packets;
    const xarray<ShadowIndirectItem>&             RigidIndirectItems    = m_pSubmission->RigidIndirectItems;
    const xarray<ShadowIndirectRun>&              RigidIndirectRuns     = m_pSubmission->RigidIndirectRuns;
    const xarray<rdraw_indexed_indirect_command>& RigidIndirectCommands = m_pSubmission->RigidIndirectCommands;
    const xarray<ShadowIndirectItem>&             SkinIndirectItems     = m_pSubmission->SkinIndirectItems;
    const xarray<ShadowIndirectRun>&              SkinIndirectRuns      = m_pSubmission->SkinIndirectRuns;
    const xarray<rdraw_indexed_indirect_command>& SkinIndirectCommands  = m_pSubmission->SkinIndirectCommands;
    const xarray<PreparedDynamicShadowDraw>&      PreparedDynamicDraws  = m_pSubmission->DynamicDraws;

    // Count the draws required to complete the tile updates.
    u32 ExpectedDrawCount = 0;

    for( s32 i = 0; i < RigidIndirectRuns.GetCount(); ++i )
    {
        ExpectedDrawCount += m_TileCache.IsDirty( RigidIndirectRuns[i].SourceIndex ) ? 1 : 0;
    }

    for( s32 i = 0; i < SkinIndirectRuns.GetCount(); ++i )
    {
        ExpectedDrawCount += m_TileCache.IsDirty( SkinIndirectRuns[i].SourceIndex ) ? 1 : 0;
    }

    for( s32 i = 0; i < PacketsData.GetCount(); ++i )
    {
        const ShadowDrawPacket& Packet = PacketsData[i];

        if( Packet.pAlphaTexture && m_TileCache.IsDirty( Packet.SourceIndex ) )
        {
            ExpectedDrawCount += Packet.Type == GEOM_DRAW_RIGID ? 1 : Packet.SkinSectionCount;
        }
    }

    for( s32 i = 0; i < PreparedDynamicDraws.GetCount(); ++i )
    {
        for( s32 Source = 0; Source < GetRenderPipeline().GetShadowMapAtlas().GetSourceCount(); ++Source )
        {
            if( m_TileCache.IsDirty( Source ) && ( PreparedDynamicDraws[i].ShadowSourceMask & ( u64( 1 ) << Source ) ) )
            {
                ++ExpectedDrawCount;
            }
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/EncodePackets" );

        m_CurrentSource        = -1;
        m_CurrentCasterVariant = SHADOW_CASTER_NONE;

        for( s32 i = 0; i < RigidIndirectRuns.GetCount(); ++i )
        {
            const ShadowIndirectRun& Run = RigidIndirectRuns[i];

            if( !m_TileCache.IsDirty( Run.SourceIndex ) )
            {
                continue;
            }

            const ShadowIndirectItem&           FirstItem = RigidIndirectItems[Run.FirstItem];
            const ShadowMapAtlas::ShadowSource& Source =
                GetRenderPipeline().GetShadowMapAtlas().GetSource( Run.SourceIndex );
            const s32 CasterVariant = GetShadowCasterVariant( SHADOW_CASTER_RIGID, FALSE, Run.isTwoSided );

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ApplyIndirectState" );

                if( ( Run.SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
                {
                    m_LastCasterStats.SourceStateChangeCount++;
                }

                ApplySource( Run.SourceIndex, SHADOW_CASTER_RIGID, FALSE, Run.isTwoSided );
            }

            if( ( Run.SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
            {
                continue;
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindIndirectRun" );

                if( !SetShadowCastConstants( m_RigidCastConstantsSlot, Source ) ||
                    !GetRenderPipeline().GetGeometryRepository().BindRigidMesh( FirstItem.Range ) ||
                    !rbuffer_BindVertex( m_RigidInstanceIndexBuffer, 1, 0 ) )
                {
                    continue;
                }
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/DrawIndirectRun" );

                if( !rdraw_DrawIndexedIndirect( m_RigidIndirectBuffer,
                                                Run.FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                                Run.CommandCount ) )
                {
                    continue;
                }
            }

            m_LastCasterStats.GpuDrawCount++;

            for( u32 CommandIndex = 0; CommandIndex < Run.CommandCount; ++CommandIndex )
            {
                const rdraw_indexed_indirect_command& Command = RigidIndirectCommands[Run.FirstCommand + CommandIndex];
                m_LastCasterStats.SubmittedIndexCount += static_cast<u64>( Command.IndexCount ) * Command.InstanceCount;
            }
        }

        for( s32 i = 0; i < SkinIndirectRuns.GetCount(); ++i )
        {
            const ShadowIndirectRun& Run = SkinIndirectRuns[i];

            if( !m_TileCache.IsDirty( Run.SourceIndex ) )
            {
                continue;
            }

            const ShadowIndirectItem&           FirstItem = SkinIndirectItems[Run.FirstItem];
            const ShadowMapAtlas::ShadowSource& Source =
                GetRenderPipeline().GetShadowMapAtlas().GetSource( Run.SourceIndex );
            const s32 CasterVariant = GetShadowCasterVariant( SHADOW_CASTER_SKIN, FALSE, Run.isTwoSided );

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ApplySkinIndirectState" );

                if( ( Run.SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
                {
                    m_LastCasterStats.SourceStateChangeCount++;
                }

                ApplySource( Run.SourceIndex, SHADOW_CASTER_SKIN, FALSE, Run.isTwoSided );
            }

            if( ( Run.SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
            {
                continue;
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindSkinIndirectRun" );

                SkinnedGeometryStorage::MeshView SkinView;

                if( !SetShadowCastConstants( m_SkinCastConstantsSlot, Source ) ||
                    !GetRenderPipeline().GetGeometryRepository().GetSkinMeshView(
                        PacketsData[FirstItem.PacketIndex].hMesh, SkinView ) ||
                    !GetRenderPipeline().GetGeometryRepository().BindSkinMesh( SkinView ) ||
                    !rbuffer_BindVertex( m_SkinBoneBaseBuffer, 1, 0 ) )
                {
                    continue;
                }
            }

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/DrawSkinIndirectRun" );

                if( !rdraw_DrawIndexedIndirect( m_SkinIndirectBuffer,
                                                Run.FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                                Run.CommandCount ) )
                {
                    continue;
                }
            }

            m_LastCasterStats.GpuDrawCount++;

            for( u32 CommandIndex = 0; CommandIndex < Run.CommandCount; ++CommandIndex )
            {
                const rdraw_indexed_indirect_command& Command = SkinIndirectCommands[Run.FirstCommand + CommandIndex];
                m_LastCasterStats.SubmittedIndexCount += static_cast<u64>( Command.IndexCount ) * Command.InstanceCount;
            }
        }

        for( s32 i = 0; i < PacketsData.GetCount(); ++i )
        {
            const ShadowDrawPacket& PacketData = PacketsData[i];

            if( !m_TileCache.IsDirty( PacketData.SourceIndex ) )
            {
                continue;
            }

            const xbool BAlphaTest = ( PacketData.pAlphaTexture != NULL );

            if( !BAlphaTest )
            {
                continue;
            }

            const s32 CasterShader  = ( PacketData.Type == GEOM_DRAW_RIGID ) ? SHADOW_CASTER_RIGID : SHADOW_CASTER_SKIN;
            const s32 CasterVariant = GetShadowCasterVariant( CasterShader, BAlphaTest, PacketData.isTwoSided );
            const ShadowMapAtlas::ShadowSource& SourceData =
                GetRenderPipeline().GetShadowMapAtlas().GetSource( PacketData.SourceIndex );

            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/ApplyState" );

                if( ( PacketData.SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
                {
                    m_LastCasterStats.SourceStateChangeCount++;
                }

                ApplySource( PacketData.SourceIndex, CasterShader, BAlphaTest, PacketData.isTwoSided );
            }

            if( ( PacketData.SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
            {
                continue;
            }

            if( BAlphaTest )
            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindAlpha" );

                if( !SetShadowAlphaConstants( *PacketData.pAlphaTexture, PacketData.isPunchThru, PacketData.UOffset,
                                              PacketData.VOffset ) )
                {
                    continue;
                }
            }

            if( PacketData.Type == GEOM_DRAW_RIGID )
            {
                StaticGeometryStorage::mesh_range RangeData;
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindMesh" );

                    if( !GetRenderPipeline().GetGeometryRepository().GetRigidMeshView( PacketData.hMesh, RangeData ) ||
                        !GetRenderPipeline().GetGeometryRepository().BindRigidMesh( RangeData ) ||
                        !rbuffer_BindVertex( m_RigidInstanceIndexBuffer, 1, 0 ) )
                    {
                        continue;
                    }
                }
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/PushDrawConstants" );

                    if( !SetShadowCastConstants( m_RigidCastConstantsSlot, SourceData ) )
                    {
                        continue;
                    }
                }
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/Draw" );

                    if( rdraw_DrawIndexedInstanced( RangeData.IndexCount, PacketData.InstanceCount,
                                                    RangeData.FirstIndex, RangeData.BaseVertex,
                                                    PacketData.FirstInstance ) )
                    {
                        m_LastCasterStats.GpuDrawCount++;
                        m_LastCasterStats.SubmittedIndexCount +=
                            static_cast<u64>( RangeData.IndexCount ) * PacketData.InstanceCount;
                    }
                }
            }
            else
            {
                SkinnedGeometryStorage::MeshView View;
                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/BindMesh" );

                    if( !GetRenderPipeline().GetGeometryRepository().GetSkinMeshView( PacketData.hMesh, View ) ||
                        !GetRenderPipeline().GetGeometryRepository().BindSkinMesh( View ) ||
                        !rbuffer_BindVertex( m_SkinBoneBaseBuffer, 1, 0 ) )
                    {
                        continue;
                    }
                }

                {
                    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/PushDrawConstants" );

                    if( !SetShadowCastConstants( m_SkinCastConstantsSlot, SourceData ) )
                    {
                        continue;
                    }
                }

                for( s32 S = 0; S < View.SectionCount; ++S )
                {
                    const SkinnedGeometryStorage::SoftSection& Section = View.pSections[S];
                    {
                        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Shadow/Draw" );

                        if( rdraw_DrawIndexedInstanced( Section.IndexCount, PacketData.InstanceCount,
                                                        View.Geom.FirstIndex + Section.FirstIndex, View.Geom.BaseVertex,
                                                        PacketData.FirstSkinDrawInstance +
                                                            S * PacketData.InstanceCount ) )
                        {
                            m_LastCasterStats.GpuDrawCount++;
                            m_LastCasterStats.SubmittedIndexCount +=
                                static_cast<u64>( Section.IndexCount ) * PacketData.InstanceCount;
                        }
                    }
                }
            }
        }

        if( PreparedDynamicDraws.GetCount() > 0 )
        {
            const s32 SourceCount = GetRenderPipeline().GetShadowMapAtlas().GetSourceCount();

            for( s32 i = 0; i < PreparedDynamicDraws.GetCount(); ++i )
            {
                const PreparedDynamicShadowDraw& DrawData = PreparedDynamicDraws[i];

                for( s32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex )
                {
                    if( !m_TileCache.IsDirty( SourceIndex ) ||
                        ( DrawData.ShadowSourceMask & ( u64{ 1 } << SourceIndex ) ) == 0 )
                    {
                        continue;
                    }

                    const s32 CasterVariant = SHADOW_CASTER_DYNAMIC;

                    if( ( SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
                    {
                        m_LastCasterStats.SourceStateChangeCount++;
                    }

                    ApplyDynamicSource( SourceIndex );

                    if( ( SourceIndex != m_CurrentSource ) || ( CasterVariant != m_CurrentCasterVariant ) )
                    {
                        continue;
                    }

                    const ShadowMapAtlas::ShadowSource& SourceData =
                        GetRenderPipeline().GetShadowMapAtlas().GetSource( SourceIndex );

                    const dynamic_geom_gpu_mesh* pGpuMesh = DrawData.pGpuMesh;
                    const rbuffer& VertexBuffer = pGpuMesh ? *pGpuMesh->pVertices : m_DynamicVertexBuffer;
                    const rbuffer& IndexBuffer = pGpuMesh ? *pGpuMesh->pIndices : m_DynamicIndexBuffer;
                    const s32 IndexCount = pGpuMesh ? pGpuMesh->IndexCount : DrawData.IndexCount;

                    if( !SetShadowCastConstants( m_DynamicCastConstantsSlot, SourceData,
                                                 DrawData.CoverageUsesTextureAlpha ) ||
                        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_DynamicDiffuseTextureSlot,
                                                                     DrawData.pDiffuse, &m_DiffuseSampler ) ) ||
                        !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, m_DynamicDamageTextureSlot,
                                                                     DrawData.pDamageMask, &m_EvsmLinearSampler ) ) ||
                        !rbuffer_BindVertex( VertexBuffer, 0, 0 ) ||
                        !rbuffer_BindIndex( IndexBuffer, RBUFFER_INDEX_FORMAT_U16 ) )
                    {
                        continue;
                    }

                    if( rdraw_DrawIndexed( IndexCount, pGpuMesh ? 0 : DrawData.FirstIndex,
                                           pGpuMesh ? 0 : static_cast<s32>( DrawData.FirstVertex ) ) )
                    {
                        m_LastCasterStats.GpuDrawCount++;
                        m_LastCasterStats.SubmittedIndexCount += IndexCount;
                    }
                }
            }
        }
    }

    if( m_LastCasterStats.GpuDrawCount != ExpectedDrawCount )
    {
        m_TileCache.Invalidate();
        RENDER_THROW( "ShadowRenderer", "incomplete shadow tile update: submitted %u of %u draws",
                      m_LastCasterStats.GpuDrawCount, ExpectedDrawCount );
        return FALSE;
    }

    if( GetRenderPipeline().GetShadowMapAtlas().GetShadowFilterType() == ShadowFilterType::Hard )
    {
        m_isTileUpdatesExecuted = TRUE;
    }

    m_CurrentSource        = -1;
    m_CurrentCasterVariant = SHADOW_CASTER_NONE;

    if( m_pSubmission )
    {
        m_pSubmission->Reset();
    }

    return TRUE;
}

//=========================================================================
//  RUNTIME QUERIES
//=========================================================================

RenderResourceHandle ShadowRenderer::GetShadowSampleAtlasHandle( void ) const
{
    return ( GetRenderPipeline().GetShadowMapAtlas().GetShadowFilterType() == ShadowFilterType::Evsm )
               ? m_ShadowMomentAtlas
               : m_ShadowAtlas;
}

//=========================================================================

f32 ShadowRenderer::GetShadowNormalBiasTexels( void ) const
{
    return SHADOW_NORMAL_BIAS_TEXELS;
}

//=========================================================================

f32 ShadowRenderer::GetShadowSeamBlendTexels( void ) const
{
    return SHADOW_SEAM_BLEND_TEXELS;
}

//=========================================================================

f32 ShadowRenderer::GetAtlasTexelSize( void ) const
{
    const s32 ShadowAtlasSize = ( m_ShadowAtlasSize > 0 ) ? m_ShadowAtlasSize : MAX_SHADOW_ATLAS_SIZE;
    return 1.0f / static_cast<f32>( ShadowAtlasSize );
}

//=========================================================================

vector4 ShadowRenderer::GetShadowFilterParams( void ) const
{
    const s32 SampleAtlasSize =
        ( GetRenderPipeline().GetShadowMapAtlas().GetShadowFilterType() == ShadowFilterType::Evsm )
            ? m_ShadowMomentAtlasSize
            : m_ShadowAtlasSize;
    const f32 SampleAtlasTexelSize = ( SampleAtlasSize > 0 ) ? ( 1.0f / static_cast<f32>( SampleAtlasSize ) ) : 1.0f;
    return vector4( SampleAtlasTexelSize, kShadowEVSMPositiveExponent, kShadowEVSMNegativeExponent,
                    kShadowEVSMLightBleedReduction );
}

//=========================================================================

const ShadowRenderer::CasterStats& ShadowRenderer::GetLastCasterStats( void ) const
{
    return m_LastCasterStats;
}

} // namespace render
