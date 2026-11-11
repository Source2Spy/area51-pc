//==============================================================================
//
//  hud_Sniper.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "hud_Sniper.hpp"
#include "WeaponSniper.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"

#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"

//==============================================================================
// STORAGE
//==============================================================================

vector3 s_ZoomPos           ( 365.0f, 345.0f, 0.0f );
vector3 s_DistancePos       ( 230.0f, 100.0f, 0.0f );
f32     s_ZoomWidth         = 20.0f;
f32     s_ZoomHeight        = 20.0f;
xcolor  s_ZoomColor         ( 49, 255, 49, 255 );
s32     s_TrackerCount      = 5;

f32     g_ZoomDistance      = 10000.0f;

static constexpr f32 s_ScopeScanLineRadiusScale = 116.0f / 128.0f;

xcolor  s_ShadowColor       ( 49, 49, 49, 255 );
s32     s_ShadowOffsetX     = 2;
s32     s_ShadowOffsetY     = 2;

xcolor hud_sniper::m_SniperHudColor;
xcolor hud_sniper::m_SniperScanLineColor;
xcolor hud_sniper::m_SniperTrackerLineColor;
xcolor hud_sniper::m_SniperZoomTrackerColor;

rhandle<texture> hud_sniper::m_SniperHud;
rhandle<texture> hud_sniper::m_SniperStencilHud;
rhandle<texture> hud_sniper::m_SniperTrackerLine;
rhandle<texture> hud_sniper::m_SniperScanLine;
rhandle<texture> hud_sniper::m_SniperZoomPitchTracker;

//==============================================================================

