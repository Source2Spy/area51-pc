//=========================================================================
//
//  FrameCompiler_Skin.cpp
//
//  Skin material handling for geometry submission.
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
#include "../Resource/GeometryRepository.hpp"
#include "../Storage/SkinnedGeometryStorage.hpp"

namespace render
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool FrameCompiler::InitSkinShaders( void )
{
    x_DebugMsg( "FrameCompiler: Initializing skin shaders\n" );

    m_SkinInstanceCapacity     = 0;
    m_SkinBoneCapacity         = 0;
    m_SkinDrawInstanceCapacity = 0;
    m_SkinIndirectCapacity     = 0;
    ResetSkinBatch();

    x_DebugMsg( "FrameCompiler: Skin shaders initialized successfully\n" );

    return TRUE;
}

//=========================================================================

void FrameCompiler::KillSkinShaders( void )
{
    ResetSkinBatch();

    DestroySkinPipelines();

    m_SkinInstanceCapacity     = 0;
    m_SkinBoneCapacity         = 0;
    m_SkinDrawInstanceCapacity = 0;
    m_SkinIndirectCapacity     = 0;

    x_DebugMsg( "FrameCompiler: Skin shaders released\n" );
}

//=========================================================================

void FrameCompiler::ResetSkinBatch( void )
{
    m_SkinBatchFirstInstance      = m_lSkinFrameInstances.GetCount();
    m_hSkinBatchMesh.Handle       = HNULL;
    m_SkinBatchFlags              = 0;
    m_isSkinBatchForcedGlow       = FALSE;
    m_SkinBatchContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT;
    m_SkinBatchUOffset            = 0;
    m_SkinBatchVOffset            = 0;
    m_SkinBatchOverrideMat        = FALSE;
    m_SkinBatchSortDepth          = 0.0f;
}

//=========================================================================

xbool FrameCompiler::HasSkinBatch( void ) const
{
    return ( static_cast<u32>( m_lSkinFrameInstances.GetCount() ) > m_SkinBatchFirstInstance );
}

//=========================================================================

xbool FrameCompiler::CanAppendSkinBatch( const SkinBatchDesc& Desc ) const
{
    if( !HasSkinBatch() )
    {
        return TRUE;
    }

    return ( m_hSkinBatchMesh.Handle == Desc.hMesh.Handle ) &&
           ( !!( m_lSkinFrameInstances[m_SkinBatchFirstInstance].ShaderFlags &
                 ( GEOM_INST_FLAG_PROJ_LIGHT | GEOM_INST_FLAG_PROJ_SHADOW ) ) ==
             !!( BuildInstFlags( Desc.InstFeatures ) & ( GEOM_INST_FLAG_PROJ_LIGHT | GEOM_INST_FLAG_PROJ_SHADOW ) ) ) &&
           ( ResolveLocalLighting( m_pContext->pLighting->Get( m_lSkinFrameLighting[m_SkinBatchFirstInstance] ) ) ==
             ResolveLocalLighting( m_pContext->pLighting->Get( Desc.Lighting ) ) ) &&
           ( BuildBatchStateFlags( m_SkinBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) ) &&
           ( m_isSkinBatchForcedGlow == !!( Desc.InstFeatures & render::GEOM_INST_FORCED_GLOW ) ) &&
           ( m_SkinBatchContributionPolicy == Desc.ContributionPolicy ) && ( m_SkinBatchUOffset == Desc.UOffset ) &&
           ( m_SkinBatchVOffset == Desc.VOffset ) && ( m_SkinBatchOverrideMat == Desc.OverrideMat );
}

//=========================================================================

