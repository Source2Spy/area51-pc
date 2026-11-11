//=========================================================================
//
//  XeGtaoFeature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "XeGtaoFeature.hpp"
#include "../../Backend/RenderBackend.hpp"
#include "../../Pipeline/RenderPipeline.hpp"
#include <cassert>
#include "../../../../xCore/3rdParty/XeGTAO/XeGTAO.h"
#include "a51_compute_xegtao_prepare.hpp"
#include "a51_compute_xegtao_prepare_msaa.hpp"
#include "a51_compute_xegtao_prepare_half.hpp"
#include "a51_compute_xegtao_prepare_half_msaa.hpp"
#include "a51_compute_xegtao_medium.hpp"
#include "a51_compute_xegtao_high.hpp"
#include "a51_compute_xegtao_ultra.hpp"
#include "a51_compute_xegtao_medium_msaa.hpp"
#include "a51_compute_xegtao_high_msaa.hpp"
#include "a51_compute_xegtao_ultra_msaa.hpp"
#include "a51_compute_xegtao_denoise.hpp"
#include "a51_compute_xegtao_denoise_final.hpp"
#include "a51_compute_xegtao_resolve.hpp"

namespace render
{

namespace
{

struct XeGtaoConstants
{
    XeGTAO::GTAOConstants GTAO;
    u32                   FullSize[2];
    u32                   AOSize[2];
    u32                   Downsample;
    f32                   NearZ;
    f32                   FarZ;
    f32                   Strength;
};

static_assert( sizeof( XeGtaoConstants ) == 128, "XeGTAO constants must match HLSL" );

//=========================================================================

RenderResourceHandle CreateTexture( FrameGraphBuilder& Graph, RenderExtent Extent, rtarget_format Format,
                                    const char* pName, xbool Sampled = FALSE, u32 Mips = 1 )
{
    RenderResourceDesc Desc;
    Desc.Extent     = Extent;
    Desc.Format     = Format;
    Desc.MipCount   = Mips;
    Desc.Usage      = RENDER_RESOURCE_USAGE_STORAGE_WRITE |
                      ( Sampled ? RENDER_RESOURCE_USAGE_SAMPLED : RENDER_RESOURCE_USAGE_STORAGE_READ );
    Desc.pDebugName = pName;
    return Graph.CreateResource( Desc );
}

//=========================================================================

void SetOutput( RenderComputeBinding& Binding, RenderResourceHandle Resource, u32 Mip = 0, xbool Cycle = TRUE )
{
    Binding.Resource = Resource;
    Binding.MipLevel = Mip;
    Binding.Cycle    = Cycle;
}

} // namespace

//=========================================================================
//  TYPES
//=========================================================================

struct XeGtaoFeature::PassData
{
    XeGtaoFeature*       pOwner;
    Kernel               Shader;
    XeGtaoConstants      Constants;
    RenderExtent         DispatchExtent;
    RenderResourceHandle Source;
    RenderResourceHandle Normals;
    RenderResourceHandle Depths;
    RenderResourceHandle FullDepth;
    RenderResourceHandle WorkingAO;
    RenderResourceHandle Edges;
    RenderResourceHandle IntermediateAO;
    RenderResourceHandle DenoiseSource;
    RenderResourceHandle FilteredAO;
    RenderResourceHandle Visibility;
};

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

XeGtaoFeature::XeGtaoFeature( void )
    : m_Quality( AMBIENT_OCCLUSION_DEFAULT ),
      m_Radius( 50.0f ),
      m_Strength( 1.0f )
{
}

//=========================================================================

xbool XeGtaoFeature::Init( void )
{
    Kill();

    typedef xbool ( *GetShaderDesc )( shader_desc&, shader_format );
    static const GetShaderDesc Kernels[KERNEL_COUNT] =
    {
        A51Compute_xegtao_prepare_GetDesc,
        A51Compute_xegtao_prepare_msaa_GetDesc,
        A51Compute_xegtao_prepare_half_GetDesc,
        A51Compute_xegtao_prepare_half_msaa_GetDesc,
        A51Compute_xegtao_medium_GetDesc,
        A51Compute_xegtao_high_GetDesc,
        A51Compute_xegtao_ultra_GetDesc,
        A51Compute_xegtao_medium_msaa_GetDesc,
        A51Compute_xegtao_high_msaa_GetDesc,
        A51Compute_xegtao_ultra_msaa_GetDesc,
        A51Compute_xegtao_denoise_GetDesc,
        A51Compute_xegtao_denoise_final_GetDesc,
        A51Compute_xegtao_resolve_GetDesc
    };

    for( u32 i = 0; i < KERNEL_COUNT; ++i )
    {
        shader_desc Desc;
        if( !Kernels[i]( Desc, shader_GetPreferredFormat() ) || !shader_Create( m_Shaders[i], Desc ) )
        {
            Kill();
            RENDER_THROW( "XeGtaoFeature", "failed to create compute kernel %u", i );
            return FALSE;
        }
        m_Compute[i] = Desc.Compute;
    }

    if( !rstate_CreateSampler( m_PointSampler, RSTATE_SAMPLER_PRESET_POINT_CLAMP, "XeGtaoPointClamp" ) )
    {
        Kill();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void XeGtaoFeature::Kill( void )
{
    for( u32 i = 0; i < KERNEL_COUNT; ++i )
    {
        shader_Destroy( m_Shaders[i] );
        m_Compute[i] = shader_compute_desc();
    }
    rstate_DestroySampler( m_PointSampler );
}

//=========================================================================

xbool XeGtaoFeature::BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph,
                                      RenderFrameContext& Frame )
{
    if( ( Stage != RENDER_FEATURE_STAGE_AMBIENT_OCCLUSION &&
          Stage != RENDER_FEATURE_STAGE_FIRST_PERSON_AMBIENT_OCCLUSION ) ||
        m_Quality == AmbientOcclusionQuality::Disabled || m_Strength == 0.0f || !Frame.SceneRendered )
    {
        return TRUE;
    }

    if( Stage == RENDER_FEATURE_STAGE_FIRST_PERSON_AMBIENT_OCCLUSION )
    {
        if( m_Quality < AmbientOcclusionQuality::Medium )
        {
            return TRUE;
        }

        if( !GetRenderPipeline().GetBackend().GetForwardEncoder().BuildFirstPersonDepthPass( Frame ) )
        {
            return FALSE;
        }

        return !Frame.FirstPersonLinearDepth.IsValid() ||
               BuildVisibility( Graph, Frame, Frame.FirstPersonLinearDepth, 50.0f, TRUE,
                                 Frame.FirstPersonAmbientVisibility );
    }

    return BuildVisibility( Graph, Frame, Frame.GetSceneLinearDepthTarget(), m_Radius, FALSE,
                             Frame.SceneAmbientVisibility );
}

//=========================================================================

xbool XeGtaoFeature::BuildVisibility( FrameGraphBuilder& Graph, const RenderFrameContext& Frame,
                                      RenderResourceHandle Source, f32 Radius, xbool FirstPerson,
                                      RenderResourceHandle& Visibility )
{
    const RenderResourceDesc* pSource = Graph.GetDesc( Source );
    const RenderResourceHandle Normals = FirstPerson ? Frame.FirstPersonNormals : Frame.SceneNormals;
    const RenderResourceDesc* pNormals = Graph.GetDesc( Normals );
    const RenderView* pView = Frame.GetView( 0 );
    const view* pSourceView = pView ? pView->GetSourceView() : NULL;
    if( !pSource || !pSourceView || !pSource->Extent.IsValid() || pSource->ArrayLayers != 1 ||
        Frame.Desc.Views.ViewCount != 1 || pSource->Format != RTARGET_FORMAT_R32F || Radius <= 0.0f ||
        !pNormals || pNormals->Format != RTARGET_FORMAT_RGBA8 ||
        pNormals->Extent.Width != pSource->Extent.Width || pNormals->Extent.Height != pSource->Extent.Height ||
        pNormals->SampleCount != pSource->SampleCount || pNormals->ArrayLayers != 1 )
    {
        RENDER_THROW( "XeGtaoFeature", "invalid scene depth or view for XeGTAO" );
        return FALSE;
    }

    const RenderExtent FullExtent     = pSource->Extent;
    const xbool        Multisample    = pSource->SampleCount > 1;
    const u32          Downsample     = m_Quality == AmbientOcclusionQuality::Low ? 2 : 1;
    const xbool        HalfResolution = Downsample == 2;
    Kernel MainKernel    = Multisample ? KERNEL_MAIN_MEDIUM_MSAA : KERNEL_MAIN_MEDIUM;
    u32    DenoisePasses = 2;

    if( m_Quality == AmbientOcclusionQuality::High )
    {
        MainKernel    = Multisample ? KERNEL_MAIN_HIGH_MSAA : KERNEL_MAIN_HIGH;
        DenoisePasses = 3;
    }
    else if( m_Quality == AmbientOcclusionQuality::Ultra )
    {
        MainKernel    = Multisample ? KERNEL_MAIN_ULTRA_MSAA : KERNEL_MAIN_ULTRA;
        DenoisePasses = 4;
    }

    const RenderExtent AOExtent( ( FullExtent.Width + Downsample - 1 ) / Downsample,
                                 ( FullExtent.Height + Downsample - 1 ) / Downsample );
    const RenderExtent PaddedExtent( ( AOExtent.Width + 15 ) & ~15u, ( AOExtent.Height + 15 ) & ~15u );

    PassData Data = {};
    Data.pOwner               = this;
    Data.Constants.FullSize[0] = FullExtent.Width;
    Data.Constants.FullSize[1] = FullExtent.Height;
    Data.Constants.AOSize[0]   = AOExtent.Width;
    Data.Constants.AOSize[1]   = AOExtent.Height;
    Data.Constants.Downsample = Downsample;
    Data.Constants.Strength   = m_Strength;
    pSourceView->GetZLimits( Data.Constants.NearZ, Data.Constants.FarZ );

    const matrix4& Projection = pSourceView->GetV2C();
    const RenderExtent OutputExtent = Frame.Desc.Output.Extent.IsValid() ? Frame.Desc.Output.Extent : FullExtent;
    rdraw_viewport Viewport = pView->Viewport;
    Viewport.TopLeftX *= static_cast<f32>( FullExtent.Width ) / OutputExtent.Width;
    Viewport.TopLeftY *= static_cast<f32>( FullExtent.Height ) / OutputExtent.Height;
    Viewport.Width    *= static_cast<f32>( FullExtent.Width ) / OutputExtent.Width;
    Viewport.Height   *= static_cast<f32>( FullExtent.Height ) / OutputExtent.Height;

    XeGTAO::GTAOSettings Settings;
    Settings.Radius        = Radius;
    Settings.DenoisePasses = DenoisePasses;
    XeGTAO::GTAOUpdateConstants( Data.Constants.GTAO, PaddedExtent.Width, PaddedExtent.Height, Settings,
                               reinterpret_cast<const f32*>( &Projection ), FALSE, 0 );

    // The engine mirrors view X; XeGTAO needs a positive horizontal pixel size.
    XeGTAO::GTAOConstants& Constants = Data.Constants.GTAO;
    const f32 TanX = 1.0f / x_abs( Projection( 0, 0 ) );
    const f32 TanY = 1.0f / x_abs( Projection( 1, 1 ) );
    Constants.CameraTanHalfFOV = { TanX, TanY };
    Constants.NDCToViewMul = { 2.0f * TanX * PaddedExtent.Width * Downsample / Viewport.Width,
                              -2.0f * TanY * PaddedExtent.Height * Downsample / Viewport.Height };
    Constants.NDCToViewAdd = { -TanX * ( 1.0f + 2.0f * Viewport.TopLeftX / Viewport.Width + Projection( 2, 0 ) ),
                               TanY * ( 1.0f + 2.0f * Viewport.TopLeftY / Viewport.Height - Projection( 2, 1 ) ) };
    Constants.NDCToViewMul_x_PixelSize = { Constants.NDCToViewMul.x * Constants.ViewportPixelSize.x,
                                          Constants.NDCToViewMul.y * Constants.ViewportPixelSize.y };
    Constants.NoiseIndex = 0;

    Data.Source = Source;
    Data.Normals = Normals;
    Data.Depths = CreateTexture( Graph, PaddedExtent, RTARGET_FORMAT_R32F,
                                 FirstPerson ? "FirstPersonXeGtaoDepths" : "XeGtaoDepths", TRUE, 5 );
    if( HalfResolution )
    {
        Data.FullDepth = CreateTexture( Graph, FullExtent, RTARGET_FORMAT_R32F, "XeGtaoFullDepth" );
    }
    Data.WorkingAO = CreateTexture( Graph, AOExtent, RTARGET_FORMAT_R8_UINT,
                                    FirstPerson ? "FirstPersonXeGtaoWorkingAO" : "XeGtaoWorkingAO" );
    Data.Edges = CreateTexture( Graph, AOExtent, RTARGET_FORMAT_R8,
                                FirstPerson ? "FirstPersonXeGtaoEdges" : "XeGtaoEdges" );
    Data.IntermediateAO = CreateTexture( Graph, AOExtent, RTARGET_FORMAT_R8_UINT,
                                         FirstPerson ? "FirstPersonXeGtaoIntermediateAO" : "XeGtaoIntermediateAO" );
    Data.FilteredAO = CreateTexture( Graph, AOExtent, RTARGET_FORMAT_R8,
        FirstPerson ? "FirstPersonAmbientVisibility" :
            ( HalfResolution ? "XeGtaoFilteredAO" : "SceneAmbientVisibility" ), !HalfResolution );
    Data.Visibility = HalfResolution
        ? CreateTexture( Graph, FullExtent, RTARGET_FORMAT_R8, "SceneAmbientVisibility", TRUE ) : Data.FilteredAO;
    if( !Data.Depths.IsValid() || ( HalfResolution && !Data.FullDepth.IsValid() ) ||
        !Data.WorkingAO.IsValid() || !Data.Edges.IsValid() ||
        !Data.IntermediateAO.IsValid() || !Data.FilteredAO.IsValid() || !Data.Visibility.IsValid() )
    {
        return FALSE;
    }

    const u32 MaxDenoisePasses = 4;
    Kernel Sequence[2 + MaxDenoisePasses + 1];
    u32 PassCount = 0;
    Sequence[PassCount++] = HalfResolution ? ( Multisample ? KERNEL_PREPARE_HALF_MSAA : KERNEL_PREPARE_HALF )
                                          : ( Multisample ? KERNEL_PREPARE_MSAA : KERNEL_PREPARE );
    Sequence[PassCount++] = MainKernel;
    for( u32 i = 0; i < DenoisePasses; ++i )
    {
        Sequence[PassCount++] = i + 1 == DenoisePasses ? KERNEL_DENOISE_FINAL : KERNEL_DENOISE;
    }
    if( HalfResolution )
    {
        Sequence[PassCount++] = KERNEL_RESOLVE;
    }

    static const char* Names[KERNEL_COUNT] =
    {
        "XeGtaoPrepare", "XeGtaoPrepareMSAA", "XeGtaoPrepareHalf", "XeGtaoPrepareHalfMSAA",
        "XeGtaoMedium", "XeGtaoHigh", "XeGtaoUltra",
        "XeGtaoMediumMSAA", "XeGtaoHighMSAA", "XeGtaoUltraMSAA",
        "XeGtaoDenoise", "XeGtaoDenoiseFinal", "XeGtaoResolve"
    };

    static const char* FirstPersonNames[KERNEL_COUNT] =
    {
        "FirstPersonXeGtaoPrepare", "FirstPersonXeGtaoPrepareMSAA",
        "FirstPersonXeGtaoPrepareHalf", "FirstPersonXeGtaoPrepareHalfMSAA",
        "FirstPersonXeGtaoMedium", "FirstPersonXeGtaoHigh", "FirstPersonXeGtaoUltra",
        "FirstPersonXeGtaoMediumMSAA", "FirstPersonXeGtaoHighMSAA", "FirstPersonXeGtaoUltraMSAA",
        "FirstPersonXeGtaoDenoise", "FirstPersonXeGtaoDenoiseFinal", "FirstPersonXeGtaoResolve"
    };

    u32 DenoiseIndex = 0;

    for( u32 i = 0; i < PassCount; ++i )
    {
        Data.Shader         = Sequence[i];
        Data.DispatchExtent = AOExtent;

        RenderPassDesc Desc;
        Desc.pDebugName     = FirstPerson ? FirstPersonNames[Data.Shader] : Names[Data.Shader];
        Desc.Type           = RENDER_PASS_COMPUTE;
        Desc.pComputeShader = &m_Shaders[Data.Shader];

        if( Data.Shader <= KERNEL_PREPARE_HALF_MSAA )
        {
            Data.DispatchExtent      = PaddedExtent;
            Desc.ComputeTextureCount = HalfResolution ? 6 : 5;

            for( u32 Mip = 0; Mip < 5; ++Mip )
            {
                SetOutput( Desc.ComputeTextures[Mip], Data.Depths, Mip, FALSE );
            }

            if( HalfResolution )
            {
                SetOutput( Desc.ComputeTextures[5], Data.FullDepth );
            }
        }
        else if( Data.Shader >= KERNEL_MAIN_MEDIUM && Data.Shader <= KERNEL_MAIN_ULTRA_MSAA )
        {
            Desc.ComputeTextureCount = 2;
            SetOutput( Desc.ComputeTextures[0], Data.WorkingAO );
            SetOutput( Desc.ComputeTextures[1], Data.Edges );
        }
        else if( Data.Shader == KERNEL_DENOISE || Data.Shader == KERNEL_DENOISE_FINAL )
        {
            Data.DenoiseSource = ( DenoiseIndex & 1 ) ? Data.IntermediateAO : Data.WorkingAO;
            const RenderResourceHandle Output =
                Data.Shader == KERNEL_DENOISE_FINAL ? Data.FilteredAO
                                                   : ( ( DenoiseIndex & 1 ) ? Data.WorkingAO : Data.IntermediateAO );
            Desc.ComputeTextureCount = 1;
            SetOutput( Desc.ComputeTextures[0], Output, 0, Data.Shader == KERNEL_DENOISE_FINAL );
            ++DenoiseIndex;
        }
        else
        {
            Data.DispatchExtent      = FullExtent;
            Desc.ComputeTextureCount = 1;
            SetOutput( Desc.ComputeTextures[0], Data.Visibility );
        }

        const RenderPassId Pass = Graph.AddPass( Desc, Execute, Data );

        if( Pass == INVALID_RENDER_PASS )
        {
            return FALSE;
        }

        if( Data.Shader <= KERNEL_PREPARE_HALF_MSAA )
        {
            Graph.Read( Pass, Data.Source, Multisample ? RENDER_RESOURCE_ACCESS_STORAGE_READ
                                                      : RENDER_RESOURCE_ACCESS_SAMPLED_READ );
        }
        else if( Data.Shader >= KERNEL_MAIN_MEDIUM && Data.Shader <= KERNEL_MAIN_ULTRA_MSAA )
        {
            Graph.Read( Pass, Data.Depths );
            Graph.Read( Pass, Data.Normals, Multisample ? RENDER_RESOURCE_ACCESS_STORAGE_READ
                                                       : RENDER_RESOURCE_ACCESS_SAMPLED_READ );
            Graph.Read( Pass, Data.Source, Multisample ? RENDER_RESOURCE_ACCESS_STORAGE_READ
                                                      : RENDER_RESOURCE_ACCESS_SAMPLED_READ );
        }
        else if( Data.Shader == KERNEL_DENOISE || Data.Shader == KERNEL_DENOISE_FINAL )
        {
            Graph.Read( Pass, Data.DenoiseSource, RENDER_RESOURCE_ACCESS_STORAGE_READ );
            Graph.Read( Pass, Data.Edges, RENDER_RESOURCE_ACCESS_STORAGE_READ );
        }
        else
        {
            Graph.Read( Pass, Data.Depths );
            Graph.Read( Pass, Data.FilteredAO, RENDER_RESOURCE_ACCESS_STORAGE_READ );
            Graph.Read( Pass, Data.FullDepth, RENDER_RESOURCE_ACCESS_STORAGE_READ );
        }
    }

    Visibility = Data.Visibility;
    return !Graph.GetLastError();
}

//=========================================================================

xbool XeGtaoFeature::Execute( RenderPassContext& Context, const PassData& Data )
{
    if( !shader_PushUniformData( SHADER_STAGE_COMPUTE, 0, &Data.Constants, sizeof( Data.Constants ) ) )
    {
        return FALSE;
    }

    const xbool Prepare = Data.Shader <= KERNEL_PREPARE_HALF_MSAA;
    const xbool Main = Data.Shader >= KERNEL_MAIN_MEDIUM && Data.Shader <= KERNEL_MAIN_ULTRA_MSAA;
    if( Data.Shader == KERNEL_PREPARE_MSAA || Data.Shader == KERNEL_PREPARE_HALF_MSAA )
    {
        const shader_resource* pDepth = Context.ReadStorageTexture( Data.Source );
        if( !pDepth || !shader_BindStorageTexture( shader_storage_texture_binding( SHADER_STAGE_COMPUTE, 0, pDepth ) ) )
        {
            return FALSE;
        }
    }
    else if( Prepare || Main || Data.Shader == KERNEL_RESOLVE )
    {
        const rtarget* pDepth = Context.Read( Prepare ? Data.Source : Data.Depths );
        if( !pDepth || !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_COMPUTE, 0,
            rtarget_GetShaderResource( *pDepth ), &Data.pOwner->m_PointSampler ) ) )
        {
            return FALSE;
        }
    }