static void DrawMirroredScope( const texture& Texture,
                               const vector2& Center,
                               const vector2& QuarterSize,
                               const xcolor& Color,
                               ui_blend_mode Blend )
{
    const vector2 TopLeft( Center.X - QuarterSize.X, Center.Y - QuarterSize.Y );
    const vector2 BottomLeft( Center.X - QuarterSize.X, Center.Y );
    const vector2 BottomRight( Center.X, Center.Y );
    const vector2 TopRight( Center.X, Center.Y - QuarterSize.Y );

    g_UIRenderer.DrawImage( Texture, TopLeft, QuarterSize,
                            vector2( 0.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                            Color, 0.0f, Blend, UI_SAMPLER_LINEAR_CLAMP );
    g_UIRenderer.DrawImage( Texture, BottomLeft, QuarterSize,
                            vector2( 0.0f, 1.0f ), vector2( 1.0f, 0.0f ),
                            Color, 0.0f, Blend, UI_SAMPLER_LINEAR_CLAMP );
    g_UIRenderer.DrawImage( Texture, BottomRight, QuarterSize,
                            vector2( 1.0f, 1.0f ), vector2( 0.0f, 0.0f ),
                            Color, 0.0f, Blend, UI_SAMPLER_LINEAR_CLAMP );
    g_UIRenderer.DrawImage( Texture, TopRight, QuarterSize,
                            vector2( 1.0f, 0.0f ), vector2( 0.0f, 1.0f ),
                             Color, 0.0f, Blend, UI_SAMPLER_LINEAR_CLAMP );
}

//==============================================================================

static void DrawScopeScanLines( texture const& Texture,
                                rect const& ViewBounds,
                                vector2 const& Center,
                                vector2 const& ScopeQuarterSize,
                                xcolor const& Color )
{
    xbitmap const& Bitmap = Texture.m_bitmap;
    f32 const TextureWidth = (f32)Bitmap.GetWidth();
    f32 const TextureHeight = (f32)Bitmap.GetHeight();
    if( (TextureWidth <= 0.0f) ||
        (TextureHeight <= 0.0f) ||
        (ScopeQuarterSize.X <= 0.0f) ||
        (ScopeQuarterSize.Y <= 0.0f) )
    {
        return;
    }

    constexpr s32 PerimeterSectionCount = 8;
    constexpr s32 QuadsPerSection = 4;
    constexpr s32 QuadCount = PerimeterSectionCount * QuadsPerSection;
    ui_vertex Vertices[QuadCount * 4];
    u32 Indices[QuadCount * 6];
    s32 VertexCount = 0;
    s32 IndexCount = 0;

    auto const GetUV = [&]( vector2 const& Position )
    {
        return vector2( (Position.X - ViewBounds.Min.X) / TextureWidth,
                        (Position.Y - ViewBounds.Min.Y) / TextureHeight );
    };

    auto const AddQuad = [&]( vector2 const& P0,
                              vector2 const& P1,
                              vector2 const& P2,
                              vector2 const& P3 )
    {
        u32 const BaseVertex = (u32)VertexCount;
        Vertices[VertexCount++] = ui_vertex( P0, GetUV( P0 ), Color );
        Vertices[VertexCount++] = ui_vertex( P1, GetUV( P1 ), Color );
        Vertices[VertexCount++] = ui_vertex( P2, GetUV( P2 ), Color );
        Vertices[VertexCount++] = ui_vertex( P3, GetUV( P3 ), Color );

        Indices[IndexCount++] = BaseVertex + 0;
        Indices[IndexCount++] = BaseVertex + 1;
        Indices[IndexCount++] = BaseVertex + 2;
        Indices[IndexCount++] = BaseVertex + 0;
        Indices[IndexCount++] = BaseVertex + 2;
        Indices[IndexCount++] = BaseVertex + 3;
    };

    f32 const LeftExtent = Center.X - ViewBounds.Min.X;
    f32 const TopExtent = Center.Y - ViewBounds.Min.Y;
    f32 const RightExtent = ViewBounds.Max.X - Center.X;
    f32 const BottomExtent = ViewBounds.Max.Y - Center.Y;

    radian const PerimeterAngles[PerimeterSectionCount + 1] =
    {
        -PI * 0.5f,
        x_atan2( -TopExtent, RightExtent ),
        0.0f,
        x_atan2( BottomExtent, RightExtent ),
        PI * 0.5f,
        x_atan2( BottomExtent, -LeftExtent ),
        PI,
        x_atan2( -TopExtent, -LeftExtent ) + PI * 2.0f,
        PI * 1.5f
    };

    auto const GetRingPoints = [&]( radian Angle,
                                    vector2& Inner,
                                    vector2& Outer )
    {
        f32 Sin;
        f32 Cos;
        x_sincos( Angle, Sin, Cos );

        f32 const ScopeX = Cos / ScopeQuarterSize.X;
        f32 const ScopeY = Sin / ScopeQuarterSize.Y;
        f32 const InnerDistance = 1.0f / x_sqrt( ScopeX * ScopeX + ScopeY * ScopeY );

        f32 HorizontalDistance = 1.0e30f;
        if( x_abs( Cos ) > 0.00001f )
        {
            HorizontalDistance = (Cos > 0.0f ? RightExtent : LeftExtent) / x_abs( Cos );
        }

        f32 VerticalDistance = 1.0e30f;
        if( x_abs( Sin ) > 0.00001f )
        {
            VerticalDistance = (Sin > 0.0f ? BottomExtent : TopExtent) / x_abs( Sin );
        }

        f32 const OuterDistance = MIN( HorizontalDistance, VerticalDistance );
        Inner.Set( Center.X + Cos * InnerDistance,
                   Center.Y + Sin * InnerDistance );
        Outer.Set( Center.X + Cos * OuterDistance,
                   Center.Y + Sin * OuterDistance );
    };

    for( s32 Section = 0; Section < PerimeterSectionCount; Section++ )
    {
        radian const SectionStart = PerimeterAngles[Section];
        radian const AngleStep = (PerimeterAngles[Section + 1] - SectionStart) /
                                 (f32)QuadsPerSection;
        for( s32 Quad = 0; Quad < QuadsPerSection; Quad++ )
        {
            radian const Angle0 = SectionStart + Quad * AngleStep;
            radian const Angle1 = Angle0 + AngleStep;
            vector2 Inner0;
            vector2 Outer0;
            vector2 Inner1;
            vector2 Outer1;
            GetRingPoints( Angle0, Inner0, Outer0 );
            GetRingPoints( Angle1, Inner1, Outer1 );
            AddQuad( Outer0, Outer1, Inner1, Inner0 );
        }
    }

    g_UIRenderer.GetDrawList().AddTriangles(
        ui_material( Texture, UI_BLEND_ALPHA, UI_SAMPLER_LINEAR_WRAP ),
        Vertices, VertexCount, Indices, IndexCount );
}

//==============================================================================

void hud_sniper::OnRender( player* pPlayer )
{
    if( !pPlayer->RenderSniperZoom() )
        return;

    rect ScreenViewDimensions;
    pPlayer->GetRenderView().GetViewport( ScreenViewDimensions );
    const irect ScreenViewport( (s32)ScreenViewDimensions.Min.X,
                                (s32)ScreenViewDimensions.Min.Y,
                                (s32)ScreenViewDimensions.Max.X,
                                (s32)ScreenViewDimensions.Max.Y );
    const rect HudViewDimensions = g_UIRenderer.GetViewport().GetHudBounds( ScreenViewport );
    const vector2 HudCenter( HudViewDimensions.GetCenter().X,
                             HudViewDimensions.GetCenter().Y );
    const irect ViewClip( (s32)x_floor( HudViewDimensions.Min.X ),
                          (s32)x_floor( HudViewDimensions.Min.Y ),
                          (s32)x_ceil ( HudViewDimensions.Max.X ),
                          (s32)x_ceil ( HudViewDimensions.Max.Y ) );

    texture* pMainTexture = m_SniperHud.GetPointer();
    if( !pMainTexture )
        return;

    const xbitmap& MainBitmap = pMainTexture->m_bitmap;
    const f32 ScaleX = ((MainBitmap.GetWidth() * 2.0f) < m_ViewDimensions.GetWidth())
                     ? 1.0f
                     : m_ViewDimensions.GetWidth() / (MainBitmap.GetWidth() * 2.0f);
    const f32 ScaleY = ((MainBitmap.GetHeight() * 2.0f) < m_ViewDimensions.GetHeight())
                     ? 1.0f
                     : m_ViewDimensions.GetHeight() / (MainBitmap.GetHeight() * 2.0f);
    const f32 Scale = MIN( ScaleX, ScaleY );
    const vector2 ScopeQuarterSize( MainBitmap.GetWidth() * Scale,
                                    MainBitmap.GetHeight() * Scale );

    g_UIRenderer.PushClipRect( ViewClip );

    texture* pScanLineTexture = m_SniperScanLine.GetPointer();
    if( pScanLineTexture )
    {
        // Match the alpha >= 64 boundary authored into the legacy PS2 stencil.
        const vector2 ScanLineRadius( ScopeQuarterSize.X * s_ScopeScanLineRadiusScale,
                                      ScopeQuarterSize.Y * s_ScopeScanLineRadiusScale );
        DrawScopeScanLines( *pScanLineTexture, HudViewDimensions, HudCenter,
                            ScanLineRadius, m_SniperScanLineColor );
    }

    DrawMirroredScope( *pMainTexture, HudCenter, ScopeQuarterSize,
                       m_SniperHudColor, UI_BLEND_ADDITIVE );

    texture* pTrackerTexture = m_SniperTrackerLine.GetPointer();
    if( pTrackerTexture )
    {
        const xbitmap& TrackerBitmap = pTrackerTexture->m_bitmap;
        const vector2 TrackerSize( (f32)TrackerBitmap.GetWidth(),
                                   (f32)TrackerBitmap.GetHeight() );
        vector2 BasePosition( HudCenter.X - (ScopeQuarterSize.X + TrackerSize.X),
                              HudCenter.Y - TrackerSize.Y );

        g_UIRenderer.DrawImage( *pTrackerTexture, BasePosition, TrackerSize,
                                vector2( 0.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                                m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );

        s32 i;
        for( i = 1; i < s_TrackerCount; i++ )
        {
            vector2 Position( BasePosition.X, BasePosition.Y + i * TrackerSize.Y );
            g_UIRenderer.DrawImage( *pTrackerTexture, Position, TrackerSize,
                                    vector2( 0.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                                    m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );

            Position.Y = BasePosition.Y - i * TrackerSize.Y;
            g_UIRenderer.DrawImage( *pTrackerTexture, Position, TrackerSize,
                                    vector2( 0.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                                    m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );
        }

        vector2 HalfSize( TrackerSize.X, TrackerSize.Y * 0.5f );
        vector2 Position( BasePosition.X,
                          BasePosition.Y + (i - 1) * TrackerSize.Y + HalfSize.Y );
        g_UIRenderer.DrawImage( *pTrackerTexture, Position, HalfSize,
                                vector2( 0.0f, 0.0f ), vector2( 1.0f, 0.5f ),
                                m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );

        BasePosition.X = HudCenter.X + ScopeQuarterSize.X;
        g_UIRenderer.DrawImage( *pTrackerTexture, BasePosition, TrackerSize,
                                vector2( 1.0f, 0.0f ), vector2( 0.0f, 1.0f ),
                                m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );

        for( i = 1; i < s_TrackerCount; i++ )
        {
            Position = vector2( BasePosition.X, BasePosition.Y + i * TrackerSize.Y );
            g_UIRenderer.DrawImage( *pTrackerTexture, Position, TrackerSize,
                                    vector2( 1.0f, 0.0f ), vector2( 0.0f, 1.0f ),
                                    m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );

            Position.Y = BasePosition.Y - i * TrackerSize.Y;
            g_UIRenderer.DrawImage( *pTrackerTexture, Position, TrackerSize,
                                    vector2( 1.0f, 0.0f ), vector2( 0.0f, 1.0f ),
                                    m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );
        }

        Position = vector2( BasePosition.X,
                            BasePosition.Y + (i - 1) * TrackerSize.Y + HalfSize.Y );
        g_UIRenderer.DrawImage( *pTrackerTexture, Position, HalfSize,
                                vector2( 1.0f, 0.0f ), vector2( 0.0f, 0.5f ),
                                m_SniperTrackerLineColor, 0.0f, UI_BLEND_ADDITIVE );

        texture* pPitchTexture = m_SniperZoomPitchTracker.GetPointer();
        if( pPitchTexture )
        {
            const xbitmap& PitchBitmap = pPitchTexture->m_bitmap;
            const vector2 PitchSize( (f32)PitchBitmap.GetWidth(),
                                     (f32)PitchBitmap.GetHeight() );
            f32 Pitch;
            f32 Yaw;
            pPlayer->GetEyesPitchYaw( Pitch, Yaw );
            const f32 YDelta = (s_TrackerCount - 1) * TrackerSize.Y * (Pitch / (PI / 2.0f));

            Position.X = HudCenter.X - (ScopeQuarterSize.X + PitchSize.X + TrackerSize.X);
            Position.Y = HudCenter.Y + YDelta - PitchSize.Y * 0.5f;
            g_UIRenderer.DrawImage( *pPitchTexture, Position, PitchSize,
                                    vector2( 0.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                                    m_SniperZoomTrackerColor, 0.0f, UI_BLEND_ADDITIVE );

            Position.X = HudCenter.X + ScopeQuarterSize.X + TrackerSize.X;
            g_UIRenderer.DrawImage( *pPitchTexture, Position, PitchSize,
                                    vector2( 1.0f, 0.0f ), vector2( 0.0f, 1.0f ),
                                    m_SniperZoomTrackerColor, 0.0f, UI_BLEND_ADDITIVE );
        }
    }

    g_UIRenderer.PopClipRect();

#ifndef X_EDITOR
    {
        irect ZoomRect;
        vector3 SniperZoomTextPos( s_ZoomPos );

        ZoomRect.l = (s32)((SniperZoomTextPos.GetX() - s_ZoomWidth) + m_ViewDimensions.Min.X);
        ZoomRect.r = (s32)((SniperZoomTextPos.GetX() + s_ZoomWidth) + m_ViewDimensions.Min.X);
        ZoomRect.t = (s32)((SniperZoomTextPos.GetY() - s_ZoomHeight) + m_ViewDimensions.Min.Y);
        ZoomRect.b = (s32)((SniperZoomTextPos.GetY() + s_ZoomHeight) + m_ViewDimensions.Min.Y);

        ZoomRect.Translate( s_ShadowOffsetX, s_ShadowOffsetY );
        g_UiMgr->RenderText( 1, ZoomRect, ui_font::h_left | ui_font::v_top,
                             s_ShadowColor, m_WeaponZoomLevel, TRUE, TRUE );
        ZoomRect.Translate( -s_ShadowOffsetX, -s_ShadowOffsetY );
        g_UiMgr->RenderText( 1, ZoomRect, ui_font::h_left | ui_font::v_top,
                             s_ZoomColor, m_WeaponZoomLevel, TRUE, TRUE );
    }

    {
        irect ZoomRect;
        vector3 DistanceTextPos( s_DistancePos );
        ui_font* pFont = g_UiMgr->GetFont( "small" );
        const s32 Count = m_WeaponZoomDistance.GetLength();
        const s32 Width = pFont->TextWidth( m_WeaponZoomDistance, Count );
        const s32 Height = pFont->TextHeight( m_WeaponZoomDistance, Count );

        DistanceTextPos.GetX() = (f32)ui_viewport::CONTENT_WIDTH * 0.5f - Width * 0.5f;

        ZoomRect.l = (s32)DistanceTextPos.GetX();
        ZoomRect.r = (s32)(DistanceTextPos.GetX() + Width);
        ZoomRect.t = (s32)(DistanceTextPos.GetY() - Height);
        ZoomRect.b = (s32)(DistanceTextPos.GetY() + Height);

        ZoomRect.Translate( s_ShadowOffsetX, s_ShadowOffsetY );
        g_UiMgr->RenderText( 1, ZoomRect, ui_font::h_left | ui_font::v_top,
                             s_ShadowColor, m_WeaponZoomDistance, TRUE, TRUE );
        ZoomRect.Translate( -s_ShadowOffsetX, -s_ShadowOffsetY );
        g_UiMgr->RenderText( 1, ZoomRect, ui_font::h_left | ui_font::v_top,
                             s_ZoomColor, m_WeaponZoomDistance, TRUE, TRUE );
    }
#endif
}

//==============================================================================

void hud_sniper::OnAdvanceSimulation( player* pPlayer, f32 DeltaTime )
{
    (void)DeltaTime;

    if( !pPlayer->RenderSniperZoom() )
        return;

    radian Pitch;
    radian Yaw;
    const vector3 ViewPos = pPlayer->GetEyesPosition();
    pPlayer->GetEyesPitchYaw( Pitch, Yaw );

    vector3 Dest( radian3( Pitch, Yaw, 0.0f ) );
    Dest *= g_ZoomDistance;

    g_CollisionMgr.AddToIgnoreList( pPlayer->GetGuid() );
    g_CollisionMgr.RaySetup( pPlayer->GetGuid(), ViewPos, ViewPos + Dest );
    g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES,
                                    object::ATTR_COLLIDABLE,
                                    object::ATTR_COLLISION_PERMEABLE );

    if( g_CollisionMgr.m_nCollisions > 0 )
    {
        const f32 ZoomDistance = g_CollisionMgr.m_Collisions[0].T * g_ZoomDistance / 100.0f;
        m_WeaponZoomDistance = xstring( xfs( "> %05.1f <", ZoomDistance ) );
    }
    else
    {
        m_WeaponZoomDistance = xstring( xfs( "> >%05.1f <", g_ZoomDistance / 100.0f ) );
    }

    new_weapon* pWeapon = pPlayer->GetCurrentWeaponPtr();
    if( pWeapon )
    {
        weapon_sniper_rifle* pSniper = (weapon_sniper_rifle*)pWeapon;
        m_WeaponZoomLevel = xstring( xfs( "%2dx", (s32)pSniper->GetZoomLevel() ) );
    }
}

//==============================================================================

xbool hud_sniper::OnProperty( prop_query& rPropQuery )
{
    if( rPropQuery.IsVar( "Hud\\Weapon Hud\\Sniper Main" ) )
    {
        if( rPropQuery.IsRead() )
            rPropQuery.SetVarExternal( m_SniperHud.GetName(), RESOURCE_NAME_SIZE );
        else
            m_SniperHud.SetName( rPropQuery.GetVarExternal() );
        return TRUE;
    }

    if( rPropQuery.IsVar( "Hud\\Weapon Hud\\Sniper Stencil" ) )
    {
        if( rPropQuery.IsRead() )
            rPropQuery.SetVarExternal( m_SniperStencilHud.GetName(), RESOURCE_NAME_SIZE );
        else
            m_SniperStencilHud.SetName( rPropQuery.GetVarExternal() );
        return TRUE;
    }

    if( rPropQuery.IsVar( "Hud\\Weapon Hud\\Sniper Traker Line" ) )
    {
        if( rPropQuery.IsRead() )
            rPropQuery.SetVarExternal( m_SniperTrackerLine.GetName(), RESOURCE_NAME_SIZE );
        else
            m_SniperTrackerLine.SetName( rPropQuery.GetVarExternal() );
        return TRUE;
    }

    if( rPropQuery.IsVar( "Hud\\Weapon Hud\\Sniper Scan Line" ) )
    {
        if( rPropQuery.IsRead() )
            rPropQuery.SetVarExternal( m_SniperScanLine.GetName(), RESOURCE_NAME_SIZE );
        else
            m_SniperScanLine.SetName( rPropQuery.GetVarExternal() );
        return TRUE;
    }

    if( rPropQuery.IsVar( "Hud\\Weapon Hud\\Sniper Zoom Tracker" ) )
    {
        if( rPropQuery.IsRead() )
            rPropQuery.SetVarExternal( m_SniperZoomPitchTracker.GetName(), RESOURCE_NAME_SIZE );
        else
            m_SniperZoomPitchTracker.SetName( rPropQuery.GetVarExternal() );
        return TRUE;
    }

    if( rPropQuery.VarColor( "Hud\\Weapon Hud\\Sniper Hud Color", m_SniperHudColor ) )
        return TRUE;
    if( rPropQuery.VarColor( "Hud\\Weapon Hud\\Sniper Tracker Line Color", m_SniperTrackerLineColor ) )
        return TRUE;
    if( rPropQuery.VarColor( "Hud\\Weapon Hud\\Sniper Scan Line Color", m_SniperScanLineColor ) )
        return TRUE;
    if( rPropQuery.VarColor( "Hud\\Weapon Hud\\Sniper Zoom Tracker Color", m_SniperZoomTrackerColor ) )
        return TRUE;

    return FALSE;
}

//==============================================================================

void hud_sniper::OnEnumProp( prop_enum& List )
{
    List.PropEnumHeader  ( "Hud\\Weapon Hud", "The bitmaps for the weapons that will be overlayed on the hud.", 0 );
    List.PropEnumExternal( "Hud\\Weapon Hud\\Sniper Main", "Resource\0xbmp\0", "The main sniper rifle hud overlay.", 0 );
    List.PropEnumExternal( "Hud\\Weapon Hud\\Sniper Stencil", "Resource\0xbmp\0", "The stencil sniper rifle hud overlay.", 0 );
    List.PropEnumExternal( "Hud\\Weapon Hud\\Sniper Traker Line", "Resource\0xbmp\0", "The sniper rifle tracker line.", 0 );
    List.PropEnumExternal( "Hud\\Weapon Hud\\Sniper Scan Line", "Resource\0xbmp\0", "The sniper rifle scan line.", 0 );
    List.PropEnumExternal( "Hud\\Weapon Hud\\Sniper Center Reticle", "Resource\0xbmp\0", "The sniper rifle center reticle piece.", 0 );
    List.PropEnumExternal( "Hud\\Weapon Hud\\Sniper Zoom Tracker", "Resource\0xbmp\0", "The sniper zoom pitch tracking piece.", 0 );

    List.PropEnumColor( "Hud\\Weapon Hud\\Sniper Hud Color", "The color of the main sniper rifle hud.", 0 );
    List.PropEnumColor( "Hud\\Weapon Hud\\Sniper Tracker Line Color", "The color of the main sniper rifle hud.", 0 );
    List.PropEnumColor( "Hud\\Weapon Hud\\Sniper Scan Line Color", "The color of the main sniper rifle hud.", 0 );
    List.PropEnumColor( "Hud\\Weapon Hud\\Sniper Zoom Tracker Color", "The color of the zoom tracker piece.", 0 );
}
