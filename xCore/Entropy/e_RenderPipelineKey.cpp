//==============================================================================
//
//  e_RenderPipelineKey.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_RenderPipelineKey.hpp"
#include "x_debug.hpp"
#include "x_memory.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void render_NormalizePipelineState( const render_pipeline_desc& Source, render_pipeline_desc& Desc )
{
    Desc = Source;

    // Disabled blending preserves only the color write mask.
    for( u32 i = 0; i < Desc.ColorCount && i < RTARGET_MAX_TARGETS; ++i )
    {
        if( !Desc.ColorTargets[i].Blend.bBlendEnable )
        {
            const u8 Mask = Desc.ColorTargets[i].Blend.ColorWriteMask;

            Desc.ColorTargets[i].Blend                = rstate_blend_desc();
            Desc.ColorTargets[i].Blend.ColorWriteMask = Mask;
        }
    }

    if( Desc.DepthFormat == RTARGET_FORMAT_COUNT )
    {
        Desc.Depth.bDepthEnable   = FALSE;
        Desc.Depth.bStencilEnable = FALSE;
    }

    if( !Desc.Depth.bDepthEnable )
    {
        Desc.Depth.bDepthWrite = FALSE;
        Desc.Depth.DepthFunc   = rstate_depth_desc().DepthFunc;
    }

    if( !Desc.Depth.bStencilEnable )
    {
        const rstate_depth_desc Default;

        Desc.Depth.StencilReadMask  = Default.StencilReadMask;
        Desc.Depth.StencilWriteMask = Default.StencilWriteMask;
        Desc.Depth.FrontFace        = Default.FrontFace;
        Desc.Depth.BackFace         = Default.BackFace;
    }

    if( !Desc.Raster.bDepthBiasEnable )
    {
        Desc.Raster.DepthBias            = 0.0f;
        Desc.Raster.DepthBiasClamp       = 0.0f;
        Desc.Raster.SlopeScaledDepthBias = 0.0f;
    }

    // Canonicalize signed zero before hashing floating-point state.
    if( Desc.Raster.DepthBias == 0.0f )
    {
        Desc.Raster.DepthBias = 0.0f;
    }

    if( Desc.Raster.DepthBiasClamp == 0.0f )
    {
        Desc.Raster.DepthBiasClamp = 0.0f;
    }

    if( Desc.Raster.SlopeScaledDepthBias == 0.0f )
    {
        Desc.Raster.SlopeScaledDepthBias = 0.0f;
    }
}

//==============================================================================

