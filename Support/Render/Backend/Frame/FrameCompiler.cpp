//=========================================================================
//
//  FrameCompiler.cpp
//
//  Geometry submission and draw backend
//
//=========================================================================

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameCompiler.hpp"
#include "a51_shader_programs.hpp"
#include "x_workers.hpp"

#include "../../LightSystem.hpp"
#include "../../ProjectionTextureRegistry.hpp"
#include "../../RigidGeom.hpp"
#include "../../SkinGeom.hpp"
#include "../Storage/StaticGeometryStorage.hpp"
#include "../Storage/SkinnedGeometryStorage.hpp"
#include "../Resource/GeometryRepository.hpp"
#include "../../Features/PostProcess/PostProcessFeature.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

FrameCompiler::GeomResourceSnapshot::GeomResourceSnapshot( void )
    : pDiffuse( NULL ),
      pDetail( NULL ),
      pEnvironment( NULL ),
      pEnvironmentCube( NULL ),
      isDistortionActive( FALSE )
{
    DistortionNormalRot.Zero();
}

//=========================================================================

FrameCompiler::GeomDrawPacket::GeomDrawPacket( void )
    : Kind( GEOM_PACKET_RIGID ),
      pMaterial( NULL ),
      hMesh(),
      RenderFlags( 0 ),
      LightingMode( LocalLightingMode::Ambient ),
      Projection( FALSE ),
      Shadows( FALSE ),
      InstFeatures( 0 ),
      ContributionPolicy( render::RENDER_CONTRIBUTION_DEFAULT ),
      FirstInstance( 0 ),
      InstanceCount( 0 ),
      UOffset( 0 ),
      VOffset( 0 ),
      MaterialOverride( FALSE ),
      Pass( GEOM_PASS_FORWARD_OPAQUE ),
      Sequence( 0 ),
      SortDepth( 0.0f ),
      FirstSkinDrawInstance( 0 ),
      pSkinSections( NULL ),
      SkinSectionCount( 0 ),
      FirstDynamicVertex( 0 ),
      pDynamicGpuMesh( NULL ),
      FirstDynamicIndex( 0 ),
      DynamicIndexCount( 0 ),
      DynamicLightingIndex( 0 ),
      CoverageUsesTextureAlpha( FALSE ),
      GlowCapable( FALSE ),
      pDamageMask( NULL ),
      pDamageTexture( NULL ),
      pDamageUpload( NULL ),
      pDamageUploadPending( NULL ),
      DamageUploadX( 0 ),
      DamageUploadY( 0 ),
      DamageUploadWidth( 0 ),
      DamageUploadHeight( 0 ),
      Resources(),
      ScenePipelineIndex( -1 ),
      pDepthPipeline( NULL ),
      NativePipelineSerial( 0 ),
      SubmissionIndex( 0 )
{
    hMesh.Handle = HNULL;
}

//=========================================================================

FrameCompiler::ShaderBindingLayout::ShaderBindingLayout( void )
    : FrameConstantsVertex( 0xFFFFFFFFu ),
      FrameConstantsPixel( 0xFFFFFFFFu ),
      DynamicConstantsVertex( 0xFFFFFFFFu ),
      DamageTexturePixel( 0xFFFFFFFFu ),
      FogConstantsPixel( 0xFFFFFFFFu ),
      FogPalettePixel( 0xFFFFFFFFu ),
      ProjTexturesPixel( 0xFFFFFFFFu ),
      ShadowMatricesPixel( 0xFFFFFFFFu ),
      ShadowDataPixel( 0xFFFFFFFFu ),
      DiffuseTexturePixel( 0xFFFFFFFFu ),
      DetailTexturePixel( 0xFFFFFFFFu ),
      EnvironmentTexturePixel( 0xFFFFFFFFu ),
      EnvironmentCubeTexturePixel( 0xFFFFFFFFu ),
      FaceShadowTexturePixel( 0xFFFFFFFFu ),
      FaceShadowDepthTexturePixel( 0xFFFFFFFFu ),
      DistortionTexturePixel( 0xFFFFFFFFu ),
      ProjectionAtlasTexturePixel( 0xFFFFFFFFu ),
      AmbientVisibilityTexturePixel( 0xFFFFFFFFu ),
      InstanceDataVertex( 0xFFFFFFFFu ),
      AuxiliaryDataVertex( 0xFFFFFFFFu ),
      LightingDataPixel( 0xFFFFFFFFu ),
      ProjectionLightingDataPixel( 0xFFFFFFFFu ),
      AmbientDataPixel( 0xFFFFFFFFu ),
      BoneRemapDataVertex( 0xFFFFFFFFu )
{
}

//=========================================================================