xbool FrameCompiler::SetSkinMaterial( const material*       pMaterial,
                                      u32                   RenderFlags,
                                      u8                    UOffset,
                                      u8                    VOffset,
                                      u8                    OverrideMat,
                                      xbool                 WriteGlowAlpha,
                                      const geom_pass_desc& PassData,
                                      LocalLightingMode     Lighting,
                                      xbool                 Projection,
                                      s32                   PacketIndex )
{
    geom_pass_desc DrawPass = PassData;
    DrawPass.EnableShadows  = PassData.EnableShadows && m_lDrawPackets[PacketIndex].Shadows;

    const PreparedMaterialPipeline* pPrepared = FindPreparedMaterialState( m_lDrawPackets[PacketIndex], DrawPass );
    xbool                           Bound;

    if( pPrepared )
    {
        Bound = BindSkinPipeline( pPrepared->State, pPrepared->pPipeline, pPrepared->pBindings );
    }
    else
    {
        // Glow-mask and other execution-only pass variants still need their own selection.
        const RenderStateSelection PipelineState = BuildMaterialPipelineState(
            pMaterial, RenderFlags, OverrideMat, WriteGlowAlpha, DrawPass, Lighting, Projection );
        Bound = BindSkinPipeline( PipelineState, FindPreparedMaterialPipeline( PacketIndex, PipelineState ) );
    }

    if( !Bound )
    {
        RENDER_THROW( "FrameCompiler", "failed to bind skin render pipeline" );
        return FALSE;
    }

    OverrideMat = OverrideMat || PassData.DepthMode == GEOM_DEPTH_PREPASS;
    if( !UpdateSkinConstants( pMaterial, UOffset, VOffset, OverrideMat, PassData ) )
    {
        RENDER_THROW( "FrameCompiler", "failed to update skin constants" );
        return FALSE;
    }

    if( OverrideMat )
    {
        if( !ResetProjTextures() )
        {
            return FALSE;
        }

        return TRUE;
    }

    if( !pMaterial )
    {
        if( !ResetProjTextures() )
        {
            return FALSE;
        }

        return TRUE;
    }

    if( pMaterial->ReceivesProjection() )
    {
        if( !UpdateProjTextures() )
        {
            return FALSE;
        }
    }
    else if( !ResetProjTextures() )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

s32 FrameCompiler::FindSkinPoseSlot( const matrix4* pBones, s32 BoneCount ) const
{
    ASSERT( m_SkinPoseSlots.GetCount() > 0 );
    u64 Hash = static_cast<u64>( reinterpret_cast<uaddr>( pBones ) ) >> 4;
    Hash ^= static_cast<u64>( BoneCount ) * 2654435761u;

    const u32 Mask = static_cast<u32>( m_SkinPoseSlots.GetCount() ) - 1u;
    s32       Slot = static_cast<s32>( Hash & Mask );

    while( m_SkinPoseSlots[Slot] >= 0 )
    {
        const SkinPose& Pose = m_SkinPoses[m_SkinPoseSlots[Slot]];

        if( Pose.pBones == pBones && Pose.BoneCount == BoneCount )
        {
            break;
        }

        Slot = static_cast<s32>( ( static_cast<u32>( Slot ) + 1u ) & Mask );
    }

    return Slot;
}

//=========================================================================

void FrameCompiler::AddSkinBatchInst( const SkinBatchDesc& Desc )
{
    ASSERT( Desc.pGeom );
    ASSERT( x_isvalid( Desc.SortDepth ) );
    ASSERT( ( Desc.iSubMesh >= 0 ) && ( Desc.iSubMesh < Desc.pGeom->m_nSubMeshes ) );

    if( !HasSkinBatch() )
    {
        m_hSkinBatchMesh              = Desc.hMesh;
        m_SkinBatchFlags              = Desc.RenderFlags;
        m_isSkinBatchForcedGlow       = !!( Desc.InstFeatures & render::GEOM_INST_FORCED_GLOW );
        m_SkinBatchContributionPolicy = Desc.ContributionPolicy;
        m_SkinBatchUOffset            = Desc.UOffset;
        m_SkinBatchVOffset            = Desc.VOffset;
        m_SkinBatchOverrideMat        = Desc.OverrideMat;
        m_SkinBatchSortDepth          = Desc.SortDepth;
    }
    else
    {
        ASSERT( m_hSkinBatchMesh.Handle == Desc.hMesh.Handle );
        ASSERT( BuildBatchStateFlags( m_SkinBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) );
        ASSERT( m_isSkinBatchForcedGlow == !!( Desc.InstFeatures & render::GEOM_INST_FORCED_GLOW ) );
        ASSERT( m_SkinBatchContributionPolicy == Desc.ContributionPolicy );
        ASSERT( m_SkinBatchUOffset == Desc.UOffset );
        ASSERT( m_SkinBatchVOffset == Desc.VOffset );
        ASSERT( m_SkinBatchOverrideMat == Desc.OverrideMat );
    }

    SkinInstanceData& GpuInst = m_lSkinFrameInstances.Append();
    x_memset( &GpuInst, 0, sizeof( GpuInst ) );

    GpuInst.ShaderFlags = BuildInstFlags( Desc.InstFeatures );
    GpuInst.BoneOffset  = 0;
    GpuInst.FadeAlpha   = BuildInstFadeAlpha( Desc.RenderFlags, Desc.Alpha );

    m_lSkinFrameLighting.Append() = Desc.Lighting;

    const cb_geom_lighting* pLighting = m_pContext->pLighting->Get( Desc.Lighting );

    if( pLighting )
    {
        m_nLitInstances++;
        m_nInstanceLights += MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );
    }

    const s32 nBones = Desc.pGeom->m_nBones;

    if( nBones <= 0 )
    {
        return;
    }

    const s32 PoseIndex = m_SkinPoseSlots[FindSkinPoseSlot( Desc.pBones, nBones )];
    ASSERT( PoseIndex >= 0 );
    GpuInst.BoneOffset = m_SkinPoses[PoseIndex].FirstBone;
}

