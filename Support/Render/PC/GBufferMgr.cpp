//==============================================================================
//  
//  GBufferMgr.cpp
//  
//  Mini G-Buffer Manager for Forward+ Renderer
//
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GBufferMgr.hpp"
#include "../../GameLib/RenderContext.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

gbuffer_mgr g_GBufferMgr;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

gbuffer_mgr::gbuffer_mgr()
{
    ResetState();
    m_bInitialized = FALSE;
}

//==============================================================================

gbuffer_mgr::~gbuffer_mgr()
{
}

//==============================================================================

void gbuffer_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    ResetState();
    m_bInitialized = TRUE;
}

//==============================================================================

void gbuffer_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    DestroyGBuffer();
    m_bInitialized = FALSE;
}

//==============================================================================

xbool gbuffer_mgr::InitGBuffer( u32 Width, u32 Height )
{
    if( !g_pd3dDevice )
        return FALSE;

    m_GBufferWidth = Width;
    m_GBufferHeight = Height;
    
    rtarget_registration reg;
    reg.Policy = RTARGET_SIZE_RELATIVE_TO_VIEW;
    reg.ScaleX = 1.0f;
    reg.ScaleY = 1.0f;
    reg.SampleCount = 1;
    reg.SampleQuality = 0;
    reg.bBindAsTexture = TRUE;

    // Create G-Buffer targets
    reg.Format = GBUFFER_FORMAT_ALBEDO;
    if( !rtarget_GetOrCreate( m_GBufferTarget[0], reg ) )
    {
        x_throw( "Failed to create GBuffer Albedo target" );
        return FALSE;
    }

    reg.Format = GBUFFER_FORMAT_NORMAL;
    if( !rtarget_GetOrCreate( m_GBufferTarget[1], reg ) )
    {
        DestroyGBuffer();
        x_throw( "Failed to create GBuffer Normal target" );
        return FALSE;
    }

    reg.Format = GBUFFER_FORMAT_DEPTH_INFO;
    if( !rtarget_GetOrCreate( m_GBufferTarget[2], reg ) )
    {
        DestroyGBuffer();
        x_throw( "Failed to create GBuffer Depth Info target" );
        return FALSE;
    }

    reg.Format = GBUFFER_FORMAT_GLOW;
    if( !rtarget_GetOrCreate( m_GBufferTarget[3], reg ) )
    {
        DestroyGBuffer();
        x_throw( "Failed to create GBuffer Glow target" );
        return FALSE;
    }

    reg.Format = RTARGET_FORMAT_DEPTH24_STENCIL8;
    if( !rtarget_GetOrCreate( m_GBufferDepth, reg ) )
    {
        DestroyGBuffer();
        x_throw( "Failed to create GBuffer depth-stencil" );
        return FALSE;
    }
    
    m_bGBufferValid = TRUE;
    return TRUE;
}

//==============================================================================

void gbuffer_mgr::DestroyGBuffer( void )
{
    if( m_bGBufferTargetsActive )
    {
        rtarget_SetBackBuffer();
        m_bGBufferTargetsActive = FALSE;
    }

    for( u32 i = 0; i < (GBUFFER_TARGET_COUNT - 2); i++ )
    {
        rtarget_Unregister( m_GBufferTarget[i] );
        rtarget_Destroy( m_GBufferTarget[i] );
    }

    rtarget_Unregister( m_GBufferDepth );
    rtarget_Destroy( m_GBufferDepth );

    ResetState();
}

//==============================================================================

xbool gbuffer_mgr::ResizeGBuffer( u32 Width, u32 Height )
{
    return InitGBuffer( Width, Height );
}

//==============================================================================

