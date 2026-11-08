//=========================================================================
//
//  FrameCompiler_Dynamic.cpp
//
//  Dynamic forward geometry submission.
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

#include "../../Texture.hpp"
#include "../Resource/GeometryRepository.hpp"
#include "../../Features/PostProcess/PostProcessFeature.hpp"

namespace render
{

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

namespace
{

//=========================================================================

u32 GrowDynamicBufferCapacity( u32 CurrentCapacity, u32 RequiredCount )
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

xbool EnsureDynamicBuffer( rbuffer&    Buffer,
                           u32&        Capacity,
                           u32         RequiredCount,
                           u32         Stride,
                           u32         Usage,
                           const char* pDebugName )
{
    if( ( Stride == 0 ) || ( RequiredCount > ( 0xffffffffu / Stride ) ) )
    {
        return FALSE;
    }

    const u32 MaxCapacity = FrameStorage::MAX_STREAM_BYTES / Stride;

    if( ( MaxCapacity == 0 ) || ( RequiredCount > MaxCapacity ) )
    {
        return FALSE;
    }

    u32 DesiredCapacity = GrowDynamicBufferCapacity( Capacity, MAX( RequiredCount, 1u ) );
    DesiredCapacity     = MIN( DesiredCapacity, MaxCapacity );

    if( Buffer && ( Capacity >= DesiredCapacity ) )
    {
        return TRUE;
    }

    rbuffer_desc Desc;
    Desc.Size       = DesiredCapacity * Stride;
    Desc.Stride     = Stride;
    Desc.UsageFlags = Usage;
    Desc.pDebugName = pDebugName;

    if( !rbuffer_Create( Buffer, Desc ) )
    {
        return FALSE;
    }

    Capacity = DesiredCapacity;

    return TRUE;
}
} // namespace

//=========================================================================
//  LIFETIME
//=========================================================================

xbool FrameCompiler::InitDynamicGeom( void )
{
    m_DynamicVertexCapacity = 0;
    m_DynamicIndexCapacity  = 0;
    m_lDynamicFrameLighting.SetCapacity( 32768 );

    return TRUE;
}

//=========================================================================

void FrameCompiler::KillDynamicGeom( void )
{
    m_lDynamicFrameLighting.Clear();
    m_DynamicVertexCapacity = 0;
    m_DynamicIndexCapacity  = 0;
    m_DynamicPipelines.Reset();
}

//=========================================================================
//  PACKETS
//=========================================================================

xbool FrameCompiler::BuildDynamicPackets( const xarray<dynamic_geom_draw>& Draws )
{
    for( s32 i = 0; i < Draws.GetCount(); ++i )
    {
        const dynamic_geom_draw& Draw     = Draws[i];
        const shader_resource*   pDiffuse = Draw.pDiffuseTexture ? Draw.pDiffuseTexture->GetShaderResource() : NULL;

        if( Draw.pGpuMesh && !geom_IsGpuMeshValid( *Draw.pGpuMesh ) )
        {
            RENDER_THROW( "FrameCompiler", "invalid dynamic GPU mesh %d", i );
            return FALSE;
        }

        if( !Draw.pVertices || !Draw.pIndices || !pDiffuse || !Draw.pDamageMask || !Draw.pDamageTexture ||
            !Draw.pDamageUploadPending || ( Draw.VertexCount <= 0 ) || ( Draw.VertexCount > 65535 ) ||
            ( Draw.IndexCount <= 0 ) || ( ( Draw.IndexCount % 3 ) != 0 ) )
        {
            RENDER_THROW( "FrameCompiler", "invalid dynamic geometry draw %d", i );
            return FALSE;
        }

        for( s32 Index = 0; Index < Draw.IndexCount; ++Index )
        {
            if( Draw.pIndices[Index] >= Draw.VertexCount )
            {
                RENDER_THROW( "FrameCompiler", "dynamic geometry draw %d index %d exceeds %d vertices", i, Index,
                              Draw.VertexCount );
                return FALSE;
            }
        }

        GeomDrawPacket Packet;
        Packet.Kind        = GEOM_PACKET_DYNAMIC;
        Packet.Pass        = GEOM_PASS_FORWARD_OPAQUE;
        Packet.RenderFlags = Draw.RenderFlags | ( ( Draw.InstFeatures & GEOM_INST_FORCED_GLOW ) ? render::GLOWING : 0 );
        Packet.InstFeatures = Draw.InstFeatures;
        Packet.Sequence     = Draw.Sequence;

        if( !m_DynamicStorage.pVertices || !m_DynamicStorage.pIndices )
        {
            RENDER_THROW( "FrameCompiler", "dynamic geometry storage is unavailable" );
            return FALSE;
        }

        const s32 FirstVertex = static_cast<s32>( Draw.pVertices - m_DynamicStorage.pVertices );
        const s32 FirstIndex  = static_cast<s32>( Draw.pIndices - m_DynamicStorage.pIndices );

        if( ( FirstVertex < 0 ) || ( Draw.VertexCount > ( m_DynamicStorage.VertexCount - FirstVertex ) ) ||
            ( FirstIndex < 0 ) || ( Draw.IndexCount > ( m_DynamicStorage.IndexCount - FirstIndex ) ) )
        {
            RENDER_THROW( "FrameCompiler", "dynamic geometry draw %d is outside packed storage", i );
            return FALSE;
        }

        Packet.FirstDynamicVertex   = static_cast<u32>( FirstVertex );
        Packet.pDynamicGpuMesh      = Draw.pGpuMesh;
        Packet.FirstDynamicIndex    = static_cast<u32>( FirstIndex );
        Packet.DynamicIndexCount    = Draw.IndexCount;
        Packet.DynamicLightingIndex = 0;
        Packet.CoverageUsesTextureAlpha = Draw.CoverageUsesTextureAlpha;
        Packet.Resources.pDiffuse   = pDiffuse;
        Packet.pDamageMask          = Draw.pDamageMask;
        Packet.pDamageTexture       = Draw.pDamageTexture;
        Packet.pDamageUpload        = Draw.pDamageUpload;
        Packet.pDamageUploadPending = Draw.pDamageUploadPending;
        Packet.DamageUploadX        = Draw.DamageUploadX;
        Packet.DamageUploadY        = Draw.DamageUploadY;
        Packet.DamageUploadWidth    = Draw.DamageUploadWidth;
        Packet.DamageUploadHeight   = Draw.DamageUploadHeight;

        m_lDrawPackets.Append()          = Packet;
        m_lDynamicFrameLighting.Append() = Draw.Lighting;
    }

    return TRUE;
}

//=========================================================================

void FrameCompiler::DeclarePacketResources( FrameGraphBuilder& Graph, RenderPassId Pass, s32 PacketIndex ) const
{
    if( PacketIndex < 0 || PacketIndex >= m_lDrawPackets.GetCount() )
    {
        return;
    }

    const GeomDrawPacket& Packet = m_lDrawPackets[PacketIndex];
    Graph.Read( Pass, Packet.Resources.pDiffuse );
    Graph.Read( Pass, Packet.Resources.pDetail );
    Graph.Read( Pass, Packet.Resources.pEnvironment );
    Graph.Read( Pass, Packet.Resources.pEnvironmentCube );
    Graph.Read( Pass, Packet.pDamageMask );

    if( Packet.Kind == GEOM_PACKET_RIGID )
    {
        m_pContext->pGeometry->DeclareRigidResources( Graph, Pass, Packet.Range );
    }
    else if( Packet.Kind == GEOM_PACKET_SKIN )
    {
        m_pContext->pGeometry->DeclareSkinResources( Graph, Pass, Packet.Range );
    }

    if( Packet.Kind == GEOM_PACKET_DYNAMIC && Packet.pDynamicGpuMesh )
    {
        Graph.Read( Pass, *Packet.pDynamicGpuMesh->pVertices, RENDER_RESOURCE_ACCESS_VERTEX_READ );
        Graph.Read( Pass, *Packet.pDynamicGpuMesh->pIndices, RENDER_RESOURCE_ACCESS_INDEX_READ );
    }
}

//=========================================================================

void FrameCompiler::DeclarePassResources( FrameGraphBuilder& Graph, RenderPassId Pass ) const
{
    for( u32 i = 0; i < FrameStorage::STREAM_COUNT; ++i )
    {
        const rbuffer& Buffer = GetFrameBuffer( static_cast<FrameStorage::Stream>( i ) );

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

        if( Access )
        {
            Graph.Read( Pass, Buffer, Access );
        }
    }

    if( m_ForwardCommandBuffer )
    {
        Graph.Read( Pass, m_ForwardCommandBuffer, RENDER_RESOURCE_ACCESS_INDIRECT_READ );
    }

    Graph.Read( Pass, m_ShadowMatrixBuffer, RENDER_RESOURCE_ACCESS_STORAGE_READ );
    Graph.Read( Pass, m_ShadowDataBuffer, RENDER_RESOURCE_ACCESS_STORAGE_READ );
    Graph.Read( Pass, vram_GetShaderResource( m_WhiteTexture ) );
    Graph.Read( Pass, vram_GetShaderResource( m_NeutralDetailTexture ) );
    Graph.Read( Pass, vram_GetShaderResource( m_BlackTexture ) );
    Graph.Read( Pass, vram_GetShaderResource( m_BlackCubeTexture ) );
    Graph.Read( Pass, m_pContext->pProjection->GetShaderResource() );
    Graph.Read( Pass, m_pContext->pPostProcess->GetFogPaletteResource() );
}

//=========================================================================

xbool FrameCompiler::ReserveDynamicBuffers( void )
{
    if( m_DynamicStorage.VertexCount == 0 )
    {
        return m_DynamicStorage.IndexCount == 0;
    }

    const u32 VertexCount = static_cast<u32>( m_DynamicStorage.VertexCount );
    const u32 IndexCount  = static_cast<u32>( m_DynamicStorage.IndexCount );

    return EnsureDynamicBuffer( GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_VERTICES ), m_DynamicVertexCapacity,
                                VertexCount, sizeof( dynamic_geom_vertex ), RBUFFER_USAGE_VERTEX,
                                "GeomDynamicVertices" ) &&
           EnsureDynamicBuffer( GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_INDICES ), m_DynamicIndexCapacity,
                                IndexCount, sizeof( u16 ), RBUFFER_USAGE_INDEX, "GeomDynamicIndices" );
}

