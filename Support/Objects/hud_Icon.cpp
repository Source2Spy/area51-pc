//==============================================================================
//
//  hud_Icon.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "hud_Icon.hpp"
#include "HudObject.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"
#include "GameLib/RenderContext.hpp"

#ifndef X_EDITOR
#include "StateMgr/StateMgr.hpp"
#endif

static f32 const ICON_FADE_IN_RATE  = 1.4f;
static f32 const ICON_FADE_OUT_RATE = 1.4f;
static f32 const ENEMY_LOCK_TIME    = 0.33f;

//==============================================================================
// FUNCTIONS
//==============================================================================

hud_icon::hud_icon( void ) 
{
    Init();
}

//==============================================================================

void hud_icon::Init( void )
{
    m_NumActiveIcons    = 0;
    m_NumStagingIcons   = 0;
    x_memset( m_PlayerIconVisible,    0, sizeof(m_PlayerIconVisible) );
    x_memset( m_PlayerIconIsAlly,     0, sizeof(m_PlayerIconIsAlly) );
    x_memset( m_PlayerIconOpacity,    0, sizeof(m_PlayerIconOpacity) );
    x_memset( m_PlayerIconSightDelay, 0, sizeof(m_PlayerIconSightDelay) );
}

//==============================================================================

void hud_icon::BeginSimulationSnapshot( void )
{
    m_NumStagingIcons = 0;
    x_memset( m_PlayerIconVisible, 0, sizeof(m_PlayerIconVisible) );
}

//==============================================================================

void hud_icon::SubmitIcon(      icon_type       IconType,
                        const   vector3&        FocusPosition,
                        const   vector3&        RenderPosition,
                                xbool           bAlignToBottom,
                                gutter_type     GutterType, 
                                xcolor          Color,
                        const   xwchar*         pCharName,
                                xbool           Pulsing, 
                                xbool           Distance, 
                                f32             Opacity,
                                f32             IconFadeDist,
                                f32             TextFadeDist,
                                s32             PlayerNum
                       )
{
    if( !IN_RANGE( 0, m_NumStagingIcons, NUM_ICONS - 1 ) )
    {
        return;
    }

#ifndef X_EDITOR
    else if( g_StateMgr.GetState() != SM_PLAYING_GAME )
    {
        return;
    }
#endif

    icon_inf TempIcon;

    TempIcon.IconType       = IconType;
    TempIcon.FocusPosition  = FocusPosition;
    TempIcon.RenderPosition = RenderPosition;
    TempIcon.bAlignToBottom = bAlignToBottom;
    TempIcon.GutterType     = GutterType;
    TempIcon.Color          = Color;
    TempIcon.Pulsing        = Pulsing;
    TempIcon.Distance       = Distance;
    TempIcon.IconFadeDist   = IconFadeDist;
    TempIcon.TextFadeDist   = TextFadeDist;
    TempIcon.Opacity        = Opacity;
    TempIcon.PlayerNum      = PlayerNum;

    // Copy the label over.
    if( pCharName )     x_wstrcpy( TempIcon.Label, pCharName );
    else                TempIcon.Label[0] = '\0';

    m_StagingIcons[m_NumStagingIcons] = TempIcon;
    m_NumStagingIcons++;

    ASSERTS( m_NumStagingIcons <= NUM_ICONS, "Too many icons!" );
}

//==============================================================================

void hud_icon::SetPlayerIconTarget( s32 PlayerNum, xbool Visible, xbool IsAlly )
{
    if( !IN_RANGE( 0, PlayerNum, 31 ) )
    {
        return;
    }

    m_PlayerIconVisible[PlayerNum] = Visible;
    m_PlayerIconIsAlly[PlayerNum] = IsAlly;
}

//==============================================================================