namespace
{

//=========================================================================

xbool CreateSolidTexture( vram_texture&       Texture,
                          vram_texture_type   Type,
                          vram_texture_format Format,
                          const void*         pData,
                          u32                 DataSize,
                          const char*         pDebugName )
{
    vram_texture_desc Desc;
    Desc.Type       = Type;
    Desc.Width      = 1;
    Desc.Height     = 1;
    Desc.LayerCount = ( Type == VRAM_TEXTURE_TYPE_CUBE ) ? 6 : 1;
    Desc.MipCount   = 1;
    Desc.Format     = Format;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.pDebugName = pDebugName;

    if( !vram_CreateTexture( Texture, Desc ) )
    {
        return FALSE;
    }

    for( u32 Layer = 0; Layer < Desc.LayerCount; Layer++ )
    {
        vram_texture_upload_desc Upload;
        Upload.Region.Layer  = Layer;
        Upload.Region.Width  = 1;
        Upload.Region.Height = 1;
        Upload.pData         = pData;
        Upload.Size          = DataSize;
        Upload.RowPitch      = DataSize;
        Upload.SlicePitch    = DataSize;

        if( !vram_UploadTexture( Texture, Upload ) )
        {
            vram_DestroyTexture( Texture );
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool CreateSolidTexture( vram_texture& Texture, vram_texture_type Type, u32 Color, const char* pDebugName )
{
    return CreateSolidTexture( Texture, Type, VRAM_TEXTURE_FORMAT_RGBA8, &Color, sizeof( Color ), pDebugName );
}

//=========================================================================

xbool CreateNeutralDetailTexture( vram_texture& Texture )
{
    const u16 Color[4] = { 0x3800, 0x3800, 0x3800, 0x3800 };
    return CreateSolidTexture( Texture, VRAM_TEXTURE_TYPE_2D, VRAM_TEXTURE_FORMAT_RGBA16F, Color, sizeof( Color ),
                               "GeomNeutralDetail" );
}

//=========================================================================

u32 GrowBufferCapacity( u32 CurrentCapacity, u32 RequiredCount )
{
    u32 Capacity = MAX( CurrentCapacity, 256u );

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

xbool EnsureBuffer( rbuffer&    Buffer,
                    u32&        Capacity,
                    u32         RequiredCount,
                    u32         ElementStride,
                    const char* pDebugName,
                    u32         UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ )
{
    if( ( ElementStride == 0 ) || ( UsageFlags == 0 ) || ( RequiredCount > ( 0xffffffffu / ElementStride ) ) )
    {
        return FALSE;
    }

    const u32 MaxCapacity = FrameStorage::MAX_STREAM_BYTES / ElementStride;

    if( ( MaxCapacity == 0 ) || ( RequiredCount > MaxCapacity ) )
    {
        return FALSE;
    }

    u32 DesiredCapacity = GrowBufferCapacity( Capacity, MAX( RequiredCount, 1u ) );
    DesiredCapacity     = MIN( DesiredCapacity, MaxCapacity );

    if( Buffer && ( Capacity >= DesiredCapacity ) &&
        ( Buffer.Desc.Size >= DesiredCapacity * ElementStride ) && ( Buffer.Desc.Stride == ElementStride ) &&
        ( ( Buffer.Desc.UsageFlags & UsageFlags ) == UsageFlags ) )
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

xbool EnsureSequentialIndexBuffer( rbuffer&     Buffer,
                                   u32&         Capacity,
                                   u32          RequiredCount,
                                   const char*  pDebugName,
                                   u32          UsageFlags,
                                   xarray<u32>& IndexStorage )
{
    if( !( UsageFlags & RBUFFER_USAGE_VERTEX ) ||
        !EnsureBuffer( Buffer, Capacity, RequiredCount, sizeof( u32 ), pDebugName, UsageFlags ) )
    {
        return FALSE;
    }

    if( static_cast<u32>( IndexStorage.GetCount() ) < Capacity )
    {
        const s32 FirstIndex = IndexStorage.GetCount();
        IndexStorage.SetCount( static_cast<s32>( Capacity ) );

        for( s32 i = FirstIndex; i < IndexStorage.GetCount(); ++i )
        {
            IndexStorage[i] = static_cast<u32>( i );
        }
    }

    return TRUE;
}

//=========================================================================

template <typename T>
xbool ReserveArrayCapacity( xarray<T>& Storage, u64 RequiredCount, const char* pName )
{
    const u64 MaximumCount = FrameStorage::MAX_STREAM_BYTES / sizeof( T );

    if( RequiredCount > static_cast<u64>( 0x7fffffffu ) || ( RequiredCount > MaximumCount ) )
    {
        RENDER_THROW( "FrameCompiler", "%s exceeds bounded staging storage", pName );
        return FALSE;
    }

    const s32 Required = static_cast<s32>( RequiredCount );

    if( Storage.GetCapacity() < Required )
    {
        s32 Capacity = MAX( Storage.GetCapacity(), 256 );

        while( Capacity < Required )
        {
            if( Capacity > ( 0x7fffffff / 2 ) )
            {
                Capacity = Required;
                break;
            }

            Capacity *= 2;
        }

        Capacity = MIN( Capacity, static_cast<s32>( MaximumCount ) );

        if( Capacity < Required )
        {
            RENDER_THROW( "FrameCompiler", "%s exceeds bounded staging capacity", pName );
            return FALSE;
        }

        Storage.SetCapacity( Capacity );
    }

    return TRUE;
}

//=========================================================================

const char* GeomTextureSlotName( TextureSlot SlotIndex )
{
    switch( SlotIndex )
    {
        case TEXTURE_SLOT_DIFFUSE:
        {
            return "diffuse";
        }
        case TEXTURE_SLOT_DETAIL:
        {
            return "detail";
        }
        case TEXTURE_SLOT_ENVIRONMENT:
        {
            return "environment";
        }
        case TEXTURE_SLOT_ENVIRONMENT_CUBE:
        {
            return "environment cube";
        }
        default:
        {
            return "unknown";
        }
    }
}

//=========================================================================

const char* GeomTextureBindingName( TextureSlot SlotIndex )
{
    switch( SlotIndex )
    {
        case TEXTURE_SLOT_DIFFUSE:
        {
            return "txDiffuse";
        }
        case TEXTURE_SLOT_DETAIL:
        {
            return "txDetail";
        }
        case TEXTURE_SLOT_ENVIRONMENT:
        {
            return "txEnvironment";
        }
        case TEXTURE_SLOT_ENVIRONMENT_CUBE:
        {
            return "txEnvironmentCube";
        }
        default:
        {
            return NULL;
        }
    }
}

//=========================================================================

} // namespace

//=========================================================================
//  INITIALIZATION / SHUTDOWN
//=========================================================================

FrameCompiler::FrameCompiler( void )
    : m_PassFrameConstants(),
      m_PassFogConstants(),
      m_isInitialized( FALSE ),
      m_pContext( NULL ),
      m_RigidInstanceCapacity( 0 ),
      m_RigidInstanceIndexCapacity( 0 ),
      m_RigidColorCapacity( 0 ),
      m_RigidIndirectCapacity( 0 ),
      m_SkinInstanceCapacity( 0 ),
      m_SkinBoneCapacity( 0 ),
      m_SkinDrawInstanceCapacity( 0 ),
      m_SkinIndirectCapacity( 0 ),
      m_nForwardOpaqueGpuDraws( 0 ),
      m_nForwardOpaqueInstances( 0 ),
      m_nForwardOpaqueSkinSectionDraws( 0 ),
      m_nForwardOpaqueSubmittedIndices( 0 ),
      m_DynamicVertexCapacity( 0 ),
      m_DynamicIndexCapacity( 0 ),
      m_LightingDataCapacity( 0 ),
      m_RequestedLightCookieMask( 0 ),
      m_ActiveShaderKind( GEOM_SHADER_NONE ),
      m_pActiveSurfaceBindings( NULL ),
      m_ActiveSamplerPreset( RSTATE_SAMPLER_PRESET_MATERIAL_WRAP ),
      m_pDiffuseResource( NULL ),
      m_pDetailResource( NULL ),
      m_pEnvironmentResource( NULL ),
      m_pEnvironmentCubeResource( NULL ),
      m_pCachedFaceShadowResource( NULL ),
      m_pCachedFaceShadowDepthResource( NULL ),
      m_FaceShadowSamplerPreset( RSTATE_SAMPLER_PRESET_POINT_CLAMP ),
      m_areShadowMapConstantsValid( FALSE ),
      m_OrderedDrawJob(),
      m_OrderedDrawJobHandle( HNULL ),
      m_areOrderedDrawsPrepared( FALSE ),
      m_isOrderedDrawJobSubmitted( FALSE )
{
    ClearPackets();
    ResetTransientState();
}

//=========================================================================

void FrameCompiler::Init( const RenderBackendContext& Context )
{
    if( m_isInitialized )
    {
        return;
    }

    if( !Context.IsComplete() )
    {
        RENDER_THROW( "FrameCompiler", "backend services were not configured" );
        return;
    }

    m_pContext = &Context;

    x_DebugMsg( "FrameCompiler: Initializing shaders\n" );

    m_ActiveShaderKind               = GEOM_SHADER_NONE;
    m_pActiveSurfaceBindings         = NULL;
    m_ActiveSamplerPreset            = RSTATE_SAMPLER_PRESET_MATERIAL_WRAP;
    m_isDistortionStateActive        = FALSE;
    m_pDistortionSceneResource       = NULL;
    m_pAmbientVisibilityResource     = NULL;
    m_hasAmbientVisibility           = FALSE;
    m_pDiffuseResource               = NULL;
    m_pDetailResource                = NULL;
    m_pEnvironmentResource           = NULL;
    m_pEnvironmentCubeResource       = NULL;
    m_pCachedFaceShadowResource      = NULL;
    m_pCachedFaceShadowDepthResource = NULL;
    m_FaceShadowSamplerPreset        = RSTATE_SAMPLER_PRESET_POINT_CLAMP;
    m_ShadowMatrixBuffer             = rbuffer();
    m_ShadowDataBuffer               = rbuffer();
    m_areShadowMapConstantsValid     = FALSE;
    m_DistortionNormalRot.Zero();
    m_DefaultDistortionMaterial               = material();
    m_DefaultDistortionMaterial.m_Type        = Material_Distortion;
    m_DefaultDistortionMaterial.m_DetailScale = 1.0f;
    m_DefaultDistortionMaterial.m_FixedAlpha  = 0.0f;
    m_DefaultDistortionMaterial.Finalize();
    m_LightingDataCapacity     = 0;
    m_RequestedLightCookieMask = 0;

    m_OrderedDraws.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lDrawPackets.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS * 2 );
    m_ScenePipelineRequirements.SetCapacity( 256 );
    m_ScenePipelineSlots.SetCapacity( 512 );
    m_lRigidFrameInstances.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lRigidFrameLighting.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lRigidFrameColors.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_RigidInstanceIndices.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lSkinFrameInstances.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lSkinFrameLighting.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lSkinFrameBones.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS * 2 );
    m_lSkinDrawInstances.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lSkinIndirectCommands.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lSkinIndirectRuns.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lSkinRemapCache.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lSkinRemapSlots.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS * 2 );
    m_lRigidIndirectCommands.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lRigidIndirectRuns.SetCapacity( RENDER_MAX_GEOM_SUBMISSIONS );
    m_lFrameLighting.SetCapacity( RENDER_MAX_LIGHTING_SETS + 1 );
    m_lProjectionLighting.SetCapacity( RENDER_MAX_LIGHTING_SETS + 1 );
    m_lAmbientLighting.SetCapacity( RENDER_MAX_LIGHTING_SETS + 1 );
    m_lLightingRemap.SetCapacity( RENDER_MAX_LIGHTING_SETS + 1 );

    x_try;

        // Initialize in stages to identify the first failed dependency.
        const char* pFailedStage = NULL;

        if( !InitSamplers() )
        {
            pFailedStage = "InitSamplers";
        }
        else if( !CreateSolidTexture( m_WhiteTexture, VRAM_TEXTURE_TYPE_2D, 0xFFFFFFFF, "GeomWhite" ) )
        {
            pFailedStage = "CreateSolidTexture(GeomWhite)";
        }
        else if( !CreateNeutralDetailTexture( m_NeutralDetailTexture ) )
        {
            pFailedStage = "CreateNeutralDetailTexture";
        }
        else if( !CreateSolidTexture( m_BlackTexture, VRAM_TEXTURE_TYPE_2D, 0xFF000000, "GeomBlack" ) )
        {
            pFailedStage = "CreateSolidTexture(GeomBlack)";
        }
        else if( !CreateSolidTexture( m_BlackCubeTexture, VRAM_TEXTURE_TYPE_CUBE, 0xFF000000, "GeomBlackCube" ) )
        {
            pFailedStage = "CreateSolidTexture(GeomBlackCube)";
        }
        else if( !InitRigidShaders() )
        {
            pFailedStage = "InitRigidShaders";
        }
        else if( !InitSkinShaders() )
        {
            pFailedStage = "InitSkinShaders";
        }
        else if( !InitDynamicGeom() )
        {
            pFailedStage = "InitDynamicGeom";
        }
        else if( !InitShaderBindings() )
        {
            pFailedStage = "InitShaderBindings";
        }
        else if( !InitShadowMaps() )
        {
            pFailedStage = "InitShadowMaps";
        }

        if( pFailedStage )
        {
            Kill();
            RENDER_THROW( "FrameCompiler", "initialization failed at stage '%s'", pFailedStage );
            return;
        }

        m_isInitialized = TRUE;
        x_DebugMsg( "FrameCompiler: Shaders initialized successfully\n" );

    x_catch_begin;

        Kill();

    x_catch_end_ret;
}

//=========================================================================

void FrameCompiler::Kill( void )
{
    x_DebugMsg( "FrameCompiler: Shutting down shaders\n" );

    ClearPackets();
    m_lDrawPackets.Clear();
    m_ScenePipelineRequirements.Clear();
    m_ScenePipelineSlots.Clear();
    m_PreparedPacketPipelines.Clear();
    m_lRigidFrameInstances.Clear();
    m_lRigidFrameLighting.Clear();
    m_lRigidFrameColors.Clear();
    m_RigidInstanceIndices.Clear();
    m_lRigidIndirectCommands.Clear();
    m_ForwardCommands.Clear();
    rbuffer_Destroy( m_ForwardCommandBuffer );
    m_ForwardCommandCapacity = 0;
    m_lRigidIndirectRuns.Clear();
    m_lSkinFrameInstances.Clear();
    m_lSkinFrameLighting.Clear();
    m_lSkinFrameBones.Clear();
    m_SkinPoses.Clear();
    m_SkinPoseSlots.Clear();
    m_lSkinDrawInstances.Clear();
    m_lSkinIndirectCommands.Clear();
    m_lSkinIndirectRuns.Clear();
    m_lSkinRemapCache.Clear();
    m_lSkinRemapSlots.Clear();
    m_lFrameLighting.Clear();
    m_lProjectionLighting.Clear();
    m_lAmbientLighting.Clear();
    m_lLightingRemap.Clear();
    m_OrderedDraws.Clear();
    m_LightingDataCapacity = 0;
    KillShaderBindings();
    KillDynamicGeom();
    KillRigidShaders();
    KillSkinShaders();
    KillShadowMaps();
    vram_DestroyTexture( m_BlackCubeTexture );
    vram_DestroyTexture( m_BlackTexture );
    vram_DestroyTexture( m_WhiteTexture );
    vram_DestroyTexture( m_NeutralDetailTexture );
    KillSamplers();
    ResetTransientState();
    SetEnvironmentCubemap( NULL );
    m_pCachedFaceShadowResource      = NULL;
    m_pCachedFaceShadowDepthResource = NULL;
    m_areShadowMapConstantsValid     = FALSE;
    m_RequestedLightCookieMask       = 0;

    m_isInitialized = FALSE;
    x_DebugMsg( "FrameCompiler: Shaders shutdown complete\n" );
}

//=========================================================================
//  DRAW CONSTANTS
//=========================================================================

u32 FrameCompiler::BuildBatchStateFlags( u32 RenderFlags )
{
    const u32 BatchStateMask = render::WIREFRAME | render::WIREFRAME_NO_CULL | render::DEBUG_RENDER_PULSED |
                               render::FADING_ALPHA | render::GLOWING | render::FIRST_PERSON;
    return RenderFlags & BatchStateMask;
}

//=========================================================================

u32 FrameCompiler::BuildInstFlags( render::geom_inst_features InstFeatures )
{
    u32 ShaderFlags = 0;

    if( InstFeatures & render::GEOM_INST_FORCED_GLOW )
    {
        ShaderFlags |= GEOM_INST_FLAG_GLOWING;
    }

    if( InstFeatures & render::GEOM_INST_PROJ_LIGHT )
    {
        ShaderFlags |= GEOM_INST_FLAG_PROJ_LIGHT;
    }

    if( InstFeatures & render::GEOM_INST_PROJ_SHADOW )
    {
        ShaderFlags |= GEOM_INST_FLAG_PROJ_SHADOW;
    }

    if( InstFeatures & render::GEOM_INST_FILTER_LIGHT )
    {
        ShaderFlags |= GEOM_INST_FLAG_FILTER_LIGHT;
    }

    return ShaderFlags;
}

//=========================================================================

f32 FrameCompiler::BuildInstFadeAlpha( u32 RenderFlags, u8 Alpha )
{
    if( RenderFlags & render::FADING_ALPHA )
    {
        return static_cast<f32>( Alpha ) / 255.0f;
    }

    return 1.0f;
}

//=========================================================================

xbool FrameCompiler::PreparePassConstants( const RenderView* pView, u32 Width, u32 Height )
{
    const view* pSource = pView ? pView->GetSourceView() : NULL;

    if( !pSource || Width == 0 || Height == 0 || !m_pContext->pPostProcess )
    {
        return FALSE;
    }

    x_memset( &m_PassFrameConstants, 0, sizeof( m_PassFrameConstants ) );
    m_PassFrameConstants.View       = pSource->GetW2V();
    m_PassFrameConstants.Projection = pSource->GetV2C();
    pSource->GetZLimits( m_PassFrameConstants.NearZ, m_PassFrameConstants.FarZ );

    const vector3& Position = pSource->GetPosition();
    m_PassFrameConstants.CameraPosition.Set( Position.GetX(), Position.GetY(), Position.GetZ(), 1.0f );
    m_PassFrameConstants.FilterLightColor = BuildFilterLightColor();
    m_PassFrameConstants.DistortionParams.Set( 8.0f, 0.0f, 1.0f / Width, 1.0f / Height );
    m_PassFrameConstants.DistortionNormalMatrix = m_PassFrameConstants.View;
    m_pContext->pPostProcess->GetFogConstants( m_PassFogConstants );

    return TRUE;
}

//=========================================================================

GeomFrameConstants FrameCompiler::BuildFrameConstants( const material*       pMaterial,
                                                       u8                    UOffset,
                                                       u8                    VOffset,
                                                       u8                    OverrideMat,
                                                       const geom_pass_desc& PassData ) const
{
    ASSERT( ( PassData.TargetWidth > 0 ) && ( PassData.TargetHeight > 0 ) );
    const xbool        BOverrideMaterial   = ( OverrideMat != FALSE );
    const f32          kInvByte            = 1.0f / 255.0f;
    const f32          kDefaultDetailScale = 1.0f;
    GeomFrameConstants FrameData           = m_PassFrameConstants;

    const f32 DetailScale = pMaterial ? pMaterial->GetDetailScale() : kDefaultDetailScale;

    FrameData.UVAnim.Set( static_cast<f32>( UOffset ) * kInvByte, static_cast<f32>( VOffset ) * kInvByte, DetailScale,
                          0.0f );

    const MaterialDesc Material        = pMaterial ? pMaterial->GetDesc() : MaterialDesc();
    FrameData.SurfaceTwoSided          = Material.TwoSidedShading;
    FrameData.IlluminationUsesDiffuse  = Material.IlluminationUsesDiffuse;
    FrameData.CoverageUsesTextureAlpha = Material.AlphaBlend || Material.AlphaTest;
    FrameData.SurfacePadding[0] = FrameData.SurfacePadding[1] = 0;
    FrameData.AlphaRef                                        = pMaterial ? pMaterial->GetAlphaRef() : 0.0f;

    const f32 FixedAlpha    = ( !BOverrideMaterial && pMaterial ) ? pMaterial->GetFixedAlpha() : 0.0f;
    const f32 CubeIntensity = BOverrideMaterial ? 0.0f : ( pMaterial ? pMaterial->GetCubeMapIntensity() : 1.0f );
    FrameData.EnvParams.Set( FixedAlpha, CubeIntensity, m_pContext->PhongShadingEnabled ? 1.0f : 0.0f, 0.0f );
    const xbool ReceivesAO = m_hasAmbientVisibility && PassData.DepthMode != GEOM_DEPTH_PREPASS &&
                             ( OverrideMat || !pMaterial || pMaterial->WritesDepth() );
    FrameData.ScreenAOParams.Set( 1.0f / PassData.TargetWidth, 1.0f / PassData.TargetHeight,
                                  ReceivesAO ? 1.0f : 0.0f, 0.0f );

    if( m_isDistortionStateActive )
    {
        matrix4 DistortionRot( m_DistortionNormalRot );
        FrameData.DistortionNormalMatrix = FrameData.View * DistortionRot;
    }

    return FrameData;
}

//=========================================================================

vector4 FrameCompiler::BuildFilterLightColor( void )
{
    const xcolor FilterColor  = render::GetFilterLightColor();
    const f32    kFilterScale = 1.0f / 128.0f;

    vector4 Result;
    Result.Set( static_cast<f32>( FilterColor.R ) * kFilterScale, static_cast<f32>( FilterColor.G ) * kFilterScale,
                static_cast<f32>( FilterColor.B ) * kFilterScale, render::IsFilterLightEnabled() ? 1.0f : 0.0f );

    return Result;
}

//=========================================================================
//  SAMPLER RESOURCES
//=========================================================================

xbool FrameCompiler::InitSamplers( void )
{
    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        if( !rstate_CreateSampler( m_Samplers[i], static_cast<rstate_sampler_preset>( i ), "GeomSampler" ) )
        {
            KillSamplers();
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

void FrameCompiler::KillSamplers( void )
{
    for( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; i++ )
    {
        rstate_DestroySampler( m_Samplers[i] );
    }
}

//=========================================================================

const rstate_sampler* FrameCompiler::GetSampler( rstate_sampler_preset Preset ) const
{
    return Preset >= 0 && Preset < RSTATE_SAMPLER_PRESET_COUNT ? &m_Samplers[Preset] : NULL;
}

//=========================================================================

const FrameCompiler::ShaderBindingLayout* FrameCompiler::GetShaderBindings( GeomShaderKind Kind ) const
{
    switch( Kind )
    {
        case GEOM_SHADER_RIGID:
        {
            return m_pActiveSurfaceBindings;
        }
        case GEOM_SHADER_SKIN:
        {
            return m_pActiveSurfaceBindings;
        }
        case GEOM_SHADER_DYNAMIC:
        {
            return m_pActiveSurfaceBindings;
        }
        default:
        {
            return NULL;
        }
    }
}

//=========================================================================
//  SHADER RESOURCES
//=========================================================================

xbool FrameCompiler::ResolveShaderBindings( ShaderBindingLayout& Bindings,
                                            const shader&        Vertex,
                                            const shader&        Pixel,
                                            A51ResourceSemantic  Instance,
                                            A51ResourceSemantic  Auxiliary,
                                            A51ResourceSemantic  Remap )
{
    Bindings                      = ShaderBindingLayout();
    Bindings.FrameConstantsVertex = Vertex.ResourceSlots[A51Resource_cbFrameConstants];

    if( Instance != A51ResourceCount )
    {
        Bindings.InstanceDataVertex = Vertex.ResourceSlots[Instance];
    }

    if( Auxiliary != A51ResourceCount )
    {
        Bindings.AuxiliaryDataVertex = Vertex.ResourceSlots[Auxiliary];
    }

    if( Remap != A51ResourceCount )
    {
        Bindings.BoneRemapDataVertex = Vertex.ResourceSlots[Remap];
    }

    if( Bindings.FrameConstantsVertex == 0xFFFFFFFFu ||
        ( Instance != A51ResourceCount && Bindings.InstanceDataVertex == 0xFFFFFFFFu ) ||
        ( Remap != A51ResourceCount &&
          ( Bindings.BoneRemapDataVertex == 0xFFFFFFFFu || Bindings.AuxiliaryDataVertex == 0xFFFFFFFFu ) ) )
    {
        RENDER_THROW( "ShaderCatalog", "surface vertex program lacks required resources" );
        return FALSE;
    }

    Bindings.DynamicConstantsVertex      = Vertex.ResourceSlots[A51Resource_cbDynamicGeometry];
    Bindings.DamageTexturePixel          = Pixel.ResourceSlots[A51Resource_txDamage];
    Bindings.FrameConstantsPixel         = Pixel.ResourceSlots[A51Resource_cbFrameConstants];
    Bindings.FogConstantsPixel           = Pixel.ResourceSlots[A51Resource_cbFogConstants];
    Bindings.ProjTexturesPixel           = Pixel.ResourceSlots[A51Resource_cbProjTextures];
    Bindings.ShadowMatricesPixel         = Pixel.ResourceSlots[A51Resource_FaceShadowMatrices];
    Bindings.ShadowDataPixel             = Pixel.ResourceSlots[A51Resource_ShadowMapData];
    Bindings.LightingDataPixel           = Pixel.ResourceSlots[A51Resource_GeomLighting];
    Bindings.ProjectionLightingDataPixel = Pixel.ResourceSlots[A51Resource_GeomProjectionLighting];
    Bindings.AmbientDataPixel            = Pixel.ResourceSlots[A51Resource_GeomAmbient];
    Bindings.FogPalettePixel             = Pixel.ResourceSlots[A51Resource_FogPalette];
    Bindings.DiffuseTexturePixel         = Pixel.ResourceSlots[A51Resource_txDiffuse];
    Bindings.DetailTexturePixel          = Pixel.ResourceSlots[A51Resource_txDetail];
    Bindings.EnvironmentTexturePixel     = Pixel.ResourceSlots[A51Resource_txEnvironment];
    Bindings.EnvironmentCubeTexturePixel = Pixel.ResourceSlots[A51Resource_txEnvironmentCube];
    Bindings.FaceShadowTexturePixel      = Pixel.ResourceSlots[A51Resource_txFaceShadowAtlas];
    Bindings.FaceShadowDepthTexturePixel = Pixel.ResourceSlots[A51Resource_txFaceShadowDepthAtlas];
    Bindings.DistortionTexturePixel      = Pixel.ResourceSlots[A51Resource_txDistortionScene];
    Bindings.ProjectionAtlasTexturePixel = Pixel.ResourceSlots[A51Resource_txProjectionAtlas];
    Bindings.AmbientVisibilityTexturePixel = Pixel.ResourceSlots[A51Resource_txScreenAO];

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::InitShaderBindings( void )
{
    return TRUE;
}

//=========================================================================

void FrameCompiler::KillShaderBindings( void )
{
    m_pActiveSurfaceBindings = NULL;
}

//=========================================================================

FrameCompiler::SurfaceStage* FrameCompiler::LoadSurfaceStage( const char*  pFile,
                                                              u64          Key,
                                                              u64          ProgramId,
                                                              shader_stage ShaderStage,
                                                              xbool        IsPrewarm )
{
    for( s32 i = 0; i < m_SurfaceStages.GetCount(); ++i )
    {
        if( m_SurfaceStages[i]->Shader.ProgramId == ProgramId )
        {
            return m_SurfaceStages[i];
        }
    }

    SurfaceStage* pStage = new SurfaceStage;
    pStage->pFile        = pFile;

    if( !( IsPrewarm ? shader_RequestComboStage( pStage->Shader, pFile, SHADER_COMBO_KEYED, Key, ShaderStage )
                     : shader_LoadComboStage( pStage->Shader, pFile, SHADER_COMBO_KEYED, Key, ShaderStage ) ) ||
        pStage->Shader.ProgramId != ProgramId )
    {
        shader_Destroy( pStage->Shader );
        delete pStage;
        RENDER_THROW( "ShaderCatalog", "required catalog stage '%s' is missing or invalid", pFile );
        return NULL;
    }

    m_SurfaceStages.Append() = pStage;

    return pStage;
}

//=========================================================================

FrameCompiler::SurfaceProgram* FrameCompiler::GetSurfaceProgram( const ShaderProgramKey& Key, xbool IsPrewarm )
{
    u32       Begin    = 0;
    u32       End      = A51SurfaceProgramCount;
    const u64 Identity = Key.Hash();

    while( Begin < End )
    {
        const u32 Middle = Begin + ( End - Begin ) / 2;

        if( A51SurfacePrograms[Middle].Key < Identity )
        {
            Begin = Middle + 1;
        }
        else
        {
            End = Middle;
        }
    }

    if( Begin == A51SurfaceProgramCount || A51SurfacePrograms[Begin].Key != Identity )
    {
        RENDER_THROW( "ShaderCatalog", "surface program key %016llx is absent from catalog",
                      ( unsigned long long )Identity );
        return NULL;
    }

    if( m_SurfacePrograms[Begin] )
    {
        SurfaceProgram* pProgram = m_SurfacePrograms[Begin];

        if( !IsPrewarm && ( pProgram->pVertex->Shader.pLoad || pProgram->pPixel->Shader.pLoad ) )
        {
            RENDER_THROW( "ShaderCatalog", "surface program used before bytecode prewarm completed" );
            return NULL;
        }

        return pProgram;
    }

    SurfaceStage* pVertex = LoadSurfaceStage( A51SurfacePrograms[Begin].PackageFile, Identity,
                                              A51SurfacePrograms[Begin].VertexId, SHADER_STAGE_VERTEX, IsPrewarm );
    SurfaceStage* pPixel  = LoadSurfaceStage( A51SurfacePrograms[Begin].PackageFile, Identity,
                                              A51SurfacePrograms[Begin].PixelId, SHADER_STAGE_PIXEL, IsPrewarm );

    if( !pVertex || !pPixel )
    {
        return NULL;
    }

    SurfaceProgram* pProgram = new SurfaceProgram;
    pProgram->pVertex        = pVertex;
    pProgram->pPixel         = pPixel;

    const xbool Skin = Key.Vertex == VertexContract::Skin;

    if( !ResolveShaderBindings( pProgram->Bindings, pVertex->Shader, pPixel->Shader,
                                Key.Vertex == VertexContract::Dynamic ? A51ResourceCount
                                : Skin                                ? A51Resource_SkinInstances
                                                                      : A51Resource_RigidInstances,
                                Key.Vertex == VertexContract::Dynamic ? A51ResourceCount
                                : Skin                                ? A51Resource_SkinBoneMatrices
                                                                      : A51Resource_RigidVertexColors,
                                Skin ? A51Resource_SkinBoneRemaps : A51ResourceCount ) )
    {
        delete pProgram;
        return NULL;
    }

    m_SurfacePrograms[Begin] = pProgram;

    return pProgram;
}

//=========================================================================

void FrameCompiler::InitSurfacePrograms( void )
{
    m_SurfacePrograms.SetCount( A51SurfaceProgramCount );
    x_memset( m_SurfacePrograms.GetPtr(), 0, m_SurfacePrograms.GetCount() * sizeof( SurfaceProgram* ) );
}

//=========================================================================

void FrameCompiler::DestroySurfacePrograms( void )
{
    m_pActiveSurfaceBindings = NULL;

    for( s32 i = 0; i < m_SurfacePrograms.GetCount(); ++i )
    {
        delete m_SurfacePrograms[i];
        m_SurfacePrograms[i] = NULL;
    }

    for( s32 i = 0; i < m_SurfaceStages.GetCount(); ++i )
    {
        shader_Destroy( m_SurfaceStages[i]->Shader );
        delete m_SurfaceStages[i];
    }

    m_SurfaceStages.Clear();
}

//=========================================================================
//  RESOURCE BINDING
//=========================================================================

xbool FrameCompiler::PushFrameConstants( GeomShaderKind Kind, const GeomFrameConstants& Frame ) const
{
    const ShaderBindingLayout* pLayout = GetShaderBindings( Kind );

    if( !pLayout )
    {
        return FALSE;
    }

    if( pLayout->FrameConstantsVertex != 0xFFFFFFFFu &&
        !shader_PushUniformData( SHADER_STAGE_VERTEX, pLayout->FrameConstantsVertex, &Frame, sizeof( Frame ) ) )
    {
        return FALSE;
    }

    return pLayout->FrameConstantsPixel == 0xFFFFFFFFu ||
           shader_PushUniformData( SHADER_STAGE_PIXEL, pLayout->FrameConstantsPixel, &Frame, sizeof( Frame ) );
}

//=========================================================================

xbool FrameCompiler::PushFogConstants( GeomShaderKind Kind, const geom_pass_desc& PassData ) const
{
    const ShaderBindingLayout* pBindings    = GetShaderBindings( Kind );
    const PostProcessFeature*  pPostProcess = m_pContext->pPostProcess;

    if( pBindings && pBindings->FogConstantsPixel == 0xFFFFFFFFu )
    {
        return TRUE;
    }

    if( !pBindings || !pPostProcess || pBindings->FogPalettePixel == 0xFFFFFFFFu )
    {
        return FALSE;
    }

    RenderFogConstants Constants = m_PassFogConstants;
    Constants.SceneMode          = PassData.SceneFogMode;
    Constants.BloomMode          = PassData.BloomFogMode;

    if( !shader_PushUniformData( SHADER_STAGE_PIXEL, pBindings->FogConstantsPixel, &Constants, sizeof( Constants ) ) )
    {
        return FALSE;
    }

    return BindPixelTexture( pBindings->FogPalettePixel, pPostProcess->GetFogPaletteResource(),
                             pPostProcess->GetFogPaletteSampler() );
}

//=========================================================================

xbool FrameCompiler::BindPixelTexture( u32                    SlotIndex,
                                       const shader_resource* pResource,
                                       const rstate_sampler*  pSampler ) const
{
    if( SlotIndex == 0xFFFFFFFFu )
    {
        return TRUE;
    }

    if( !pResource || !*pResource || !pSampler || !*pSampler )
    {
        return FALSE;
    }

    return shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL, SlotIndex, pResource, pSampler ) );
}

//=========================================================================

FrameCompiler::GeomResourceSnapshot FrameCompiler::CaptureResourceSnapshot( void ) const
{
    GeomResourceSnapshot Resources;
    Resources.pDiffuse            = m_pDiffuseResource;
    Resources.pDetail             = m_pDetailResource;
    Resources.pEnvironment        = m_pEnvironmentResource;
    Resources.pEnvironmentCube    = m_pEnvironmentCubeResource;
    Resources.isDistortionActive  = m_isDistortionStateActive;
    Resources.DistortionNormalRot = m_DistortionNormalRot;

    return Resources;
}

//=========================================================================

xbool FrameCompiler::BindPacketResources( const GeomDrawPacket& Packet, xbool DepthOnly )
{
    const ShaderBindingLayout* pBindings            = GetShaderBindings( m_ActiveShaderKind );
    const rstate_sampler*      pMaterialSampler     = GetSampler( m_ActiveSamplerPreset );
    const shader_resource*     pWhiteResource       = vram_GetShaderResource( m_WhiteTexture );
    const shader_resource*     pBlackResource       = vram_GetShaderResource( m_BlackTexture );
    const shader_resource*     pBlackCubeResource   = vram_GetShaderResource( m_BlackCubeTexture );
    const shader_resource*     pFaceShadowResource  = m_pCachedFaceShadowResource ? m_pCachedFaceShadowResource : pWhiteResource;
    const shader_resource* pFaceShadowDepthResource = m_pCachedFaceShadowDepthResource ? m_pCachedFaceShadowDepthResource : pWhiteResource;
    const shader_resource* pProjectionAtlasResource = m_pContext->pProjection->GetShaderResource();
    const rstate_sampler*  pShadowSampler           = GetSampler( m_FaceShadowSamplerPreset );
    const rstate_sampler*  pShadowDepthSampler      = GetSampler( RSTATE_SAMPLER_PRESET_POINT_CLAMP );
    const rstate_sampler*  pProjectionSampler       = GetSampler( RSTATE_SAMPLER_PRESET_LINEAR_CLAMP );

    if( !pBindings )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->DiffuseTexturePixel,
                           Packet.Resources.pDiffuse ? Packet.Resources.pDiffuse : pWhiteResource, pMaterialSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->DetailTexturePixel,
                           Packet.Resources.pDetail ? Packet.Resources.pDetail
                                                    : vram_GetShaderResource( m_NeutralDetailTexture ),
                           pMaterialSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->EnvironmentTexturePixel,
                           Packet.Resources.pEnvironment ? Packet.Resources.pEnvironment : pBlackResource,
                           pMaterialSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->EnvironmentCubeTexturePixel,
                           Packet.Resources.pEnvironmentCube ? Packet.Resources.pEnvironmentCube : pBlackCubeResource,
                           pMaterialSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->FaceShadowTexturePixel, pFaceShadowResource, pShadowSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->FaceShadowDepthTexturePixel, pFaceShadowDepthResource, pShadowDepthSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->DistortionTexturePixel, m_pDistortionSceneResource, pMaterialSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->ProjectionAtlasTexturePixel, pProjectionAtlasResource, pProjectionSampler ) )
    {
        return FALSE;
    }

    if( !BindPixelTexture( pBindings->AmbientVisibilityTexturePixel,
                           m_pAmbientVisibilityResource ? m_pAmbientVisibilityResource : pWhiteResource,
                           pShadowDepthSampler ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  PACKET COLLECTION
//=========================================================================

void FrameCompiler::BeginPacketCollection( void )
{
    ClearPackets();
    m_nForwardOpaqueGpuDraws         = 0;
    m_nForwardOpaqueInstances        = 0;
    m_nForwardOpaqueSkinSectionDraws = 0;
    m_nForwardOpaqueSubmittedIndices = 0;
    ResetShadowTargets();
}

//=========================================================================

s32 FrameCompiler::CompareScenePackets( const void* pA, const void* pB )
{
    const GeomDrawPacket& A       = *static_cast<const GeomDrawPacket*>( pA );
    const GeomDrawPacket& B       = *static_cast<const GeomDrawPacket*>( pB );
    const xbool           OpaqueA = A.Pass == GEOM_PASS_FORWARD_OPAQUE && A.Kind != GEOM_PACKET_DYNAMIC &&
                          !( A.RenderFlags & render::FIRST_PERSON );
    const xbool OpaqueB = B.Pass == GEOM_PASS_FORWARD_OPAQUE && B.Kind != GEOM_PACKET_DYNAMIC &&
                          !( B.RenderFlags & render::FIRST_PERSON );

    if( OpaqueA != OpaqueB )
    {
        return OpaqueA ? -1 : 1;
    }

    if( OpaqueA )
    {
        if( A.NativePipelineSerial < B.NativePipelineSerial )
        {
            return -1;
        }

        if( A.NativePipelineSerial > B.NativePipelineSerial )
        {
            return 1;
        }
    }

    // Preserve material order within a PSO and submission order for other passes.
    if( A.SubmissionIndex < B.SubmissionIndex )
    {
        return -1;
    }

    if( A.SubmissionIndex > B.SubmissionIndex )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

s32 FrameCompiler::CompareForwardOpaqueDraws( const void* pA, const void* pB )
{
    const geom_draw_item& A = **static_cast<const geom_draw_item* const*>( pA );
    const geom_draw_item& B = **static_cast<const geom_draw_item* const*>( pB );

    if( A.MaterialOrder < B.MaterialOrder )
    {
        return -1;
    }

    if( A.MaterialOrder > B.MaterialOrder )
    {
        return 1;
    }

    if( A.Type < B.Type )
    {
        return -1;
    }

    if( A.Type > B.Type )
    {
        return 1;
    }

    const u32 StateA = BuildBatchStateFlags( A.RenderFlags );
    const u32 StateB = BuildBatchStateFlags( B.RenderFlags );

    if( StateA < StateB )
    {
        return -1;
    }

    if( StateA > StateB )
    {
        return 1;
    }

    if( A.UOffset < B.UOffset )
    {
        return -1;
    }

    if( A.UOffset > B.UOffset )
    {
        return 1;
    }

    if( A.VOffset < B.VOffset )
    {
        return -1;
    }

    if( A.VOffset > B.VOffset )
    {
        return 1;
    }

    if( A.MaterialOverride < B.MaterialOverride )
    {
        return -1;
    }

    if( A.MaterialOverride > B.MaterialOverride )
    {
        return 1;
    }

    if( A.ContributionPolicy < B.ContributionPolicy )
    {
        return -1;
    }

    if( A.ContributionPolicy > B.ContributionPolicy )
    {
        return 1;
    }

    if( A.hRenderGeom.Handle < B.hRenderGeom.Handle )
    {
        return -1;
    }

    if( A.hRenderGeom.Handle > B.hRenderGeom.Handle )
    {
        return 1;
    }

    if( A.iSurface < B.iSurface )
    {
        return -1;
    }

    if( A.iSurface > B.iSurface )
    {
        return 1;
    }

    if( A.Sequence < B.Sequence )
    {
        return -1;
    }

    if( A.Sequence > B.Sequence )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

void FrameCompiler::ConfigureDrawResources( const material*  pMaterial,
                                            geom_render_pass PassData,
                                            const radian3&   DistortionNormalRot,
                                            const cubemap*   pCubeMap )
{
    SetBitmap( NULL, TEXTURE_SLOT_DIFFUSE );
    SetBitmap( NULL, TEXTURE_SLOT_DETAIL );
    SetBitmap( NULL, TEXTURE_SLOT_ENVIRONMENT );
    SetEnvironmentCubemap( NULL );
    ResetTransientState();

    if( PassData == GEOM_PASS_ZPRIME )
    {
        return;
    }

    if( PassData == GEOM_PASS_DISTORTION )
    {
        if( pMaterial && pMaterial->IsDistortionPerPolyEnv() )
        {
            if( pMaterial->UsesCubeMap() )
            {
                SetEnvironmentCubemap( pCubeMap );
            }
            else
            {
                SetBitmap( pMaterial->m_EnvironmentMap.GetPointer(), TEXTURE_SLOT_ENVIRONMENT );
            }
        }

        SetDistortionState( DistortionNormalRot );
        return;
    }

    if( !pMaterial )
    {
        return;
    }

    SetBitmap( pMaterial->m_DiffuseMap.GetPointer(), TEXTURE_SLOT_DIFFUSE );
    SetBitmap( pMaterial->m_DetailMap.GetPointer(), TEXTURE_SLOT_DETAIL );

    if( pMaterial->UsesCubeMap() )
    {
        SetEnvironmentCubemap( pCubeMap );
    }
    else
    {
        SetBitmap( pMaterial->m_EnvironmentMap.GetPointer(), TEXTURE_SLOT_ENVIRONMENT );
    }
}

//=========================================================================
//  PACKET PREPARATION
//=========================================================================

void FrameCompiler::BuildOrderedDraws( void* pData )
{
    OrderedDrawJob& Job = *static_cast<OrderedDrawJob*>( pData );
    s32 DrawCount = 0;

    for( s32 i = 0; i < Job.DrawCount; ++i )
    {
        if( Job.pDraws[i].Pass == GEOM_PASS_FORWARD_OPAQUE )
        {
            Job.pOrderedDraws[DrawCount++] = &Job.pDraws[i];
        }
    }

    if( DrawCount > 1 )
    {
        x_qsort( Job.pOrderedDraws, DrawCount,
                 []( const geom_draw_item* pA, const geom_draw_item* pB )
                 { return CompareForwardOpaqueDraws( &pA, &pB ); } );
    }

    for( s32 i = 0; i < Job.DrawCount; ++i )
    {
        if( Job.pDraws[i].Pass != GEOM_PASS_FORWARD_OPAQUE )
        {
            Job.pOrderedDraws[DrawCount++] = &Job.pDraws[i];
        }
    }
}

//=========================================================================

xbool FrameCompiler::PrepareOrderedDraws( const xarray<geom_draw_item>& Draws )
{
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/ResetPackets" );

        BeginPacketCollection();
    }

    if( !ReserveArrayCapacity( m_OrderedDraws, Draws.GetCount(), "ordered geometry draws" ) )
    {
        return FALSE;
    }

    m_OrderedDraws.SetCount( Draws.GetCount() );
    m_OrderedDrawJob.pDraws        = Draws.GetPtr();
    m_OrderedDrawJob.pOrderedDraws = m_OrderedDraws.GetPtr();
    m_OrderedDrawJob.DrawCount     = Draws.GetCount();
    m_areOrderedDrawsPrepared     = TRUE;
    m_isOrderedDrawJobSubmitted   = ( Draws.GetCount() >= 256 ) && x_WorkersIsInit() &&
        x_WorkerJobSubmit( BuildOrderedDraws, &m_OrderedDrawJob, "Geom/CollectAndSort", m_OrderedDrawJobHandle );

    return TRUE;
}

//=========================================================================

void FrameCompiler::WaitForOrderedDraws( void )
{
    if( m_OrderedDrawJobHandle.IsNonNull() )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/WaitForDrawSort" );
        x_WorkerJobWait( m_OrderedDrawJobHandle );
        x_WorkerJobRelease( m_OrderedDrawJobHandle );
        m_OrderedDrawJobHandle = xhandle( HNULL );
    }

    m_areOrderedDrawsPrepared = FALSE;
}

//=========================================================================

xbool FrameCompiler::BuildPackets( const xarray<geom_draw_item>&    Draws,
                                   const xarray<dynamic_geom_draw>& DynamicDraws,
                                   const dynamic_geom_storage&      DynamicStorage,
                                   const cubemap*                   pCubeMap,
                                   const RenderFrameContext&        Frame )
{
    if( !m_areOrderedDrawsPrepared || ( m_OrderedDrawJob.pDraws != Draws.GetPtr() ) ||
        ( m_OrderedDrawJob.DrawCount != Draws.GetCount() ) )
    {
        if( !PrepareOrderedDraws( Draws ) )
        {
            return FALSE;
        }
    }

    m_DynamicStorage = DynamicStorage;

    // Shadow indices must be ready before packet program selection.
    const xbool Prepared = PrepareSharedShadowData( *Frame.pGraph ) && ReserveFrameArrays( Draws, DynamicDraws );

    WaitForOrderedDraws();

    if( !Prepared )
    {
        return FALSE;
    }

    if( !m_isOrderedDrawJobSubmitted )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/CollectAndSort" );
        BuildOrderedDraws( &m_OrderedDrawJob );
    }

    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/AssemblePackets" );

    xbool            BatchActive    = FALSE;
    geom_draw_type   BatchType      = GEOM_DRAW_RIGID;
    geom_render_pass BatchPass      = GEOM_PASS_FORWARD_OPAQUE;
    const material*  pBatchMaterial = NULL;
    u8               BatchOverride  = FALSE;
    u32              BatchSequence  = 0;

    for( s32 i = 0; i < m_OrderedDraws.GetCount(); ++i )
    {
        const geom_draw_item& Item      = *m_OrderedDraws[i];
        const material*       pMaterial = Item.pMaterial;

        if( ( Item.Pass == GEOM_PASS_DISTORTION ) && !pMaterial )
        {
            pMaterial = &m_DefaultDistortionMaterial;
        }

        const u8    MaterialOverride = Item.MaterialOverride;
        const xbool DepthSorted = ( Item.Pass == GEOM_PASS_TRANSPARENT ) || ( Item.Pass == GEOM_PASS_ADDITIVE ) ||
                                  ( Item.Pass == GEOM_PASS_FORCE_LAST ) || ( Item.Pass == GEOM_PASS_FADING ) ||
                                  ( Item.Pass == GEOM_PASS_DISTORTION );

        xbool CanAppend = BatchActive && !DepthSorted && ( BatchType == Item.Type ) && ( BatchPass == Item.Pass ) &&
                          ( pBatchMaterial == pMaterial ) && ( BatchOverride == MaterialOverride );

        RigidBatchDesc RigidBatch;
        SkinBatchDesc  SkinBatch;

        if( Item.Type == GEOM_DRAW_RIGID )
        {
            RigidBatch.pGeom        = Item.Data.Rigid.pGeom;
            RigidBatch.pL2W         = Item.Data.Rigid.pL2W;
            RigidBatch.Lighting     = Item.Lighting;
            RigidBatch.pColorInfo   = Item.Data.Rigid.pColorInfo;
            RigidBatch.hMesh        = m_pContext->pGeometry->GetRigidSurface( Item.hRenderGeom, Item.iSurface );
            RigidBatch.iSubMesh     = Item.iSurface;
            RigidBatch.RenderFlags  = Item.RenderFlags;
            RigidBatch.InstFeatures = Item.InstFeatures;

            const geom_inst_features ProjectionMask = GEOM_INST_PROJ_LIGHT | GEOM_INST_PROJ_SHADOW;

            if( RigidBatch.InstFeatures & ProjectionMask )
            {
                GeometryRepository::RigidMeshView Mesh;

                if( !RigidBatch.pL2W || !m_pContext->pGeometry->GetRigidMeshView( RigidBatch.hMesh, Mesh ) ||
                    !Mesh.HasLocalBounds )
                {
                    return FALSE;
                }

                bbox WorldBounds = Mesh.LocalBounds;
                WorldBounds.Transform( *RigidBatch.pL2W );

                // Object bounds are a broad phase; each surface can reject its own projections.
                const geom_inst_features SurfaceFeatures =
                    m_pContext->pProjections->CollectProjFeatures( Item.RenderFlags, WorldBounds );
                RigidBatch.InstFeatures &= ~ProjectionMask | SurfaceFeatures;
            }

            RigidBatch.UOffset            = Item.UOffset;
            RigidBatch.VOffset            = Item.VOffset;
            RigidBatch.Alpha              = Item.Alpha;
            RigidBatch.OverrideMat        = MaterialOverride;
            RigidBatch.SortDepth          = Item.SortDepth;
            RigidBatch.ContributionPolicy = Item.ContributionPolicy;
            CanAppend                     = CanAppend && CanAppendRigidBatch( RigidBatch );
        }
        else
        {
            SkinBatch.pGeom              = Item.Data.Skin.pGeom;
            SkinBatch.pBones             = Item.Data.Skin.pBones;
            SkinBatch.BoneCount          = Item.Data.Skin.BoneCount;
            SkinBatch.Lighting           = Item.Lighting;
            SkinBatch.hMesh              = m_pContext->pGeometry->GetSkinSurface( Item.hRenderGeom, Item.iSurface );
            SkinBatch.iSubMesh           = Item.iSurface;
            SkinBatch.RenderFlags        = Item.RenderFlags;
            SkinBatch.InstFeatures       = Item.InstFeatures;
            SkinBatch.UOffset            = Item.UOffset;
            SkinBatch.VOffset            = Item.VOffset;
            SkinBatch.Alpha              = Item.Alpha;
            SkinBatch.OverrideMat        = MaterialOverride;
            SkinBatch.SortDepth          = Item.SortDepth;
            SkinBatch.ContributionPolicy = Item.ContributionPolicy;
            CanAppend                    = CanAppend && CanAppendSkinBatch( SkinBatch );
        }

        if( BatchActive && !CanAppend )
        {
            if( BatchType == GEOM_DRAW_RIGID )
            {
                FlushRigidBatch( pBatchMaterial, BatchOverride, BatchPass, BatchSequence );
            }
            else
            {
                FlushSkinBatch( pBatchMaterial, BatchOverride, BatchPass, BatchSequence );
            }

            BatchActive = FALSE;
        }

        if( !BatchActive )
        {
            ConfigureDrawResources( pMaterial, Item.Pass, Item.DistortionNormalRot, pCubeMap );
            BatchType      = Item.Type;
            BatchPass      = Item.Pass;
            pBatchMaterial = pMaterial;
            BatchOverride  = MaterialOverride;
            BatchSequence  = Item.Sequence;
            BatchActive    = TRUE;
        }

        if( Item.Type == GEOM_DRAW_RIGID )
        {
            AddRigidBatchInst( RigidBatch );
        }
        else
        {
            AddSkinBatchInst( SkinBatch );
        }

        if( DepthSorted )
        {
            if( BatchType == GEOM_DRAW_RIGID )
            {
                FlushRigidBatch( pBatchMaterial, BatchOverride, BatchPass, BatchSequence );
            }
            else
            {
                FlushSkinBatch( pBatchMaterial, BatchOverride, BatchPass, BatchSequence );
            }

            BatchActive = FALSE;
        }
    }

    if( BatchActive )
    {
        if( BatchType == GEOM_DRAW_RIGID )
        {
            FlushRigidBatch( pBatchMaterial, BatchOverride, BatchPass, BatchSequence );
        }
        else
        {
            FlushSkinBatch( pBatchMaterial, BatchOverride, BatchPass, BatchSequence );
        }
    }

    ResetTransientState();

    if( !BuildDynamicPackets( DynamicDraws ) )
    {
        return FALSE;
    }

    // Resolve lighting before program selection and indirect run grouping.
    s32 DynamicIndex = 0;

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket& Packet = m_lDrawPackets[i];

        if( Packet.Kind == GEOM_PACKET_DYNAMIC )
        {
            if( DynamicIndex >= m_lDynamicFrameLighting.GetCount() )
            {
                return FALSE;
            }
        }
        else if( Packet.Kind == GEOM_PACKET_SKIN )
        {
            if( !Packet.InstanceCount || static_cast<u64>( Packet.FirstInstance ) + Packet.InstanceCount >
                                             static_cast<u32>( m_lSkinFrameLighting.GetCount() ) )
            {
                return FALSE;
            }
        }
        else
        {
            if( !Packet.InstanceCount || static_cast<u64>( Packet.FirstInstance ) + Packet.InstanceCount >
                                             static_cast<u32>( m_lRigidFrameLighting.GetCount() ) )
            {
                return FALSE;
            }
        }

        const RenderLightingHandle Handle = Packet.Kind == GEOM_PACKET_DYNAMIC ? m_lDynamicFrameLighting[DynamicIndex++]
                                            : Packet.Kind == GEOM_PACKET_SKIN
                                                ? m_lSkinFrameLighting[Packet.FirstInstance]
                                                : m_lRigidFrameLighting[Packet.FirstInstance];
        Packet.LightingMode               = ResolveLocalLighting( m_pContext->pLighting->Get( Handle ) );
        Packet.Shadows                    = FALSE;

        const u32 Count = Packet.Kind == GEOM_PACKET_DYNAMIC ? 1u : Packet.InstanceCount;

        for( u32 Instance = 0; Instance < Count && !Packet.Shadows; ++Instance )
        {
            const RenderLightingHandle Lighting = Packet.Kind == GEOM_PACKET_DYNAMIC ? Handle
                                                  : Packet.Kind == GEOM_PACKET_SKIN
                                                      ? m_lSkinFrameLighting[Packet.FirstInstance + Instance]
                                                      : m_lRigidFrameLighting[Packet.FirstInstance + Instance];
            Packet.Shadows = HasLocalShadows( m_pContext->pLighting->Get( Lighting ), m_DynamicLightShadowIndex,
                                              LightSystem::MAX_SHADOW_LIGHTS );
        }

        const u32 ShaderFlags = Packet.Kind == GEOM_PACKET_DYNAMIC ? BuildInstFlags( Packet.InstFeatures )
                                : Packet.Kind == GEOM_PACKET_SKIN
                                    ? m_lSkinFrameInstances[Packet.FirstInstance].ShaderFlags
                                    : m_lRigidFrameInstances[Packet.FirstInstance].ShaderFlags;
        Packet.Projection     = ( ShaderFlags & ( GEOM_INST_FLAG_PROJ_LIGHT | GEOM_INST_FLAG_PROJ_SHADOW ) ) != 0;
    }

    if( DynamicIndex != m_lDynamicFrameLighting.GetCount() )
    {
        return FALSE;
    }

    // Resolve both depth and shading pipelines before graph execution.
    const RenderResourceDesc* pDepthDesc = Frame.pGraph->GetDesc( Frame.GetSceneDepthTarget() );
    if( !pDepthDesc )
    {
        return FALSE;
    }

    u64 SlotCount = 512;

    while( SlotCount < static_cast<u64>( m_lDrawPackets.GetCount() ) * 2 )
    {
        SlotCount *= 2;
    }

    if( !ReserveArrayCapacity( m_ScenePipelineSlots, SlotCount, "scene pipeline slots" ) )
    {
        return FALSE;
    }

    m_ScenePipelineSlots.SetCount( static_cast<s32>( SlotCount ) );

    if( !ReserveArrayCapacity( m_PreparedPacketPipelines, m_lDrawPackets.GetCount(), "prepared packet pipelines" ) )
    {
        return FALSE;
    }

    m_PreparedPacketPipelines.SetCount( m_lDrawPackets.GetCount() );

    for( s32 Slot = 0; Slot < m_ScenePipelineSlots.GetCount(); ++Slot )
    {
        m_ScenePipelineSlots[Slot] = -1;
    }

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket& Packet = m_lDrawPackets[i];

        Packet.SubmissionIndex = i;
        Packet.GlowCapable     = ComputePacketGlowCapable( Packet );

        PreparedPacketPipelines& Prepared = m_PreparedPacketPipelines[i];

        Prepared = PreparedPacketPipelines();

        geom_pass_desc Pass;
        Pass.ColorFormat     = Frame.Desc.Formats.SceneColorFormat;
        Pass.DepthFormat     = pDepthDesc->Format;
        Pass.SampleCount     = Frame.Desc.SampleCount;
        Pass.EnableShadows   = Frame.Desc.EnableShadows && Packet.Shadows;
        const xbool EarlyDepth = Packet.Pass == GEOM_PASS_FORWARD_OPAQUE &&
                                 !( Packet.RenderFlags & render::FIRST_PERSON );
        Pass.DepthMode       = EarlyDepth ? GEOM_DEPTH_READ_ONLY : GEOM_DEPTH_DEFAULT;
        Pass.WriteSceneDepth = Packet.Pass == GEOM_PASS_FORWARD_OPAQUE && !EarlyDepth;
        Pass.SceneFogMode    = Packet.Pass != GEOM_PASS_FORWARD_OPAQUE && Packet.Pass != GEOM_PASS_DISTORTION
                                   ? RENDER_FOG_FORWARD_COLOR
                                   : RENDER_FOG_NONE;
        Pass.BloomFogMode    = Packet.ContributionPolicy != RENDER_CONTRIBUTION_SUPPRESS_BLOOM &&
                                    m_pContext->pPostProcess->GetBloomFeature().IsEnabled()
                                   ? RENDER_FOG_FORWARD_EMISSION
                                   : RENDER_FOG_NONE;

        const u32 Flags =
            Packet.RenderFlags | ( ( Packet.InstFeatures & GEOM_INST_FORCED_GLOW ) ? render::GLOWING : 0 );
        Packet.pDepthPipeline = NULL;
        if( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE &&
            ( Packet.Kind == GEOM_PACKET_DYNAMIC || Packet.MaterialOverride ||
                            !Packet.pMaterial || Packet.pMaterial->WritesDepth() ) )
        {
            geom_pass_desc DepthPass;
            DepthPass.ColorFormat   = RTARGET_FORMAT_R32F;
            DepthPass.DepthFormat   = Pass.DepthFormat;
            DepthPass.SampleCount   = Pass.SampleCount;
            DepthPass.DepthMode     = GEOM_DEPTH_PREPASS;
            DepthPass.EnableShadows = FALSE;
            DepthPass.WriteNormals  = Frame.SceneNormals.IsValid();

            RenderStateSelection DepthState;

            if( Packet.Kind == GEOM_PACKET_DYNAMIC )
            {
                DepthState = BuildDynamicPipelineState( Flags, DepthPass, Packet.LightingMode, Packet.Projection );
                Packet.pDepthPipeline = GetDynamicPipeline( DepthState );
            }
            else
            {
                DepthState = BuildMaterialPipelineState(
                    Packet.pMaterial, Flags, Packet.MaterialOverride, IsPacketGlowCapable( i ),
                    DepthPass, Packet.LightingMode, Packet.Projection );
                Packet.pDepthPipeline = Packet.Kind == GEOM_PACKET_SKIN ? GetSkinPipeline( DepthState )
                                                                        : GetRigidPipeline( DepthState );
                DepthState.Program.Vertex = Packet.Kind == GEOM_PACKET_SKIN ? VertexContract::Skin
                                                                            : VertexContract::Rigid;
            }

            if( !Packet.pDepthPipeline )
            {
                RENDER_THROW( "FrameCompiler", "failed to prepare depth pipeline for packet %d", i );
                return FALSE;
            }

            SurfaceProgram* pDepthProgram = GetSurfaceProgram( DepthState.Program );

            if( !pDepthProgram )
            {
                return FALSE;
            }

            Prepared.Depth.Pass      = DepthPass;
            Prepared.Depth.State     = DepthState;
            Prepared.Depth.pPipeline = Packet.pDepthPipeline;
            Prepared.Depth.pBindings = &pDepthProgram->Bindings;
        }

        RenderStateSelection MaterialState;
        render_pipeline*     pPipeline = NULL;

        if( Packet.Kind == GEOM_PACKET_DYNAMIC )
        {
            MaterialState = BuildDynamicPipelineState( Flags, Pass, Packet.LightingMode, Packet.Projection );
            pPipeline = GetDynamicPipeline( MaterialState );
        }
        else
        {
            MaterialState =
                BuildMaterialPipelineState( Packet.pMaterial, Flags, Packet.MaterialOverride, IsPacketGlowCapable( i ),
                                            Pass, Packet.LightingMode, Packet.Projection );
            MaterialState.Program.Vertex =
                Packet.Kind == GEOM_PACKET_SKIN ? VertexContract::Skin : VertexContract::Rigid;
            pPipeline =
                Packet.Kind == GEOM_PACKET_SKIN ? GetSkinPipeline( MaterialState ) : GetRigidPipeline( MaterialState );
        }

        if( !pPipeline )
        {
            return FALSE;
        }

        Packet.SceneProgram = MaterialState.Program;

        SurfaceProgram* pProgram = GetSurfaceProgram( MaterialState.Program );

        if( !pProgram )
        {
            return FALSE;
        }

        Prepared.Scene.Pass      = Pass;
        Prepared.Scene.State     = MaterialState;
        Prepared.Scene.pPipeline = pPipeline;
        Prepared.Scene.pBindings = &pProgram->Bindings;

        const u64 Hash     = render_GetPipelineSerial( *pPipeline );
        const u32 SlotMask = static_cast<u32>( m_ScenePipelineSlots.GetCount() - 1 );
        u32       Slot     = static_cast<u32>( Hash ^ ( Hash >> 32 ) ) & SlotMask;

        while( m_ScenePipelineSlots[Slot] >= 0 )
        {
            const ScenePipelineRequirement& Existing = m_ScenePipelineRequirements[m_ScenePipelineSlots[Slot]];

            if( Existing.NativeSerial == Hash )
            {
                break;
            }

            Slot = ( Slot + 1 ) & SlotMask;
        }

        Packet.ScenePipelineIndex = m_ScenePipelineSlots[Slot];

        if( Packet.ScenePipelineIndex < 0 )
        {
            if( !ReserveArrayCapacity( m_ScenePipelineRequirements, m_ScenePipelineRequirements.GetCount() + 1,
                                       "scene pipeline requirements" ) )
            {
                return FALSE;
            }

            Packet.ScenePipelineIndex = m_ScenePipelineRequirements.GetCount();

            ScenePipelineRequirement& Requirement = m_ScenePipelineRequirements.Append();
            Requirement.Hash                      = Hash;
            Requirement.MaterialState             = MaterialState;

            GraphicsPipelineCache&     Cache = Packet.Kind == GEOM_PACKET_DYNAMIC ? m_DynamicPipelines
                                               : Packet.Kind == GEOM_PACKET_SKIN  ? m_SkinPipelines
                                                                                  : m_RigidPipelines;
            const GraphicsPipelineKey* pKey  = NULL;
            const u64 State = GraphicsPipelineStateKey( MaterialState.ColorFormat, MaterialState.DepthFormat,
                                                        MaterialState.SampleCount, MaterialState.Blend,
                                                        MaterialState.Depth, MaterialState.Raster );

            if( !Cache.FindPrepared( MaterialState.Program, State, FALSE, &pKey ) || !pKey )
            {
                return FALSE;
            }

            Requirement.Key            = *pKey;
            Requirement.pPipeline      = pPipeline;
            Requirement.NativeSerial   = Hash;
            m_ScenePipelineSlots[Slot] = Packet.ScenePipelineIndex;
        }

        Packet.NativePipelineSerial = m_ScenePipelineRequirements[Packet.ScenePipelineIndex].NativeSerial;
    }

    if( m_lDrawPackets.GetCount() > 1 )
    {
        x_qsort( m_lDrawPackets.GetPtr(), m_lDrawPackets.GetCount(),
                 []( const GeomDrawPacket& A, const GeomDrawPacket& B )
                 { return CompareScenePackets( &A, &B ); } );
    }

    if( !BuildRigidIndirectRuns() || !BuildSkinDrawData() )
    {
        return FALSE;
    }

    // Publish bloom requirements after packet grouping.
    xbool HasForcedGlow = FALSE;

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        const GeomDrawPacket& PacketData = m_lDrawPackets[i];

        if( PacketData.ContributionPolicy != render::RENDER_CONTRIBUTION_SUPPRESS_BLOOM &&
            ( ( PacketData.RenderFlags & render::GLOWING ) ||
              ( PacketData.InstFeatures & render::GEOM_INST_FORCED_GLOW ) ) )
        {
            HasForcedGlow = TRUE;
            break;
        }
    }

