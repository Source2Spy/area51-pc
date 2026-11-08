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

#include "hud_MutantVision.hpp"
#include "GameLib/RenderContext.hpp"

//==============================================================================
// STATICS
//==============================================================================

static const f32 s_MutantOverlayHeight = 425.0f;
static f32 const s_OverlayAuthoringXFOV   = R_60;
static f32 const s_OverlayReferenceAspect = 4.0f / 3.0f;

rhandle<char>   hud_mutant_vision::m_OverlayResource;
fx_handle       hud_mutant_vision::m_OverlayHandle;

//==============================================================================
// FUNCTIONS
//==============================================================================

hud_mutant_vision::hud_mutant_vision( void )
{
}

//==============================================================================

hud_mutant_vision::~hud_mutant_vision( void )
{
    m_OverlayHandle.KillInstance();
}

//==============================================================================

void hud_mutant_vision::OnRender( player* pPlayer )
{
    (void)pPlayer;

    // mutant vision on or off?
    if( !g_RenderContext.m_bIsMutated )
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

xbool hud_mutant_vision::OnProperty( prop_query& rPropQuery )
{
    if ( rPropQuery.IsVar( "MutantVision\\OverlayFX" ) )
    {
        if ( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarExternal( m_OverlayResource.GetName(), RESOURCE_NAME_SIZE );
        }
        else
        {
            const char* pString = rPropQuery.GetVarExternal();
            if ( pString[0] )
            {
                m_OverlayResource.SetName( pString );

                // initialize the effect
                if ( m_OverlayResource.GetPointer() )
                {
                    m_OverlayHandle.KillInstance();
                    m_OverlayHandle.InitInstance    ( m_OverlayResource.GetPointer() );
                    m_OverlayHandle.SetRotation     ( radian3( R_0, R_0, R_0 ) );
                    m_OverlayHandle.SetTranslation  ( vector3( 0.0f, 0.0f, 0.0f ) );
                    m_OverlayHandle.SetScale        ( vector3( 1.0f, 1.0f, 1.0f ) );
                    m_OverlayHandle.SetColor        ( XCOLOR_WHITE );
                }
            }
        }

        return TRUE;
    }

    return FALSE;
}

//==============================================================================

void hud_mutant_vision::OnEnumProp( prop_enum& List )
{
    List.PropEnumHeader  ( "MutantVision", "Properties for the mutant vision HUD elements", 0 );
    List.PropEnumExternal( "MutantVision\\OverlayFX" , "Resource\0fxo\0", "Particle for the overlay effect" , PROP_TYPE_MUST_ENUM );
}

//==============================================================================

void hud_mutant_vision::UpdateEffects( f32 DeltaTime )
{
    m_OverlayHandle.AdvanceLogic( DeltaTime );
}
