//=========================================================================
//
//  FrameCompiler_Rigid.cpp
//
//  Rigid material handling for geometry submission.
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

namespace render
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool FrameCompiler::InitRigidShaders( void )
{
    x_DebugMsg( "FrameCompiler: Initializing rigid shaders\n" );

    m_RigidInstanceCapacity      = 0;
    m_RigidInstanceIndexCapacity = 0;
    m_RigidColorCapacity         = 0;
    m_RigidIndirectCapacity      = 0;
    ResetRigidBatch();
    InitSurfacePrograms();

    x_DebugMsg( "FrameCompiler: Rigid shaders initialized successfully\n" );

    return TRUE;
}

//=========================================================================

void FrameCompiler::KillRigidShaders( void )
{
    ResetRigidBatch();

    m_RigidInstanceCapacity      = 0;
    m_RigidInstanceIndexCapacity = 0;
    m_RigidColorCapacity         = 0;
    m_RigidIndirectCapacity      = 0;

    DestroyRigidPipelines();
    DestroySkinPipelines();
    DestroySurfacePrograms();

    x_DebugMsg( "FrameCompiler: Rigid shaders released\n" );
}

//=========================================================================

void FrameCompiler::ResetRigidBatch( void )
{
    m_RigidBatchFirstInstance      = m_lRigidFrameInstances.GetCount();
    m_hRigidBatchMesh.Handle       = HNULL;
    m_RigidBatchFlags              = 0;
    m_isRigidBatchForcedGlow       = FALSE;
    m_RigidBatchUOffset            = 0;
    m_RigidBatchVOffset            = 0;
    m_RigidBatchOverrideMat        = FALSE;
    m_RigidBatchSortDepth          = 0.0f;
    m_RigidBatchContributionPolicy = render::RENDER_CONTRIBUTION_DEFAULT;
}

//=========================================================================

xbool FrameCompiler::HasRigidBatch( void ) const
{
    return ( static_cast<u32>( m_lRigidFrameInstances.GetCount() ) > m_RigidBatchFirstInstance );
}

//=========================================================================

xbool FrameCompiler::CanAppendRigidBatch( const RigidBatchDesc& Desc ) const
{
    if( !HasRigidBatch() )
    {
        return TRUE;
    }

    return ( m_hRigidBatchMesh.Handle == Desc.hMesh.Handle ) &&
           ( !!( m_lRigidFrameInstances[m_RigidBatchFirstInstance].ShaderFlags &
                 ( GEOM_INST_FLAG_PROJ_LIGHT | GEOM_INST_FLAG_PROJ_SHADOW ) ) ==
             !!( BuildInstFlags( Desc.InstFeatures ) & ( GEOM_INST_FLAG_PROJ_LIGHT | GEOM_INST_FLAG_PROJ_SHADOW ) ) ) &&
           ( ResolveLocalLighting( m_pContext->pLighting->Get( m_lRigidFrameLighting[m_RigidBatchFirstInstance] ) ) ==
             ResolveLocalLighting( m_pContext->pLighting->Get( Desc.Lighting ) ) ) &&
           ( BuildBatchStateFlags( m_RigidBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) ) &&
           ( m_isRigidBatchForcedGlow == !!( Desc.InstFeatures & render::GEOM_INST_FORCED_GLOW ) ) &&
           ( m_RigidBatchUOffset == Desc.UOffset ) && ( m_RigidBatchVOffset == Desc.VOffset ) &&
           ( m_RigidBatchOverrideMat == Desc.OverrideMat ) &&
           ( m_RigidBatchContributionPolicy == Desc.ContributionPolicy );
}

//=========================================================================

