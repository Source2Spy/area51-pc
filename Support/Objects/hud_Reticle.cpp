//==============================================================================
//
//  hud_Reticle.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "hud_Reticle.hpp"
#include "HudObject.hpp"
#include "Objects/Actor/Actor.hpp"
#include "UI/ui_renderer.hpp"

namespace
{
    static const xcolor RETICLE_COLOR                 ( 134, 255, 255, 255 );
    static const xcolor RETICLE_TARGET_COLOR          ( 125,   0,   0, 229 );
    static const xcolor MUTATION_RETICLE_COLOR        ( 255,  55,  66, 255 );
    static const xcolor MUTATION_RETICLE_TARGET_COLOR ( 255, 255,  66, 255 );

    static const f32 ZOOM_CROSS_INNER_OFFSET = 3.0f;
    static const f32 ZOOM_CROSS_OUTER_OFFSET = 6.0f;
    static const f32 RETICLE_SPREAD_RANGE     = 40.0f;
    static const f32 MUTATION_PIECE_SIZE      = 32.0f;

    struct reticle_piece_layout
    {
        vector2 Center;
        radian  Rotation;
    };

    void DrawReticlePiece( const texture& Texture,
                           const vector2& Center,
                           const vector2& Size,
                           const vector2& UV0,
                           const vector2& UV1,
                           const xcolor&  Color,
                           radian         Rotation,
                           ui_blend_mode  Blend = UI_BLEND_ALPHA )
    {
        const vector2 TopLeft = Center - Size * 0.5f;

        g_UIRenderer.DrawImage( Texture,
                                TopLeft,
                                Size,
                                UV0,
                                UV1,
                                Color,
                                Rotation,
                                Blend,
                                UI_SAMPLER_LINEAR_CLAMP );
    }

    void DrawZoomCross( const vector2& Center, const xcolor& Color )
    {
        g_UIRenderer.DrawLine( Center + vector2( -ZOOM_CROSS_INNER_OFFSET, 0.0f ),
                               Center + vector2( -ZOOM_CROSS_OUTER_OFFSET, 0.0f ), Color );
        g_UIRenderer.DrawLine( Center + vector2(  ZOOM_CROSS_INNER_OFFSET, 0.0f ),
                               Center + vector2(  ZOOM_CROSS_OUTER_OFFSET, 0.0f ), Color );
        g_UIRenderer.DrawLine( Center + vector2( 0.0f, -ZOOM_CROSS_INNER_OFFSET ),
                               Center + vector2( 0.0f, -ZOOM_CROSS_OUTER_OFFSET ), Color );
        g_UIRenderer.DrawLine( Center + vector2( 0.0f,  ZOOM_CROSS_INNER_OFFSET ),
                               Center + vector2( 0.0f,  ZOOM_CROSS_OUTER_OFFSET ), Color );
    }
}

//==============================================================================
// FUNCTIONS
//==============================================================================

hud_reticle::hud_reticle( void )
{
    m_WeaponGuid            = NULL_GUID;
    m_AimDegradation        = 0.0f;
    m_ShouldRender          = FALSE;
    m_IsZoomed              = FALSE;
    m_UsesMutationReticle   = FALSE;
    m_UsesCardinalEdges     = FALSE;
    m_HasLiveEnemyTarget    = FALSE;

    m_MutationReticle.SetName( PRELOAD_FILE("HUD/GENERAL/HUD_RETICLE_MUTATION.XBMP") );
}

//==============================================================================

void hud_reticle::OnAdvanceSimulation( player* pPlayer, f32 DeltaTime )
{
    (void)DeltaTime;

    m_WeaponGuid          = NULL_GUID;
    m_AimDegradation      = 0.0f;
    m_ShouldRender        = FALSE;
    m_IsZoomed            = FALSE;
    m_UsesMutationReticle = FALSE;
    m_UsesCardinalEdges   = FALSE;
    m_HasLiveEnemyTarget  = FALSE;

    if( !pPlayer )
    {
        return;
    }

    new_weapon* pWeapon = pPlayer->GetCurrentWeaponPtr();
    if( !pWeapon || !pWeapon->ShouldDrawReticle() )
    {
        return;
    }

    m_WeaponGuid          = pWeapon->GetGuid();
    m_AimDegradation      = pPlayer->GetAimDegradation();
    m_ShouldRender        = TRUE;
    m_IsZoomed            = pWeapon->IsZoomEnabled();
    m_UsesMutationReticle = (pPlayer->GetCurrentWeapon2() == INVEN_WEAPON_MUTATION);
    m_UsesCardinalEdges   = (pPlayer->GetCurrentWeapon2() == INVEN_WEAPON_SMP);

    object* pTarget = g_ObjMgr.GetObjectByGuid( pPlayer->GetEnemyOnReticle() );
    if( pTarget && pTarget->IsKindOf( actor::GetRTTI() ) )
    {
        m_HasLiveEnemyTarget = !actor::GetSafeType( *pTarget ).IsDead();
    }
}

//==============================================================================