//=========================================================================

void FrameCompiler::FlushSkinBatch( const material*  pMaterial,
                                    u8               MaterialOverride,
                                    geom_render_pass PassData,
                                    u32              Sequence )
{
    if( !HasSkinBatch() )
    {
        ResetSkinBatch();
        return;
    }

    GeomDrawPacket Packet;
    Packet.Kind               = GEOM_PACKET_SKIN;
    Packet.pMaterial          = pMaterial;
    Packet.hMesh              = m_hSkinBatchMesh;
    Packet.RenderFlags        = m_SkinBatchFlags;
    Packet.ContributionPolicy = m_SkinBatchContributionPolicy;
    Packet.FirstInstance      = m_SkinBatchFirstInstance;
    Packet.InstanceCount      = m_lSkinFrameInstances.GetCount() - m_SkinBatchFirstInstance;
    Packet.UOffset            = m_SkinBatchUOffset;
    Packet.VOffset            = m_SkinBatchVOffset;
    Packet.MaterialOverride   = MaterialOverride;
    Packet.Pass               = PassData;
    Packet.Sequence           = Sequence;
    Packet.SortDepth          = m_SkinBatchSortDepth;
    Packet.Resources          = CaptureResourceSnapshot();

    m_lDrawPackets.Append() = Packet;
    ResetSkinBatch();
}

//=========================================================================

xbool FrameCompiler::UpdateSkinConstants( const material*       pMaterial,
                                          u8                    UOffset,
                                          u8                    VOffset,
                                          u8                    OverrideMat,
                                          const geom_pass_desc& PassData )
{
    const view* pView = PassData.pView ? PassData.pView->GetSourceView() : NULL;

    if( !pView )
    {
        RENDER_THROW( "FrameCompiler", "UpdateSkinConstants called without an active view" );
        return FALSE;
    }

    const GeomFrameConstants FrameData = BuildFrameConstants( pMaterial, UOffset, VOffset, OverrideMat, PassData );

    const xbool DepthOnly = OverrideMat && !PassData.WriteGlowMask;

    if( !PushFrameConstants( GEOM_SHADER_SKIN, FrameData ) )
    {
        return FALSE;
    }

    if( !DepthOnly && !PushFogConstants( GEOM_SHADER_SKIN, PassData ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::BindSkinFrameBuffers( xbool DepthOnly ) const
{
    const ShaderBindingLayout& Layout = *GetShaderBindings( GEOM_SHADER_SKIN );
    const shader_resource*     pInstanceResource =
        rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_SKIN_INSTANCES ) );
    const shader_resource* pBoneResource = rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_SKIN_BONES ) );
    const shader_resource* pBoneRemapResource = m_pContext->pGeometry->GetSkinBoneRemapResource();
    const shader_resource* pLightingResource  = rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_LIGHTING ) );
    const shader_resource* pShadowMatrices    = rbuffer_GetResource( m_ShadowMatrixBuffer );
    const shader_resource* pShadowData        = rbuffer_GetResource( m_ShadowDataBuffer );

    if( !pInstanceResource )
    {
        return FALSE;
    }

    const shader_storage_buffer_binding Bindings[] = {
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, Layout.InstanceDataVertex, pInstanceResource ),
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, Layout.AuxiliaryDataVertex, pBoneResource ),
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, Layout.BoneRemapDataVertex, pBoneRemapResource ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, Layout.LightingDataPixel, pLightingResource ),
        shader_storage_buffer_binding(
            SHADER_STAGE_PIXEL, Layout.ProjectionLightingDataPixel,
            rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_PROJECTION_LIGHTING ) ) ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, Layout.AmbientDataPixel,
                                       rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_AMBIENT_LIGHTING ) ) ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, Layout.ShadowMatricesPixel, pShadowMatrices ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, Layout.ShadowDataPixel, pShadowData )
    };

    for( u32 i = 0; i < ARRAYSIZE( Bindings ); ++i )
    {
        if( Bindings[i].Slot == 0xFFFFFFFFu )
        {
            continue;
        }

        if( !shader_BindStorageBuffers( &Bindings[i], 1 ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool FrameCompiler::BindSkinGeomBuffers( const GeomDrawPacket& Packet ) const
{
    GeometryRepository::SkinMeshView View;
    return m_pContext->pGeometry->GetSkinMeshView( Packet.hMesh, View ) &&
           m_pContext->pGeometry->BindSkinMesh( View ) &&
           rbuffer_BindVertex( GetFrameBuffer( FrameStorage::STREAM_SKIN_DRAW_INSTANCES ), 1, 0 );
}

} // namespace render