xbool FrameCompiler::SetRigidMaterial( const material*       pMaterial,
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

    RenderStateSelection PipelineState = BuildMaterialPipelineState( pMaterial, RenderFlags, OverrideMat,
                                                                     WriteGlowAlpha, DrawPass, Lighting, Projection );

    if( !BindRigidPipeline( PipelineState, FindPreparedMaterialPipeline( PacketIndex, PipelineState ) ) )
    {
        RENDER_THROW( "FrameCompiler", "failed to bind rigid render pipeline" );
        return FALSE;
    }

    OverrideMat = OverrideMat || PassData.DepthMode == GEOM_DEPTH_PREPASS;
    if( !UpdateRigidConstants( pMaterial, UOffset, VOffset, OverrideMat, PassData ) )
    {
        RENDER_THROW( "FrameCompiler", "failed to update rigid constants" );
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

void FrameCompiler::AddRigidBatchInst( const RigidBatchDesc& Desc )
{
    ASSERT( Desc.pGeom );
    ASSERT( x_isvalid( Desc.SortDepth ) );
    ASSERT( ( Desc.iSubMesh >= 0 ) && ( Desc.iSubMesh < Desc.pGeom->m_nSubMeshes ) );

    if( !HasRigidBatch() )
    {
        m_hRigidBatchMesh              = Desc.hMesh;
        m_RigidBatchFlags              = Desc.RenderFlags;
        m_isRigidBatchForcedGlow       = !!( Desc.InstFeatures & render::GEOM_INST_FORCED_GLOW );
        m_RigidBatchUOffset            = Desc.UOffset;
        m_RigidBatchVOffset            = Desc.VOffset;
        m_RigidBatchOverrideMat        = Desc.OverrideMat;
        m_RigidBatchSortDepth          = Desc.SortDepth;
        m_RigidBatchContributionPolicy = Desc.ContributionPolicy;
    }
    else
    {
        ASSERT( m_hRigidBatchMesh.Handle == Desc.hMesh.Handle );
        ASSERT( BuildBatchStateFlags( m_RigidBatchFlags ) == BuildBatchStateFlags( Desc.RenderFlags ) );
        ASSERT( m_isRigidBatchForcedGlow == !!( Desc.InstFeatures & render::GEOM_INST_FORCED_GLOW ) );
        ASSERT( m_RigidBatchUOffset == Desc.UOffset );
        ASSERT( m_RigidBatchVOffset == Desc.VOffset );
        ASSERT( m_RigidBatchOverrideMat == Desc.OverrideMat );
        ASSERT( m_RigidBatchContributionPolicy == Desc.ContributionPolicy );
    }

    const geom::submesh& SubMesh = Desc.pGeom->m_pSubMesh[Desc.iSubMesh];
    ASSERT( SubMesh.nSections == 1 );
    const rigid_geom::section& SectionData = Desc.pGeom->m_pSection[SubMesh.iSection];

    RigidInstanceData& GpuInst = m_lRigidFrameInstances.Append();
    x_memset( &GpuInst, 0, sizeof( GpuInst ) );

    if( Desc.pL2W )
    {
        GpuInst.World = *Desc.pL2W;
    }
    else
    {
        GpuInst.World.Identity();
    }

    GpuInst.ShaderFlags = BuildInstFlags( Desc.InstFeatures );
    GpuInst.ColorOffset = 0xFFFFFFFFu;
    GpuInst.FadeAlpha   = BuildInstFadeAlpha( Desc.RenderFlags, Desc.Alpha );

    m_lRigidFrameLighting.Append() = Desc.Lighting;

    const cb_geom_lighting* pLighting = m_pContext->pLighting->Get( Desc.Lighting );

    if( pLighting )
    {
        m_nLitInstances++;
        m_nInstanceLights += MIN( pLighting->LightCount, MAX_GEOM_LIGHTS );
    }

    if( Desc.pColorInfo )
    {
        GpuInst.ColorOffset = m_lRigidFrameColors.GetCount();

        if( SectionData.nVertices > 0 )
        {
            const s32 FirstColor = m_lRigidFrameColors.GetCount();
            m_lRigidFrameColors.SetCount( FirstColor + SectionData.nVertices );
            x_memcpy( &m_lRigidFrameColors[FirstColor], &Desc.pColorInfo[SectionData.iColor],
                      SectionData.nVertices * sizeof( u32 ) );
        }
    }
}

//=========================================================================

void FrameCompiler::FlushRigidBatch( const material*  pMaterial,
                                     u8               MaterialOverride,
                                     geom_render_pass PassData,
                                     u32              Sequence )
{
    if( !HasRigidBatch() )
    {
        ResetRigidBatch();
        return;
    }

    GeomDrawPacket Packet;
    Packet.Kind               = GEOM_PACKET_RIGID;
    Packet.pMaterial          = pMaterial;
    Packet.hMesh              = m_hRigidBatchMesh;
    Packet.RenderFlags        = m_RigidBatchFlags;
    Packet.ContributionPolicy = m_RigidBatchContributionPolicy;
    Packet.FirstInstance      = m_RigidBatchFirstInstance;
    Packet.InstanceCount      = m_lRigidFrameInstances.GetCount() - m_RigidBatchFirstInstance;
    Packet.UOffset            = m_RigidBatchUOffset;
    Packet.VOffset            = m_RigidBatchVOffset;
    Packet.MaterialOverride   = MaterialOverride;
    Packet.Pass               = PassData;
    Packet.Sequence           = Sequence;
    Packet.SortDepth          = m_RigidBatchSortDepth;
    Packet.Resources          = CaptureResourceSnapshot();

    m_lDrawPackets.Append() = Packet;
    ResetRigidBatch();
}

//=========================================================================

xbool FrameCompiler::BindRigidFrameBuffers( xbool DepthOnly ) const
{
    const ShaderBindingLayout& Layout = *GetShaderBindings( GEOM_SHADER_RIGID );
    const shader_resource*     pInstanceResource =
        rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCES ) );
    const shader_resource* pColorResource = rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_RIGID_COLORS ) );
    const shader_resource* pLightingResource = rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_LIGHTING ) );
    const shader_resource* pShadowMatrices   = rbuffer_GetResource( m_ShadowMatrixBuffer );
    const shader_resource* pShadowData       = rbuffer_GetResource( m_ShadowDataBuffer );

    if( !pInstanceResource )
    {
        return FALSE;
    }

    const shader_storage_buffer_binding Bindings[] = {
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, Layout.InstanceDataVertex, pInstanceResource ),
        shader_storage_buffer_binding( SHADER_STAGE_VERTEX, Layout.AuxiliaryDataVertex, pColorResource ),
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