    if( Main )
    {
        const xbool Multisample = Data.Shader >= KERNEL_MAIN_MEDIUM_MSAA;
        if( Multisample )
        {
            const shader_resource* pDepth = Context.ReadStorageTexture( Data.Source );
            const shader_resource* pNormals = Context.ReadStorageTexture( Data.Normals );
            if( !pDepth || !pNormals ||
                !shader_BindStorageTexture( shader_storage_texture_binding( SHADER_STAGE_COMPUTE, 0, pDepth ) ) ||
                !shader_BindStorageTexture( shader_storage_texture_binding( SHADER_STAGE_COMPUTE, 1, pNormals ) ) )
            {
                return FALSE;
            }
        }
        else
        {
            const rtarget* pDepth = Context.Read( Data.Source );
            const rtarget* pNormals = Context.Read( Data.Normals );
            if( !pDepth || !pNormals ||
                !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_COMPUTE, 1,
                    rtarget_GetShaderResource( *pDepth ), &Data.pOwner->m_PointSampler ) ) ||
                !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_COMPUTE, 2,
                    rtarget_GetShaderResource( *pNormals ), &Data.pOwner->m_PointSampler ) ) )
            {
                return FALSE;
            }
        }
    }

    if( Data.Shader == KERNEL_DENOISE || Data.Shader == KERNEL_DENOISE_FINAL || Data.Shader == KERNEL_RESOLVE )
    {
        const xbool Resolve = Data.Shader == KERNEL_RESOLVE;
        const RenderResourceHandle AO = Resolve ? Data.FilteredAO : Data.DenoiseSource;
        const shader_resource* pAO = Context.ReadStorageTexture( AO );
        const shader_resource* pGuide = Context.ReadStorageTexture( Resolve ? Data.FullDepth : Data.Edges );
        if( !pAO || !pGuide ||
            !shader_BindStorageTexture( shader_storage_texture_binding( SHADER_STAGE_COMPUTE, 0, pAO ) ) ||
            !shader_BindStorageTexture( shader_storage_texture_binding( SHADER_STAGE_COMPUTE, 1, pGuide ) ) )
        {
            return FALSE;
        }
    }

    const shader_compute_desc& Compute = Data.pOwner->m_Compute[Data.Shader];
    const u32 Coverage = Prepare ? 2 : 1;
    const u32 Width = Compute.ThreadCountX * Coverage;
    const u32 Height = Compute.ThreadCountY * Coverage;
    return Width && Height && Context.Dispatch( ( Data.DispatchExtent.Width + Width - 1 ) / Width,
                                                ( Data.DispatchExtent.Height + Height - 1 ) / Height );
}

} // namespace render