//=========================================================================

xbool FrameCompiler::UploadDynamicGeom( FrameGraphBuilder& Graph )
{
    if( m_DynamicStorage.VertexCount == 0 )
    {
        return m_DynamicStorage.IndexCount == 0;
    }

    const u32 VertexCount = static_cast<u32>( m_DynamicStorage.VertexCount );
    const u32 IndexCount  = static_cast<u32>( m_DynamicStorage.IndexCount );

    if( ( VertexCount > ( 0xffffffffu / sizeof( dynamic_geom_vertex ) ) ) ||
        ( IndexCount > ( 0xffffffffu / sizeof( u16 ) ) ) )
    {
        return FALSE;
    }

    const u32 VertexBytes = VertexCount * sizeof( dynamic_geom_vertex );
    const u32 IndexBytes  = IndexCount * sizeof( u16 );

    enum
    {
        MAX_TEXTURE_UPLOAD_BATCH = 64
    };

    vram_texture_upload_batch_item TextureUploads[MAX_TEXTURE_UPLOAD_BATCH];
    xbool*                         PendingFlags[MAX_TEXTURE_UPLOAD_BATCH];
    u32                            TextureUploadCount = 0;
    static xprofile_counter        DamageUploadBytes =
        x_GetProfiler().RegisterCounter( "ClothDamageUploadBytes", "RenderCounter" );

    if( !EnsureDynamicBuffer( GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_VERTICES ), m_DynamicVertexCapacity,
                              VertexCount, sizeof( dynamic_geom_vertex ), RBUFFER_USAGE_VERTEX,
                              "GeomDynamicVertices" ) ||
        !EnsureDynamicBuffer( GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_INDICES ), m_DynamicIndexCapacity,
                              IndexCount, sizeof( u16 ), RBUFFER_USAGE_INDEX, "GeomDynamicIndices" ) )
    {
        return FALSE;
    }

    rbuffer_upload_batch_item Uploads[2];
    Uploads[0].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_VERTICES );
    Uploads[0].Upload.pData  = m_DynamicStorage.pVertices;
    Uploads[0].Upload.Size   = VertexBytes;
    Uploads[0].Upload.bCycle = TRUE;

    Uploads[1].pBuffer       = &GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_INDICES );
    Uploads[1].Upload.pData  = m_DynamicStorage.pIndices;
    Uploads[1].Upload.Size   = IndexBytes;
    Uploads[1].Upload.bCycle = TRUE;

    if( !Graph.QueueUploadBatch( Uploads, ARRAYSIZE( Uploads ) ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < m_lDrawPackets.GetCount(); ++i )
    {
        const GeomDrawPacket& Packet = m_lDrawPackets[i];

        if( ( Packet.Kind != GEOM_PACKET_DYNAMIC ) || !Packet.pDamageUploadPending || !*Packet.pDamageUploadPending )
        {
            continue;
        }

        u32 DamageSize = 0;

        if( !Packet.pDamageTexture || !Packet.pDamageUpload ||
            !geom_CalculateDamageUploadSize( Packet.DamageUploadWidth, Packet.DamageUploadHeight, DamageSize ) )
        {
            return FALSE;
        }

        vram_texture_upload_desc Upload;
        Upload.Region.X      = Packet.DamageUploadX;
        Upload.Region.Y      = Packet.DamageUploadY;
        Upload.Region.Width  = Packet.DamageUploadWidth;
        Upload.Region.Height = Packet.DamageUploadHeight;
        Upload.pData         = Packet.pDamageUpload;
        Upload.Size          = DamageSize;
        Upload.RowPitch      = Packet.DamageUploadWidth;
        Upload.SlicePitch    = Upload.Size;

        if( TextureUploadCount == MAX_TEXTURE_UPLOAD_BATCH )
        {
            if( !Graph.QueueUploadBatch( TextureUploads, TextureUploadCount, PendingFlags ) )
            {
                return FALSE;
            }

            for( u32 UploadIndex = 0; UploadIndex < TextureUploadCount; ++UploadIndex )
            {
                DamageUploadBytes.Add( TextureUploads[UploadIndex].Upload.Size );
            }

            TextureUploadCount = 0;
        }

        TextureUploads[TextureUploadCount].pTexture = Packet.pDamageTexture;
        TextureUploads[TextureUploadCount].Upload   = Upload;
        PendingFlags[TextureUploadCount]            = Packet.pDamageUploadPending;
        ++TextureUploadCount;
    }

    if( TextureUploadCount > 0 )
    {
        if( !Graph.QueueUploadBatch( TextureUploads, TextureUploadCount, PendingFlags ) )
        {
            return FALSE;
        }

        for( u32 UploadIndex = 0; UploadIndex < TextureUploadCount; ++UploadIndex )
        {
            DamageUploadBytes.Add( TextureUploads[UploadIndex].Upload.Size );
        }
    }

    static xprofile_counter DynamicUploadBytes =
        x_GetProfiler().RegisterCounter( "GeomDynamicUploadBytes", "RenderCounter" );
    DynamicUploadBytes.Add( VertexBytes + IndexBytes );

    return TRUE;
}

