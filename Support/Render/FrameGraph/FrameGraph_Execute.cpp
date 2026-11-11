//=========================================================================
//
//  FrameGraph_Execute.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameGraph_Private.hpp"
#include "e_Engine.hpp"
#include "x_debug.hpp"

namespace render
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool FrameGraphBuilder::PrewarmCompositePipelines( void )
{
    for( s32 i = 0; i < m_ExecutionOrder.GetCount(); ++i )
    {
        const RenderPassId PassId   = m_ExecutionOrder[i];
        const Pass&        PassData = *m_Passes[PassId];

        if( PassData.Desc.CompositePipelineCount == 0 )
        {
            continue;
        }

        if( PassData.Desc.Type != RENDER_PASS_GRAPHICS && PassData.Desc.Type != RENDER_PASS_PRESENT )
        {
            Fail( "composite pipeline requires a graphics pass" );
            return FALSE;
        }

        const rtarget* pColor = PassData.Desc.Color.IsValid() ? Get( PassData.Desc.Color ) : NULL;
        const rtarget* pDepth = PassData.Desc.Depth.IsValid() ? Get( PassData.Desc.Depth ) : NULL;

        if( !pColor )
        {
            Fail( "composite pipeline requirement has no color target" );
            return FALSE;
        }

        for( u32 RequestIndex = 0; RequestIndex < PassData.Desc.CompositePipelineCount; ++RequestIndex )
        {
            const RenderPassDesc::CompositePipelineRequest& Request = PassData.Desc.CompositePipelines[RequestIndex];

            if( Request.pPixelShader && !( *Request.pPixelShader ) )
            {
                Fail( "composite pipeline requirement references an unavailable shader" );
                return FALSE;
            }

            shader_backend*      pPixel      = Request.pPixelShader ? Request.pPixelShader->pBackend : NULL;
            const rtarget_format DepthFormat = pDepth ? pDepth->Desc.Format : RTARGET_FORMAT_COUNT;
            xbool                Found       = FALSE;

            for( s32 j = 0; j < m_CompositeRequirements.GetCount(); ++j )
            {
                const CompositeRequirement& Existing = m_CompositeRequirements[j];

                if( Existing.pPixelShader == pPixel && Existing.Blend == Request.Blend &&
                    Existing.ColorFormat == pColor->Desc.Format && Existing.DepthFormat == DepthFormat &&
                    Existing.Samples == pColor->Desc.SampleCount )
                {
                    Found = TRUE;
                    break;
                }
            }

            if( Found )
            {
                continue;
            }

            render_pipeline_desc Desc;

            if( !composite_GetPipelineDesc( Desc, Request.Blend, pColor->Desc.Format,
                                            pDepth ? pDepth->Desc.Format : RTARGET_FORMAT_COUNT,
                                            pColor->Desc.SampleCount, Request.pPixelShader ) )
            {
                m_pFailurePass = PassData.Desc.pDebugName;
                Fail( "failed to prewarm a graph-declared composite pipeline" );
                return FALSE;
            }

            render_pipeline* pPipeline = composite_QueuePipeline( Desc );

            if( !pPipeline )
            {
                Fail( "failed to queue a graph composite pipeline" );
                return FALSE;
            }

            CompositeRequirement& Requirement = m_CompositeRequirements.Append();
            Requirement.pPixelShader          = pPixel;
            Requirement.Blend                 = Request.Blend;
            Requirement.ColorFormat           = pColor->Desc.Format;
            Requirement.DepthFormat           = DepthFormat;
            Requirement.Samples               = pColor->Desc.SampleCount;
            Requirement.pPipeline             = pPipeline;
        }
    }

    for( s32 i = 0; i < m_CompositeRequirements.GetCount(); ++i )
    {
        if( !*m_CompositeRequirements[i].pPipeline &&
            !composite_FinishPipeline( *m_CompositeRequirements[i].pPipeline ) )
        {
            Fail( "composite pipeline prewarm failed" );
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::CanMergePasses( const Pass& Previous, const Pass& Next ) const
{
    if( Previous.Desc.Type != RENDER_PASS_GRAPHICS || Next.Desc.Type != RENDER_PASS_GRAPHICS )
    {
        return FALSE;
    }

    // Both passes must use the same attachments.
    if( Previous.Desc.Color.Index != Next.Desc.Color.Index ||
        Previous.Desc.Color.Generation != Next.Desc.Color.Generation ||
        Previous.Desc.Color1.Index != Next.Desc.Color1.Index ||
        Previous.Desc.Color1.Generation != Next.Desc.Color1.Generation ||
        Previous.Desc.Color2.Index != Next.Desc.Color2.Index ||
        Previous.Desc.Color2.Generation != Next.Desc.Color2.Generation ||
        Previous.Desc.Depth.Index != Next.Desc.Depth.Index ||
        Previous.Desc.Depth.Generation != Next.Desc.Depth.Generation )
    {
        return FALSE;
    }

    // Resolves require a pass boundary.
    if( Previous.Desc.ColorResolve.IsValid() || Next.Desc.ColorResolve.IsValid() ||
        Previous.Desc.Color1Resolve.IsValid() || Next.Desc.Color1Resolve.IsValid() ||
        Previous.Desc.Color2Resolve.IsValid() || Next.Desc.Color2Resolve.IsValid() )
    {
        return FALSE;
    }

    // Attachment contents must survive both passes.
    if( ( ( Previous.Desc.Color.IsValid() || Previous.Desc.Color1.IsValid() || Previous.Desc.Color2.IsValid() ) &&
          Previous.Desc.ColorStoreOp != RTARGET_STORE_STORE ) ||
        ( Previous.Desc.Depth.IsValid() && Previous.Desc.DepthStoreOp != RTARGET_STORE_STORE ) ||
        ( ( Next.Desc.Color.IsValid() || Next.Desc.Color1.IsValid() || Next.Desc.Color2.IsValid() ) &&
          Next.Desc.ColorStoreOp != RTARGET_STORE_STORE ) ||
        ( Next.Desc.Depth.IsValid() && Next.Desc.DepthStoreOp != RTARGET_STORE_STORE ) )
    {
        return FALSE;
    }

    if( Previous.Desc.ViewIndex != Next.Desc.ViewIndex || Previous.Desc.ViewMask != Next.Desc.ViewMask )
    {
        return FALSE;
    }

    // The next pass must load the existing attachment contents.
    if( ( ( Next.Desc.Color.IsValid() || Next.Desc.Color1.IsValid() || Next.Desc.Color2.IsValid() ) &&
          Next.Desc.ColorLoadOp != RTARGET_LOAD_LOAD ) ||
        ( Next.Desc.Depth.IsValid() && Next.Desc.DepthLoadOp != RTARGET_LOAD_LOAD ) )
    {
        return FALSE;
    }

    // Sampling an active attachment requires a separate pass.
    for( s32 Read = 0; Read < Next.Reads.GetCount(); ++Read )
    {
        if( ( Next.Desc.Color.IsValid() && Next.Reads[Read] == Next.Desc.Color.Index ) ||
            ( Next.Desc.Color1.IsValid() && Next.Reads[Read] == Next.Desc.Color1.Index ) ||
            ( Next.Desc.Color2.IsValid() && Next.Reads[Read] == Next.Desc.Color2.Index ) ||
            ( Next.Desc.Depth.IsValid() && Next.Reads[Read] == Next.Desc.Depth.Index ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

rtarget_store_op FrameGraphBuilder::GetAttachmentStoreOp( RenderResourceHandle Handle,
                                                          rtarget_store_op     StoreOp,
                                                          u32                  LastPassIndex ) const
{
    if( !IsHandleValid( Handle ) )
    {
        return StoreOp;
    }

    const Resource& ResourceData = *m_GraphResources[Handle.Index];

    if( ResourceData.Desc.Lifetime != RENDER_RESOURCE_TRANSIENT )
    {
        return StoreOp;
    }

    for( u32 i = LastPassIndex + 1u; i < static_cast<u32>( m_ExecutionOrder.GetCount() ); ++i )
    {
        const Pass& PassData = *m_Passes[m_ExecutionOrder[i]];

        if( PassDeclaresRead( m_ExecutionOrder[i], Handle ) )
        {
            return StoreOp;
        }

        if( PassDeclaresWrite( m_ExecutionOrder[i], Handle ) )
        {
            const xbool Depth = PassData.Desc.Depth.IsValid() && PassData.Desc.Depth.Index == Handle.Index;
            const xbool Color = ( PassData.Desc.Color.IsValid() && PassData.Desc.Color.Index == Handle.Index ) ||
                                ( PassData.Desc.Color1.IsValid() && PassData.Desc.Color1.Index == Handle.Index ) ||
                                ( PassData.Desc.Color2.IsValid() && PassData.Desc.Color2.Index == Handle.Index );

            if( ( Depth && PassData.Desc.DepthLoadOp == RTARGET_LOAD_LOAD ) ||
                ( Color && PassData.Desc.ColorLoadOp == RTARGET_LOAD_LOAD ) )
            {
                return StoreOp;
            }

            break;
        }
    }

    return StoreOp == RTARGET_STORE_RESOLVE_AND_STORE ? RTARGET_STORE_RESOLVE
           : StoreOp == RTARGET_STORE_STORE           ? RTARGET_STORE_DONT_CARE
                                                      : StoreOp;
}

//=========================================================================

xbool FrameGraphBuilder::BeginPass( const Pass& PassToBegin, u32 LastPassIndex )
{
    rtarget_SetGraphPassOwner( NULL );
    rtarget_EndPass();

    if( PassToBegin.Desc.Type == RENDER_PASS_COMPUTE )
    {
        shader_compute_texture_binding Textures[8];
        shader_compute_buffer_binding Buffers[8];

        for( u32 i = 0; i < PassToBegin.Desc.ComputeTextureCount; ++i )
        {
            const RenderComputeBinding& Binding = PassToBegin.Desc.ComputeTextures[i];
            Textures[i].pTexture = GetShaderResource( Binding.Resource );
            Textures[i].MipLevel = Binding.MipLevel;
            Textures[i].Layer    = Binding.Layer;
            Textures[i].bCycle   = Binding.Cycle;
        }

        for( u32 i = 0; i < PassToBegin.Desc.ComputeBufferCount; ++i )
        {
            const RenderComputeBinding& Binding = PassToBegin.Desc.ComputeBuffers[i];
            Buffers[i].pBuffer = GetBuffer( Binding.Resource );
            Buffers[i].bCycle  = Binding.Cycle;
        }

        shader_compute_pass_desc Desc;
        Desc.pTextures    = Textures;
        Desc.TextureCount = PassToBegin.Desc.ComputeTextureCount;
        Desc.pBuffers     = Buffers;
        Desc.BufferCount  = PassToBegin.Desc.ComputeBufferCount;
        return shader_BeginComputePass( *PassToBegin.Desc.pComputeShader, Desc );
    }

    if( PassToBegin.Desc.Type == RENDER_PASS_COPY || PassToBegin.Desc.Type == RENDER_PASS_TRANSFER )
    {
        return TRUE;
    }

    const rtarget* pColor           = PassToBegin.Desc.Color.IsValid() ? m_pFrame->Get( PassToBegin.Desc.Color ) : NULL;
    const rtarget* pColor1          = PassToBegin.Desc.Color1.IsValid() ? m_pFrame->Get( PassToBegin.Desc.Color1 ) : NULL;
    const rtarget* pColor2          = PassToBegin.Desc.Color2.IsValid() ? m_pFrame->Get( PassToBegin.Desc.Color2 ) : NULL;
    const rtarget* pColorResolve    = PassToBegin.Desc.ColorResolve.IsValid() ? m_pFrame->Get( PassToBegin.Desc.ColorResolve ) : NULL;
    const rtarget* pColor1Resolve   = PassToBegin.Desc.Color1Resolve.IsValid() ? m_pFrame->Get( PassToBegin.Desc.Color1Resolve ) : NULL;
    const rtarget* pColor2Resolve   = PassToBegin.Desc.Color2Resolve.IsValid() ? m_pFrame->Get( PassToBegin.Desc.Color2Resolve ) : NULL;
    const rtarget* pDepth           = PassToBegin.Desc.Depth.IsValid() ? m_pFrame->Get( PassToBegin.Desc.Depth ) : NULL;

    if( !pColor && !pColor1 && !pColor2 && !pDepth )
    {
        Fail( "render pass has no valid attachment resource" );
        return FALSE;
    }

    if( ( pColor && !rtarget_HasRenderTarget( *pColor ) ) || ( pColor1 && !rtarget_HasRenderTarget( *pColor1 ) ) ||
        ( pColor2 && !rtarget_HasRenderTarget( *pColor2 ) ) ||
        ( pColorResolve && !rtarget_HasRenderTarget( *pColorResolve ) ) ||
        ( pColor1Resolve && !rtarget_HasRenderTarget( *pColor1Resolve ) ) ||
        ( pColor2Resolve && !rtarget_HasRenderTarget( *pColor2Resolve ) ) ||
        ( pDepth && !rtarget_HasDepthStencil( *pDepth ) ) ||
        ( PassToBegin.Desc.ViewMask != m_pFrame->Desc.Views.ViewMask ) )
    {
        Fail( "render pass attachment has an incompatible resource or view mask" );
        return FALSE;
    }

    const rtarget* pPrimaryTarget = pColor ? pColor : ( pColor1 ? pColor1 : pColor2 );

    if( pPrimaryTarget && pDepth &&
        ( ( pPrimaryTarget->Desc.Width != pDepth->Desc.Width ) ||
          ( pPrimaryTarget->Desc.Height != pDepth->Desc.Height ) ||
          ( pPrimaryTarget->Desc.SampleCount != pDepth->Desc.SampleCount ) ) )
    {
        Fail( "render pass attachments have mismatched extent or sample count" );
        return FALSE;
    }

    rtarget_color_attachment_desc Colors[RTARGET_MAX_TARGETS] = {};
    u32                           ColorCount                  = 0;

    if( pColor )
    {
        Colors[ColorCount].pTarget        = pColor;
        Colors[ColorCount].pResolveTarget = pColorResolve;
        Colors[ColorCount].LoadOp         = PassToBegin.Desc.ColorLoadOp;
        Colors[ColorCount].StoreOp        = PassToBegin.ColorStore[0];

        const RenderResourceDesc* pColorDesc = GetDesc( PassToBegin.Desc.Color );
        const RenderResourceDesc* pResolveDesc =
            PassToBegin.Desc.ColorResolve.IsValid() ? GetDesc( PassToBegin.Desc.ColorResolve ) : NULL;
        Colors[ColorCount].bCycle = pColorDesc && ( pColorDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) &&
                                    !pColorDesc->PreserveContents &&
                                    ( PassToBegin.Desc.ColorLoadOp != RTARGET_LOAD_LOAD );
        Colors[ColorCount].bCycleResolve =
            pResolveDesc && ( pResolveDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) && !pResolveDesc->PreserveContents;
        CopyClearColor( Colors[ColorCount].ClearColor, pColorDesc->ClearColor );
        ++ColorCount;
    }

    if( pColor1 )
    {
        Colors[ColorCount].pTarget        = pColor1;
        Colors[ColorCount].pResolveTarget = pColor1Resolve;
        Colors[ColorCount].LoadOp         = PassToBegin.Desc.ColorLoadOp;
        Colors[ColorCount].StoreOp        = PassToBegin.ColorStore[1];

        const RenderResourceDesc* pColorDesc = GetDesc( PassToBegin.Desc.Color1 );
        const RenderResourceDesc* pResolveDesc =
            PassToBegin.Desc.Color1Resolve.IsValid() ? GetDesc( PassToBegin.Desc.Color1Resolve ) : NULL;
        Colors[ColorCount].bCycle = pColorDesc && ( pColorDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) &&
                                    !pColorDesc->PreserveContents &&
                                    ( PassToBegin.Desc.ColorLoadOp != RTARGET_LOAD_LOAD );
        Colors[ColorCount].bCycleResolve =
            pResolveDesc && ( pResolveDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) && !pResolveDesc->PreserveContents;
        CopyClearColor( Colors[ColorCount].ClearColor, pColorDesc->ClearColor );
        ++ColorCount;
    }

    if( pColor2 )
    {
        Colors[ColorCount].pTarget        = pColor2;
        Colors[ColorCount].pResolveTarget = pColor2Resolve;
        Colors[ColorCount].LoadOp         = PassToBegin.Desc.ColorLoadOp;
        Colors[ColorCount].StoreOp        = PassToBegin.ColorStore[2];

        const RenderResourceDesc* pColorDesc = GetDesc( PassToBegin.Desc.Color2 );
        const RenderResourceDesc* pResolveDesc =
            PassToBegin.Desc.Color2Resolve.IsValid() ? GetDesc( PassToBegin.Desc.Color2Resolve ) : NULL;
        Colors[ColorCount].bCycle = pColorDesc && ( pColorDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) &&
                                    !pColorDesc->PreserveContents &&
                                    ( PassToBegin.Desc.ColorLoadOp != RTARGET_LOAD_LOAD );
        Colors[ColorCount].bCycleResolve =
            pResolveDesc && ( pResolveDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) && !pResolveDesc->PreserveContents;
        CopyClearColor( Colors[ColorCount].ClearColor, pColorDesc->ClearColor );
        ++ColorCount;
    }

    rtarget_depth_attachment_desc DepthData = {};

    if( pDepth )
    {
        DepthData.pTarget        = pDepth;
        DepthData.DepthLoadOp    = PassToBegin.Desc.DepthLoadOp;
        DepthData.DepthStoreOp   = PassToBegin.DepthStore;
        DepthData.StencilLoadOp  = PassToBegin.Desc.DepthLoadOp;
        DepthData.StencilStoreOp = DepthData.DepthStoreOp;

        const RenderResourceDesc* pDepthDesc = GetDesc( PassToBegin.Desc.Depth );
        DepthData.bCycle                     = pDepthDesc && ( pDepthDesc->Lifetime == RENDER_RESOURCE_PERSISTENT ) &&
                           !pDepthDesc->PreserveContents && ( PassToBegin.Desc.DepthLoadOp != RTARGET_LOAD_LOAD );
        DepthData.ClearDepth   = pDepthDesc->ClearDepth;
        DepthData.ClearStencil = pDepthDesc->ClearStencil;
    }

    if( !rtarget_BeginPass( ColorCount ? Colors : NULL, ColorCount, pDepth ? &DepthData : NULL ) )
    {
        Fail( "backend rejected render pass attachments" );
        return FALSE;
    }

    const RenderViewFamily& ViewsData = m_pFrame->Desc.Views;

    if( ViewsData.pViews && ViewsData.ViewCount )
    {
        rdraw_viewport Viewport;
        const rtarget* pViewportTarget = pPrimaryTarget ? pPrimaryTarget : pDepth;

        if( !pViewportTarget || !m_pFrame->GetViewport( Viewport, *pViewportTarget, PassToBegin.Desc.ViewIndex ) ||
            !rdraw_SetViewport( Viewport ) )
        {
            rtarget_EndPass();
            Fail( "failed to set render pass viewport" );
            return FALSE;
        }

        rdraw_scissor Scissor;
        Scissor.X      = static_cast<s32>( Viewport.TopLeftX );
        Scissor.Y      = static_cast<s32>( Viewport.TopLeftY );
        Scissor.Width  = static_cast<s32>( Viewport.Width );
        Scissor.Height = static_cast<s32>( Viewport.Height );

        if( !rdraw_SetScissor( Scissor ) )
        {
            rtarget_EndPass();
            Fail( "failed to set render pass scissor" );
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool FrameGraphBuilder::Execute( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Renderer", "FrameGraph/Execute" );

    if( !m_isCompiled )
    {
        RENDER_THROW( "FrameGraph", "Execute requires a compiled graph" );
        return FALSE;
    }

    if( m_isExecuted )
    {
        RENDER_THROW( "FrameGraph", "Execute may only be called once per frame" );
        return FALSE;
    }

    const Pass* pPrevious = NULL;

    for( s32 i = 0; i < m_ExecutionOrder.GetCount(); ++i )
    {
        Pass& PassData     = *m_Passes[m_ExecutionOrder[i]];
        m_pFailurePass     = PassData.Desc.pDebugName;
        m_pFailureResource = NULL;

        const xbool MergeGraphics = pPrevious && pPrevious->GroupEnd >= static_cast<u32>( i );
        const u32   LastPassIndex = PassData.GroupEnd;

        if( !MergeGraphics && !BeginPass( PassData, LastPassIndex ) )
        {
            RENDER_THROW( "FrameGraph", "failed to begin pass '%s'", PassData.Desc.pDebugName );
            Fail( "failed to begin render pass" );
            return FALSE;
        }

        RenderPassContext Context( *this, *m_pFrame, PassData.Desc, m_ExecutionOrder[i] );
        render_BeginFailureScope( PassData.Desc.bOptional, PassData.Desc.pDebugName );
        rtarget_SetGraphPassOwner( PassData.Desc.pDebugName );

        BeginResourceValidation( PassData );
        xbool Succeeded = !m_pLastError && PassData.pPayload && PassData.pPayload->Execute( Context );
        shader_SetResourceAccessValidator( NULL, NULL );
        Succeeded = Succeeded && !m_pLastError;
        if( PassData.Desc.Type == RENDER_PASS_COMPUTE )
        {
            const xbool Ended = shader_EndComputePass();
            Succeeded = Succeeded && Context.m_isDispatched && Ended;
        }

        if( !Succeeded )
        {
            rtarget_SetGraphPassOwner( NULL );
            rtarget_EndPass();
            render_EndFailureScope();

            if( PassData.Desc.bOptional )
            {
                if( PassData.Writes.GetCount() != 0 )
                {
                    RENDER_THROW( "FrameGraph", "optional pass '%s' failed after declaring graph writes",
                                  PassData.Desc.pDebugName );
                    Fail( "optional pass produced an invalid partial resource" );
                    return FALSE;
                }
                RENDER_WARNING( "FrameGraph", "optional pass '%s' was skipped after a recoverable feature failure",
                                PassData.Desc.pDebugName );
                render_ClearRecoverableFailure();
                pPrevious = NULL;
                continue;
            }
            RENDER_THROW( "FrameGraph", "execution failed in pass '%s'", PassData.Desc.pDebugName );
            Fail( "render pass execution failed" );

            return FALSE;
        }

        rtarget_SetGraphPassOwner( NULL );
        render_EndFailureScope();
        render_ClearRecoverableFailure();
        pPrevious = &PassData;
    }

    rtarget_EndPass();
    m_isExecuted = TRUE;

    return TRUE;
}

//=========================================================================
} // namespace render
