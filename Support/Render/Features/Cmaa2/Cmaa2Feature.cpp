//=========================================================================
//
//  Cmaa2Feature.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Cmaa2Feature.hpp"
#include "a51_compute_cmaa2_init.hpp"
#include "a51_compute_cmaa2_edges.hpp"
#include "a51_compute_cmaa2_args.hpp"
#include "a51_compute_cmaa2_candidates.hpp"
#include "a51_compute_cmaa2_apply.hpp"

namespace render
{

namespace
{

struct Cmaa2Constants
{
    u32 Width;
    u32 Height;
    u32 CandidateCapacity;
    u32 BlendCapacity;
    u32 LocationCapacity;
    u32 ArgsMode;
    u32 Padding[2];
};

static_assert( sizeof( Cmaa2Constants ) == 32, "CMAA2 constants must match HLSL" );

//=========================================================================

RenderResourceHandle CreateBuffer( FrameGraphBuilder& Graph, u32 Count, u32 Stride, const char* pName,
                                   xbool Indirect = FALSE )
{
    RenderResourceDesc Desc;
    Desc.Type              = RENDER_RESOURCE_BUFFER;
    Desc.Buffer.Size       = Count * Stride;
    Desc.Buffer.Stride     = Stride;
    Desc.Buffer.UsageFlags = RBUFFER_USAGE_COMPUTE_STORAGE_READ | RBUFFER_USAGE_COMPUTE_STORAGE_WRITE |
                             ( Indirect ? RBUFFER_USAGE_INDIRECT : 0 );
    Desc.pDebugName = pName;
    return Graph.CreateResource( Desc );
}

//=========================================================================

RenderResourceHandle CreateTexture( FrameGraphBuilder& Graph, RenderExtent Extent, rtarget_format Format,
                                    const char* pName )
{
    RenderResourceDesc Desc;
    Desc.Extent = Extent;
    Desc.Format = Format;
    Desc.Usage  = RENDER_RESOURCE_USAGE_SAMPLED | RENDER_RESOURCE_USAGE_STORAGE_WRITE;
    Desc.pDebugName = pName;
    return Graph.CreateResource( Desc );
}

//=========================================================================

void SetOutput( RenderComputeBinding& Binding, RenderResourceHandle Resource, xbool ReadWrite = FALSE,
                xbool Preserve = FALSE )
{
    Binding.Resource         = Resource;
    Binding.ReadWrite        = ReadWrite;
    Binding.PreserveContents = Preserve;
    Binding.Cycle            = !ReadWrite && !Preserve;
}

} // namespace

//=========================================================================
//  TYPES
//=========================================================================

struct Cmaa2Feature::PassData
{
    Cmaa2Feature*        pOwner;
    Kernel               Shader;
    Cmaa2Constants       Constants;
    RenderResourceHandle Source;
    RenderResourceHandle Color;
    RenderResourceHandle Edges;
    RenderResourceHandle Heads;
    RenderResourceHandle Candidates;
    RenderResourceHandle Items;
    RenderResourceHandle Locations;
    RenderResourceHandle Control;
    RenderResourceHandle Arguments;
};

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

Cmaa2Feature::Cmaa2Feature( void ) : m_isEnabled( FALSE )
{
}

//=========================================================================

xbool Cmaa2Feature::Init( void )
{
    Kill();

    const shader_format Format = shader_GetPreferredFormat();
    struct KernelDesc
    {
        const char* pName;
        xbool ( *GetDesc )( shader_desc& Desc, shader_format Format );
    };

    static const KernelDesc Kernels[KERNEL_COUNT] =
    {
        { "Cmaa2Initialize", A51Compute_cmaa2_init_GetDesc },
        { "Cmaa2Edges", A51Compute_cmaa2_edges_GetDesc },
        { "Cmaa2Arguments", A51Compute_cmaa2_args_GetDesc },
        { "Cmaa2Candidates", A51Compute_cmaa2_candidates_GetDesc },
        { "Cmaa2Apply", A51Compute_cmaa2_apply_GetDesc }
    };

    for( u32 i = 0; i < KERNEL_COUNT; ++i )
    {
        const KernelDesc& KernelData = Kernels[i];
        shader_desc       Desc;
        if( !KernelData.GetDesc( Desc, Format ) )
        {
            Kill();
            RENDER_THROW( "Cmaa2Feature", "no embedded compute shader '%s' for this format", KernelData.pName );
            return FALSE;
        }
        Desc.pDebugName = KernelData.pName;

        if( !shader_Create( m_Shaders[i], Desc ) )
        {
            Kill();
            RENDER_THROW( "Cmaa2Feature", "failed to create compute shader '%s'", KernelData.pName );
            return FALSE;
        }
    }

    if( !rstate_CreateSampler( m_PointSampler, RSTATE_SAMPLER_PRESET_POINT_CLAMP, "Cmaa2PointClamp" ) )
    {
        Kill();
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void Cmaa2Feature::Kill( void )
{
    for( u32 i = 0; i < KERNEL_COUNT; ++i )
    {
        shader_Destroy( m_Shaders[i] );
    }

    rstate_DestroySampler( m_PointSampler );
}

//=========================================================================

xbool Cmaa2Feature::BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph,
                                     RenderFrameContext& Frame )
{
    if( Stage != RENDER_FEATURE_STAGE_ANTI_ALIASING || !m_isEnabled || !Frame.SceneRendered ||
        !Frame.Desc.EnablePostProcessing )
    {
        return TRUE;
    }

    const RenderResourceDesc* pSource = Graph.GetDesc( Frame.AntiAliasingSource );
    if( !pSource || pSource->Format != RTARGET_FORMAT_RGBA8 || pSource->SampleCount != 1 ||
        !pSource->Extent.IsValid() || pSource->Extent.Width > 16384 || pSource->Extent.Height > 16384 )
    {
        RENDER_THROW( "Cmaa2Feature", "CMAA2 requires an RGBA8 source with dimensions in [1, 16384]" );
        return FALSE;
    }

    const u64 PixelCount = static_cast<u64>( pSource->Extent.Width ) * pSource->Extent.Height;
    if( PixelCount > 0x04000000u )
    {
        RENDER_THROW( "Cmaa2Feature", "image exceeds the 26-bit blend-list address range" );
        return FALSE;
    }

    const RenderExtent Extent = pSource->Extent;
    const RenderExtent QuadExtent( ( Extent.Width + 1 ) / 2, ( Extent.Height + 1 ) / 2 );
    const u32          Count     = static_cast<u32>( PixelCount );
    const u32          QuadCount = QuadExtent.Width * QuadExtent.Height;

    PassData Data                    = {};
    Data.pOwner                      = this;
    Data.Constants.Width             = Extent.Width;
    Data.Constants.Height            = Extent.Height;
    Data.Constants.CandidateCapacity = Count;
    Data.Constants.BlendCapacity     = Count;
    Data.Constants.LocationCapacity  = QuadCount;
    Data.Source                      = Frame.AntiAliasingSource;
    Data.Color                       = CreateTexture( Graph, Extent, RTARGET_FORMAT_RGBA8, "Cmaa2Color" );
    Data.Edges =
        CreateTexture( Graph, RenderExtent( QuadExtent.Width, Extent.Height ), RTARGET_FORMAT_R8, "Cmaa2Edges" );
    Data.Heads      = CreateBuffer( Graph, QuadCount, sizeof( u32 ), "Cmaa2Heads" );
    Data.Candidates = CreateBuffer( Graph, Count, sizeof( u32 ), "Cmaa2Candidates" );
    Data.Items      = CreateBuffer( Graph, Count, sizeof( u32 ) * 2, "Cmaa2BlendItems" );
    Data.Locations  = CreateBuffer( Graph, QuadCount, sizeof( u32 ), "Cmaa2Locations" );
    Data.Control    = CreateBuffer( Graph, 16, sizeof( u32 ), "Cmaa2Control" );
    Data.Arguments  = CreateBuffer( Graph, 3, sizeof( u32 ), "Cmaa2DispatchArguments", TRUE );

    if( !Data.Color.IsValid() || !Data.Edges.IsValid() || !Data.Heads.IsValid() || !Data.Candidates.IsValid() ||
        !Data.Items.IsValid() || !Data.Locations.IsValid() || !Data.Control.IsValid() || !Data.Arguments.IsValid() )
    {
        return FALSE;
    }

    struct PassSpec
    {
        Kernel      Shader;
        u32         ArgsMode;
        const char* pName;
    };

    static const PassSpec Sequence[] =
    {
        { KERNEL_INIT,       0, "Cmaa2Initialize" },
        { KERNEL_EDGES,      0, "Cmaa2Edges" },
        { KERNEL_ARGS,       0, "Cmaa2CandidateArguments" },
        { KERNEL_CANDIDATES, 0, "Cmaa2Candidates" },
        { KERNEL_ARGS,       1, "Cmaa2ApplyArguments" },
        { KERNEL_APPLY,      0, "Cmaa2Apply" }
    };

    for( u32 i = 0; i < ARRAYSIZE( Sequence ); ++i )
    {
        Data.Shader             = Sequence[i].Shader;
        Data.Constants.ArgsMode = Sequence[i].ArgsMode;

        RenderPassDesc Desc;
        Desc.pDebugName     = Sequence[i].pName;
        Desc.Type           = RENDER_PASS_COMPUTE;
        Desc.pComputeShader = &m_Shaders[Data.Shader];
        switch( Data.Shader )
        {
            case KERNEL_INIT:
            {
                Desc.ComputeBufferCount = 1;
                SetOutput( Desc.ComputeBuffers[0], Data.Control );
                break;
            }
            case KERNEL_EDGES:
            {
                Desc.ComputeTextureCount = 2;
                Desc.ComputeBufferCount  = 3;
                SetOutput( Desc.ComputeTextures[0], Data.Color );
                SetOutput( Desc.ComputeTextures[1], Data.Edges );
                SetOutput( Desc.ComputeBuffers[0], Data.Heads );
                SetOutput( Desc.ComputeBuffers[1], Data.Candidates );
                SetOutput( Desc.ComputeBuffers[2], Data.Control, TRUE );
                break;
            }
            case KERNEL_ARGS:
            {
                Desc.ComputeBufferCount = 2;
                SetOutput( Desc.ComputeBuffers[0], Data.Control, TRUE );
                SetOutput( Desc.ComputeBuffers[1], Data.Arguments );
                break;
            }
            case KERNEL_CANDIDATES:
            {
                Desc.ComputeBufferCount = 4;
                SetOutput( Desc.ComputeBuffers[0], Data.Heads, TRUE );
                SetOutput( Desc.ComputeBuffers[1], Data.Items );
                SetOutput( Desc.ComputeBuffers[2], Data.Locations );
                SetOutput( Desc.ComputeBuffers[3], Data.Control, TRUE );
                break;
            }
            case KERNEL_APPLY:
            {
                Desc.ComputeTextureCount = 1;
                SetOutput( Desc.ComputeTextures[0], Data.Color, FALSE, TRUE );
                break;
            }
            default:
            {
                return FALSE;
            }
        }

        const RenderPassId Pass = Graph.AddPass( Desc, Execute, Data );
        if( Pass == INVALID_RENDER_PASS )
        {
            return FALSE;
        }

        if( Data.Shader == KERNEL_EDGES || Data.Shader == KERNEL_CANDIDATES || Data.Shader == KERNEL_APPLY )
        {
            Graph.Read( Pass, Data.Source );
        }
        if( Data.Shader == KERNEL_CANDIDATES )
        {
            Graph.Read( Pass, Data.Edges );
            Graph.Read( Pass, Data.Candidates, RENDER_RESOURCE_ACCESS_STORAGE_READ );
        }
        if( Data.Shader == KERNEL_APPLY )
        {
            Graph.Read( Pass, Data.Heads, RENDER_RESOURCE_ACCESS_STORAGE_READ );
            Graph.Read( Pass, Data.Items, RENDER_RESOURCE_ACCESS_STORAGE_READ );
            Graph.Read( Pass, Data.Locations, RENDER_RESOURCE_ACCESS_STORAGE_READ );
            Graph.Read( Pass, Data.Control, RENDER_RESOURCE_ACCESS_STORAGE_READ );
        }
        if( Data.Shader == KERNEL_CANDIDATES || Data.Shader == KERNEL_APPLY )
        {
            Graph.Read( Pass, Data.Arguments, RENDER_RESOURCE_ACCESS_INDIRECT_READ );
        }
    }

    RenderPassDesc Resolve;
    Resolve.pDebugName      = "Cmaa2Resolve";
    Resolve.Color           = Frame.FinalColor;
    Resolve.ColorLoadOp     = RTARGET_LOAD_DONT_CARE;
    const RenderPassId Pass = Graph.AddPass( Resolve, ExecuteResolve, Data.Color );
    if( Pass == INVALID_RENDER_PASS )
    {
        return FALSE;
    }

    Graph.Read( Pass, Data.Color );
    return !Graph.GetLastError() && Graph.DeclareCompositePipeline( Pass, COMPOSITE_BLEND_COPY );
}

//=========================================================================

xbool Cmaa2Feature::Execute( RenderPassContext& Context, PassData Data )
{
    if( !shader_PushUniformData( SHADER_STAGE_COMPUTE, 0, &Data.Constants, sizeof( Data.Constants ) ) )
    {
        return FALSE;
    }

    if( Data.Shader == KERNEL_EDGES || Data.Shader == KERNEL_CANDIDATES || Data.Shader == KERNEL_APPLY )
    {
        const rtarget* pSource = Context.Read( Data.Source );
        if( !pSource ||
            !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_COMPUTE, 0, rtarget_GetShaderResource( *pSource ),
                                                         &Data.pOwner->m_PointSampler ) ) )
        {
            return FALSE;
        }
    }