//=========================================================================
//  PIPELINE
//=========================================================================

ShaderProgramKey FrameCompiler::BuildDynamicProgram( u32                   RenderFlags,
                                                     const geom_pass_desc& Pass,
                                                     LocalLightingMode     Lighting,
                                                     xbool                 Projection ) const
{
    MaterialDesc Material;
    Material.Valid              = TRUE;
    Material.AlphaTest          = TRUE;
    Material.TwoSidedShading    = TRUE;
    Material.ReceivesProjection = TRUE;
    Material.ReceivesShadow     = TRUE;

    DrawShaderFeatures Draw;
    Draw.SetLocalLighting( Lighting );
    Draw.DamageCoverage = TRUE;
    Draw.ForcedGlow     = ( RenderFlags & render::GLOWING ) != 0;
    Draw.Fog            = Pass.SceneFogMode != RENDER_FOG_NONE || Pass.BloomFogMode != RENDER_FOG_NONE;
    Draw.Projection     = Projection;
    Draw.Shadows        = Pass.EnableShadows && m_pContext->pShadowAtlas->GetShadowMode() != ShadowMode::Disabled;

    ShaderPassContract ShaderPass;
    ShaderPass.Outputs = Pass.DepthMode == GEOM_DEPTH_PREPASS && Pass.WriteNormals ? ShaderOutputs::LinearDepthNormals
                         : Pass.DepthMode == GEOM_DEPTH_PREPASS ? ShaderOutputs::LinearDepth
                         : Pass.WriteSceneDepth ? ShaderOutputs::SceneColorDepth : ShaderOutputs::SceneColor;

    return ResolveShaderProgram( Material, ShaderFamily::Surface, VertexContract::Dynamic, Draw, ShaderPass );
}