xbool FrameCompiler::BindRigidGeomBuffers( const StaticGeometryStorage::mesh_range& Range ) const
{
    return m_pContext->pGeometry->BindRigidMesh( Range ) && m_pContext->pGeometry->BindRigidVertexIndices( Range, 2 ) &&
           rbuffer_BindVertex( GetFrameBuffer( FrameStorage::STREAM_RIGID_INSTANCE_INDICES ), 1, 0 );
}

//=========================================================================

xbool FrameCompiler::UpdateRigidConstants( const material*       pMaterial,
                                           u8                    UOffset,
                                           u8                    VOffset,
                                           u8                    OverrideMat,
                                           const geom_pass_desc& PassData )
{
    const view* pView = PassData.pView ? PassData.pView->GetSourceView() : NULL;

    if( !pView )
    {
        RENDER_THROW( "FrameCompiler", "UpdateRigidConstants called without an active view" );
        return FALSE;
    }

    const GeomFrameConstants FrameData = BuildFrameConstants( pMaterial, UOffset, VOffset, OverrideMat, PassData );

    const xbool DepthOnly = OverrideMat && !PassData.WriteGlowMask;

    if( !PushFrameConstants( GEOM_SHADER_RIGID, FrameData ) )
    {
        return FALSE;
    }

    if( !DepthOnly && !PushFogConstants( GEOM_SHADER_RIGID, PassData ) )
    {
        return FALSE;
    }

    return TRUE;
}

} // namespace render