void hud_reticle::OnRender( player* pPlayer )
{
    (void)pPlayer;

    if( !m_ShouldRender )
    {
        return;
    }

    object* pWeaponObject = g_ObjMgr.GetObjectByGuid( m_WeaponGuid );
    if( !pWeaponObject || !pWeaponObject->IsKindOf( new_weapon::GetRTTI() ) )
    {
        return;
    }

    new_weapon& Weapon = new_weapon::GetSafeType( *pWeaponObject );
    const vector2 Center( m_XPos, m_YPos );
    const vector2 UV0( 0.0f, 0.0f );
    const vector2 UV1( 1.0f, 1.0f );
    const xcolor ReticleColor = m_HasLiveEnemyTarget
                              ? RETICLE_TARGET_COLOR
                              : RETICLE_COLOR;

    if( m_IsZoomed )
    {
        DrawZoomCross( Center, ReticleColor );
        return;
    }

    g_UIRenderer.DrawPoint( Center, ReticleColor );

    xcolor PieceColor = ReticleColor;
    if( m_bPulsing )
    {
        PieceColor.A = (u8)(((f32)PieceColor.A / 255.0f) * hud_object::m_PulseAlpha);
    }

    vector2 CenterReticleSize( 0.0f, 0.0f );
    texture* pCenterReticle = Weapon.GetCenterReticleTexture();
    if( pCenterReticle )
    {
        const xbitmap& Bitmap = pCenterReticle->m_bitmap;
        CenterReticleSize.Set( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() );

        g_UIRenderer.DrawImage( *pCenterReticle,
                                Center - CenterReticleSize * 0.5f,
                                CenterReticleSize,
                                UV0,
                                UV1,
                                PieceColor,
                                0.0f,
                                UI_BLEND_ALPHA,
                                UI_SAMPLER_POINT_CLAMP );
    }

    if( m_UsesMutationReticle )
    {
        texture* pMutationReticle = m_MutationReticle.GetPointer();
        if( !pMutationReticle )
        {
            return;
        }

        const vector2 MutationSize( MUTATION_PIECE_SIZE, MUTATION_PIECE_SIZE );
        const vector2 HalfMutationSize = MutationSize * 0.5f;
        const vector2 UV2( 1.0f, 0.0f );
        const vector2 UV3( 0.0f, 1.0f );
        const xcolor MutationColor = m_HasLiveEnemyTarget
                                   ? MUTATION_RETICLE_TARGET_COLOR
                                   : MUTATION_RETICLE_COLOR;

        DrawReticlePiece( *pMutationReticle, Center + vector2( -HalfMutationSize.X, -HalfMutationSize.Y ),
                          MutationSize, UV0, UV1, MutationColor, R_0, UI_BLEND_ADDITIVE );
        DrawReticlePiece( *pMutationReticle, Center + vector2( -HalfMutationSize.X,  HalfMutationSize.Y ),
                          MutationSize, UV3, UV2, MutationColor, R_0, UI_BLEND_ADDITIVE );
        DrawReticlePiece( *pMutationReticle, Center + vector2(  HalfMutationSize.X, -HalfMutationSize.Y ),
                          MutationSize, UV2, UV3, MutationColor, R_0, UI_BLEND_ADDITIVE );
        DrawReticlePiece( *pMutationReticle, Center + vector2(  HalfMutationSize.X,  HalfMutationSize.Y ),
                          MutationSize, UV1, UV0, MutationColor, R_0, UI_BLEND_ADDITIVE );
        return;
    }

    texture* pEdgeReticle = Weapon.GetEdgeReticleTexture();
    if( !pEdgeReticle )
    {
        return;
    }

    const xbitmap& EdgeBitmap = pEdgeReticle->m_bitmap;
    const vector2 EdgeSize( (f32)EdgeBitmap.GetWidth(), (f32)EdgeBitmap.GetHeight() );
    const f32 CenterPixelOffset = Weapon.GetCenterPixelOffset();
    const f32 Spread = RETICLE_SPREAD_RANGE * m_AimDegradation;
    const f32 MainHalfWidth  = CenterReticleSize.X * 0.5f;
    const f32 MainHalfHeight = CenterReticleSize.Y * 0.5f;
    reticle_piece_layout Pieces[4];

    if( m_UsesCardinalEdges )
    {
        const f32 Radius = MainHalfWidth + CenterPixelOffset + Spread;
        Pieces[0].Center   = Center + vector2(  Radius, 0.0f );
        Pieces[0].Rotation = RADIAN( 270 );
        Pieces[1].Center   = Center + vector2( 0.0f, -Radius );
        Pieces[1].Rotation = RADIAN(   0 );
        Pieces[2].Center   = Center + vector2( -Radius, 0.0f );
        Pieces[2].Rotation = RADIAN(  90 );
        Pieces[3].Center   = Center + vector2( 0.0f,  Radius );
        Pieces[3].Rotation = RADIAN( 180 );
    }
    else
    {
        const f32 RadiusX = MainHalfWidth  * 0.5f + CenterPixelOffset + Spread;
        const f32 RadiusY = MainHalfHeight * 0.5f + CenterPixelOffset + Spread;
        Pieces[0].Center   = Center + vector2(  RadiusX, -RadiusY );
        Pieces[0].Rotation = RADIAN( 315 );
        Pieces[1].Center   = Center + vector2(  RadiusX,  RadiusY );
        Pieces[1].Rotation = RADIAN( 225 );
        Pieces[2].Center   = Center + vector2( -RadiusX,  RadiusY );
        Pieces[2].Rotation = RADIAN( 135 );
        Pieces[3].Center   = Center + vector2( -RadiusX, -RadiusY );
        Pieces[3].Rotation = RADIAN(  45 );
    }

    for( s32 i = 0; i < 4; i++ )
    {
        DrawReticlePiece( *pEdgeReticle,
                          Pieces[i].Center,
                          EdgeSize,
                          UV0,
                          UV1,
                          PieceColor,
                          Pieces[i].Rotation );
    }
}

//==============================================================================

xbool hud_reticle::OnProperty( prop_query& rPropQuery )
{
    (void)rPropQuery;
    return FALSE;
}

//==============================================================================

void hud_reticle::OnEnumProp( prop_enum&  List )
{
    (void)List;
}