//=========================================================================

FrameCompiler::RenderStateSelection FrameCompiler::BuildDynamicPipelineState( u32                   Flags,
                                                                              const geom_pass_desc& Pass,
                                                                              LocalLightingMode     Lighting,
                                                                              xbool                 Projection ) const
{
    RenderStateSelection State;
    State.Program = BuildDynamicProgram( Flags, Pass, Lighting, Projection );
    State.Blend   = RSTATE_BLEND_PRESET_NONE;
    State.Depth   = Pass.DepthMode == GEOM_DEPTH_READ_ONLY ? RSTATE_DEPTH_PRESET_NO_WRITE
                                                         : RSTATE_DEPTH_PRESET_NORMAL;

    const xbool Wire = ( Flags & ( render::WIREFRAME | render::WIREFRAME_NO_CULL | render::DEBUG_RENDER_PULSED ) ) != 0;
    State.Raster =
        Wire ? ( ( Flags & render::WIREFRAME_NO_CULL ) ? RSTATE_RASTER_PRESET_WIRE_NO_CULL : RSTATE_RASTER_PRESET_WIRE )
             : RSTATE_RASTER_PRESET_SOLID_NO_CULL;
    State.ColorFormat     = Pass.ColorFormat;
    State.DepthFormat     = Pass.DepthFormat;
    State.SampleCount     = Pass.SampleCount;
    State.WriteSceneDepth = Pass.WriteSceneDepth && Pass.DepthMode != GEOM_DEPTH_PREPASS;

    return State;
}