    m_pContext->pPostProcess->SetFrameHasForcedGlow( HasForcedGlow );

    return ReserveUploadBuffers();
}

//=========================================================================
//  FRAME STORAGE
//=========================================================================

void FrameCompiler::BeginFrameStorage( void )
{
    if( !m_pContext || !m_pContext->pStorage )
    {
        return;
    }

    m_RigidInstanceCapacity      = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_RIGID_INSTANCES );
    m_RigidInstanceIndexCapacity = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_RIGID_INSTANCE_INDICES );
    m_RigidColorCapacity         = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_RIGID_COLORS );
    m_RigidIndirectCapacity      = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_RIGID_INDIRECT );
    m_SkinInstanceCapacity       = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_SKIN_INSTANCES );
    m_SkinBoneCapacity           = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_SKIN_BONES );
    m_SkinDrawInstanceCapacity   = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_SKIN_DRAW_INSTANCES );
    m_SkinIndirectCapacity       = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_SKIN_INDIRECT );
    m_DynamicVertexCapacity      = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_DYNAMIC_VERTICES );
    m_DynamicIndexCapacity       = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_DYNAMIC_INDICES );
    m_LightingDataCapacity       = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_LIGHTING );
    m_ProjectionLightingCapacity = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_PROJECTION_LIGHTING );
    m_AmbientLightingCapacity    = m_pContext->pStorage->GetGpuCapacity( FrameStorage::STREAM_AMBIENT_LIGHTING );
}

