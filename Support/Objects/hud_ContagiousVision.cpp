//==============================================================================
//
//  hud_MutantVision.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================
 
//==============================================================================
// INCLUDES
//==============================================================================

#include "hud_ContagiousVision.hpp"
#include "GameLib/RenderContext.hpp"

//==============================================================================
// STATICS
//==============================================================================

static const f32 s_MutantOverlayHeight = 425.0f;
static f32 const s_OverlayAuthoringXFOV   = R_60;
static f32 const s_OverlayReferenceAspect = 4.0f / 3.0f;

fx_handle       hud_contagious_vision::m_OverlayHandle;
 
//==============================================================================
// FUNCTIONS
//==============================================================================
 
hud_contagious_vision::hud_contagious_vision( void )
{
    if (m_OverlayHandle.Validate())
        m_OverlayHandle.KillInstance();

    SMP_UTIL_InitFXFromString( "HUD_ContagionBorder.fxo", m_OverlayHandle );

    // initialize the effect
    if ( m_OverlayHandle.Validate() )
    {
        m_OverlayHandle.SetRotation     ( radian3( R_0, R_0, R_0 ) );
        m_OverlayHandle.SetTranslation  ( vector3( 0.0f, 0.0f, 0.0f ) );
        m_OverlayHandle.SetScale        ( vector3( 1.0f, 1.0f, 1.0f ) );
        m_OverlayHandle.SetColor        ( XCOLOR_WHITE );
    }
}

//==============================================================================

hud_contagious_vision::~hud_contagious_vision( void )
{
    if (m_OverlayHandle.Validate())
        m_OverlayHandle.KillInstance();
}

//==============================================================================

void hud_contagious_vision::OnRender( player* pPlayer )
{
    // If the fx isn't loaded for some horrible reason, bail out.
    if (!m_OverlayHandle.Validate())
        return;

    // Is the player contagious?
    if (!pPlayer->IsContagious())
        return;
   
    // back up the current view so we can set it back
    view OrigView = *eng_GetView();

    s32 L, T, R, B;
    OrigView.GetViewport( L, T, R, B );

    s32 const ViewportWidth  = R - L + 1;
    s32 const ViewportHeight = B - T + 1;
    if( (ViewportWidth <= 0) || (ViewportHeight <= 0) )
    {
        return;
    }

    f32 const ViewportAspect = static_cast<f32>( ViewportWidth ) / static_cast<f32>( ViewportHeight );
    radian const OverlayXFOV = OrigView.GetXFOV();
    f32 const OverlayScale   = x_tan( OverlayXFOV * 0.5f ) / x_tan( s_OverlayAuthoringXFOV * 0.5f );

    // Keep the authored effect at a constant screen size as the player FOV changes.
    m_OverlayHandle.SetScale( vector3( OverlayScale, OverlayScale, OverlayScale ) );
    
    // set up a camera that looks directly at our particle effect
    view OverlayView;
    OverlayView.SetPosition( vector3( 0.0f, s_MutantOverlayHeight, 0.0f ) );
    OverlayView.SetRotation( radian3( R_90, R_0, R_0 ) );
    OverlayView.SetXFOV    ( OverlayXFOV );
    OverlayView.SetViewport( L, T, R, B );
    OverlayView.SetPixelScale( ViewportAspect / s_OverlayReferenceAspect );
    OverlayView.SetZLimits ( 10.0f, 5000.0f );
    eng_SetView( OverlayView );
 
    // render it!
    m_OverlayHandle.Render( );

    // restore the original view
    eng_SetView( OrigView );
}

//==============================================================================

void hud_contagious_vision::UpdateEffects( f32 DeltaTime )
{
    m_OverlayHandle.AdvanceLogic( DeltaTime );
}