void hud_icon::CommitSimulationSnapshot( void )
{
    if( m_NumStagingIcons > 0 )
    {
        x_memcpy( m_Icons,
                  m_StagingIcons,
                  sizeof(icon_inf) * m_NumStagingIcons );
    }
    m_NumActiveIcons = m_NumStagingIcons;
}

//==============================================================================

void hud_icon::RenderIcon( player* pPlayer, const icon_inf& Icon )
{
    const view& rView = pPlayer->GetRenderView();
    rect ScreenViewDimensions;
    rView.GetViewport( ScreenViewDimensions );

    const irect ScreenViewport( (s32)ScreenViewDimensions.Min.X,
                                (s32)ScreenViewDimensions.Min.Y,
                                (s32)ScreenViewDimensions.Max.X,
                                (s32)ScreenViewDimensions.Max.Y );
    const rect ViewDimensions = g_UIRenderer.GetViewport().GetHudBounds( ScreenViewport );

    const f32 Margin     = 0.85f;
    const f32 Hud_Width  = ViewDimensions.GetWidth()  / 2.0f;
    const f32 Hud_Height = ViewDimensions.GetHeight() / 2.0f;

    const f32 IconOpacity = IN_RANGE( 0, Icon.PlayerNum, 31 )
                          ? m_PlayerIconOpacity[Icon.PlayerNum]
                          : Icon.Opacity;
    if( IconOpacity <= 0.0f )
    {
        return;
    }

    const vector3 RenderWorldPos = Icon.RenderPosition;
    const vector3 EyesWorldPos   = pPlayer->GetEyesPosition();
    const vector3 TargetWorldPos = Icon.FocusPosition;

    vector3 WorldToTarget  = TargetWorldPos - EyesWorldPos;
    f32     WorldDist      = WorldToTarget.Length();

    const vector3 FocusScreenPos  = rView.PointToScreen( TargetWorldPos );
    const vector3 RenderScreenPos = rView.PointToScreen( RenderWorldPos );
    const vector3 RenderViewPos   = rView.ConvertW2V( RenderWorldPos );
    const vector2 FocusHudPosition = g_UIRenderer.GetViewport().ScreenToHud(
        vector2( FocusScreenPos.GetX(), FocusScreenPos.GetY() ), ScreenViewport );
    const vector2 RenderHudPosition = g_UIRenderer.GetViewport().ScreenToHud(
        vector2( RenderScreenPos.GetX(), RenderScreenPos.GetY() ), ScreenViewport );
    vector3 FocusHudPos( FocusHudPosition.X, FocusHudPosition.Y, FocusScreenPos.GetZ() );
    vector3 RenderHudPos( RenderHudPosition.X, RenderHudPosition.Y, RenderScreenPos.GetZ() );

    const vector3 HudCenterPos( ViewDimensions.GetCenter().X,
                                ViewDimensions.GetCenter().Y,
                                0.0f );
    const f32 HalfWidth  = MAX( Hud_Width  * Margin, 1.0f );
    const f32 HalfHeight = MAX( Hud_Height * Margin, 1.0f );
    const xbool IsBehind = RenderViewPos.GetZ() <= 0.001f;

    // PointToScreen projects with abs(Z) for points behind the camera, so its
    // X/Y already identify the correct screen edge. Do not invert them again.
    vector3 DirToIcon = RenderHudPos - HudCenterPos;
    DirToIcon.GetZ() = 0.0f;

    if( DirToIcon.LengthSquared() < 0.0001f )
    {
        if( IsBehind )
        {
            // A target exactly on the rear camera axis has no left/right
            // preference. Keep the indicator stable at the bottom edge.
            DirToIcon.Set( 0.0f, 1.0f, 0.0f );
        }
        else
        {
            DirToIcon.Set( -RenderViewPos.GetX(), -RenderViewPos.GetY(), 0.0f );
            if( DirToIcon.LengthSquared() < 0.0001f )
            {
                DirToIcon.Set( 0.0f, 1.0f, 0.0f );
            }
        }
    }

    xbool InRegion = FALSE;

    //
    // Figure out what the icon's position on the screen should be
    // based on the bounding type chosen.
    // 
    switch( Icon.GutterType )
    {
    case GUTTER_ELLIPSE:
        {
            const f32 RelativeX = RenderHudPos.GetX() - HudCenterPos.GetX();
            const f32 RelativeY = RenderHudPos.GetY() - HudCenterPos.GetY();
            const f32 EllipseValue = (RelativeX * RelativeX) / (HalfWidth  * HalfWidth ) +
                                     (RelativeY * RelativeY) / (HalfHeight * HalfHeight);
            InRegion = !IsBehind && (EllipseValue <= 1.0f);
            if( !InRegion )
            {
                const f32 Denominator = x_sqrt(
                    (DirToIcon.GetX() * DirToIcon.GetX()) / (HalfWidth  * HalfWidth ) +
                    (DirToIcon.GetY() * DirToIcon.GetY()) / (HalfHeight * HalfHeight) );
                const f32 RayScale = (Denominator > 0.0001f) ? (1.0f / Denominator) : 0.0f;
                RenderHudPos = HudCenterPos + DirToIcon * RayScale;
            }
        }
        break;

    case GUTTER_RECTANGLE:
        {
            const f32 RelativeX = RenderHudPos.GetX() - HudCenterPos.GetX();
            const f32 RelativeY = RenderHudPos.GetY() - HudCenterPos.GetY();
            InRegion = !IsBehind &&
                       (x_abs( RelativeX ) <= HalfWidth) &&
                       (x_abs( RelativeY ) <= HalfHeight);
            if( !InRegion )
            {
                const f32 ScaleX = (x_abs( DirToIcon.GetX() ) > 0.0001f)
                                 ? HalfWidth / x_abs( DirToIcon.GetX() )
                                 : F32_MAX;
                const f32 ScaleY = (x_abs( DirToIcon.GetY() ) > 0.0001f)
                                 ? HalfHeight / x_abs( DirToIcon.GetY() )
                                 : F32_MAX;
                RenderHudPos = HudCenterPos + DirToIcon * MIN( ScaleX, ScaleY );
            }
        }
        break;

    case GUTTER_NONE:
        InRegion = TRUE;
        if( IsBehind )
        {
            return;
        }
        break;

    default:
        break;
    }

    RenderHudPos.GetZ() = 0.0f;
    FocusHudPos.GetZ() = 0.0f;
    const f32 HudDistance = (FocusHudPos - HudCenterPos).Length();

    //
    // Draw the icon.
    //
    {
        //
        // Get rotation.
        //
        radian  BitmapRotation = R_0;

        if( !InRegion )
        {
            BitmapRotation = x_atan2( DirToIcon.GetX(), DirToIcon.GetY() );
        }

        m_ScreenEdgeBmp.SetName( PRELOAD_FILE("HUD/GENERAL/HUD_ICON.XBMP") );

        texture* pTexture = m_ScreenEdgeBmp.GetPointer();
        if( pTexture == NULL )
        {
            return;
        }

        const xbitmap& Bitmap = pTexture->m_bitmap;

        f32 IconMargin = 0.01f;

        // This indexes into the icon bitmap to get the appropriate coordinates for the icon.
        f32 x1 = ((f32)(((Icon.IconType) % 4) + 0.0f) / 4.0f) + IconMargin;
        f32 y1 = (((Icon.IconType / 4) + 0) / 4.0f)           + IconMargin;

        f32 x2 = x1 + 0.25f - (2.0f * IconMargin);
        f32 y2 = y1 + 0.25f - (2.0f * IconMargin);
                           
        vector2 WH( (f32)Bitmap.GetWidth() / 4.0f, (f32)Bitmap.GetHeight() / 4.0f );

        if( Icon.IconType == ICON_WAYPOINT )
        {
            WH *= 1.0f / g_UIRenderer.GetViewport().GetHudUserScale();
        }

        if( (Icon.IconType == ICON_FLAG_INNER) || 
            (Icon.IconType == ICON_FLAG_OUTER) )
        {
            x1 = (f32)((Icon.IconType - ICON_FLAG_INNER) * 0.50f) + IconMargin;
            x2 = x1 + 0.5f - (2.0f * IconMargin);

            y1 = 0.5f + IconMargin;
            y2 = 1.0f - IconMargin;

            WH *= 2.0f;
        }

        vector2 UV0(  x1, y1 );
        vector2 UV1(  x2, y2 );

        xcolor  Color            = Icon.Color;
        Color.A                  = (s32)(GetOpacity( HudDistance, IconOpacity, Icon.IconFadeDist ) * 255);

        if( Icon.bAlignToBottom )
        {
            RenderHudPos.GetY() -= 8.0f;
        }

        // The legacy rotated-sprite API treated Position as the sprite center.
        // DrawImage uses the top-left corner, so preserve the original anchor.
        const vector2 DrawPosition( RenderHudPos.GetX() - WH.X * 0.5f,
                                    RenderHudPos.GetY() - WH.Y * 0.5f );
        g_UIRenderer.DrawImage( *pTexture,
                                DrawPosition,
                                WH,
                                UV0,
                                UV1,
                                Color,
                                BitmapRotation,
                                UI_BLEND_ALPHA,
                                UI_SAMPLER_LINEAR_CLAMP );
    }

#ifndef X_EDITOR
    //
    // Render any optional text.
    //
    if( InRegion )
    {
        // Render the label, if any.
        if( ((xwchar*)Icon.Label)[0] != 0 )
        {
            // Draw label to the left.
            xcolor LabelColor( XCOLOR_WHITE );

            f32 FinalOpacity = GetOpacity( HudDistance, IconOpacity, Icon.TextFadeDist );

            LabelColor.A = (u8)(FinalOpacity * 255); 

            irect LabelPos(
                (s32)(RenderHudPos.GetX() - 200),
                (s32)(RenderHudPos.GetY() -   8),
                (s32)(RenderHudPos.GetX() -  12),
                (s32)(RenderHudPos.GetY() + 200)
                );

            xcolor TextColor = XCOLOR_WHITE;
            RenderLine( Icon.Label, LabelPos, LabelColor.A, TextColor, 1, ui_font::h_right|ui_font::v_top, TRUE );
        }

        // Render the distance if enabled.
        if( Icon.Distance )
        {
            // Draw the distance to the right.
            xcolor LabelColor( XCOLOR_WHITE );

            f32 FinalOpacity = GetOpacity( HudDistance, IconOpacity, Icon.TextFadeDist );

            LabelColor.A = (u8)(FinalOpacity * 255); 

            irect LabelPos(
                (s32)(RenderHudPos.GetX() + 12),
                (s32)(RenderHudPos.GetY() - 8),
                (s32)(RenderHudPos.GetX() + 200),
                (s32)(RenderHudPos.GetY() + 200)
                );

            xwstring DistanceStr( xfs( "%.2fm", WorldDist / 100.0f ) );

            xcolor TextColor = XCOLOR_WHITE;
            RenderLine( DistanceStr, LabelPos, LabelColor.A, TextColor, 1, ui_font::h_left|ui_font::v_top, TRUE );
        }
    }
#endif


}