//=========================================================================

void FrameCompiler::CommitFrameBufferCapacities( void ) const
{
    if( !m_pContext || !m_pContext->pStorage )
    {
        return;
    }

    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_RIGID_INSTANCES, m_RigidInstanceCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_RIGID_INSTANCE_INDICES, m_RigidInstanceIndexCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_RIGID_COLORS, m_RigidColorCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_RIGID_INDIRECT, m_RigidIndirectCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_SKIN_INSTANCES, m_SkinInstanceCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_SKIN_BONES, m_SkinBoneCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_SKIN_DRAW_INSTANCES, m_SkinDrawInstanceCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_SKIN_INDIRECT, m_SkinIndirectCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_DYNAMIC_VERTICES, m_DynamicVertexCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_DYNAMIC_INDICES, m_DynamicIndexCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_LIGHTING, m_LightingDataCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_PROJECTION_LIGHTING, m_ProjectionLightingCapacity );
    m_pContext->pStorage->SetGpuCapacity( FrameStorage::STREAM_AMBIENT_LIGHTING, m_AmbientLightingCapacity );
}

//=========================================================================
//  INDIRECT DRAW PREPARATION
//=========================================================================

xbool FrameCompiler::CanAppendRigidIndirectRun( const RigidIndirectRun& Run,
                                                u32                     PacketIndex,
                                                const GeomDrawPacket&   Packet ) const
{
    if( ( Run.CommandCount == 0 ) || ( ( Run.LastPacket + 1 ) != PacketIndex ) ||
        ( Run.VertexPool != Packet.Range.VertexPool ) || ( Run.IndexPool != Packet.Range.IndexPool ) )
    {
        return FALSE;
    }

    const GeomDrawPacket& First = m_lDrawPackets[Run.FirstPacket];
    return ( First.Kind == GEOM_PACKET_RIGID ) && ( Packet.Kind == GEOM_PACKET_RIGID ) &&
           ( First.Pass == GEOM_PASS_FORWARD_OPAQUE ) && ( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE ) &&
           ( First.pMaterial == Packet.pMaterial ) && ( First.ScenePipelineIndex >= 0 ) &&
           ( First.ScenePipelineIndex == Packet.ScenePipelineIndex ) &&
           ( BuildBatchStateFlags( First.RenderFlags ) == BuildBatchStateFlags( Packet.RenderFlags ) ) &&
           ( ( ( First.InstFeatures ^ Packet.InstFeatures ) & render::GEOM_INST_FORCED_GLOW ) == 0 ) &&
           ( First.UOffset == Packet.UOffset ) && ( First.VOffset == Packet.VOffset ) &&
           ( First.MaterialOverride == Packet.MaterialOverride ) &&
           ( First.Resources.pDiffuse == Packet.Resources.pDiffuse ) &&
           ( First.Resources.pDetail == Packet.Resources.pDetail ) &&
           ( First.Resources.pEnvironment == Packet.Resources.pEnvironment ) &&
           ( First.Resources.pEnvironmentCube == Packet.Resources.pEnvironmentCube ) &&
           ( First.Resources.isDistortionActive == Packet.Resources.isDistortionActive ) &&
           ( First.ContributionPolicy == Packet.ContributionPolicy ) &&
           ( IsPacketGlowCapable( Run.FirstPacket ) == IsPacketGlowCapable( PacketIndex ) );
}

//=========================================================================

xbool FrameCompiler::BuildRigidIndirectRuns( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildIndirectRuns" );

    m_lRigidIndirectCommands.SetCount( 0 );
    m_lRigidIndirectRuns.SetCount( 0 );

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket& Packet = m_lDrawPackets[i];

        if( Packet.Kind != GEOM_PACKET_RIGID )
        {
            continue;
        }

        if( !m_pContext->pGeometry->GetRigidMeshView( Packet.hMesh, Packet.Range ) )
        {
            return FALSE;
        }

        if( ( Packet.FirstInstance + Packet.InstanceCount ) > static_cast<u32>( m_lRigidFrameInstances.GetCount() ) )
        {
            return FALSE;
        }

        if( ( Packet.Pass != GEOM_PASS_FORWARD_OPAQUE ) || ( Packet.RenderFlags & render::FIRST_PERSON ) )
        {
            continue;
        }

        s32 RunIndex = -1;

        if( m_lRigidIndirectRuns.GetCount() > 0 )
        {
            RigidIndirectRun& Last = m_lRigidIndirectRuns[m_lRigidIndirectRuns.GetCount() - 1];

            if( CanAppendRigidIndirectRun( Last, i, Packet ) )
            {
                RunIndex = m_lRigidIndirectRuns.GetCount() - 1;
            }
        }

        if( RunIndex < 0 )
        {
            RigidIndirectRun& Run = m_lRigidIndirectRuns.Append();
            Run.FirstPacket       = i;
            Run.LastPacket        = i;
            Run.FirstCommand      = m_lRigidIndirectCommands.GetCount();
            Run.CommandCount      = 0;
            Run.VertexPool        = Packet.Range.VertexPool;
            Run.IndexPool         = Packet.Range.IndexPool;
            RunIndex              = m_lRigidIndirectRuns.GetCount() - 1;
        }

        RigidIndirectRun&               Run     = m_lRigidIndirectRuns[RunIndex];
        rdraw_indexed_indirect_command& Command = m_lRigidIndirectCommands.Append();
        Command.IndexCount                      = Packet.Range.IndexCount;
        Command.InstanceCount                   = Packet.InstanceCount;
        Command.FirstIndex                      = Packet.Range.FirstIndex;
        Command.BaseVertex                      = Packet.Range.BaseVertex;
        Command.FirstInstance                   = Packet.FirstInstance;

        Run.LastPacket = i;
        Run.CommandCount++;
    }

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::CanAppendSkinIndirectRun( const SkinIndirectRun& Run,
                                               u32                    PacketIndex,
                                               const GeomDrawPacket&  Packet ) const
{
    if( ( Run.CommandCount == 0 ) || ( Run.VertexPool != Packet.Range.VertexPool ) ||
        ( Run.IndexPool != Packet.Range.IndexPool ) )
    {
        return FALSE;
    }

    const GeomDrawPacket& First = m_lDrawPackets[Run.FirstPacket];
    return ( First.Kind == GEOM_PACKET_SKIN ) && ( Packet.Kind == GEOM_PACKET_SKIN ) &&
           ( First.Pass == GEOM_PASS_FORWARD_OPAQUE ) && ( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE ) &&
           ( First.pMaterial == Packet.pMaterial ) && ( First.ScenePipelineIndex >= 0 ) &&
           ( First.ScenePipelineIndex == Packet.ScenePipelineIndex ) &&
           ( BuildBatchStateFlags( First.RenderFlags ) == BuildBatchStateFlags( Packet.RenderFlags ) ) &&
           ( ( ( First.InstFeatures ^ Packet.InstFeatures ) & render::GEOM_INST_FORCED_GLOW ) == 0 ) &&
           ( First.UOffset == Packet.UOffset ) && ( First.VOffset == Packet.VOffset ) &&
           ( First.MaterialOverride == Packet.MaterialOverride ) &&
           ( First.Resources.pDiffuse == Packet.Resources.pDiffuse ) &&
           ( First.Resources.pDetail == Packet.Resources.pDetail ) &&
           ( First.Resources.pEnvironment == Packet.Resources.pEnvironment ) &&
           ( First.Resources.pEnvironmentCube == Packet.Resources.pEnvironmentCube ) &&
           ( First.Resources.isDistortionActive == Packet.Resources.isDistortionActive ) &&
           ( First.ContributionPolicy == Packet.ContributionPolicy ) &&
           ( IsPacketGlowCapable( Run.FirstPacket ) == IsPacketGlowCapable( PacketIndex ) );
}

//=========================================================================

xbool FrameCompiler::BuildSkinDrawData( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildSkinDrawData" );

    m_lSkinDrawInstances.SetCount( 0 );
    m_lSkinIndirectCommands.SetCount( 0 );
    m_lSkinIndirectRuns.SetCount( 0 );
    m_lSkinRemapCache.SetCount( 0 );
    m_lSkinRemapSlots.SetCount( 0 );

    u32 SkinPacketCount = 0;

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        if( m_lDrawPackets[i].Kind == GEOM_PACKET_SKIN )
        {
            ++SkinPacketCount;
        }
    }

    const u64 RequestedCacheSize = MAX( 8ull, static_cast<u64>( SkinPacketCount ) * 2ull );
    u32       CacheSize          = 8;

    while( static_cast<u64>( CacheSize ) < RequestedCacheSize )
    {
        if( CacheSize > ( 0x7fffffffu / 2u ) )
        {
            RENDER_THROW( "FrameCompiler", "skin remap cache exceeds backend limit" );
            return FALSE;
        }

        CacheSize *= 2u;
    }

    m_lSkinRemapSlots.SetCount( static_cast<s32>( CacheSize ) );

    for( u32 i = 0; i < CacheSize; ++i )
    {
        m_lSkinRemapSlots[static_cast<s32>( i )] = -1;
    }

    u64 TotalDrawInstances    = 0;
    u64 TotalIndirectCommands = 0;

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket& Packet = m_lDrawPackets[i];

        if( Packet.Kind != GEOM_PACKET_SKIN )
        {
            continue;
        }

        const u32 Hash      = static_cast<u32>( Packet.hMesh.Handle ) * 2654435761u;
        s32       SlotIndex = static_cast<s32>( Hash & ( CacheSize - 1u ) );

        while( ( m_lSkinRemapSlots[SlotIndex] >= 0 ) &&
               ( m_lSkinRemapCache[m_lSkinRemapSlots[SlotIndex]].MeshHandle != Packet.hMesh.Handle ) )
        {
            SlotIndex = static_cast<s32>( ( static_cast<u32>( SlotIndex ) + 1u ) & ( CacheSize - 1u ) );
        }

        s32 CacheIndex = m_lSkinRemapSlots[SlotIndex];

        if( CacheIndex < 0 )
        {
            SkinRemapCacheEntry& EntryData = m_lSkinRemapCache.Append();
            EntryData.MeshHandle           = Packet.hMesh.Handle;

            if( !m_pContext->pGeometry->GetSkinMeshView( Packet.hMesh, EntryData.View ) ||
                ( EntryData.View.SectionCount <= 0 ) || !EntryData.View.pSections )
            {
                RENDER_THROW( "FrameCompiler", "failed to resolve skin mesh for packet %d", i );
                return FALSE;
            }

            EntryData.FirstRemap         = EntryData.View.FirstBoneRemap;
            CacheIndex                   = m_lSkinRemapCache.GetCount() - 1;
            m_lSkinRemapSlots[SlotIndex] = CacheIndex;
        }

        const SkinRemapCacheEntry& EntryData = m_lSkinRemapCache[CacheIndex];
        Packet.Range                         = EntryData.View.Geom;
        Packet.pSkinSections                 = EntryData.View.pSections;
        Packet.SkinSectionCount              = EntryData.View.SectionCount;

        if( ( Packet.FirstInstance + Packet.InstanceCount ) > static_cast<u32>( m_lSkinFrameInstances.GetCount() ) )
        {
            RENDER_THROW( "FrameCompiler", "skin packet %d has invalid instances %u+%u/%d", i, Packet.FirstInstance,
                          Packet.InstanceCount, m_lSkinFrameInstances.GetCount() );
            return FALSE;
        }

        TotalDrawInstances += static_cast<u64>( EntryData.View.SectionCount ) * Packet.InstanceCount;

        if( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE )
        {
            TotalIndirectCommands += static_cast<u64>( EntryData.View.SectionCount );
        }

        if( ( TotalDrawInstances > 0x7fffffffu ) || ( TotalIndirectCommands > 0x7fffffffu ) )
        {
            RENDER_THROW( "FrameCompiler", "skin frame storage exceeds backend limit" );
            return FALSE;
        }
    }

    if( !ReserveArrayCapacity( m_lSkinDrawInstances, TotalDrawInstances, "skin draw instances" ) ||
        !ReserveArrayCapacity( m_lSkinIndirectCommands, TotalIndirectCommands, "skin indirect commands" ) ||
        !ReserveArrayCapacity( m_lSkinIndirectRuns, TotalIndirectCommands, "skin indirect runs" ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        GeomDrawPacket& Packet = m_lDrawPackets[i];

        if( Packet.Kind != GEOM_PACKET_SKIN )
        {
            continue;
        }

        const u32 Hash      = static_cast<u32>( Packet.hMesh.Handle ) * 2654435761u;
        s32       SlotIndex = static_cast<s32>( Hash & ( CacheSize - 1u ) );

        while( m_lSkinRemapSlots[SlotIndex] >= 0 &&
               m_lSkinRemapCache[m_lSkinRemapSlots[SlotIndex]].MeshHandle != Packet.hMesh.Handle )
        {
            SlotIndex = static_cast<s32>( ( static_cast<u32>( SlotIndex ) + 1u ) & ( CacheSize - 1u ) );
        }

        const s32 CacheIndex = m_lSkinRemapSlots[SlotIndex];

        if( CacheIndex < 0 )
        {
            return FALSE;
        }

        const SkinRemapCacheEntry& EntryData = m_lSkinRemapCache[CacheIndex];
        Packet.FirstSkinDrawInstance         = m_lSkinDrawInstances.GetCount();

        for( s32 S = 0; S < EntryData.View.SectionCount; ++S )
        {
            const SkinnedGeometryStorage::SoftSection& Section = EntryData.View.pSections[S];
            const u32 BoneRemapOffset   = EntryData.FirstRemap + S * SkinnedGeometryStorage::MAX_BONE_PALETTE;
            const u32 FirstDrawInstance = m_lSkinDrawInstances.GetCount();

            for( u32 j = 0; j < Packet.InstanceCount; ++j )
            {
                SkinDrawInstance& DrawInstance = m_lSkinDrawInstances.Append();
                DrawInstance.InstanceIndex     = Packet.FirstInstance + j;
                DrawInstance.BoneRemapOffset   = BoneRemapOffset;
            }

            if( ( Packet.Pass != GEOM_PASS_FORWARD_OPAQUE ) || ( Packet.RenderFlags & render::FIRST_PERSON ) )
            {
                continue;
            }

            s32 RunIndex = -1;

            if( m_lSkinIndirectRuns.GetCount() > 0 )
            {
                SkinIndirectRun& Last = m_lSkinIndirectRuns[m_lSkinIndirectRuns.GetCount() - 1];

                if( CanAppendSkinIndirectRun( Last, i, Packet ) )
                {
                    RunIndex = m_lSkinIndirectRuns.GetCount() - 1;
                }
            }

            if( RunIndex < 0 )
            {
                SkinIndirectRun& Run = m_lSkinIndirectRuns.Append();
                Run.FirstPacket      = i;
                Run.FirstCommand     = m_lSkinIndirectCommands.GetCount();
                Run.CommandCount     = 0;
                Run.VertexPool       = Packet.Range.VertexPool;
                Run.IndexPool        = Packet.Range.IndexPool;
                RunIndex             = m_lSkinIndirectRuns.GetCount() - 1;
            }

            SkinIndirectRun&                Run     = m_lSkinIndirectRuns[RunIndex];
            rdraw_indexed_indirect_command& Command = m_lSkinIndirectCommands.Append();
            Command.IndexCount                      = Section.IndexCount;
            Command.InstanceCount                   = Packet.InstanceCount;
            Command.FirstIndex                      = EntryData.View.Geom.FirstIndex + Section.FirstIndex;
            Command.BaseVertex                      = EntryData.View.Geom.BaseVertex;
            Command.FirstInstance                   = FirstDrawInstance;
            Run.CommandCount++;
        }
    }

    return TRUE;
}

//=========================================================================
//  PROJECTION RESOURCES
//=========================================================================

xbool FrameCompiler::RequestLightCookies( const xarray<RenderLightingHandle>& Lighting )
{
    for( s32 i = 0; i < Lighting.GetCount(); ++i )
    {
        const cb_geom_lighting* pLighting = m_pContext->pLighting->Get( Lighting[i] );

        if( !pLighting )
        {
            continue;
        }

        const s32 LightCount = MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );

        for( s32 j = 0; j < LightCount; ++j )
        {
            const s32 CookieIndex = static_cast<s32>( pLighting->LightCookieU[j].GetW() ) - 1;

            if( CookieIndex < 0 )
            {
                continue;
            }

            if( CookieIndex >= m_pContext->pLights->GetLightCookieCount() )
            {
                return FALSE;
            }

            if( CookieIndex >= 64 )
            {
                return FALSE;
            }

            const u64 CookieBit = 1ull << static_cast<u32>( CookieIndex );

            if( m_RequestedLightCookieMask & CookieBit )
            {
                continue;
            }

            const texture::handle& Texture = m_pContext->pLights->GetLightCookieHandle( CookieIndex );

            if( !Texture.GetPointer() ||
                !m_pContext->pProjection->Request( Texture, ProjectionAtlasEncoding::LuminanceAlpha ) )
            {
                return FALSE;
            }

            m_RequestedLightCookieMask |= CookieBit;
        }
    }

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::PrepareProjectionAtlas( FrameGraphBuilder& Graph )
{
    m_hasProjectionConstants   = FALSE;
    m_RequestedLightCookieMask = 0;

    const s32 nProjLights = m_pContext->pProjections->GetProjLightCount();

    for( s32 i = 0; i < nProjLights; ++i )
    {
        const texture::handle& Texture = m_pContext->pProjections->GetProjLightTexture( i );

        if( !Texture.GetPointer() || !m_pContext->pProjection->Request( Texture, ProjectionAtlasEncoding::BlueMask ) )
        {
            RENDER_THROW( "FrameCompiler", "failed to request projected light texture index=%d", i );
            return FALSE;
        }
    }

    const s32 nProjShadows = m_pContext->pProjections->GetProjShadowCount();

    for( s32 i = 0; i < nProjShadows; ++i )
    {
        const texture::handle& Texture = m_pContext->pProjections->GetProjShadowTexture( i );

        if( !Texture.GetPointer() || !m_pContext->pProjection->Request( Texture, ProjectionAtlasEncoding::BlueMask ) )
        {
            RENDER_THROW( "FrameCompiler", "failed to request projected shadow texture index=%d", i );
            return FALSE;
        }
    }

    if( !RequestLightCookies( m_lRigidFrameLighting ) || !RequestLightCookies( m_lSkinFrameLighting ) ||
        !RequestLightCookies( m_lDynamicFrameLighting ) )
    {
        RENDER_THROW( "FrameCompiler", "failed to request light-cookie texture" );
        return FALSE;
    }

    if( !m_pContext->pProjection->Prepare( Graph ) )
    {
        RENDER_THROW( "FrameCompiler", "projection atlas preparation failed lights=%d shadows=%d", nProjLights,
                      nProjShadows );
        return FALSE;
    }

    return PrepareProjectionConstants();
}

