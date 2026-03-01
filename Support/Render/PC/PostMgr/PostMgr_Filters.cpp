//==============================================================================
//
//  PostMgr_Filters.cpp
//
//  Post-processing place for effect stubs and simple effects for PC platform
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PostMgr.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  CONSTANTS
//==============================================================================

//==============================================================================
//  CONSTANT BUFFER LAYOUT
//==============================================================================

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

//==============================================================================
//  EFFECT IMPLEMENTATIONS
//==============================================================================

void post_mgr::ExecuteMotionBlur( void )
{
    // DEPRECATED
}

//==============================================================================

void post_mgr::ExecuteMultScreen( void )
{
    // DEPRECATED
}

//==============================================================================

void post_mgr::ExecuteRadialBlur( void )
{
    // DEPRECATED
}

//==============================================================================

void post_mgr::ExecuteZFogFilter( void )
{
    PrepareFullscreenQuad();
    //TODO:	
}

//==============================================================================

void post_mgr::ExecuteMipFilter( void )
{
    PrepareFullscreenQuad();
    const s32 palette = m_MipFilter.PaletteIndex;
    const s32 count = (palette >= 0) ? m_MipFilter.Count[palette] : 0;
	//TODO:
}

//==============================================================================

void post_mgr::ExecuteNoiseFilter( void )
{
    // DEPRECATED
}

//==============================================================================

void post_mgr::ExecuteScreenFade( void )
{
    if( !g_pd3dContext )
        return;

    PrepareFullscreenQuad();

    irect Rect;
    Rect.l = m_PostViewL;
    Rect.t = m_PostViewT;
    Rect.r = m_PostViewR;
    Rect.b = m_PostViewB;

    state_SetBlend( STATE_BLEND_ALPHA );
    draw_Rect( Rect, m_Simple.FadeColor, FALSE, DRAW_UI_RTARGET );
}

//==============================================================================
//  SUPPORT FUNCTIONS
//==============================================================================

void post_mgr::BuildFogPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    m_FogFilter.PaletteIndex = 2;

    if( (m_FogFilter.Fn[m_FogFilter.PaletteIndex] == Fn) &&
        (m_FogFilter.Param1[m_FogFilter.PaletteIndex] == Param1) &&
        (m_FogFilter.Param2[m_FogFilter.PaletteIndex] == Param2) &&
        (m_FogFilter.Color[m_FogFilter.PaletteIndex] == Color) )
    {
        return;
    }

    m_FogFilter.Fn[m_FogFilter.PaletteIndex] = Fn;
    m_FogFilter.Param1[m_FogFilter.PaletteIndex] = Param1;
    m_FogFilter.Param2[m_FogFilter.PaletteIndex] = Param2;
    m_FogFilter.Color[m_FogFilter.PaletteIndex] = Color;

    m_bFogValid[m_FogFilter.PaletteIndex] = TRUE;
}

//==============================================================================

void post_mgr::BuildMipPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex )
{
    if( (m_MipFilter.Fn[PaletteIndex] == Fn) &&
        (m_MipFilter.Param1[PaletteIndex] == Param1) &&
        (m_MipFilter.Param2[PaletteIndex] == Param2) &&
        (m_MipFilter.Color[PaletteIndex] == Color) )
    {
        return;
    }

    m_MipFilter.Fn[PaletteIndex] = Fn;
    m_MipFilter.Param1[PaletteIndex] = Param1;
    m_MipFilter.Param2[PaletteIndex] = Param2;
    m_MipFilter.Color[PaletteIndex] = Color;
    m_MipFilter.PaletteIndex = PaletteIndex;
}

//==============================================================================

void post_mgr::CopyBackBuffer( void )
{
    // DEPRECATED
}

//==============================================================================

void post_mgr::BuildScreenMips( s32 nMips, xbool UseAlpha )
{
    // DEPRECATED
}