//==============================================================================

void hud_icon::OnRender( player* pPlayer )
{
    xbool   IconRendered[ NUM_ICONS ];
    x_memset( IconRendered, FALSE, NUM_ICONS );

    f32 DistancesSquared[ NUM_ICONS ];

    vector3 PlayerPos = pPlayer->GetEyesPosition();

    // Precompute the squared distances.
    for( s32 i = 0; i < m_NumActiveIcons; i++ )
    {
        DistancesSquared[ i ] = (PlayerPos - m_Icons[ i ].RenderPosition).LengthSquared();
    }

    // Now render them from back to front.
    for( s32 i = 0; i < m_NumActiveIcons; i++ )
    {
        f32 Farthest = -1.0f;
        s32 iIcon = -1;

        // Look for the farthest away icon that hasn't been rendered.
        for( s32 j = 0; j < m_NumActiveIcons; j++ )
        {
            if( IconRendered[ j ] ) continue;

            if( DistancesSquared[ j ] > Farthest )
            {
                iIcon    = j;
                Farthest = DistancesSquared[ j ];
            }
        }

        ASSERT( iIcon != -1 );
        ASSERT( !IconRendered[ iIcon ] );

        RenderIcon( pPlayer, m_Icons[ iIcon ] );
        IconRendered[ iIcon ] = TRUE;
    }
}