render_pipeline_key::render_pipeline_key( const render_pipeline_desc& Source )
    : Count( 0 )
{
    render_pipeline_desc Desc;
    render_NormalizePipelineState( Source, Desc );

    auto Add = [&]( u64 Value )
    {
        ASSERT( Count < ARRAYSIZE( Values ) );
        Values[Count++] = Value;
    };

    auto FloatBits = []( f32 Value )
    {
        u32 Bits;

        if( Value == 0.0f )
        {
            Value = 0.0f;
        }

        x_memcpy( &Bits, &Value, sizeof( Bits ) );
        return Bits;
    };

    Add( Desc.Shader.pVertexShader ? Desc.Shader.pVertexShader->ProgramId : 0 );
    Add( Desc.Shader.pPixelShader ? Desc.Shader.pPixelShader->ProgramId : 0 );
    Add( Desc.Shader.Topology );
    Add( Desc.Shader.VertexBufferCount );

    u32 BufferOrder[16];
    u32 AttributeOrder[16];

    if( Desc.Shader.VertexBufferCount > ARRAYSIZE( BufferOrder ) ||
        Desc.Shader.InputElementCount > ARRAYSIZE( AttributeOrder ) ||
        ( Desc.Shader.VertexBufferCount && !Desc.Shader.pVertexBuffers ) ||
        ( Desc.Shader.InputElementCount && !Desc.Shader.pInputElements ) || Desc.ColorCount > RTARGET_MAX_TARGETS )
    {
        ASSERT( FALSE );
        Count = 0;
        Add( 0xffffffffffffffffull );
        return;
    }

    // Sort vertex buffers by slot before serializing their state.
    for( u32 i = 0; i < Desc.Shader.VertexBufferCount; ++i )
    {
        u32 j = i;

        while( j && Desc.Shader.pVertexBuffers[BufferOrder[j - 1]].Slot > Desc.Shader.pVertexBuffers[i].Slot )
        {
            BufferOrder[j] = BufferOrder[j - 1];
            --j;
        }

        BufferOrder[j] = i;
    }

    for( u32 i = 0; i < Desc.Shader.VertexBufferCount; ++i )
    {
        const shader_vertex_buffer_desc& Buffer = Desc.Shader.pVertexBuffers[BufferOrder[i]];
        Add( Buffer.Slot );
        Add( Buffer.Stride );
        Add( !!Buffer.bPerInstance );
        Add( Buffer.bPerInstance ? Buffer.InstanceStepRate : 0 );
    }

    Add( Desc.Shader.InputElementCount );

    // Attribute declaration order does not affect the key.
    for( u32 i = 0; i < Desc.Shader.InputElementCount; ++i )
    {
        u32 j = i;

        while( j &&
               Desc.Shader.pInputElements[AttributeOrder[j - 1]].Location > Desc.Shader.pInputElements[i].Location )
        {
            AttributeOrder[j] = AttributeOrder[j - 1];
            --j;
        }

        AttributeOrder[j] = i;
    }

    for( u32 i = 0; i < Desc.Shader.InputElementCount; ++i )
    {
        const shader_vertex_element& Element = Desc.Shader.pInputElements[AttributeOrder[i]];
        Add( Element.Location );
        Add( Element.InputSlot );
        Add( Element.Format );
        Add( Element.AlignedByteOffset );
    }

    // Color formats and blend state.
    Add( Desc.ColorCount );

    for( u32 i = 0; i < Desc.ColorCount; ++i )
    {
        const render_color_target_desc& Target = Desc.ColorTargets[i];
        const rstate_blend_desc&        Blend  = Target.Blend;
        Add( Target.Format );
        Add( !!Target.Blend.bBlendEnable );
        Add( Target.Blend.ColorWriteMask );
        Add( Blend.SrcBlend );
        Add( Blend.DestBlend );
        Add( Blend.BlendOp );
        Add( Blend.SrcBlendAlpha );
        Add( Blend.DestBlendAlpha );
        Add( Blend.BlendOpAlpha );
    }

    // Depth, multisampling and stencil state.
    Add( Desc.DepthFormat );
    Add( Desc.SampleCount );
    Add( Desc.SampleMask );
    Add( !!Desc.bAlphaToCoverage );

    const rstate_depth_desc& Depth = Desc.Depth;

    Add( !!Depth.bDepthEnable );
    Add( !!Depth.bDepthWrite );
    Add( Depth.DepthFunc );
    Add( !!Depth.bStencilEnable );
    Add( Depth.StencilReadMask );
    Add( Depth.StencilWriteMask );

    for( s32 iFace = 0; iFace < 2; ++iFace )
    {
        const auto& Face = iFace ? Depth.BackFace : Depth.FrontFace;
        Add( Face.StencilFailOp );
        Add( Face.StencilDepthFailOp );
        Add( Face.StencilPassOp );
        Add( Face.StencilFunc );
    }

    // Raster state uses the canonical floating-point representation.
    const rstate_raster_desc& Raster = Desc.Raster;

    Add( Raster.FillMode );
    Add( Raster.CullMode );
    Add( Raster.FrontFace );
    Add( !!Raster.bDepthBiasEnable );
    Add( FloatBits( Raster.bDepthBiasEnable ? Raster.DepthBias : 0.0f ) );
    Add( FloatBits( Raster.bDepthBiasEnable ? Raster.DepthBiasClamp : 0.0f ) );
    Add( FloatBits( Raster.bDepthBiasEnable ? Raster.SlopeScaledDepthBias : 0.0f ) );
    Add( !!Raster.bDepthClipEnable );
}

//==============================================================================

u64 render_pipeline_key::Hash( void ) const
{
    u64 Result = 14695981039346656037ull;

    for( u32 i = 0; i < Count; ++i )
    {
        Result = ( Result ^ Values[i] ) * 1099511628211ull;
    }

    return Result;
}

//==============================================================================

xbool render_pipeline_key::operator==( const render_pipeline_key& Other ) const
{
    return Count == Other.Count && !x_memcmp( Values, Other.Values, Count * sizeof( Values[0] ) );
}