//=========================================================================
//  PACKET STATE
//=========================================================================

void FrameCompiler::ClearPackets( void )
{
    m_hasProjectionConstants = FALSE;

    WaitForOrderedDraws();
    m_OrderedDrawJob             = OrderedDrawJob();
    m_areOrderedDrawsPrepared    = FALSE;
    m_isOrderedDrawJobSubmitted  = FALSE;
    m_DynamicStorage = dynamic_geom_storage();
    m_OrderedDraws.SetCount( 0 );
    m_lDrawPackets.SetCount( 0 );
    m_ScenePipelineRequirements.SetCount( 0 );
    m_ScenePipelineSlots.SetCount( 0 );
    m_PreparedPacketPipelines.SetCount( 0 );
    m_lRigidFrameInstances.SetCount( 0 );
    m_lRigidFrameLighting.SetCount( 0 );
    m_lRigidFrameColors.SetCount( 0 );
    m_lSkinFrameInstances.SetCount( 0 );
    m_lSkinFrameLighting.SetCount( 0 );
    m_lDynamicFrameLighting.SetCount( 0 );
    m_lSkinFrameBones.SetCount( 0 );
    m_SkinPoses.SetCount( 0 );
    m_SkinPoseSlots.SetCount( 0 );
    m_lSkinDrawInstances.SetCount( 0 );
    m_lSkinIndirectCommands.SetCount( 0 );
    m_lSkinIndirectRuns.SetCount( 0 );
    m_lSkinRemapCache.SetCount( 0 );
    m_lSkinRemapSlots.SetCount( 0 );
    m_lFrameLighting.SetCount( 0 );
    m_lProjectionLighting.SetCount( 0 );
    m_lAmbientLighting.SetCount( 0 );
    m_lLightingRemap.SetCount( 0 );
    m_lRigidIndirectCommands.SetCount( 0 );
    m_lRigidIndirectRuns.SetCount( 0 );
    m_nLitInstances   = 0;
    m_nInstanceLights = 0;
    ResetRigidBatch();
    ResetSkinBatch();
}

//=========================================================================

s32 FrameCompiler::GetOpaquePacketCount( void ) const
{
    s32 Count = 0;

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        if( m_lDrawPackets[i].Pass == GEOM_PASS_FORWARD_OPAQUE )
        {
            ++Count;
        }
    }

    return Count;
}

//=========================================================================

s32 FrameCompiler::GetOrderedPacketCount( void ) const
{
    return m_lDrawPackets.GetCount() - GetOpaquePacketCount();
}

//=========================================================================

xbool FrameCompiler::ReserveFrameArrays( const xarray<geom_draw_item>&    Draws,
                                         const xarray<dynamic_geom_draw>& DynamicDraws )
{
    u64 RigidColorCount = 0;
    u64 SkinBoneCount   = 0;
    u32 PoseSlots       = 8;

    while( PoseSlots < static_cast<u32>( Draws.GetCount() ) * 2u )
    {
        PoseSlots *= 2u;
    }

    m_SkinPoseSlots.SetCount( static_cast<s32>( PoseSlots ) );

    for( s32 i = 0; i < m_SkinPoseSlots.GetCount(); ++i )
    {
        m_SkinPoseSlots[i] = -1;
    }

    if( !ReserveArrayCapacity( m_SkinPoses, Draws.GetCount(), "skin poses" ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < Draws.GetCount(); ++i )
    {
        const geom_draw_item& Item = Draws[i];

        if( Item.Type == GEOM_DRAW_RIGID )
        {
            rigid_geom* pGeom = Item.Data.Rigid.pGeom;

            if( pGeom && Item.Data.Rigid.pColorInfo && ( Item.iSurface >= 0 ) &&
                ( Item.iSurface < pGeom->m_nSubMeshes ) )
            {
                const geom::submesh& SubMesh = pGeom->m_pSubMesh[Item.iSurface];

                if( ( SubMesh.iSection >= 0 ) && ( SubMesh.iSection < pGeom->m_nSections ) )
                {
                    RigidColorCount += static_cast<u64>( pGeom->m_pSection[SubMesh.iSection].nVertices );
                }
            }
        }
        else if( Item.Type == GEOM_DRAW_SKIN )
        {
            skin_geom* pGeom = Item.Data.Skin.pGeom;

            const s32 BoneCount = Item.Data.Skin.BoneCount;
            if( !pGeom || !Item.Data.Skin.pBones || ( BoneCount <= 0 ) || ( BoneCount > pGeom->m_nBones ) )
            {
                RENDER_THROW( "FrameCompiler", "invalid submitted skin pose bone count %d", BoneCount );
                return FALSE;
            }

            const s32 Slot = FindSkinPoseSlot( Item.Data.Skin.pBones, BoneCount );

            if( m_SkinPoseSlots[Slot] < 0 )
            {
                SkinPose& Pose = m_SkinPoses.Append();
                Pose.pBones    = Item.Data.Skin.pBones;
                Pose.BoneCount = BoneCount;
                Pose.FirstBone = static_cast<u32>( SkinBoneCount );
                SkinBoneCount += static_cast<u64>( Pose.BoneCount );
                m_SkinPoseSlots[Slot] = m_SkinPoses.GetCount() - 1;
            }
        }
    }

    const u64 DrawCount        = static_cast<u64>( Draws.GetCount() );
    const u64 DynamicDrawCount = static_cast<u64>( DynamicDraws.GetCount() );
    const u64 PacketCount      = DrawCount + DynamicDrawCount;

    if( !ReserveArrayCapacity( m_lDrawPackets, PacketCount, "geometry packets" ) ||
        !ReserveArrayCapacity( m_lRigidFrameInstances, DrawCount, "rigid instances" ) ||
        !ReserveArrayCapacity( m_lRigidFrameLighting, DrawCount, "rigid lighting" ) ||
        !ReserveArrayCapacity( m_lRigidFrameColors, RigidColorCount, "rigid colors" ) ||
        !ReserveArrayCapacity( m_lSkinFrameInstances, DrawCount, "skin instances" ) ||
        !ReserveArrayCapacity( m_lSkinFrameLighting, DrawCount, "skin lighting" ) ||
        !ReserveArrayCapacity( m_lSkinFrameBones, SkinBoneCount, "skin bones" ) ||
        !ReserveArrayCapacity( m_lDynamicFrameLighting, DynamicDrawCount, "dynamic lighting" ) ||
        !ReserveArrayCapacity( m_lRigidIndirectCommands, PacketCount, "rigid indirect commands" ) ||
        !ReserveArrayCapacity( m_lRigidIndirectRuns, PacketCount, "rigid indirect runs" ) ||
        !ReserveArrayCapacity( m_lSkinIndirectCommands, PacketCount, "skin indirect commands" ) ||
        !ReserveArrayCapacity( m_lSkinIndirectRuns, PacketCount, "skin indirect runs" ) ||
        !ReserveArrayCapacity( m_lSkinRemapCache, DrawCount, "skin remap cache" ) )
    {
        return FALSE;
    }

    m_lSkinFrameBones.SetCount( static_cast<s32>( SkinBoneCount ) );

    for( s32 i = 0; i < m_SkinPoses.GetCount(); ++i )
    {
        const SkinPose& Pose = m_SkinPoses[i];

        x_memcpy( &m_lSkinFrameBones[Pose.FirstBone], Pose.pBones, Pose.BoneCount * sizeof( matrix4 ) );
    }

    const u64 RequestedRemapSlots = MAX( 8ull, DrawCount * 2ull );
    u64       RemapSlotCount      = 8;

    while( RemapSlotCount < RequestedRemapSlots )
    {
        if( RemapSlotCount > ( static_cast<u64>( 0x7fffffffu ) / 2ull ) )
        {
            RENDER_THROW( "FrameCompiler", "skin remap slot count exceeds backend limit" );
            return FALSE;
        }

        RemapSlotCount *= 2;
    }

    if( !ReserveArrayCapacity( m_lSkinRemapSlots, RemapSlotCount, "skin remap slots" ) )
    {
        return FALSE;
    }

    const u64 LightingCount = static_cast<u64>( m_pContext->pLighting->GetCount() ) + 1ull;

    return ReserveArrayCapacity( m_lFrameLighting, LightingCount, "frame lighting" ) &&
           ReserveArrayCapacity( m_lLightingRemap, LightingCount, "lighting remap" );
}

//=========================================================================
//  PACKET QUERIES
//=========================================================================

s32 FrameCompiler::GetPacketCount( void ) const
{
    return m_lDrawPackets.GetCount();
}

//=========================================================================

geom_render_pass FrameCompiler::GetPacketPass( s32 PacketIndex ) const
{
    ASSERT( ( PacketIndex >= 0 ) && ( PacketIndex < m_lDrawPackets.GetCount() ) );
    return m_lDrawPackets[PacketIndex].Pass;
}

//=========================================================================

xbool FrameCompiler::IsFirstPersonPacket( s32 PacketIndex ) const
{
    ASSERT( ( PacketIndex >= 0 ) && ( PacketIndex < m_lDrawPackets.GetCount() ) );
    return ( m_lDrawPackets[PacketIndex].RenderFlags & render::FIRST_PERSON ) != 0;
}

//=========================================================================

xbool FrameCompiler::IsFirstPersonDepthPacket( s32 PacketIndex ) const
{
    ASSERT( ( PacketIndex >= 0 ) && ( PacketIndex < m_lDrawPackets.GetCount() ) );
    const GeomDrawPacket& Packet = m_lDrawPackets[PacketIndex];

    return IsFirstPersonPacket( PacketIndex ) && Packet.Pass == GEOM_PASS_FORWARD_OPAQUE &&
           ( Packet.Kind == GEOM_PACKET_DYNAMIC || Packet.MaterialOverride ||
             !Packet.pMaterial || Packet.pMaterial->WritesDepth() );
}

//=========================================================================

xbool FrameCompiler::PreparedMaterialPipeline::MatchesPass( const geom_pass_desc& PassData ) const
{
    // View/dimensions are pushed as uniforms separately; these fields select the pipeline.
    return pPipeline && pBindings &&
           Pass.ColorFormat == PassData.ColorFormat && Pass.DepthFormat == PassData.DepthFormat &&
           Pass.SampleCount == PassData.SampleCount && Pass.DepthMode == PassData.DepthMode &&
           Pass.WriteSceneDepth == PassData.WriteSceneDepth && Pass.WriteGlowMask == PassData.WriteGlowMask &&
           Pass.WriteNormals == PassData.WriteNormals && Pass.EnableShadows == PassData.EnableShadows &&
           Pass.SceneFogMode == PassData.SceneFogMode && Pass.BloomFogMode == PassData.BloomFogMode;
}

//=========================================================================

const FrameCompiler::PreparedMaterialPipeline* FrameCompiler::FindPreparedMaterialState(
    const GeomDrawPacket& Packet, const geom_pass_desc& PassData ) const
{
    if( Packet.SubmissionIndex >= static_cast<u32>( m_PreparedPacketPipelines.GetCount() ) )
    {
        return NULL;
    }

    const PreparedPacketPipelines&  Prepared = m_PreparedPacketPipelines[Packet.SubmissionIndex];
    const PreparedMaterialPipeline& Pipeline = PassData.DepthMode == GEOM_DEPTH_PREPASS ? Prepared.Depth
                                                                                      : Prepared.Scene;

    return Pipeline.MatchesPass( PassData ) ? &Pipeline : NULL;
}

//=========================================================================

render_pipeline* FrameCompiler::FindPreparedMaterialPipeline( s32 PacketIndex, const RenderStateSelection& State ) const
{
    ASSERT( PacketIndex >= 0 && PacketIndex < m_lDrawPackets.GetCount() );
    const GeomDrawPacket& Packet = m_lDrawPackets[PacketIndex];
    if( State.Program.Outputs == ShaderOutputs::LinearDepth ||
        State.Program.Outputs == ShaderOutputs::LinearDepthNormals )
    {
        ASSERT( Packet.pDepthPipeline );
        return Packet.pDepthPipeline;
    }

    const ScenePipelineRequirement& Requirement = m_ScenePipelineRequirements[Packet.ScenePipelineIndex];
    const RenderStateSelection&     Prepared    = Requirement.MaterialState;
    ShaderProgramKey                Program     = State.Program;
    Program.Vertex = Packet.Kind == GEOM_PACKET_SKIN ? VertexContract::Skin : VertexContract::Rigid;

    if( State.Blend == Prepared.Blend && State.Depth == Prepared.Depth && State.Raster == Prepared.Raster &&
        State.WriteSceneDepth == Prepared.WriteSceneDepth && Program == Prepared.Program &&
        State.ColorFormat == Prepared.ColorFormat && State.DepthFormat == Prepared.DepthFormat &&
        State.SampleCount == Prepared.SampleCount )
    {
        return Requirement.pPipeline;
    }

    return NULL;
}

//=========================================================================

const GraphicsPipelineKey& FrameCompiler::GetPacketPipelineKey( s32 PacketIndex ) const
{
    ASSERT( PacketIndex >= 0 && PacketIndex < m_lDrawPackets.GetCount() );
    const s32 Requirement = m_lDrawPackets[PacketIndex].ScenePipelineIndex;
    ASSERT( Requirement >= 0 && Requirement < m_ScenePipelineRequirements.GetCount() );
    return m_ScenePipelineRequirements[Requirement].Key;
}

//=========================================================================

xbool FrameCompiler::PacketReadsSceneColor( s32 PacketIndex ) const
{
    ASSERT( PacketIndex >= 0 && PacketIndex < m_lDrawPackets.GetCount() );
    return m_lDrawPackets[PacketIndex].SceneProgram.Surface == SurfaceRecipe::Distortion;
}

//=========================================================================

u32 FrameCompiler::GetPacketSequence( s32 PacketIndex ) const
{
    ASSERT( ( PacketIndex >= 0 ) && ( PacketIndex < m_lDrawPackets.GetCount() ) );
    return m_lDrawPackets[PacketIndex].Sequence;
}

//=========================================================================

f32 FrameCompiler::GetPacketSortDepth( s32 PacketIndex ) const
{
    ASSERT( ( PacketIndex >= 0 ) && ( PacketIndex < m_lDrawPackets.GetCount() ) );
    return m_lDrawPackets[PacketIndex].SortDepth;
}

//=========================================================================

xbool FrameCompiler::IsPacketGlowCapable( s32 PacketIndex ) const
{
    if( ( PacketIndex < 0 ) || ( PacketIndex >= m_lDrawPackets.GetCount() ) )
    {
        return FALSE;
    }

    return m_lDrawPackets[PacketIndex].GlowCapable;
}

//=========================================================================

xbool FrameCompiler::ComputePacketGlowCapable( const GeomDrawPacket& Packet ) const
{
    if( Packet.ContributionPolicy == render::RENDER_CONTRIBUTION_SUPPRESS_BLOOM )
    {
        return FALSE;
    }

    if( ( Packet.RenderFlags & render::GLOWING ) || ( Packet.InstFeatures & render::GEOM_INST_FORCED_GLOW ) )
    {
        return TRUE;
    }

    if( Packet.Kind == GEOM_PACKET_RIGID )
    {
        const u32 First = Packet.FirstInstance;
        const u32 Last  = First + Packet.InstanceCount;

        for( u32 i = First; i < Last && i < static_cast<u32>( m_lRigidFrameInstances.GetCount() ); ++i )
        {
            if( m_lRigidFrameInstances[i].ShaderFlags & GEOM_INST_FLAG_GLOWING )
            {
                return TRUE;
            }
        }
    }
    else if( Packet.Kind == GEOM_PACKET_SKIN )
    {
        const u32 First = Packet.FirstInstance;
        const u32 Last  = First + Packet.InstanceCount;

        for( u32 i = First; i < Last && i < static_cast<u32>( m_lSkinFrameInstances.GetCount() ); ++i )
        {
            if( m_lSkinFrameInstances[i].ShaderFlags & GEOM_INST_FLAG_GLOWING )
            {
                return TRUE;
            }
        }
    }

    if( !Packet.pMaterial )
    {
        return FALSE;
    }

    const u8 RenderOrder = Packet.pMaterial->GetRenderOrder();

    return ( RenderOrder == MATERIAL_RENDER_ORDER_GLOWING ) || ( RenderOrder == MATERIAL_RENDER_ORDER_ALPHA_GLOWING );
}

//=========================================================================
//  DRAW STATISTICS
//=========================================================================

s32 FrameCompiler::GetRigidInstanceCount( void ) const
{
    return m_lRigidFrameInstances.GetCount();
}

//=========================================================================

s32 FrameCompiler::GetSkinInstanceCount( void ) const
{
    return m_lSkinFrameInstances.GetCount();
}

//=========================================================================

s32 FrameCompiler::GetLitInstanceCount( void ) const
{
    return m_nLitInstances;
}

//=========================================================================

s32 FrameCompiler::GetInstanceLightCount( void ) const
{
    return m_nInstanceLights;
}

//=========================================================================

s32 FrameCompiler::GetLightingRecordCount( void ) const
{
    return MAX( m_lFrameLighting.GetCount() - 1, 0 );
}

//=========================================================================

u32 FrameCompiler::GetForwardOpaqueGpuDrawCount( void ) const
{
    return m_nForwardOpaqueGpuDraws;
}

//=========================================================================

u32 FrameCompiler::GetForwardOpaqueInstanceCount( void ) const
{
    return m_nForwardOpaqueInstances;
}

//=========================================================================

u32 FrameCompiler::GetForwardOpaqueSkinSectionDrawCount( void ) const
{
    return m_nForwardOpaqueSkinSectionDraws;
}

//=========================================================================

u64 FrameCompiler::GetForwardOpaqueSubmittedIndexCount( void ) const
{
    return m_nForwardOpaqueSubmittedIndices;
}

//=========================================================================

u32 FrameCompiler::GetForwardOpaqueRigidIndirectRunCount( void ) const
{
    return m_lRigidIndirectRuns.GetCount();
}

//=========================================================================

u32 FrameCompiler::GetForwardOpaqueRigidIndirectCommandCount( void ) const
{
    return m_lRigidIndirectCommands.GetCount();
}

//=========================================================================

u32 FrameCompiler::GetForwardOpaqueSkinIndirectRunCount( void ) const
{
    return m_lSkinIndirectRuns.GetCount();
}

//=========================================================================

u32 FrameCompiler::GetForwardOpaqueSkinIndirectCommandCount( void ) const
{
    return m_lSkinIndirectCommands.GetCount();
}

//=========================================================================
//  LIGHTING PREPARATION
//=========================================================================

xbool FrameCompiler::CopyLightingData( GeomLightingConstants&           Destination,
                                       GeomProjectionLightingConstants& Projection,
                                       GeomAmbientConstants&            Ambient,
                                       const cb_geom_lighting&          Source ) const
{
    x_memset( &Destination, 0, sizeof( Destination ) );
    x_memset( &Projection, 0, sizeof( Projection ) );
    x_memset( &Ambient, 0, sizeof( Ambient ) );

    for( s32 i = 0; i < MAX_GEOM_LIGHTS; ++i )
    {
        Projection.LightShadowIndex[i] = 0xFFFFFFFFu;
    }

    x_memcpy( Destination.LightVec, Source.LightVec, sizeof( Destination.LightVec ) );
    x_memcpy( Destination.LightCol, Source.LightCol, sizeof( Destination.LightCol ) );
    x_memcpy( Destination.LightDir, Source.LightDir, sizeof( Destination.LightDir ) );
    x_memcpy( Destination.LightCone, Source.LightCone, sizeof( Destination.LightCone ) );
    x_memcpy( Projection.LightCookieU, Source.LightCookieU, sizeof( Projection.LightCookieU ) );
    x_memcpy( Projection.LightCookieV, Source.LightCookieV, sizeof( Projection.LightCookieV ) );

    const s32 LightCount = MIN( Source.LightCount, MAX_GEOM_LIGHTS );

    for( s32 i = 0; i < LightCount; ++i )
    {
        const f32 Radius      = MAX( Source.LightVec[i].GetW(), 1.0e-4f );
        const f32 InnerRadius = Radius * ( 1.0f - x_clamp( Source.LightCol[i].GetW(), 0.0f, 1.0f ) );
        Destination.LightRadial[i].Set( InnerRadius, 1.0f / MAX( Radius - InnerRadius, 1.0e-4f ), 0.0f, 0.0f );

        const f32 CosOuter              = x_clamp( Source.LightCone[i].GetY(), 0.0f, 1.0f );
        Destination.LightCone[i].GetZ() = x_sqrt( MAX( 1.0f - CosOuter * CosOuter, 0.0f ) ) / MAX( CosOuter, 1.0e-4f );
        Destination.LightCone[i].GetW() =
            1.0f / MAX( Source.LightCone[i].GetX() - Source.LightCone[i].GetY(), 1.0e-4f );

        const s32 DynamicLightIndex = Source.DynamicLightIndex[i];

        if( ( DynamicLightIndex >= 0 ) && ( DynamicLightIndex < LightSystem::MAX_SHADOW_LIGHTS ) )
        {
            Projection.LightShadowIndex[i] = m_DynamicLightShadowIndex[DynamicLightIndex];
        }

        const s32 CookieIndex             = static_cast<s32>( Source.LightCookieU[i].GetW() ) - 1;
        Projection.LightCookieU[i].GetW() = 0.0f;
        Projection.LightCookieV[i].GetW() = 0.0f;

        if( CookieIndex < 0 )
        {
            continue;
        }

        if( CookieIndex >= m_pContext->pLights->GetLightCookieCount() )
        {
            return FALSE;
        }

        ProjectionAtlasRegion Region;

        if( !m_pContext->pProjection->GetRegion( m_pContext->pLights->GetLightCookieHandle( CookieIndex ),
                                                 ProjectionAtlasEncoding::LuminanceAlpha, Region ) )
        {
            return FALSE;
        }

        Projection.LightCookieAtlas[i]      = Region.UVScaleBias;
        Projection.LightCookieTileBounds[i] = Region.TileBounds;
        Projection.LightCookieInfo[i]       = Region.Sampling;
        Projection.LightCookieInfo[i].GetX() += 1.0f;
    }

    Ambient.LightAmbCol = Source.AmbCol;
    Ambient.LightCount  = LightCount;

    return TRUE;
}

//=========================================================================

// Map stale scope handles to record zero instead of indexing another scope.
xbool FrameCompiler::ResolveLightingIndex( const RenderLightingHandle& Lighting, u32& OutIndex )
{
    OutIndex = 0;

    const cb_geom_lighting* pLighting = m_pContext->pLighting->Get( Lighting );

    if( !pLighting )
    {
        return TRUE;
    }

    if( Lighting.Index >= static_cast<u32>( m_lLightingRemap.GetCount() ) )
    {
        return FALSE;
    }

    u32& RemappedIndex = m_lLightingRemap[static_cast<s32>( Lighting.Index )];

    if( RemappedIndex == 0xffffffffu )
    {
        if( m_lFrameLighting.GetCount() >= 0x7fffffff )
        {
            return FALSE;
        }

        RemappedIndex = static_cast<u32>( m_lFrameLighting.GetCount() );

        GeomLightingConstants& Destination = m_lFrameLighting.Append();

        if( !CopyLightingData( Destination, m_lProjectionLighting.Append(), m_lAmbientLighting.Append(), *pLighting ) )
        {
            return FALSE;
        }
    }

    OutIndex = RemappedIndex;

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::BuildLightingData( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildLighting" );

    if( m_lRigidFrameLighting.GetCount() != m_lRigidFrameInstances.GetCount() ||
        m_lSkinFrameLighting.GetCount() != m_lSkinFrameInstances.GetCount() )
    {
        return FALSE;
    }

    const RenderLightingTable& LightingTable = *m_pContext->pLighting;
    m_lFrameLighting.SetCount( 1 );
    m_lProjectionLighting.SetCount( 1 );
    m_lAmbientLighting.SetCount( 1 );
    x_memset( &m_lProjectionLighting[0], 0, sizeof( GeomProjectionLightingConstants ) );
    x_memset( &m_lAmbientLighting[0], 0, sizeof( GeomAmbientConstants ) );
    x_memset( &m_lFrameLighting[0], 0, sizeof( GeomLightingConstants ) );
    m_lLightingRemap.SetCount( LightingTable.GetCount() + 1 );

    for( s32 i = 0; i < m_lLightingRemap.GetCount(); ++i )
    {
        m_lLightingRemap[i] = 0xffffffffu;
    }

    for( s32 i = 0; i < m_lRigidFrameInstances.GetCount(); ++i )
    {
        if( !ResolveLightingIndex( m_lRigidFrameLighting[i], m_lRigidFrameInstances[i].LightingIndex ) )
        {
            return FALSE;
        }
    }

    for( s32 i = 0; i < m_lSkinFrameInstances.GetCount(); ++i )
    {
        if( !ResolveLightingIndex( m_lSkinFrameLighting[i], m_lSkinFrameInstances[i].LightingIndex ) )
        {
            return FALSE;
        }
    }

    s32 DynamicIndex = 0;

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        if( m_lDrawPackets[i].Kind != GEOM_PACKET_DYNAMIC )
        {
            continue;
        }

        if( DynamicIndex >= m_lDynamicFrameLighting.GetCount() )
        {
            return FALSE;
        }

        if( !ResolveLightingIndex( m_lDynamicFrameLighting[DynamicIndex], m_lDrawPackets[i].DynamicLightingIndex ) )
        {
            return FALSE;
        }

        ++DynamicIndex;
    }

    if( DynamicIndex != m_lDynamicFrameLighting.GetCount() )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  GPU UPLOADS
//=========================================================================

xbool FrameCompiler::ReserveUploadBuffers( void )
{
    if( m_lDrawPackets.GetCount() == 0 )
    {
        return TRUE;
    }

    if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_LIGHTING ), m_LightingDataCapacity,
                       static_cast<u32>( m_pContext->pLighting->GetCount() + 1 ), sizeof( GeomLightingConstants ),
                       "GeomLighting" ) )
    {
        return FALSE;
    }

    if( m_lRigidFrameInstances.GetCount() > 0 )
    {
        const u32 InstanceCount = m_lRigidFrameInstances.GetCount();
        const u32 ColorCount    = MAX( m_lRigidFrameColors.GetCount(), 1 );

        if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCES ), m_RigidInstanceCapacity,
                           InstanceCount, sizeof( RigidInstanceData ), "GeomRigidInstances" ) ||
            !EnsureSequentialIndexBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCE_INDICES ),
                                          m_RigidInstanceIndexCapacity, InstanceCount, "GeomRigidInstanceIndices",
                                          RBUFFER_USAGE_VERTEX, m_RigidInstanceIndices ) ||
            !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_COLORS ), m_RigidColorCapacity, ColorCount,
                           sizeof( u32 ), "GeomRigidColors" ) )
        {
            return FALSE;
        }
    }

    if( m_lRigidIndirectCommands.GetCount() > 0 &&
        !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_INDIRECT ), m_RigidIndirectCapacity,
                       m_lRigidIndirectCommands.GetCount(), sizeof( rdraw_indexed_indirect_command ),
                       "GeomRigidIndirect", RBUFFER_USAGE_INDIRECT ) )
    {
        return FALSE;
    }

    if( m_lSkinFrameInstances.GetCount() > 0 )
    {
        const u32 InstanceCount     = m_lSkinFrameInstances.GetCount();
        const u32 BoneCount         = MAX( m_lSkinFrameBones.GetCount(), 1 );
        const u32 DrawInstanceCount = m_lSkinDrawInstances.GetCount();

        if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_INSTANCES ), m_SkinInstanceCapacity, InstanceCount,
                           sizeof( SkinInstanceData ), "GeomSkinInstances" ) ||
            !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_BONES ), m_SkinBoneCapacity, BoneCount,
                           sizeof( matrix4 ), "GeomSkinBones" ) ||
            !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_DRAW_INSTANCES ), m_SkinDrawInstanceCapacity,
                           DrawInstanceCount, sizeof( SkinDrawInstance ), "GeomSkinDrawInstances",
                           RBUFFER_USAGE_VERTEX ) )
        {
            return FALSE;
        }
    }

    if( m_lSkinIndirectCommands.GetCount() > 0 &&
        !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_INDIRECT ), m_SkinIndirectCapacity,
                       m_lSkinIndirectCommands.GetCount(), sizeof( rdraw_indexed_indirect_command ), "GeomSkinIndirect",
                       RBUFFER_USAGE_INDIRECT ) )
    {
        return FALSE;
    }

    const xbool BReserved = ReserveDynamicBuffers();
    CommitFrameBufferCapacities();

    return BReserved;
}