//==============================================================================

f32 hud_icon::GetOpacity( f32 DistFromCenter, f32 Opacity, f32 FadeDist )
{
    f32 PercentageDist = (FadeDist - DistFromCenter) / FadeDist;

    if( FadeDist < 0.0f )
    {
        PercentageDist = 1.0f;

    }

    else
    {
        if( PercentageDist < 0.0f )
        {
            PercentageDist = 0.0f;
        }
        PercentageDist *= 1.5f;
        if( PercentageDist > 1.0f ) 
        {
            PercentageDist = 1.0f;
        }
    }

    return Opacity * PercentageDist;
}

//==============================================================================

xbool hud_icon::OnProperty( prop_query& rPropQuery )
{
    if( rPropQuery.IsVar( "Nav Point\\ScreenEdge Bmp" ) )
    {
        if( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarExternal( m_ScreenEdgeBmp.GetName(), RESOURCE_NAME_SIZE );
        }
        else            
        {
            const char* pStr = rPropQuery.GetVarExternal();
            m_ScreenEdgeBmp.SetName( pStr );
        }
        return TRUE;
    }    

    if( rPropQuery.VarColor( "Nav Point\\ScreenEdge Color", m_ScreenEdgeColor ) )
        return TRUE;

    if( rPropQuery.IsVar( "Nav Point\\ScreenCenter Bmp" ) )
    {
        if( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarExternal( m_ScreenCenterBmp.GetName(), RESOURCE_NAME_SIZE );
        }
        else            
        {
            const char* pStr = rPropQuery.GetVarExternal();
            m_ScreenCenterBmp.SetName( pStr );
        }
        return TRUE;
    }    

    if( rPropQuery.VarColor( "Nav Point\\ScreenCenter Color", m_ScreenCenterColor ) )
        return TRUE;

    if( rPropQuery.VarBool( "Nav Point\\Start Active", m_Active ) )
        return TRUE;

    return FALSE;
}

//==============================================================================

void hud_icon::OnEnumProp( prop_enum&  List )
{
    List.PropEnumHeader  ( "Nav Point", "Point to navigate the player too.", 0 );

    List.PropEnumExternal( "Nav Point\\ScreenEdge Bmp",   "Resource\0xbmp\0", "Bitmap to use when the nav point is outside of the view", PROP_TYPE_MUST_ENUM  );
    List.PropEnumColor   ( "Nav Point\\ScreenEdge Color", "The color for the bitmap that will be draw when the nav point is not in view.", 0 );

    List.PropEnumExternal( "Nav Point\\ScreenCenter Bmp",   "Resource\0xbmp\0", "Bitmap to use when the nav point is inside of the view", PROP_TYPE_MUST_ENUM  );
    List.PropEnumColor   ( "Nav Point\\ScreenCenter Color", "The color for the bitmap that will be draw when the nav point is in view.", 0 );

    List.PropEnumBool    ( "Nav Point\\Start Active", "Do you want this object to start active.", PROP_TYPE_EXPOSE );
}

//==============================================================================