//=========================================================================

xbool FrameCompiler::BuildDynamicPipelineDesc( render_pipeline_desc&       Desc,
                                               const RenderStateSelection& State,
                                               xbool                       IsPrewarm )
{
    static xprofile_counter Descriptors =
        x_GetProfiler().RegisterCounter( "DynamicPipelineDescriptors", "RenderCounter" );
    Descriptors.Add();

    static shader_vertex_buffer_desc VertexBuffer;
    VertexBuffer.Slot   = 0;
    VertexBuffer.Stride = sizeof( dynamic_geom_vertex );

    static shader_vertex_element Layout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( dynamic_geom_vertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( dynamic_geom_vertex, Normal ) ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( dynamic_geom_vertex, UV ) ),
        shader_vertex_element( 3, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof( dynamic_geom_vertex, Color ) )
    };

    SurfaceProgram* pProgram = GetSurfaceProgram( State.Program, IsPrewarm );

    if( !pProgram )
    {
        return FALSE;
    }

    Desc.Shader.pVertexShader     = &pProgram->pVertex->Shader;
    Desc.Shader.pPixelShader      = &pProgram->pPixel->Shader;
    Desc.Shader.pVertexBuffers    = &VertexBuffer;
    Desc.Shader.VertexBufferCount = 1;
    Desc.Shader.pInputElements    = Layout;
    Desc.Shader.InputElementCount = ARRAYSIZE( Layout );
    Desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Desc.Depth                    = rstate_GetDepthDesc( State.Depth );
    Desc.Raster                   = rstate_GetRasterDesc( State.Raster );
    const xbool WriteNormals      = State.Program.Outputs == ShaderOutputs::LinearDepthNormals;
    Desc.ColorCount               = State.WriteSceneDepth || WriteNormals ? 2 : 1;
    Desc.ColorTargets[0].Format    = State.ColorFormat;
    Desc.ColorTargets[0].Blend     = rstate_GetBlendDesc( State.Blend );

    if( WriteNormals )
    {
        Desc.ColorTargets[1].Format = RTARGET_FORMAT_RGBA8;
        Desc.ColorTargets[1].Blend.ColorWriteMask = RSTATE_COLOR_WRITE_ALL;
    }
    else if( State.WriteSceneDepth )
    {
        Desc.ColorTargets[1].Format               = RTARGET_FORMAT_R32F;
        Desc.ColorTargets[1].Blend.ColorWriteMask = RSTATE_COLOR_WRITE_RED;
    }

    Desc.DepthFormat = State.DepthFormat;
    Desc.SampleCount = State.SampleCount;
    Desc.pDebugName  = "GeomDynamic";

    return TRUE;
}

//=========================================================================

