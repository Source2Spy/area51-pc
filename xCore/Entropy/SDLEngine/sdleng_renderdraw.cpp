//==============================================================================
//
//  sdleng_renderdraw.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

//==============================================================================
//  GLOBAL DYNAMIC STATE
//==============================================================================

sdleng_dynamic_state g_SDLDynamicState = {};

//==============================================================================
//  DYNAMIC STATE RESET
//==============================================================================

void sdleng_ResetDynamicState( void )
{
    g_SDLDynamicState.ViewportValid = FALSE;
    g_SDLDynamicState.ScissorValid  = FALSE;
    g_SDLDynamicState.BlendValid    = FALSE;
    g_SDLDynamicState.StencilValid  = FALSE;
}

//==============================================================================
//  VIEWPORT AND SCISSOR
//==============================================================================

xbool rdraw_SetViewport( const rdraw_viewport& Viewport )
{
    if( !x_isvalid( Viewport.TopLeftX ) || !x_isvalid( Viewport.TopLeftY ) ||
        !x_isvalid( Viewport.Width ) || !x_isvalid( Viewport.Height ) ||
        !x_isvalid( Viewport.MinDepth ) || !x_isvalid( Viewport.MaxDepth ) ||
        ( Viewport.Width <= 0.0f ) || ( Viewport.Height <= 0.0f ) )
    {
        return FALSE;
    }

    if( ( Viewport.MinDepth < 0.0f ) || ( Viewport.MaxDepth > 1.0f ) ||
        ( Viewport.MinDepth > Viewport.MaxDepth ) )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
    {
        return FALSE;
    }

    SDL_GPUViewport SDLViewport;
    SDLViewport.x         = Viewport.TopLeftX;
    SDLViewport.y         = Viewport.TopLeftY;
    SDLViewport.w         = Viewport.Width;
    SDLViewport.h         = Viewport.Height;
    SDLViewport.min_depth = Viewport.MinDepth;
    SDLViewport.max_depth = Viewport.MaxDepth;

    static xprofile_counter DynamicStateRequests =
        x_GetProfiler().RegisterCounter( "DynamicStateRequests", "Renderer" );

    DynamicStateRequests.Add();

    if( g_SDLDynamicState.ViewportValid &&
        x_memcmp( &g_SDLDynamicState.Viewport, &SDLViewport, sizeof( SDLViewport ) ) == 0 )
    {
        return TRUE;
    }

    static xprofile_counter DynamicStateCountMetric =
        x_GetProfiler().RegisterCounter( "DynamicStateCalls", "Renderer" );

    DynamicStateCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_SetGPUViewport( pRenderPass, &SDLViewport );
    g_SDLDynamicState.Viewport      = SDLViewport;
    g_SDLDynamicState.ViewportValid = TRUE;
    const xtick End                 = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone DynamicStateMetric =
            x_GetProfiler().RegisterZone( "DynamicStateAPI", "RendererAPI" );

        DynamicStateMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool rdraw_SetScissor( const rdraw_scissor& Scissor )
{
    if( ( Scissor.X < 0 ) || ( Scissor.Y < 0 ) ||
        ( Scissor.Width <= 0 ) || ( Scissor.Height <= 0 ) ||
        ( Scissor.Width > ( S32_MAX - Scissor.X ) ) ||
        ( Scissor.Height > ( S32_MAX - Scissor.Y ) ) )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        return FALSE;
    }

    SDL_Rect SDLScissor;
    SDLScissor.x = Scissor.X;
    SDLScissor.y = Scissor.Y;
    SDLScissor.w = Scissor.Width;
    SDLScissor.h = Scissor.Height;

    static xprofile_counter DynamicStateRequests =
        x_GetProfiler().RegisterCounter( "DynamicStateRequests", "Renderer" );

    DynamicStateRequests.Add();

    if( g_SDLDynamicState.ScissorValid &&
        x_memcmp( &g_SDLDynamicState.Scissor, &SDLScissor, sizeof( SDLScissor ) ) == 0 )
    {
        return TRUE;
    }

    static xprofile_counter DynamicStateCountMetric =
        x_GetProfiler().RegisterCounter( "DynamicStateCalls", "Renderer" );

    DynamicStateCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_SetGPUScissor( pRenderPass, &SDLScissor );
    g_SDLDynamicState.Scissor      = SDLScissor;
    g_SDLDynamicState.ScissorValid = TRUE;
    const xtick End                = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone DynamicStateMetric =
            x_GetProfiler().RegisterZone( "DynamicStateAPI", "RendererAPI" );

        DynamicStateMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================
//  DRAW COMMANDS
//==============================================================================

xbool rdraw_Draw( s32 VertexCount, s32 StartVertex )
{
    return rdraw_DrawInstanced( VertexCount, 1, StartVertex, 0 );
}

//==============================================================================

xbool rdraw_DrawInstanced( s32 VertexCount, s32 InstanceCount, s32 StartVertex, s32 StartInstance )
{
    if( ( VertexCount <= 0 ) || ( InstanceCount <= 0 ) || ( StartVertex < 0 ) || ( StartInstance < 0 ) )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
    {
        return FALSE;
    }

    if( !sdleng_ValidateGraphicsBindings() )
    {
        return FALSE;
    }

    static xprofile_counter DrawCountMetric =
        x_GetProfiler().RegisterCounter( "DrawCalls", "Renderer" );

    DrawCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_DrawGPUPrimitives( pRenderPass,
                           (Uint32)VertexCount,
                           (Uint32)InstanceCount,
                           (Uint32)StartVertex,
                           (Uint32)StartInstance );

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone DrawMetric =
            x_GetProfiler().RegisterZone( "DrawAPI", "RendererAPI" );

        DrawMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool rdraw_DrawIndexed( s32 IndexCount, s32 StartIndex, s32 BaseVertex )
{
    return rdraw_DrawIndexedInstanced( IndexCount, 1, StartIndex, BaseVertex, 0 );
}

//==============================================================================

xbool rdraw_DrawIndexedInstanced( s32 IndexCount,
                                  s32 InstanceCount,
                                  s32 StartIndex,
                                  s32 BaseVertex,
                                  s32 StartInstance )
{
    if( ( IndexCount <= 0 ) || ( InstanceCount <= 0 ) || ( StartIndex < 0 ) || ( StartInstance < 0 ) )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass )
    {
        return FALSE;
    }

    if( !sdleng_ValidateGraphicsBindings() )
    {
        return FALSE;
    }

    static xprofile_counter DrawCountMetric =
        x_GetProfiler().RegisterCounter( "DrawCalls", "Renderer" );

    DrawCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_DrawGPUIndexedPrimitives( pRenderPass,
                                  (Uint32)IndexCount,
                                  (Uint32)InstanceCount,
                                  (Uint32)StartIndex,
                                  (Sint32)BaseVertex,
                                  (Uint32)StartInstance );

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone DrawMetric =
            x_GetProfiler().RegisterZone( "DrawAPI", "RendererAPI" );

        DrawMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================

xbool rdraw_DrawIndexedIndirect( const rbuffer& Buffer, u32 Offset, u32 DrawCount )
{
    if( !Buffer.pBackend ||
        !Buffer.pBackend->pBuffer ||
        !( Buffer.Desc.UsageFlags & RBUFFER_USAGE_INDIRECT ) ||
        ( DrawCount == 0 ) ||
        ( Offset > Buffer.Desc.Size ) )
    {
        return FALSE;
    }

    const u64 RequiredBytes = (u64)DrawCount * sizeof( rdraw_indexed_indirect_command );

    if( RequiredBytes > ( (u64)Buffer.Desc.Size - Offset ) )
    {
        return FALSE;
    }

    if( !shader_ValidateResourceAccess( *rbuffer_GetResource( Buffer ), SHADER_RESOURCE_ACCESS_INDIRECT ) )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();

    if( !pRenderPass || !sdleng_ValidateGraphicsBindings() )
    {
        return FALSE;
    }

    static xprofile_counter DrawCountMetric =
        x_GetProfiler().RegisterCounter( "DrawCalls", "Renderer" );

    static xprofile_counter IndirectCommandMetric =
        x_GetProfiler().RegisterCounter( "IndirectDrawCommands", "Renderer" );

    DrawCountMetric.Add();
    IndirectCommandMetric.Add( DrawCount );

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start       = bFineTiming ? x_GetTime() : 0;
    SDL_DrawGPUIndexedPrimitivesIndirect( pRenderPass,
                                          Buffer.pBackend->pBuffer,
                                          Offset,
                                          DrawCount );

    const xtick End = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone DrawMetric =
            x_GetProfiler().RegisterZone( "DrawIndirectAPI", "RendererAPI" );

        DrawMetric.Record( End - Start );
    }

    return TRUE;
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