//=========================================================================

xbool FrameCompiler::UploadPackets( FrameGraphBuilder& Graph )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadPackets" );

    static xprofile_counter GeomUploadBytes = x_GetProfiler().RegisterCounter( "GeomUploadBytes", "RenderCounter" );
    static xprofile_counter GeomLightingUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomLightingUploadBytes", "RenderCounter" );
    static xprofile_counter GeomRigidUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomRigidUploadBytes", "RenderCounter" );
    static xprofile_counter GeomSkinUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomSkinUploadBytes", "RenderCounter" );
    static xprofile_counter GeomShadowUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomShadowUploadBytes", "RenderCounter" );
    static xprofile_counter GeomBufferReallocs =
        x_GetProfiler().RegisterCounter( "GeomBufferReallocs", "RenderCounter" );

    if( m_lDrawPackets.GetCount() == 0 )
    {
        return TRUE;
    }

    // Collect the frame streams into one upload batch.
    rbuffer_upload_batch_item Uploads[11];
    u32                       UploadCount = 0;
    const u32                 EmptyColor  = 0;
    matrix4                   Identity;
    Identity.Identity();

    if( !m_areShadowMapConstantsValid )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadShadowMaps" );

        if( !PrepareSharedShadowData( Graph ) )
        {
            return FALSE;
        }

        const u32 ShadowBytes = sizeof( m_FaceShadowMatrices ) + sizeof( m_ShadowMapData );
        GeomShadowUploadBytes.Add( ShadowBytes );
        GeomUploadBytes.Add( ShadowBytes );
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BuildLightingData" );

        if( !BuildLightingData() )
        {
            return FALSE;
        }
    }

    const u32 LightingCount = m_lFrameLighting.GetCount();
    const u32 LightingBytes = sizeof( GeomLightingConstants ) * LightingCount;

    if( !GetFrameBuffer( FrameStorage::STREAM_LIGHTING ) || ( m_LightingDataCapacity < LightingCount ) )
    {
        GeomBufferReallocs.Add();
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadLighting" );

        if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_LIGHTING ), m_LightingDataCapacity, LightingCount,
                           sizeof( GeomLightingConstants ), "GeomLighting" ) )
        {
            return FALSE;
        }

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_LIGHTING );
        Uploads[UploadCount].Upload.pData  = m_lFrameLighting.GetPtr();
        Uploads[UploadCount].Upload.Size   = LightingBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;
    }

    // Upload projection and ambient lighting separately.
    const u32 ProjectionBytes = LightingCount * sizeof( GeomProjectionLightingConstants );
    const u32 AmbientBytes    = LightingCount * sizeof( GeomAmbientConstants );

    if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_PROJECTION_LIGHTING ), m_ProjectionLightingCapacity,
                       LightingCount, sizeof( GeomProjectionLightingConstants ), "GeomProjectionLighting" ) ||
        !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_AMBIENT_LIGHTING ), m_AmbientLightingCapacity,
                       LightingCount, sizeof( GeomAmbientConstants ), "GeomAmbient" ) )
    {
        return FALSE;
    }

    Uploads[UploadCount].pBuffer      = &GetFrameBuffer( FrameStorage::STREAM_PROJECTION_LIGHTING );
    Uploads[UploadCount].Upload.pData = m_lProjectionLighting.GetPtr();
    Uploads[UploadCount].Upload.Size  = ProjectionBytes;

    Uploads[UploadCount++].Upload.bCycle = TRUE;

    Uploads[UploadCount].pBuffer      = &GetFrameBuffer( FrameStorage::STREAM_AMBIENT_LIGHTING );
    Uploads[UploadCount].Upload.pData = m_lAmbientLighting.GetPtr();
    Uploads[UploadCount].Upload.Size  = AmbientBytes;

    Uploads[UploadCount++].Upload.bCycle = TRUE;
    GeomLightingUploadBytes.Add( ProjectionBytes + AmbientBytes );
    GeomUploadBytes.Add( ProjectionBytes + AmbientBytes );
    GeomLightingUploadBytes.Add( LightingBytes );
    GeomUploadBytes.Add( LightingBytes );

    if( m_lRigidFrameInstances.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadRigid" );

        const u32 InstanceCount = m_lRigidFrameInstances.GetCount();
        const u32 ColorCount    = MAX( m_lRigidFrameColors.GetCount(), 1 );
        const u32 InstanceBytes = sizeof( RigidInstanceData ) * InstanceCount;
        const u32 ColorBytes    = sizeof( u32 ) * ColorCount;

        if( !GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCES ) || ( m_RigidInstanceCapacity < InstanceCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCE_INDICES ) ||
            ( m_RigidInstanceIndexCapacity < InstanceCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !GetFrameBuffer( FrameStorage::STREAM_RIGID_COLORS ) || ( m_RigidColorCapacity < ColorCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCES ), m_RigidInstanceCapacity,
                           InstanceCount, sizeof( RigidInstanceData ), "GeomRigidInstances" ) ||
            !EnsureSequentialIndexBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCE_INDICES ),
                                          m_RigidInstanceIndexCapacity, InstanceCount, "GeomRigidInstanceIndices",
                                          RBUFFER_USAGE_VERTEX, m_RigidInstanceIndices ) ||
            !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_COLORS ), m_RigidColorCapacity, ColorCount,
                           sizeof( u32 ), "GeomRigidColors" ) )
        {
            return FALSE;
        }

        if( !Graph.QueueUpload( GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCE_INDICES ),
                                m_RigidInstanceIndices.GetPtr(), InstanceCount * sizeof( u32 ), 0, TRUE ) )
        {
            return FALSE;
        }

        const void* pColorData = m_lRigidFrameColors.GetCount() ? m_lRigidFrameColors.GetPtr() : &EmptyColor;

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCES );
        Uploads[UploadCount].Upload.pData  = m_lRigidFrameInstances.GetPtr();
        Uploads[UploadCount].Upload.Size   = InstanceBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_RIGID_COLORS );
        Uploads[UploadCount].Upload.pData  = pColorData;
        Uploads[UploadCount].Upload.Size   = ColorBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;

        GeomRigidUploadBytes.Add( InstanceBytes + ColorBytes );
        GeomUploadBytes.Add( InstanceBytes + ColorBytes );
    }

    if( m_lRigidIndirectCommands.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadIndirect" );

        const u32 CommandCount = m_lRigidIndirectCommands.GetCount();
        const u32 CommandBytes = CommandCount * sizeof( rdraw_indexed_indirect_command );

        if( !GetFrameBuffer( FrameStorage::STREAM_RIGID_INDIRECT ) || ( m_RigidIndirectCapacity < CommandCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_RIGID_INDIRECT ), m_RigidIndirectCapacity, CommandCount,
                           sizeof( rdraw_indexed_indirect_command ), "GeomRigidIndirect", RBUFFER_USAGE_INDIRECT ) )
        {
            return FALSE;
        }

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_RIGID_INDIRECT );
        Uploads[UploadCount].Upload.pData  = m_lRigidIndirectCommands.GetPtr();
        Uploads[UploadCount].Upload.Size   = CommandBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;

        GeomRigidUploadBytes.Add( CommandBytes );
        GeomUploadBytes.Add( CommandBytes );
    }

    if( m_lSkinFrameInstances.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadSkin" );

        const u32 InstanceCount     = m_lSkinFrameInstances.GetCount();
        const u32 BoneCount         = MAX( m_lSkinFrameBones.GetCount(), 1 );
        const u32 DrawInstanceCount = m_lSkinDrawInstances.GetCount();
        const u32 InstanceBytes     = sizeof( SkinInstanceData ) * InstanceCount;
        const u32 BoneBytes         = sizeof( matrix4 ) * BoneCount;
        const u32 DrawInstanceBytes = sizeof( SkinDrawInstance ) * DrawInstanceCount;

        if( DrawInstanceCount == 0 )
        {
            RENDER_THROW( "FrameCompiler", "skin packets have no draw-instance data" );
            return FALSE;
        }

        if( !GetFrameBuffer( FrameStorage::STREAM_SKIN_INSTANCES ) || ( m_SkinInstanceCapacity < InstanceCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !GetFrameBuffer( FrameStorage::STREAM_SKIN_BONES ) || ( m_SkinBoneCapacity < BoneCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !GetFrameBuffer( FrameStorage::STREAM_SKIN_DRAW_INSTANCES ) ||
            ( m_SkinDrawInstanceCapacity < DrawInstanceCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_INSTANCES ), m_SkinInstanceCapacity, InstanceCount,
                           sizeof( SkinInstanceData ), "GeomSkinInstances" ) ||
            !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_BONES ), m_SkinBoneCapacity, BoneCount,
                           sizeof( matrix4 ), "GeomSkinBones" ) ||
            !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_DRAW_INSTANCES ), m_SkinDrawInstanceCapacity,
                           DrawInstanceCount, sizeof( SkinDrawInstance ), "GeomSkinDrawInstances",
                           RBUFFER_USAGE_VERTEX ) )
        {
            return FALSE;
        }

        const void* pBoneData = m_lSkinFrameBones.GetCount() ? m_lSkinFrameBones.GetPtr() : &Identity;

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_SKIN_INSTANCES );
        Uploads[UploadCount].Upload.pData  = m_lSkinFrameInstances.GetPtr();
        Uploads[UploadCount].Upload.Size   = InstanceBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_SKIN_BONES );
        Uploads[UploadCount].Upload.pData  = pBoneData;
        Uploads[UploadCount].Upload.Size   = BoneBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_SKIN_DRAW_INSTANCES );
        Uploads[UploadCount].Upload.pData  = m_lSkinDrawInstances.GetPtr();
        Uploads[UploadCount].Upload.Size   = DrawInstanceBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;

        const u32 SkinBytes = InstanceBytes + BoneBytes + DrawInstanceBytes;
        GeomSkinUploadBytes.Add( SkinBytes );
        GeomUploadBytes.Add( SkinBytes );
    }

    if( m_lSkinIndirectCommands.GetCount() > 0 )
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/UploadSkinIndirect" );

        const u32 CommandCount = m_lSkinIndirectCommands.GetCount();
        const u32 CommandBytes = CommandCount * sizeof( rdraw_indexed_indirect_command );

        if( !GetFrameBuffer( FrameStorage::STREAM_SKIN_INDIRECT ) || ( m_SkinIndirectCapacity < CommandCount ) )
        {
            GeomBufferReallocs.Add();
        }

        if( !EnsureBuffer( GetFrameBuffer( FrameStorage::STREAM_SKIN_INDIRECT ), m_SkinIndirectCapacity, CommandCount,
                           sizeof( rdraw_indexed_indirect_command ), "GeomSkinIndirect", RBUFFER_USAGE_INDIRECT ) )
        {
            return FALSE;
        }

        Uploads[UploadCount].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_SKIN_INDIRECT );
        Uploads[UploadCount].Upload.pData  = m_lSkinIndirectCommands.GetPtr();
        Uploads[UploadCount].Upload.Size   = CommandBytes;
        Uploads[UploadCount].Upload.bCycle = TRUE;
        ++UploadCount;

        GeomSkinUploadBytes.Add( CommandBytes );
        GeomUploadBytes.Add( CommandBytes );
    }

    if( !Graph.QueueUploadBatch( Uploads, UploadCount ) )
    {
        return FALSE;
    }

    if( !UploadDynamicGeom( Graph ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================
//  DRAW EXECUTION
//=========================================================================

xbool FrameCompiler::ExecuteRigidIndirectRun( const RigidIndirectRun& Run, const geom_pass_desc& PassData )
{
    if( ( Run.CommandCount == 0 ) || ( Run.FirstPacket > Run.LastPacket ) ||
        ( Run.FirstPacket >= static_cast<u32>( m_lDrawPackets.GetCount() ) ) ||
        ( ( Run.FirstCommand + Run.CommandCount ) > static_cast<u32>( m_lRigidIndirectCommands.GetCount() ) ) )
    {
        RENDER_THROW( "FrameCompiler", "invalid indirect run packets %u-%u commands %u+%u", Run.FirstPacket,
                      Run.LastPacket, Run.FirstCommand, Run.CommandCount );
        return FALSE;
    }

    const GeomDrawPacket& Packet   = m_lDrawPackets[Run.FirstPacket];
    geom_pass_desc        DrawPass = PassData;

    if( Packet.ContributionPolicy == render::RENDER_CONTRIBUTION_SUPPRESS_BLOOM )
    {
        DrawPass.BloomFogMode = RENDER_FOG_NONE;
    }

    m_pDiffuseResource         = Packet.Resources.pDiffuse;
    m_pDetailResource          = Packet.Resources.pDetail;
    m_pEnvironmentResource     = Packet.Resources.pEnvironment;
    m_pEnvironmentCubeResource = Packet.Resources.pEnvironmentCube;
    m_isDistortionStateActive  = Packet.Resources.isDistortionActive;
    m_DistortionNormalRot      = Packet.Resources.DistortionNormalRot;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindIndirectRun" );

        if( !SetRigidMaterial(
                Packet.pMaterial,
                Packet.RenderFlags | ( ( Packet.InstFeatures & render::GEOM_INST_FORCED_GLOW ) ? render::GLOWING : 0u ),
                Packet.UOffset, Packet.VOffset, Packet.MaterialOverride, IsPacketGlowCapable( Run.FirstPacket ),
                DrawPass, Packet.LightingMode, Packet.Projection, Run.FirstPacket ) )
        {
            RENDER_THROW( "FrameCompiler", "indirect run %u failed to bind material", Run.FirstCommand );
            return FALSE;
        }

        const xbool DepthOnly = ( Packet.MaterialOverride || PassData.DepthMode == GEOM_DEPTH_PREPASS ) &&
                                !PassData.WriteGlowMask;

        if( !BindPacketResources( Packet, DepthOnly ) || !BindRigidFrameBuffers( DepthOnly ) )
        {
            RENDER_THROW( "FrameCompiler", "indirect run %u failed to bind shader resources", Run.FirstCommand );
            return FALSE;
        }

        if( !BindRigidGeomBuffers( Packet.Range ) )
        {
            RENDER_THROW( "FrameCompiler", "indirect run %u failed to bind pool %d/%d", Run.FirstCommand,
                          Run.VertexPool, Run.IndexPool );
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/DrawIndirectRun" );

        if( !rdraw_DrawIndexedIndirect( GetFrameBuffer( FrameStorage::STREAM_RIGID_INDIRECT ),
                                        Run.FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                        Run.CommandCount ) )
        {
            RENDER_THROW( "FrameCompiler", "indirect run %u rejected (%u commands)", Run.FirstCommand,
                          Run.CommandCount );
            return FALSE;
        }
    }

    if( PassData.DepthMode == GEOM_DEPTH_PREPASS )
    {
        return TRUE;
    }

    m_nForwardOpaqueGpuDraws++;

    for( u32 i = 0; i < Run.CommandCount; ++i )
    {
        const rdraw_indexed_indirect_command& Command = m_lRigidIndirectCommands[Run.FirstCommand + i];
        m_nForwardOpaqueInstances += Command.InstanceCount;
        m_nForwardOpaqueSubmittedIndices += static_cast<u64>( Command.IndexCount ) * Command.InstanceCount;
    }

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::ExecuteSkinIndirectRun( const SkinIndirectRun& Run, const geom_pass_desc& PassData )
{
    if( ( Run.CommandCount == 0 ) || ( Run.FirstPacket >= static_cast<u32>( m_lDrawPackets.GetCount() ) ) ||
        ( ( Run.FirstCommand + Run.CommandCount ) > static_cast<u32>( m_lSkinIndirectCommands.GetCount() ) ) )
    {
        RENDER_THROW( "FrameCompiler", "invalid skin indirect run packet %u commands %u+%u", Run.FirstPacket,
                      Run.FirstCommand, Run.CommandCount );
        return FALSE;
    }

    const GeomDrawPacket& Packet   = m_lDrawPackets[Run.FirstPacket];
    geom_pass_desc        DrawPass = PassData;

    if( Packet.ContributionPolicy == render::RENDER_CONTRIBUTION_SUPPRESS_BLOOM )
    {
        DrawPass.BloomFogMode = RENDER_FOG_NONE;
    }

    m_pDiffuseResource         = Packet.Resources.pDiffuse;
    m_pDetailResource          = Packet.Resources.pDetail;
    m_pEnvironmentResource     = Packet.Resources.pEnvironment;
    m_pEnvironmentCubeResource = Packet.Resources.pEnvironmentCube;
    m_isDistortionStateActive  = Packet.Resources.isDistortionActive;
    m_DistortionNormalRot      = Packet.Resources.DistortionNormalRot;

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindSkinIndirectRun" );

        if( !SetSkinMaterial(
                Packet.pMaterial,
                Packet.RenderFlags | ( ( Packet.InstFeatures & render::GEOM_INST_FORCED_GLOW ) ? render::GLOWING : 0u ),
                Packet.UOffset, Packet.VOffset, Packet.MaterialOverride, IsPacketGlowCapable( Run.FirstPacket ),
                DrawPass, Packet.LightingMode, Packet.Projection, Run.FirstPacket ) )
        {
            RENDER_THROW( "FrameCompiler", "skin indirect run %u failed to bind material", Run.FirstCommand );
            return FALSE;
        }

        const xbool DepthOnly = ( Packet.MaterialOverride || PassData.DepthMode == GEOM_DEPTH_PREPASS ) &&
                                !PassData.WriteGlowMask;

        if( !BindPacketResources( Packet, DepthOnly ) || !BindSkinFrameBuffers( DepthOnly ) )
        {
            RENDER_THROW( "FrameCompiler", "skin indirect run %u failed to bind resources", Run.FirstCommand );
            return FALSE;
        }

        if( !BindSkinGeomBuffers( Packet ) )
        {
            RENDER_THROW( "FrameCompiler", "skin indirect run %u failed to bind pool %d/%d", Run.FirstCommand,
                          Run.VertexPool, Run.IndexPool );
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/DrawSkinIndirectRun" );

        if( !rdraw_DrawIndexedIndirect( GetFrameBuffer( FrameStorage::STREAM_SKIN_INDIRECT ),
                                        Run.FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                        Run.CommandCount ) )
        {
            RENDER_THROW( "FrameCompiler", "skin indirect run %u rejected (%u commands)", Run.FirstCommand,
                          Run.CommandCount );
            return FALSE;
        }
    }

    if( PassData.DepthMode == GEOM_DEPTH_PREPASS )
    {
        return TRUE;
    }

    m_nForwardOpaqueGpuDraws++;
    m_nForwardOpaqueSkinSectionDraws += Run.CommandCount;

    for( u32 i = 0; i < Run.CommandCount; ++i )
    {
        const rdraw_indexed_indirect_command& Command = m_lSkinIndirectCommands[Run.FirstCommand + i];
        m_nForwardOpaqueInstances += Command.InstanceCount;
        m_nForwardOpaqueSubmittedIndices += static_cast<u64>( Command.IndexCount ) * Command.InstanceCount;
    }

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::ExecuteForwardOpaque( const geom_pass_desc& PassData, const BackendPassInputs& Inputs )
{
    static xprofile_counter DepthPrepassGpuDraws =
        x_GetProfiler().RegisterCounter( "GeomDepthPrepassGpuDraws", "RenderCounter" );

    if( ( PassData.TargetWidth == 0 ) || ( PassData.TargetHeight == 0 ) )
    {
        RENDER_THROW( "FrameCompiler", "invalid forward opaque extent %ux%u", PassData.TargetWidth,
                      PassData.TargetHeight );
        return FALSE;
    }

    if( !ApplyPassInputs( Inputs ) )
    {
        return FALSE;
    }

    // Opaque rigid and skin geometry has no cross-kind ordering dependency.
    for( s32 RunIndex = 0; RunIndex < m_lRigidIndirectRuns.GetCount(); ++RunIndex )
    {
        const GeomDrawPacket& Packet = m_lDrawPackets[m_lRigidIndirectRuns[RunIndex].FirstPacket];
        if( PassData.DepthMode == GEOM_DEPTH_PREPASS && !Packet.MaterialOverride &&
            Packet.pMaterial && !Packet.pMaterial->WritesDepth() )
        {
            continue;
        }

        if( !ExecuteRigidIndirectRun( m_lRigidIndirectRuns[RunIndex], PassData ) )
        {
            RENDER_THROW( "FrameCompiler", "forward opaque rigid run %d/%d failed", RunIndex,
                          m_lRigidIndirectRuns.GetCount() );
            return FALSE;
        }

        if( PassData.DepthMode == GEOM_DEPTH_PREPASS )
        {
            DepthPrepassGpuDraws.Add();
        }
    }

    for( s32 RunIndex = 0; RunIndex < m_lSkinIndirectRuns.GetCount(); ++RunIndex )
    {
        const GeomDrawPacket& Packet = m_lDrawPackets[m_lSkinIndirectRuns[RunIndex].FirstPacket];
        if( PassData.DepthMode == GEOM_DEPTH_PREPASS && !Packet.MaterialOverride &&
            Packet.pMaterial && !Packet.pMaterial->WritesDepth() )
        {
            continue;
        }

        if( !ExecuteSkinIndirectRun( m_lSkinIndirectRuns[RunIndex], PassData ) )
        {
            RENDER_THROW( "FrameCompiler", "forward opaque skin run %d/%d failed", RunIndex,
                          m_lSkinIndirectRuns.GetCount() );
            return FALSE;
        }

        if( PassData.DepthMode == GEOM_DEPTH_PREPASS )
        {
            DepthPrepassGpuDraws.Add();
        }
    }

    for( s32 PacketIndex = 0; PacketIndex < m_lDrawPackets.GetCount(); ++PacketIndex )
    {
        const GeomDrawPacket& Packet = m_lDrawPackets[PacketIndex];

        if( ( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE ) && ( Packet.Kind == GEOM_PACKET_DYNAMIC ) &&
            !( Packet.RenderFlags & render::FIRST_PERSON ) )
        {
            if( !ExecuteDynamicPacket( Packet, PassData ) )
            {
                RENDER_THROW( "FrameCompiler", "forward opaque dynamic packet %d failed", PacketIndex );
                return FALSE;
            }

            if( PassData.DepthMode == GEOM_DEPTH_PREPASS )
            {
                DepthPrepassGpuDraws.Add();
            }
        }
    }

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::CanBatchForwardPackets( s32 FirstPacket, s32 NextPacket ) const
{
    const GeomDrawPacket& A = m_lDrawPackets[FirstPacket];
    const GeomDrawPacket& B = m_lDrawPackets[NextPacket];

    // These draws have per-object passes or constants outside the shared instance streams.
    if( A.Kind == GEOM_PACKET_DYNAMIC || B.Kind != A.Kind || A.Pass == GEOM_PASS_FORWARD_OPAQUE ||
        A.Pass == GEOM_PASS_DISTORTION || A.Pass != B.Pass || ( A.RenderFlags & render::FIRST_PERSON ) ||
        ( B.RenderFlags & render::FIRST_PERSON ) ||
        ( A.pMaterial && A.pMaterial->m_Type == Material_Alpha_PerPolyIllum ) )
    {
        return FALSE;
    }

    return A.ScenePipelineIndex >= 0 && A.ScenePipelineIndex == B.ScenePipelineIndex &&
           A.Range.VertexPool == B.Range.VertexPool && A.Range.IndexPool == B.Range.IndexPool &&
           A.pMaterial == B.pMaterial && A.MaterialOverride == B.MaterialOverride &&
           BuildBatchStateFlags( A.RenderFlags ) == BuildBatchStateFlags( B.RenderFlags ) &&
           ( ( A.InstFeatures ^ B.InstFeatures ) & render::GEOM_INST_FORCED_GLOW ) == 0 && A.UOffset == B.UOffset &&
           A.VOffset == B.VOffset && A.LightingMode == B.LightingMode && A.Projection == B.Projection &&
           A.ContributionPolicy == B.ContributionPolicy && A.Resources.pDiffuse == B.Resources.pDiffuse &&
           A.Resources.pDetail == B.Resources.pDetail && A.Resources.pEnvironment == B.Resources.pEnvironment &&
           A.Resources.pEnvironmentCube == B.Resources.pEnvironmentCube &&
           A.Resources.isDistortionActive == B.Resources.isDistortionActive &&
           IsPacketGlowCapable( FirstPacket ) == IsPacketGlowCapable( NextPacket );
}

//=========================================================================

void FrameCompiler::BeginForwardBatches( void )
{
    m_ForwardCommands.SetCount( 0 );
}

//=========================================================================

s32 FrameCompiler::AppendForwardCommands( s32 PacketIndex )
{
    const GeomDrawPacket& Packet = m_lDrawPackets[PacketIndex];
    const s32             Count  = Packet.Kind == GEOM_PACKET_SKIN ? Packet.SkinSectionCount : 1;

    if( Packet.Kind == GEOM_PACKET_DYNAMIC || Count <= 0 ||
        !ReserveArrayCapacity( m_ForwardCommands, static_cast<u64>( m_ForwardCommands.GetCount() ) + Count,
                               "forward indirect commands" ) )
    {
        return -1;
    }

    const s32 First = m_ForwardCommands.GetCount();

    for( s32 i = 0; i < Count; ++i )
    {
        rdraw_indexed_indirect_command& Command = m_ForwardCommands.Append();
        Command.IndexCount =
            Packet.Kind == GEOM_PACKET_SKIN ? Packet.pSkinSections[i].IndexCount : Packet.Range.IndexCount;
        Command.InstanceCount = Packet.InstanceCount;
        Command.FirstIndex =
            Packet.Range.FirstIndex + ( Packet.Kind == GEOM_PACKET_SKIN ? Packet.pSkinSections[i].FirstIndex : 0 );
        Command.BaseVertex    = Packet.Range.BaseVertex;
        Command.FirstInstance = Packet.Kind == GEOM_PACKET_SKIN
                                    ? Packet.FirstSkinDrawInstance + i * Packet.InstanceCount
                                    : Packet.FirstInstance;
    }

    return First;
}

//=========================================================================

xbool FrameCompiler::UploadForwardCommands( FrameGraphBuilder& Graph )
{
    const u32 Count = m_ForwardCommands.GetCount();
    return !Count ||
           ( EnsureBuffer( m_ForwardCommandBuffer, m_ForwardCommandCapacity, Count,
                           sizeof( rdraw_indexed_indirect_command ), "ForwardIndirect", RBUFFER_USAGE_INDIRECT ) &&
             Graph.QueueUpload( m_ForwardCommandBuffer, m_ForwardCommands.GetPtr(),
                             Count * sizeof( rdraw_indexed_indirect_command ), 0, TRUE ) );
}

//=========================================================================

xbool FrameCompiler::ExecutePacket( s32                      PacketIndex,
                                    const geom_pass_desc&    Pass,
                                    const BackendPassInputs& Inputs,
                                    u32                      FirstCommand,
                                    u32                      CommandCount )
{
    geom_pass_desc PassData = Pass;

    if( ( PacketIndex < 0 ) || ( PacketIndex >= m_lDrawPackets.GetCount() ) || ( PassData.TargetWidth == 0 ) ||
        ( PassData.TargetHeight == 0 ) )
    {
        RENDER_THROW( "FrameCompiler", "invalid direct packet %d/%d extent %ux%u", PacketIndex,
                      m_lDrawPackets.GetCount(), PassData.TargetWidth, PassData.TargetHeight );
        return FALSE;
    }

    if( !ApplyPassInputs( Inputs ) )
    {
        return FALSE;
    }

    const GeomDrawPacket& Packet = m_lDrawPackets[PacketIndex];

    if( Packet.ContributionPolicy == render::RENDER_CONTRIBUTION_SUPPRESS_BLOOM )
    {
        PassData.BloomFogMode = RENDER_FOG_NONE;
    }

    if( Packet.Kind == GEOM_PACKET_DYNAMIC )
    {
        return ExecuteDynamicPacket( Packet, PassData );
    }

    const xbool WriteFixedGlowMask = PassData.DepthMode != GEOM_DEPTH_PREPASS &&
                                     !PassData.WriteGlowMask && !Packet.MaterialOverride && Packet.pMaterial &&
                                     ( Packet.pMaterial->m_Type == Material_Alpha_PerPolyIllum ) &&
                                     ( Packet.ContributionPolicy != render::RENDER_CONTRIBUTION_SUPPRESS_BLOOM ) &&
                                     !( Packet.RenderFlags & ( render::FADING_ALPHA | render::GLOWING ) ) &&
                                     !( Packet.InstFeatures & render::GEOM_INST_FORCED_GLOW );
    const u32 MaterialRenderFlags =
        Packet.RenderFlags | ( ( Packet.InstFeatures & render::GEOM_INST_FORCED_GLOW ) ? render::GLOWING : 0u );

    m_pDiffuseResource         = Packet.Resources.pDiffuse;
    m_pDetailResource          = Packet.Resources.pDetail;
    m_pEnvironmentResource     = Packet.Resources.pEnvironment;
    m_pEnvironmentCubeResource = Packet.Resources.pEnvironmentCube;
    m_isDistortionStateActive  = Packet.Resources.isDistortionActive;
    m_DistortionNormalRot      = Packet.Resources.DistortionNormalRot;

    if( Packet.Kind == GEOM_PACKET_RIGID )
    {
        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMaterial" );

            if( !SetRigidMaterial( Packet.pMaterial, MaterialRenderFlags, Packet.UOffset, Packet.VOffset,
                                   Packet.MaterialOverride, IsPacketGlowCapable( PacketIndex ), PassData,
                                   Packet.LightingMode, Packet.Projection, PacketIndex ) )
            {
                RENDER_THROW( "FrameCompiler", "rigid packet %d failed to bind material", PacketIndex );
                return FALSE;
            }
        }

        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindResources" );

            const xbool DepthOnly = ( Packet.MaterialOverride || PassData.DepthMode == GEOM_DEPTH_PREPASS ) &&
                                    !PassData.WriteGlowMask;

            if( !BindPacketResources( Packet, DepthOnly ) || !BindRigidFrameBuffers( DepthOnly ) )
            {
                RENDER_THROW( "FrameCompiler", "rigid packet %d failed to bind resources", PacketIndex );
                return FALSE;
            }
        }

        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMesh" );

            if( ( Packet.Range.IndexCount <= 0 ) ||
                !BindRigidGeomBuffers( Packet.Range ) )
            {
                RENDER_THROW( "FrameCompiler", "rigid packet %d failed to bind mesh (indices %d)", PacketIndex,
                              Packet.Range.IndexCount );
                return FALSE;
            }
        }

        xbool isDrawn;
        {
            X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/Draw" );

            isDrawn = CommandCount ? rdraw_DrawIndexedIndirect( m_ForwardCommandBuffer,
                                                                FirstCommand * sizeof( rdraw_indexed_indirect_command ),
                                                                CommandCount )
                                   : rdraw_DrawIndexedInstanced( Packet.Range.IndexCount, Packet.InstanceCount,
                                                                 Packet.Range.FirstIndex, Packet.Range.BaseVertex,
                                                                 Packet.FirstInstance );
        }

        if( isDrawn && ( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE ) )
        {
            m_nForwardOpaqueGpuDraws++;
            m_nForwardOpaqueInstances += Packet.InstanceCount;
            m_nForwardOpaqueSubmittedIndices += static_cast<u64>( Packet.Range.IndexCount ) * Packet.InstanceCount;
        }

        if( !isDrawn )
        {
            RENDER_THROW( "FrameCompiler", "rigid packet %d draw rejected", PacketIndex );
        }

        if( isDrawn && WriteFixedGlowMask )
        {
            geom_pass_desc MaskPass = PassData;
            MaskPass.WriteGlowMask  = TRUE;
            return ExecutePacket( PacketIndex, MaskPass, Inputs, FirstCommand, CommandCount );
        }

        return isDrawn;
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMaterial" );

        if( !SetSkinMaterial( Packet.pMaterial, MaterialRenderFlags, Packet.UOffset, Packet.VOffset,
                              Packet.MaterialOverride, IsPacketGlowCapable( PacketIndex ), PassData,
                              Packet.LightingMode, Packet.Projection, PacketIndex ) )
        {
            RENDER_THROW( "FrameCompiler", "skin packet %d failed to bind material", PacketIndex );
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindResources" );

        const xbool DepthOnly = ( Packet.MaterialOverride || PassData.DepthMode == GEOM_DEPTH_PREPASS ) &&
                                !PassData.WriteGlowMask;

        if( !BindPacketResources( Packet, DepthOnly ) || !BindSkinFrameBuffers( DepthOnly ) )
        {
            RENDER_THROW( "FrameCompiler", "skin packet %d failed to bind resources", PacketIndex );
            return FALSE;
        }
    }

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/BindMesh" );

        if( !Packet.pSkinSections || ( Packet.SkinSectionCount <= 0 ) || !BindSkinGeomBuffers( Packet ) )
        {
            RENDER_THROW( "FrameCompiler", "skin packet %d failed to bind mesh", PacketIndex );
            return FALSE;
        }
    }

    if( CommandCount )
    {
        return rdraw_DrawIndexedIndirect( m_ForwardCommandBuffer,
                                          FirstCommand * sizeof( rdraw_indexed_indirect_command ), CommandCount );
    }

    xbool isAnyDraw = FALSE;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/SkinSections" );

        for( s32 S = 0; S < Packet.SkinSectionCount; ++S )
        {
            const SkinnedGeometryStorage::SoftSection& Section = Packet.pSkinSections[S];
            xbool                                      isDrawn;
            {
                X_PROFILE_SCOPE_CATEGORY( "Renderer", "Geom/Draw" );

                isDrawn = rdraw_DrawIndexedInstanced(
                    Section.IndexCount, Packet.InstanceCount, Packet.Range.FirstIndex + Section.FirstIndex,
                    Packet.Range.BaseVertex, Packet.FirstSkinDrawInstance + ( S * Packet.InstanceCount ) );
            }

            if( !isDrawn )
            {
                RENDER_THROW( "FrameCompiler", "skin packet %d section %d draw rejected", PacketIndex, S );
                return FALSE;
            }

            isAnyDraw = TRUE;

            if( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE )
            {
                m_nForwardOpaqueGpuDraws++;
                m_nForwardOpaqueSkinSectionDraws++;
                m_nForwardOpaqueSubmittedIndices += static_cast<u64>( Section.IndexCount ) * Packet.InstanceCount;
            }
        }
    }

    if( isAnyDraw && ( Packet.Pass == GEOM_PASS_FORWARD_OPAQUE ) )
    {
        m_nForwardOpaqueInstances += Packet.InstanceCount;
    }

    if( isAnyDraw && WriteFixedGlowMask )
    {
        geom_pass_desc MaskPass = PassData;
        MaskPass.WriteGlowMask  = TRUE;
        return ExecutePacket( PacketIndex, MaskPass, Inputs, FirstCommand, CommandCount );
    }

    return TRUE;
}

//=========================================================================
//  GENERAL STATE HELPERS
//=========================================================================

FrameCompiler::RenderStateSelection FrameCompiler::BuildMaterialPipelineState( const material*       pMaterial,
                                                                               u32                   RenderFlags,
                                                                               u8                    OverrideMat,
                                                                               xbool                 WriteGlowAlpha,
                                                                               const geom_pass_desc& Pass,
                                                                               LocalLightingMode     Lighting,
                                                                               xbool                 Projection ) const
{
    const xbool DepthPrepass = Pass.DepthMode == GEOM_DEPTH_PREPASS;
    RenderStateSelection State =
        ResolveRenderStates( pMaterial, RenderFlags, OverrideMat, WriteGlowAlpha );

    if( DepthPrepass )
    {
        State.Blend = RSTATE_BLEND_PRESET_NONE;
        State.Depth = RSTATE_DEPTH_PRESET_NORMAL;
    }
    else if( Pass.DepthMode == GEOM_DEPTH_READ_ONLY )
    {
        State.Depth = RSTATE_DEPTH_PRESET_NO_WRITE;
    }

    if( Pass.WriteGlowMask )
    {
        State.Blend = RSTATE_BLEND_PRESET_ALPHA_WRITE_ONLY;
        State.Depth = RSTATE_DEPTH_PRESET_NO_WRITE;
    }

    State.ColorFormat     = Pass.ColorFormat;
    State.DepthFormat     = Pass.DepthFormat;
    State.SampleCount     = Pass.SampleCount;
    State.WriteSceneDepth = Pass.WriteSceneDepth && !DepthPrepass;

    const MaterialDesc Material = pMaterial ? pMaterial->GetDesc() : MaterialDesc();
    DrawShaderFeatures Draw;
    Draw.SetLocalLighting( Lighting );
    Draw.ForcedGlow = ( RenderFlags & render::GLOWING ) != 0;
    Draw.Fade       = ( RenderFlags & render::FADING_ALPHA ) != 0;
    Draw.Fog        = Pass.SceneFogMode != RENDER_FOG_NONE || Pass.BloomFogMode != RENDER_FOG_NONE;
    Draw.Projection = Projection;
    Draw.Shadows    = Material.ReceivesShadow && Pass.EnableShadows &&
                   m_pContext->pShadowAtlas->GetShadowMode() != ShadowMode::Disabled;

    ShaderPassContract ShaderPass;
    ShaderPass.Outputs = DepthPrepass && Pass.WriteNormals     ? ShaderOutputs::LinearDepthNormals
                         : DepthPrepass                       ? ShaderOutputs::LinearDepth
                         : OverrideMat && !Pass.WriteGlowMask ? ShaderOutputs::Depth
                         : Pass.WriteGlowMask                 ? ShaderOutputs::GlowMask
                         : Pass.WriteSceneDepth               ? ShaderOutputs::SceneColorDepth
                                                              : ShaderOutputs::SceneColor;
    State.Program = ResolveShaderProgram( Material, ShaderFamily::Surface, VertexContract::Rigid, Draw, ShaderPass );

    return State;
}

//=========================================================================

xbool FrameCompiler::QueueMaterialPipelines( const material& Material, xbool Skin, const geom_pass_desc& Pass )
{
    const u32 Flags[] = { 0, render::FADING_ALPHA, render::GLOWING, render::FADING_ALPHA | render::GLOWING };
    const MaterialDesc& Desc = Material.GetDesc();
    const xbool EnableShadows = Pass.EnableShadows && Desc.ReceivesShadow &&
                                m_pContext->pShadowAtlas->GetShadowMode() != ShadowMode::Disabled;

    for( u32 Projection = 0; Projection <= static_cast<u32>( !!Desc.ReceivesProjection ); ++Projection )
    {
        for( u32 Lighting = 0; Lighting < 3; ++Lighting )
        {
            const xbool LocalShadows = EnableShadows && Lighting != static_cast<u32>( LocalLightingMode::Ambient );

            for( u32 Shadows = 0; Shadows <= static_cast<u32>( LocalShadows ); ++Shadows )
            {
                for( u32 i = 0; i < ARRAYSIZE( Flags ); ++i )
                {
                    const xbool WriteGlowAlpha = !( Flags[i] & render::FADING_ALPHA ) &&
                        Material.GetBlendMode() != MATERIAL_BLEND_OPAQUE &&
                        ( Material.m_Type != Material_Alpha_PerPolyIllum || ( Flags[i] & render::GLOWING ) );

                    for( u32 Fog = 0; Fog < 2; ++Fog )
                    {
                        for( u32 Glow = 0; Glow <= static_cast<u32>( WriteGlowAlpha ); ++Glow )
                        {
                            geom_pass_desc DrawPass = Pass;
                            DrawPass.EnableShadows  = Shadows != 0;
                            DrawPass.SceneFogMode   = Fog ? RENDER_FOG_FORWARD_COLOR : RENDER_FOG_NONE;
                            DrawPass.WriteSceneDepth =
                                Pass.WriteSceneDepth && ClassifyGeomPass( Material, Flags[i],
                                                                          ( Flags[i] & render::FADING_ALPHA ) != 0 ) ==
                                                            GEOM_PASS_FORWARD_OPAQUE;

                            RenderStateSelection State = BuildMaterialPipelineState(
                                &Material, Flags[i], FALSE, Glow != 0, DrawPass,
                                static_cast<LocalLightingMode>( Lighting ), Projection != 0 );

                            if( !( Skin ? GetSkinPipeline( State, TRUE ) : GetRigidPipeline( State, TRUE ) ) )
                            {
                                return FALSE;
                            }

                            if( DrawPass.WriteSceneDepth )
                            {
                                DrawPass.WriteSceneDepth = FALSE;
                                DrawPass.DepthMode       = GEOM_DEPTH_READ_ONLY;
                                State = BuildMaterialPipelineState( &Material, Flags[i], FALSE, Glow != 0,
                                                                     DrawPass,
                                                                     static_cast<LocalLightingMode>( Lighting ),
                                                                     Projection != 0 );
                                if( !( Skin ? GetSkinPipeline( State, TRUE ) : GetRigidPipeline( State, TRUE ) ) )
                                {
                                    return FALSE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    RenderStateSelection State;

    for( u32 DepthOutput = 0; DepthOutput < 3; ++DepthOutput )
    {
        if( DepthOutput && !Pass.WriteSceneDepth )
        {
            continue;
        }

        geom_pass_desc DepthPass  = Pass;
        DepthPass.WriteSceneDepth = FALSE;
        DepthPass.DepthMode       = DepthOutput ? GEOM_DEPTH_PREPASS : GEOM_DEPTH_DEFAULT;
        DepthPass.ColorFormat     = DepthOutput ? RTARGET_FORMAT_R32F : Pass.ColorFormat;
        DepthPass.WriteNormals    = DepthOutput == 2;
        State = BuildMaterialPipelineState( &Material, 0, !DepthOutput, FALSE, DepthPass, LocalLightingMode::Ambient, FALSE );

        if( !( Skin ? GetSkinPipeline( State, TRUE ) : GetRigidPipeline( State, TRUE ) ) )
        {
            return FALSE;
        }
    }

    if( Material.m_Type != Material_Alpha_PerPolyIllum )
    {
        return TRUE;
    }

    geom_pass_desc MaskPass  = Pass;
    MaskPass.WriteSceneDepth = FALSE;
    MaskPass.WriteGlowMask   = TRUE;
    State = BuildMaterialPipelineState( &Material, 0, FALSE, FALSE, MaskPass, LocalLightingMode::Ambient, FALSE );

    return ( Skin ? GetSkinPipeline( State, TRUE ) : GetRigidPipeline( State, TRUE ) ) != NULL;
}

//=========================================================================

xbool FrameCompiler::QueueDefaultMaterialPipelines( const geom_pass_desc& Pass )
{
    return QueueMaterialPipelines( m_DefaultDistortionMaterial, FALSE, Pass ) &&
           QueueMaterialPipelines( m_DefaultDistortionMaterial, TRUE, Pass );
}

//=========================================================================

xbool FrameCompiler::QueueDynamicPipeline( const geom_pass_desc& Pass, u32 RenderFlags )
{
    geom_pass_desc DepthPass = Pass;
    DepthPass.ColorFormat     = RTARGET_FORMAT_R32F;
    DepthPass.DepthMode       = GEOM_DEPTH_PREPASS;
    DepthPass.WriteSceneDepth = FALSE;
    DepthPass.EnableShadows   = FALSE;
    DepthPass.SceneFogMode    = RENDER_FOG_NONE;
    DepthPass.BloomFogMode    = RENDER_FOG_NONE;

    for( u32 Normals = 0; Normals < 2; ++Normals )
    {
        DepthPass.WriteNormals = Normals != 0;
        const RenderStateSelection DepthState =
            BuildDynamicPipelineState( RenderFlags, DepthPass, LocalLightingMode::Ambient, FALSE );
        if( !GetDynamicPipeline( DepthState, TRUE ) )
        {
            return FALSE;
        }
    }

    const xbool EnableShadows = Pass.EnableShadows &&
                                m_pContext->pShadowAtlas->GetShadowMode() != ShadowMode::Disabled;

    for( u32 Projection = 0; Projection < 2; ++Projection )
    {
        for( u32 Lighting = 0; Lighting < 3; ++Lighting )
        {
            const xbool LocalShadows = EnableShadows && Lighting != static_cast<u32>( LocalLightingMode::Ambient );

            for( u32 Shadows = 0; Shadows <= static_cast<u32>( LocalShadows ); ++Shadows )
            {
                for( u32 Glow = 0; Glow < 2; ++Glow )
                {
                    for( u32 Fog = 0; Fog < 2; ++Fog )
                    {
                        const u32 Flags = RenderFlags | ( Glow ? render::GLOWING : 0 );

                        geom_pass_desc DrawPass = Pass;
                        DrawPass.SceneFogMode  = Fog ? RENDER_FOG_FORWARD_COLOR : RENDER_FOG_NONE;
                        DrawPass.EnableShadows = Shadows != 0;
                        RenderStateSelection State = BuildDynamicPipelineState(
                            Flags, DrawPass, static_cast<LocalLightingMode>( Lighting ), Projection != 0 );
                        if( !GetDynamicPipeline( State, TRUE ) )
                        {
                            return FALSE;
                        }

                        DrawPass.WriteSceneDepth = FALSE;
                        DrawPass.DepthMode       = GEOM_DEPTH_READ_ONLY;
                        State = BuildDynamicPipelineState( Flags, DrawPass,
                                                            static_cast<LocalLightingMode>( Lighting ), Projection != 0 );
                        if( !GetDynamicPipeline( State, TRUE ) )
                        {
                            return FALSE;
                        }
                    }
                }
            }
        }
    }

    return TRUE;
}

//=========================================================================

void FrameCompiler::ReleaseLevelPipelines( void )
{
    ClearPackets();
    m_RigidPipelines.Reset();
    m_SkinPipelines.Reset();
    m_DynamicPipelines.Reset();
    DestroySurfacePrograms();
}

//=========================================================================

FrameCompiler::RenderStateSelection FrameCompiler::ResolveRenderStates( const material* pMaterial,
                                                                        u32             RenderFlags,
                                                                        u8              OverrideMat,
                                                                        xbool           WriteGlowAlpha ) const
{
    RenderStateSelection PipelineState;

    if( OverrideMat )
    {
        PipelineState.Blend  = RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE;
        PipelineState.Depth  = RSTATE_DEPTH_PRESET_NORMAL;
        PipelineState.Raster = RSTATE_RASTER_PRESET_SOLID_NO_CULL;

        return PipelineState;
    }

    const u32   FadeMask     = render::FADING_ALPHA;
    const xbool BFadingAlpha = !!( RenderFlags & FadeMask );

    if( pMaterial && ( pMaterial->m_Type == Material_Alpha_PerPolyIllum ) && !( RenderFlags & render::GLOWING ) )
    {
        WriteGlowAlpha = FALSE;
    }

    if( BFadingAlpha )
    {
        PipelineState.Blend = RSTATE_BLEND_PRESET_PREMULT_ALPHA_RGB;
    }
    else
    {
        const material_blend_mode BlendMode = pMaterial ? pMaterial->GetBlendMode() : MATERIAL_BLEND_OPAQUE;

        switch( BlendMode )
        {
            case MATERIAL_BLEND_ALPHA:
            {
                PipelineState.Blend =
                    WriteGlowAlpha ? RSTATE_BLEND_PRESET_PREMULT_ALPHA : RSTATE_BLEND_PRESET_PREMULT_ALPHA_RGB;
            }
            break;
            case MATERIAL_BLEND_ADDITIVE:
            {
                PipelineState.Blend =
                    WriteGlowAlpha ? RSTATE_BLEND_PRESET_PREMULT_ADD : RSTATE_BLEND_PRESET_PREMULT_ADD_RGB;
            }
            break;
            case MATERIAL_BLEND_SUBTRACTIVE:
            {
                PipelineState.Blend =
                    WriteGlowAlpha ? RSTATE_BLEND_PRESET_PREMULT_SUB : RSTATE_BLEND_PRESET_PREMULT_SUB_RGB;
            }
            break;
            case MATERIAL_BLEND_DISTORTION:
            {
                PipelineState.Blend = WriteGlowAlpha ? RSTATE_BLEND_PRESET_NONE : RSTATE_BLEND_PRESET_NONE_RGB;
            }
            break;
            case MATERIAL_BLEND_OPAQUE:
            {
                PipelineState.Blend = RSTATE_BLEND_PRESET_NONE;
            }
            break;

            default:
            {
                ASSERT( FALSE );
            }
            break;
        }
    }

    if( pMaterial && pMaterial->ClampsSampler() )
    {
        PipelineState.Sampler = RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    }

    const material_blend_mode ResolvedBlendMode  = pMaterial ? pMaterial->GetBlendMode() : MATERIAL_BLEND_OPAQUE;
    const xbool               BBlended           = BFadingAlpha || ( ResolvedBlendMode != MATERIAL_BLEND_OPAQUE );
    const xbool               BDisableDepthWrite = BBlended || ( pMaterial && !pMaterial->WritesDepth() );

    PipelineState.Depth = BDisableDepthWrite ? RSTATE_DEPTH_PRESET_NO_WRITE : RSTATE_DEPTH_PRESET_NORMAL;

    const xbool BWireframe =
        ( RenderFlags & ( render::WIREFRAME | render::WIREFRAME_NO_CULL | render::DEBUG_RENDER_PULSED ) ) != 0;
    const xbool BWireframeNoCull = ( RenderFlags & render::WIREFRAME_NO_CULL ) != 0;
    const xbool BDisableCull     = pMaterial && !pMaterial->CullsBackFaces();

    if( BWireframe )
    {
        PipelineState.Raster =
            ( BWireframeNoCull || BDisableCull ) ? RSTATE_RASTER_PRESET_WIRE_NO_CULL : RSTATE_RASTER_PRESET_WIRE;
    }
    else
    {
        PipelineState.Raster = BDisableCull ? RSTATE_RASTER_PRESET_SOLID_NO_CULL : RSTATE_RASTER_PRESET_SOLID;
    }

    return PipelineState;
}

//=========================================================================

xbool FrameCompiler::BuildGeometryPipelineDesc( render_pipeline_desc&       Desc,
                                                const RenderStateSelection& PipelineState,
                                                VertexContract              Vertex,
                                                xbool                       IsPrewarm )
{
    static xprofile_counter Descriptors =
        x_GetProfiler().RegisterCounter( "GeometryPipelineDescriptors", "RenderCounter" );
    Descriptors.Add();
    ASSERT( Vertex == VertexContract::Rigid || Vertex == VertexContract::Skin );
    static shader_vertex_element RigidLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, 0 ),
                                                   shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT3, 12 ),
                                                   shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, 24 ),
                                                   shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ),
                                                   shader_vertex_element( 4, 2, SHADER_VERTEX_FORMAT_UINT1, 0 ) };

    static shader_vertex_buffer_desc RigidVertexBuffers[3];
    RigidVertexBuffers[0].Slot   = 0;
    RigidVertexBuffers[0].Stride = sizeof( rigid_geom::vertex );

    RigidVertexBuffers[1].Slot         = 1;
    RigidVertexBuffers[1].Stride       = sizeof( u32 );
    RigidVertexBuffers[1].bPerInstance = TRUE;

    RigidVertexBuffers[2].Slot   = 2;
    RigidVertexBuffers[2].Stride = sizeof( u32 );

    static shader_vertex_element SkinLayout[] = { shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT4, 0 ),
                                                  shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT4, 16 ),
                                                  shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT4, 32 ),
                                                  shader_vertex_element( 3, 1, SHADER_VERTEX_FORMAT_UINT1, 0 ),
                                                  shader_vertex_element( 4, 1, SHADER_VERTEX_FORMAT_UINT1, 4 ) };

    static shader_vertex_buffer_desc SkinVertexBuffers[2];
    SkinVertexBuffers[0].Slot   = 0;
    SkinVertexBuffers[0].Stride = sizeof( SkinGpuVertex );

    SkinVertexBuffers[1].Slot         = 1;
    SkinVertexBuffers[1].Stride       = sizeof( SkinDrawInstance );
    SkinVertexBuffers[1].bPerInstance = TRUE;

    ShaderProgramKey ProgramKey = PipelineState.Program;
    ProgramKey.Vertex           = Vertex;

    SurfaceProgram* pProgram = GetSurfaceProgram( ProgramKey, IsPrewarm );

    if( !pProgram )
    {
        return FALSE;
    }

    Desc.Shader.pVertexShader  = &pProgram->pVertex->Shader;
    Desc.Shader.pPixelShader   = &pProgram->pPixel->Shader;
    Desc.Shader.pVertexBuffers = Vertex == VertexContract::Skin ? SkinVertexBuffers : RigidVertexBuffers;
    Desc.Shader.VertexBufferCount =
        Vertex == VertexContract::Skin ? ARRAYSIZE( SkinVertexBuffers ) : ARRAYSIZE( RigidVertexBuffers );
    Desc.Shader.pInputElements    = Vertex == VertexContract::Skin ? SkinLayout : RigidLayout;
    Desc.Shader.InputElementCount = Vertex == VertexContract::Skin ? ARRAYSIZE( SkinLayout ) : ARRAYSIZE( RigidLayout );
    Desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Desc.Depth                    = rstate_GetDepthDesc( PipelineState.Depth );
    Desc.Raster                   = rstate_GetRasterDesc( PipelineState.Raster );
    const xbool WriteNormals      = PipelineState.Program.Outputs == ShaderOutputs::LinearDepthNormals;
    Desc.ColorCount               = PipelineState.WriteSceneDepth || WriteNormals ? 2 : 1;
    Desc.ColorTargets[0].Format   = PipelineState.ColorFormat;
    Desc.ColorTargets[0].Blend    = rstate_GetBlendDesc( PipelineState.Blend );

    if( WriteNormals )
    {
        Desc.ColorTargets[1].Format = RTARGET_FORMAT_RGBA8;
        Desc.ColorTargets[1].Blend.ColorWriteMask = RSTATE_COLOR_WRITE_ALL;
    }
    else if( PipelineState.WriteSceneDepth )
    {
        Desc.ColorTargets[1].Format = RTARGET_FORMAT_R32F;
        Desc.ColorTargets[1].Blend.ColorWriteMask =
            Desc.Depth.bDepthWrite ? RSTATE_COLOR_WRITE_RED : RSTATE_COLOR_WRITE_NONE;
    }

    Desc.DepthFormat = PipelineState.DepthFormat;
    Desc.SampleCount = PipelineState.SampleCount;
    Desc.pDebugName  = Vertex == VertexContract::Skin ? "GeomSkinForward" : "GeomRigidForward";

    return TRUE;
}

//=========================================================================

render_pipeline* FrameCompiler::GetRigidPipeline( const RenderStateSelection& PipelineState, xbool isPrewarm )
{
    ShaderProgramKey Program = PipelineState.Program;
    Program.Vertex           = VertexContract::Rigid;

    const u64 State =
        GraphicsPipelineStateKey( PipelineState.ColorFormat, PipelineState.DepthFormat, PipelineState.SampleCount,
                                  PipelineState.Blend, PipelineState.Depth, PipelineState.Raster );
    render_pipeline* pPipeline = m_RigidPipelines.FindPrepared( Program, State, isPrewarm );

    if( pPipeline )
    {
        return pPipeline;
    }

    render_pipeline_desc Desc;

    if( !BuildGeometryPipelineDesc( Desc, PipelineState, VertexContract::Rigid, isPrewarm ) )
    {
        return NULL;
    }

    pPipeline = isPrewarm ? m_RigidPipelines.QueuePrewarm( Desc ) : m_RigidPipelines.GetOrCreate( Desc );
    m_RigidPipelines.RememberPrepared( Program, State, pPipeline );

    return pPipeline;
}

//=========================================================================

xbool FrameCompiler::BindRigidPipeline( const RenderStateSelection& PipelineState, render_pipeline* pPrepared,
                                       const ShaderBindingLayout* pPreparedBindings )
{
    if( ( PipelineState.Program.Outputs == ShaderOutputs::LinearDepth ||
          PipelineState.Program.Outputs == ShaderOutputs::LinearDepthNormals ) && !pPrepared )
    {
        return FALSE;
    }

    render_pipeline* pPipeline = pPrepared ? pPrepared : GetRigidPipeline( PipelineState, FALSE );

    if( !pPipeline || !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }

    m_ActiveShaderKind = GEOM_SHADER_RIGID;

    if( !pPreparedBindings )
    {
        ShaderProgramKey ProgramKey = PipelineState.Program;
        ProgramKey.Vertex           = VertexContract::Rigid;

        SurfaceProgram* pProgram = GetSurfaceProgram( ProgramKey );

        if( !pProgram )
        {
            return FALSE;
        }

        pPreparedBindings = &pProgram->Bindings;
    }

    m_pActiveSurfaceBindings = pPreparedBindings;
    m_ActiveSamplerPreset    = PipelineState.Sampler;

    return TRUE;
}

//=========================================================================

render_pipeline* FrameCompiler::GetSkinPipeline( const RenderStateSelection& PipelineState, xbool isPrewarm )
{
    ShaderProgramKey Program = PipelineState.Program;
    Program.Vertex           = VertexContract::Skin;

    const u64 State =
        GraphicsPipelineStateKey( PipelineState.ColorFormat, PipelineState.DepthFormat, PipelineState.SampleCount,
                                  PipelineState.Blend, PipelineState.Depth, PipelineState.Raster );
    render_pipeline* pPipeline = m_SkinPipelines.FindPrepared( Program, State, isPrewarm );

    if( pPipeline )
    {
        return pPipeline;
    }

    render_pipeline_desc Desc;

    if( !BuildGeometryPipelineDesc( Desc, PipelineState, VertexContract::Skin, isPrewarm ) )
    {
        return NULL;
    }

    pPipeline = isPrewarm ? m_SkinPipelines.QueuePrewarm( Desc ) : m_SkinPipelines.GetOrCreate( Desc );
    m_SkinPipelines.RememberPrepared( Program, State, pPipeline );

    return pPipeline;
}

//=========================================================================

xbool FrameCompiler::BindSkinPipeline( const RenderStateSelection& PipelineState, render_pipeline* pPrepared,
                                      const ShaderBindingLayout* pPreparedBindings )
{
    if( ( PipelineState.Program.Outputs == ShaderOutputs::LinearDepth ||
          PipelineState.Program.Outputs == ShaderOutputs::LinearDepthNormals ) && !pPrepared )
    {
        return FALSE;
    }

    render_pipeline* pPipeline = pPrepared ? pPrepared : GetSkinPipeline( PipelineState, FALSE );

    if( !pPipeline || !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }

    m_ActiveShaderKind = GEOM_SHADER_SKIN;

    if( !pPreparedBindings )
    {
        ShaderProgramKey ProgramKey = PipelineState.Program;
        ProgramKey.Vertex           = VertexContract::Skin;

        SurfaceProgram* pProgram = GetSurfaceProgram( ProgramKey );

        if( !pProgram )
        {
            return FALSE;
        }

        pPreparedBindings = &pProgram->Bindings;
    }

    m_pActiveSurfaceBindings = pPreparedBindings;
    m_ActiveSamplerPreset    = PipelineState.Sampler;

    return TRUE;
}

//=========================================================================

void FrameCompiler::DestroyRigidPipelines( void )
{
    m_RigidPipelines.Reset();
}

//=========================================================================

void FrameCompiler::DestroySkinPipelines( void )
{
    m_SkinPipelines.Reset();
}

//=========================================================================

void FrameCompiler::SetBitmap( const texture* pTexture, TextureSlot SlotIndex )
{
    const char* pSlotName    = GeomTextureSlotName( SlotIndex );
    const char* pBindingName = GeomTextureBindingName( SlotIndex );

    if( !pBindingName )
    {
        RENDER_WARNING( "FrameCompiler", "SetBitmap does not manage slot %d (%s)", SlotIndex, pSlotName );
        return;
    }

    const shader_resource* pResource = NULL;

    if( pTexture )
    {
        pResource = pTexture->GetShaderResource();

        if( !pResource )
        {
            RENDER_WARNING( "FrameCompiler", "%s texture has no GPU resource", pSlotName );
        }
    }

    switch( SlotIndex )
    {
        case TEXTURE_SLOT_DIFFUSE:
        {
            m_pDiffuseResource = pResource;
        }
        break;
        case TEXTURE_SLOT_DETAIL:
        {
            m_pDetailResource = pResource;
        }
        break;
        case TEXTURE_SLOT_ENVIRONMENT:
        {
            m_pEnvironmentResource = pResource;
        }
        break;
        case TEXTURE_SLOT_ENVIRONMENT_CUBE:
        {
            m_pEnvironmentCubeResource = pResource;
        }
        break;
        default:
        {
        }
        break;
    }
}

//=========================================================================

void FrameCompiler::SetEnvironmentCubemap( const cubemap* pCubemap )
{
    const shader_resource* pResource = NULL;

    if( pCubemap )
    {
        pResource = pCubemap->GetShaderResource();

        if( !pResource )
        {
            RENDER_WARNING( "FrameCompiler", "cubemap has no GPU resource" );
        }
    }

    m_pEnvironmentCubeResource = pResource;
}

//=========================================================================

void FrameCompiler::ResetShadowTargets( void )
{
    const shader_resource* pWhite    = vram_GetShaderResource( m_WhiteTexture );
    m_pCachedFaceShadowResource      = pWhite;
    m_pCachedFaceShadowDepthResource = pWhite;
    m_areShadowMapConstantsValid     = FALSE;
}

//=========================================================================

void FrameCompiler::SetDistortionState( const radian3& NormalRot )
{
    m_isDistortionStateActive = TRUE;
    m_DistortionNormalRot     = NormalRot;
}

//=========================================================================

void FrameCompiler::ResetTransientState( void )
{
    m_isDistortionStateActive    = FALSE;
    m_pDistortionSceneResource   = NULL;
    m_pAmbientVisibilityResource = NULL;
    m_hasAmbientVisibility       = FALSE;
    m_DistortionNormalRot.Zero();
}

} // namespace render