render_pipeline* FrameCompiler::GetDynamicPipeline( const RenderStateSelection& State, xbool IsPrewarm )
{
    const u64 Key =
        GraphicsPipelineStateKey( State.ColorFormat, State.DepthFormat, State.SampleCount,
                                  State.Blend, State.Depth, State.Raster );
    render_pipeline* pPipeline = m_DynamicPipelines.FindPrepared( State.Program, Key, IsPrewarm );

    if( pPipeline )
    {
        return pPipeline;
    }

    render_pipeline_desc Desc;

    if( !BuildDynamicPipelineDesc( Desc, State, IsPrewarm ) )
    {
        return NULL;
    }

    pPipeline = IsPrewarm ? m_DynamicPipelines.QueuePrewarm( Desc ) : m_DynamicPipelines.GetOrCreate( Desc );
    m_DynamicPipelines.RememberPrepared( State.Program, Key, pPipeline );

    return pPipeline;
}

//=========================================================================

xbool FrameCompiler::ExecuteDynamicPacket( const GeomDrawPacket& Packet, const geom_pass_desc& Pass )
{
    if( ( Packet.Kind != GEOM_PACKET_DYNAMIC ) || ( Packet.DynamicIndexCount == 0 ) || ( Pass.TargetWidth == 0 ) ||
        ( Pass.TargetHeight == 0 ) )
    {
        return FALSE;
    }

    const u32 RenderFlags =
        Packet.RenderFlags | ( ( Packet.InstFeatures & render::GEOM_INST_FORCED_GLOW ) ? render::GLOWING : 0u );
    geom_pass_desc DrawPass = Pass;
    DrawPass.EnableShadows = Pass.EnableShadows && Packet.Shadows;
    const RenderStateSelection State =
        BuildDynamicPipelineState( RenderFlags, DrawPass, Packet.LightingMode, Packet.Projection );
    const view*      pView     = Pass.pView ? Pass.pView->GetSourceView() : NULL;
    render_pipeline* pPipeline = Pass.DepthMode == GEOM_DEPTH_PREPASS ? Packet.pDepthPipeline
                                                                     : GetDynamicPipeline( State );
    const rstate_sampler* pSampler = GetSampler( RSTATE_SAMPLER_PRESET_LINEAR_CLAMP );

    if( !pView || !pPipeline || !pSampler || !Packet.Resources.pDiffuse || !Packet.pDamageMask ||
        !render_BindPipeline( *pPipeline ) )
    {
        return FALSE;
    }

    m_ActiveShaderKind = GEOM_SHADER_DYNAMIC;

    SurfaceProgram* pProgram = GetSurfaceProgram( State.Program );

    if( !pProgram )
    {
        return FALSE;
    }

    const ShaderBindingLayout& Bindings = pProgram->Bindings;
    m_pActiveSurfaceBindings            = &Bindings;

    DynamicDrawConstants Constants = {};
    Constants.LightingIndex        = Packet.DynamicLightingIndex;
    Constants.ShaderFlags          = BuildInstFlags( Packet.InstFeatures ) | GEOM_INST_FLAG_RECEIVE_LOCAL_SHADOW;

    const shader_resource* pLighting          = rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_LIGHTING ) );
    const shader_resource* pShadowMatrices    = GetShadowMatricesResource();
    const shader_resource* pShadowData        = GetShadowDataResource();
    const shader_resource* pFaceShadow        = GetFaceShadowResource();
    const shader_resource* pFaceShadowDepth   = GetFaceShadowDepthResource();
    const shader_resource* pProjectionAtlas   = m_pContext->pProjection->GetShaderResource();
    const dynamic_geom_gpu_mesh* pGpuMesh = Packet.pDynamicGpuMesh;
    const rbuffer& VertexBuffer = pGpuMesh ? *pGpuMesh->pVertices : GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_VERTICES );
    const rbuffer& IndexBuffer = pGpuMesh ? *pGpuMesh->pIndices : GetFrameBuffer( FrameStorage::STREAM_DYNAMIC_INDICES );
    const s32 IndexCount = pGpuMesh ? pGpuMesh->IndexCount : Packet.DynamicIndexCount;
    const rstate_sampler*  pPointSampler      = GetSampler( RSTATE_SAMPLER_PRESET_POINT_CLAMP );
    const rstate_sampler*  pFaceShadowSampler = GetSampler( m_FaceShadowSamplerPreset );

    if( ( Bindings.LightingDataPixel != 0xFFFFFFFFu && !pLighting ) ||
        ( Bindings.ShadowMatricesPixel != 0xFFFFFFFFu && !pShadowMatrices ) ||
        ( Bindings.ShadowDataPixel != 0xFFFFFFFFu && !pShadowData ) || !pPointSampler || !pFaceShadowSampler )
    {
        return FALSE;
    }

    GeomFrameConstants Frame       = BuildFrameConstants( NULL, 0, 0, FALSE, Pass );
    Frame.SurfaceTwoSided          = TRUE;
    Frame.CoverageUsesTextureAlpha = Packet.CoverageUsesTextureAlpha;

    if( !Packet.pDamageTexture || !Packet.pDamageTexture->Desc.Width || !Packet.pDamageTexture->Desc.Height )
    {
        return FALSE;
    }

    Frame.UVAnim.Set( 1.5f / static_cast<f32>( Packet.pDamageTexture->Desc.Width ),
                      1.5f / static_cast<f32>( Packet.pDamageTexture->Desc.Height ), Frame.UVAnim.GetZ(),
                      Frame.UVAnim.GetW() );

    if( ( Bindings.DynamicConstantsVertex != 0xFFFFFFFFu &&
          !shader_PushUniformData( SHADER_STAGE_VERTEX, Bindings.DynamicConstantsVertex, &Constants,
                                   sizeof( Constants ) ) ) ||
        !PushFrameConstants( GEOM_SHADER_DYNAMIC, Frame ) || !PushFogConstants( GEOM_SHADER_DYNAMIC, Pass ) ||
        !UpdateProjTextures() ||
        !BindPixelTexture( Bindings.DiffuseTexturePixel, Packet.Resources.pDiffuse, pSampler ) ||
        !BindPixelTexture( Bindings.DamageTexturePixel, Packet.pDamageMask, pSampler ) ||
        !BindPixelTexture( Bindings.FaceShadowTexturePixel, pFaceShadow, pFaceShadowSampler ) ||
        !BindPixelTexture( Bindings.FaceShadowDepthTexturePixel, pFaceShadowDepth, pPointSampler ) ||
        !BindPixelTexture( Bindings.ProjectionAtlasTexturePixel, pProjectionAtlas, pSampler ) ||
        !BindPixelTexture( Bindings.AmbientVisibilityTexturePixel,
                           m_pAmbientVisibilityResource ? m_pAmbientVisibilityResource
                                                        : vram_GetShaderResource( m_WhiteTexture ), pPointSampler ) ||
        ( Bindings.LightingDataPixel != 0xFFFFFFFFu &&
          !shader_BindStorageBuffer(
              shader_storage_buffer_binding( SHADER_STAGE_PIXEL, Bindings.LightingDataPixel, pLighting ) ) ) ||
        ( Bindings.ProjectionLightingDataPixel != 0xFFFFFFFFu &&
          !shader_BindStorageBuffer( shader_storage_buffer_binding(
              SHADER_STAGE_PIXEL, Bindings.ProjectionLightingDataPixel,
              rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_PROJECTION_LIGHTING ) ) ) ) ) ||
        ( Bindings.AmbientDataPixel != 0xFFFFFFFFu &&
          !shader_BindStorageBuffer( shader_storage_buffer_binding(
              SHADER_STAGE_PIXEL, Bindings.AmbientDataPixel,
              rbuffer_GetResource( GetFrameBuffer( FrameStorage::STREAM_AMBIENT_LIGHTING ) ) ) ) ) ||
        ( Bindings.ShadowMatricesPixel != 0xFFFFFFFFu &&
          !shader_BindStorageBuffer(
              shader_storage_buffer_binding( SHADER_STAGE_PIXEL, Bindings.ShadowMatricesPixel, pShadowMatrices ) ) ) ||
        ( Bindings.ShadowDataPixel != 0xFFFFFFFFu &&
          !shader_BindStorageBuffer(
              shader_storage_buffer_binding( SHADER_STAGE_PIXEL, Bindings.ShadowDataPixel, pShadowData ) ) ) ||
        !rbuffer_BindVertex( VertexBuffer, 0 ) ||
        !rbuffer_BindIndex( IndexBuffer, RBUFFER_INDEX_FORMAT_U16 ) ||
        !rdraw_DrawIndexedInstanced( IndexCount, 1, pGpuMesh ? 0 : Packet.FirstDynamicIndex,
                                     pGpuMesh ? 0 : Packet.FirstDynamicVertex, 0 ) )
    {
        return FALSE;
    }

    if( Pass.DepthMode == GEOM_DEPTH_PREPASS )
    {
        return TRUE;
    }

    m_nForwardOpaqueGpuDraws++;
    m_nForwardOpaqueInstances++;
    m_nForwardOpaqueSubmittedIndices += IndexCount;

    return TRUE;
}

} // namespace render