    if( Data.Shader == KERNEL_CANDIDATES )
    {
        const rtarget* pTexture = Context.Read( Data.Edges );
        if( !pTexture ||
            !shader_BindSampler( shader_sampler_binding(
                SHADER_STAGE_COMPUTE, 1, rtarget_GetShaderResource( *pTexture ), &Data.pOwner->m_PointSampler ) ) )
        {
            return FALSE;
        }
    }

    if( Data.Shader == KERNEL_CANDIDATES || Data.Shader == KERNEL_APPLY )
    {
        const RenderResourceHandle Inputs[] =
        {
            Data.Shader == KERNEL_CANDIDATES ? Data.Candidates : Data.Heads, Data.Items, Data.Locations, Data.Control
        };
        const u32 Count = Data.Shader == KERNEL_CANDIDATES ? 1 : 4;
        for( u32 i = 0; i < Count; ++i )
        {
            const rbuffer* pBuffer = Context.ReadBuffer( Inputs[i] );
            if( !pBuffer || !shader_BindStorageBuffer( shader_storage_buffer_binding(
                                SHADER_STAGE_COMPUTE, i, rbuffer_GetResource( *pBuffer ) ) ) )
            {
                return FALSE;
            }
        }

        return Context.DispatchIndirect( Data.Arguments );
    }

    if( Data.Shader == KERNEL_EDGES )
    {
        return Context.Dispatch( ( Data.Constants.Width + 27 ) / 28, ( Data.Constants.Height + 27 ) / 28 );
    }

    return Context.Dispatch( 1 );
}

//=========================================================================

xbool Cmaa2Feature::ExecuteResolve( RenderPassContext& Context, RenderResourceHandle Color )
{
    const rtarget* pSource = Context.Read( Color );
    return pSource && Context.Write( Context.GetFrame().FinalColor ) &&
           composite_Blit( *pSource, COMPOSITE_BLEND_COPY, 1.0f, NULL, RSTATE_SAMPLER_PRESET_POINT_CLAMP );
}

} // namespace render