xbool gbuffer_mgr::SetGBufferTargets( void )
{
    if( !m_bGBufferValid )
    {
        x_throw( "SetGBufferTargets called before a valid G-Buffer was created" );
        return FALSE;
    }

    if( !g_pd3dContext )
        return FALSE;

    if( m_bGBufferTargetsActive )
        return m_bGBufferTargetsActive;

    const rtarget* pBackBuffer = rtarget_GetBackBuffer();
    if( !pBackBuffer )
    {
        x_throw( "Back buffer is not available" );
        return FALSE;
    }

    const rtarget* pFinalColor = pBackBuffer;
    const rtarget* pDepthTarget = &m_GBufferDepth;

    if( g_RenderContext.m_bIsPipRender && g_RenderContext.ArePipTargetsActive() )
    {
        pip_render_target_pc* pPipTarget = g_RenderContext.GetActivePipTarget();
        if( pPipTarget && pPipTarget->bValid )
        {
            pFinalColor = &pPipTarget->ColorTarget;
            if( pPipTarget->DepthTarget.bIsDepthTarget && pPipTarget->DepthTarget.pDepthStencilView )
                pDepthTarget = &pPipTarget->DepthTarget;
        }
    }

    m_GBufferTargetSet[GBUFFER_FINAL_COLOR] = *pFinalColor;
    m_GBufferTargetSet[GBUFFER_ALBEDO]      = m_GBufferTarget[0];
    m_GBufferTargetSet[GBUFFER_NORMAL]      = m_GBufferTarget[1];
    m_GBufferTargetSet[GBUFFER_DEPTH_INFO]  = m_GBufferTarget[2];
    m_GBufferTargetSet[GBUFFER_GLOW]        = m_GBufferTarget[3];
    m_GBufferTargetSet[GBUFFER_DEPTH]       = rtarget();

    if( rtarget_SetTargets( m_GBufferTargetSet, GBUFFER_TARGET_COUNT - 1, pDepthTarget ) )
    {
        m_bGBufferTargetsActive = TRUE;
        return TRUE;
    }

    return FALSE;
}

//==============================================================================

void gbuffer_mgr::SetBackBufferTarget( void )
{
    if( m_bGBufferTargetsActive )
    {
        rtarget_SetBackBuffer();
        m_bGBufferTargetsActive = FALSE;
    }
}

//==============================================================================

void gbuffer_mgr::ClearGBuffer( void )
{
    if( !m_bGBufferValid || !m_bGBufferTargetsActive )
        return;

    if( !g_pd3dContext )
        return;

    // Albedo
    static const f32 clearAlbedo[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };
    rtarget_ClearColor( m_GBufferTarget[0], clearAlbedo );

    // Normals
    static const f32 clearNormal[4]    = { 0.5f, 0.5f, 1.0f, 0.0f };
    rtarget_ClearColor( m_GBufferTarget[1], clearNormal );

    // Depth info
    static const f32 clearDepthInfo[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    rtarget_ClearColor( m_GBufferTarget[2], clearDepthInfo );

    // Glow
    static const f32 clearGlow[4]      = { 0.0f, 0.0f, 0.0f, 0.0f };
    rtarget_ClearColor( m_GBufferTarget[3], clearGlow );

    // Depth-stencil
    rtarget_ClearDepthStencil( m_GBufferDepth, RTARGET_CLEAR_DEPTH | RTARGET_CLEAR_STENCIL, 1.0f, 0 );
}

//==============================================================================

void gbuffer_mgr::ResetState( void )
{
    m_bGBufferValid = FALSE;
    m_GBufferWidth = 0;
    m_GBufferHeight = 0;
    m_bGBufferTargetsActive = FALSE;
}

//==============================================================================

const rtarget* gbuffer_mgr::GetGBufferTarget( gbuffer_target Target ) const
{
    if( !m_bGBufferValid )
        return NULL;
        
    switch( Target )
    {
        case GBUFFER_FINAL_COLOR:
        {
            if( g_RenderContext.m_bIsPipRender && g_RenderContext.ArePipTargetsActive() )
            {
                pip_render_target_pc* pTarget = g_RenderContext.GetActivePipTarget();
                if( pTarget && pTarget->bValid )
                    return &pTarget->ColorTarget;
            }

            return rtarget_GetBackBuffer();
        }
            
        case GBUFFER_DEPTH:
            return &m_GBufferDepth;
            
        case GBUFFER_ALBEDO:
        case GBUFFER_NORMAL:
        case GBUFFER_DEPTH_INFO:
        case GBUFFER_GLOW:
            return &m_GBufferTarget[Target - 1];
            
        default:
            return NULL;
    }
}

//==============================================================================

void gbuffer_mgr::GetGBufferSize( u32& Width, u32& Height ) const
{
    Width = m_GBufferWidth;
    Height = m_GBufferHeight;
}
