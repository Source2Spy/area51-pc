//=========================================================================

//
//  PlayerRender.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "Render/PrimitiveDebug.hpp"
#include "Render/RenderScope.hpp"
#include "GameLib/RenderContext.hpp"
#include "Objects/HudObject.hpp"
#include "Objects/LoreObject.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "UI/ui_renderer.hpp"
#include "StringMgr/StringMgr.hpp"
#include "e_Audio.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

extern f32 g_SpawnFadeTime;

#if defined( X_EDITOR )
xbool g_ShowPlayerPos = TRUE;
#else
xbool g_ShowPlayerPos = FALSE;
#endif

extern xbool g_ShowLoreObjectCollision;
extern f32   g_LO_SphereSize;
extern f32   g_LO_RenderDist;
extern f32   g_Dist;

#ifdef DEBUG_GRENADE_THROWING
extern xbool   g_ShowGrenadeEventCollision;
extern vector3 g_EventPos;
extern vector3 g_NewEventPos;
#endif

void player::OnRenderTransparent(void)
{
    actor::OnRenderTransparent();

    if( render::GetActiveRenderScopeDesc().Purpose == render::RENDER_VIEW_SCENE_CAPTURE && !IsAvatar() )
        return;

#ifndef X_EDITOR
    if ( m_CurrentAnimState == ANIM_STATE_MISSION_FAILED )
    {

        if( g_AudioMgr.GetLanguage() == XL_LANG_ENGLISH )
        {
            texture* pTexture = m_MissionFailedBmp.GetPointer();
            if( pTexture )
            {
                const xbitmap& Bitmap = pTexture->m_bitmap;
                f32 X = f32( 256 - (Bitmap.GetWidth()/2) );
                f32 Y = 100.0f;
                g_UIRenderer.DrawImage( *pTexture,
                                        vector2( X, Y ),
                                        vector2( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() ),
                                        vector2( 0.0f, 0.0f ),
                                        vector2( 1.0f, 1.0f ),
                                        g_HudColor,
                                        0.0f,
                                        UI_BLEND_ALPHA,
                                        UI_SAMPLER_LINEAR_CLAMP );
            }
        }
        else
        {
            // display a localized text message instead of the bitmap            
            irect Rect( 0, 100, 512, 130 );
            RenderLine( g_StringTableMgr( "ui", "IDS_MISSION_FAILED" ), Rect, 255, g_HudColor, 0, ui_font::h_center | ui_font::v_top );
        }

        const s32 mfx = 0;
        const s32 mfy = 180;
        xcolor Color( XCOLOR_RED );
        irect Rect;
        Rect.Set( mfx, mfy + 50, 512, mfy+51 );
        Color.Set( XCOLOR_YELLOW );
        RenderLine( g_StringTableMgr( g_StringMgr.GetString( m_MissionFailedTableName ), g_StringMgr.GetString( m_MissionFailedReasonName ) ), Rect, 255, Color, 0, ui_font::h_center | ui_font::v_top );
    }
#endif

#ifndef X_RETAIL
    if( g_ShowLoreObjectCollision )
    {
        vector3 StartPos, EndPos;
        s32 i = 0;

        for( i = 0; i < MAX_LORE_ITEMS; i++ )
        {
            lore_object* pLoreObject = (lore_object*)g_ObjMgr.GetObjectByGuid(m_LoreObjectGuids[i]);

            if( !pLoreObject ) continue;

            pLoreObject->DoCollisionCheck(this, StartPos, EndPos);

            vector3 Diff = EndPos-StartPos;
            g_Dist = Diff.Length();

            // only render the debug stuff for the one that is close to us
            if( g_Dist < g_LO_RenderDist )
            {
                // default modifier to full distance in case the collision manager returns no collisions
                f32 DistModifier = 1.0f;

                // if we don't hit anything, T is undefined
                if( g_CollisionMgr.m_nCollisions > 0 )
                {
                    DistModifier = g_CollisionMgr.m_Collisions[0].T;

                    object *pHitObj = g_ObjMgr.GetObjectByGuid(g_CollisionMgr.m_Collisions[0].ObjectHitGuid);

                    if( pHitObj )
                    {
                        render::debug::Line(StartPos, pHitObj->GetPosition());
                        render::debug::Sphere(pHitObj->GetPosition(), g_LO_SphereSize, XCOLOR_GREEN);
                    }
                }

                // get our new end position
                EndPos = StartPos + (DistModifier*Diff);

                //render::debug::Sphere(Pos, 10.0f, XCOLOR_WHITE);
                render::debug::Line(StartPos, EndPos);

                if( g_CollisionMgr.m_nCollisions )
                {
                    render::debug::Sphere(EndPos, g_LO_SphereSize, XCOLOR_RED);
                }
                else
                {
                    // we can see it
                    render::debug::Sphere(EndPos, g_LO_SphereSize, XCOLOR_WHITE);
                }

                pLoreObject->OnColRender( TRUE );
            }
        }
    }

#ifdef DEBUG_GRENADE_THROWING
    if( g_ShowGrenadeEventCollision )
    {
        // get player position
        vector3 Point1 = vector3(0.0f, 0.0f, 0.0f);
        // not set yet
        vector3 Point2 = vector3(0.0f, 0.0f, 0.0f);    
        // get event position
        vector3 Point3 = g_EventPos;

        GetThrowPoints( Point1, Point2, Point3 );
        render::debug::Sphere(Point1, 5.0f);
        render::debug::Sphere(Point2, 6.0f, XCOLOR_GREEN);
        render::debug::Sphere(Point3, 7.0f, XCOLOR_YELLOW);
        render::debug::Sphere(g_NewEventPos, 8.0f, XCOLOR_RED);
    }
#endif // DEBUG_GRENADE_THROWING

#endif // X_RETAIL

    //render weapon
    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon )
    {
        // set the proper render state so we can refrain from drawing the 1st person muzzle fx
        if( IsAvatar() && pWeapon->IsUsingSplitScreen() )
        {
            pWeapon->SetRenderState( new_weapon::RENDER_STATE_NPC );
        }

        pWeapon->OnRenderTransparent();

        // put renderstate back like it was
        pWeapon->SetRenderState( new_weapon::RENDER_STATE_PLAYER );
    }
}

//=============================================================================

void player::UpdateFirstPersonArmsVisibility( void )
{
    const xbool IsEnteringMutation = (m_CurrentAnimState == ANIM_STATE_SWITCH_TO)
                                 && (m_PrevWeaponItem != INVEN_WEAPON_MUTATION);
    const xbool IsMutationAnimation = (m_CurrentAnimState == ANIM_STATE_CHANGE_MUTATION)
                                  && (m_AnimStage < 3);
    const xbool ShowHumanHands = (m_CurrentWeaponItem != INVEN_WEAPON_MUTATION)
                             || IsEnteringMutation || IsMutationAnimation;
    const xbool HasMutation = m_Inventory2.HasItem( INVEN_WEAPON_MUTATION );

    m_Skin.SetVMeshBit( "MESH_Arms_Hazmat",  TRUE );
    m_Skin.SetVMeshBit( "MESH_Hands_Bare",   ShowHumanHands && HasMutation );
    m_Skin.SetVMeshBit( "MESH_Hands_Hazmat", ShowHumanHands && !HasMutation );
}

//=============================================================================

void player::OnRender( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "player::OnRender" );

    if( render::GetActiveRenderScopeDesc().Purpose == render::RENDER_VIEW_SCENE_CAPTURE && !IsAvatar() )
    {
        actor::OnRender();
        return;
    }

    //
    // Make sure 1st person/3rd person weapon is in correct position for rendering!
    // This chunk of code makes sure that the weapon is in the right place for the
    // weapon pullback calculations, preventing it from being in an avatar position,
    // when it's supposed to be in the player rig's hands
    //
#ifdef X_EDITOR
    const xbool bIsSplitScreen = FALSE;
#else
    const xbool bIsSplitScreen = (g_NetworkMgr.GetLocalPlayerCount() > 1);
#endif
    if( bIsSplitScreen )
    {
        if( IsAvatar() )
        {
            // this will move the weapon into the avatar's hands
            actor::MoveWeapon( TRUE );  // 3rd person
        }
        else
        {
            // this will move the weapon into the player rig hands
            OnMoveWeapon(); // 1st person
        }
    }

    UpdateWeaponPullback();

#ifndef X_EDITOR
    // If the rendering isn't short circuited then dead players will
    // still be rendered in split screen as their corpse falls out of them.
    if( IsDead() && (g_RenderContext.NetPlayerSlot != m_NetSlot) )
    {
        return;
    }
#endif

    if( m_DeathCamera.IsActive() && IsDead() )
    {
        return;
    }

    UpdateFirstPersonArmsVisibility();

#if defined(X_EDITOR)
    if ( g_ShowPlayerPos )
    {
        vector3 Pos = GetPosition();
        x_printfxy( 1, 2, "Player( %7.1f, %7.1f, %7.1f )", Pos.GetX(), Pos.GetY(), Pos.GetZ() );
    }
#endif

    // We need to render debug stuff at least, if sniper zoom is enabled
    RenderAimAssistDebugInfo();

    if( RenderSniperZoom()   ||                 // if we're in sniper mode, don't render player arms and such
        IsCinemaRunning() ||                    // if we are playing a cinematic, don't draw arms
        (m_bHidePlayerArms && !IsAvatar()) )    // Has a trigger or something turned off our arms?
    {
        // KSS -- FIXME -- HACK -- This will cause sniper zoom on moving platforms to now work.
        // PREVIOUSLY, you would get locked in and were not able to YAW at all.
        const matrix4& mat = GetL2W();
        (void)mat;

        return;
    }

    if( !IsAvatar() )
    {
        // gather flags and ambient color
        xcolor Ambient;
        u32    Flags = (GetFlagBits() & object::FLAG_CHECK_PLANES) ? render::CLIPPED : 0;
        if ( g_RenderContext.m_bIsMutated && render::GetActiveRenderScopeDesc().EnablePostProcessing && m_bAllowedToGlow )
        {
            Flags  |= render::GLOWING;

            // TODO: Fill in the logic for determining if this is friend or foe.

            // TODO: This color should come from the blueprint properties (m_EnemyGlowColor)
            Ambient = xcolor(255,200,200,255);
        }
        else
        {
            Ambient = GetFloorColor();
        }


#ifdef X_EDITOR
        if( m_bRenderBBox )
        {
            if( GetAttrBits() & ATTR_EDITOR_SELECTED )
            {
                render::debug::Box( GetBBox(), XCOLOR_RED );
                render::debug::Frustum( GetRenderView() );
            }
        }
#endif // X_EDITOR

        if ( !m_bActivePlayer )
            return;

        if( m_LocalSlot == -1 )
            return;

#if defined(X_EDITOR)
        const view* ActiveView = eng_GetView();
        if ( ActiveView && ((ActiveView->GetPosition() - GetRenderView().GetPosition()).LengthSquared() > 0.5f) )
        {
           return;
        }
#endif // X_EDITOR

        void* pPtr1 = m_AnimGroup.GetPointer();
        void* pPtr2 = m_Skin.GetSkinGeom();

        // Don't render the player arms if he doesn't have a weapon
        // GaryW -> Commented out GetCurrentWeaponPtr() because it was
        // causing a bug where the player was unable to turn while standing
        // on an Anim Surface.  Bug was added to the BugBase to have the
        // appropriate person find a resultion to this problem.
        if(    pPtr1 
            && pPtr2 
            && (GetCurrentWeaponPtr() || (m_CurrentAnimState == ANIM_STATE_DEATH))
            && !(m_bIsMutated && (m_CurrentAnimState == ANIM_STATE_DEATH)) )
        {
            s32            nBones    = m_AnimPlayer.GetNBones();
            matrix4*       pBone     = (matrix4*)smem_BufferAlloc( nBones * sizeof( matrix4 ) );
            const matrix4* pAnimBone = m_AnimPlayer.GetBoneL2Ws();
            const vector3& WeaponCollisionOffset = GetCurrentWeaponCollisionOffset();
            for( s32 i=0; i<nBones; i++ )
            {
                pBone[i] = pAnimBone[i];

                pBone[i].Translate( WeaponCollisionOffset );
            }

#if !defined( CONFIG_RETAIL )
            if( m_bRenderSkeleton )
            {
                m_AnimPlayer.RenderSkeleton( m_bRenderSkeletonNames );
            }
#endif // !defined( CONFIG_RETAIL )

            // Handle fade-in on spawn
            if( m_SpawnFadeTime > 0.0f )
            {
                Flags |= render::FADING_ALPHA;
                f32 Alpha = 1.0f - (m_SpawnFadeTime / g_SpawnFadeTime);
                Alpha = MIN( Alpha, 1.0f );
                Alpha = MAX( Alpha, 0.0f );
                Ambient.A  = (u8)(Alpha*255.0f);
            }

            skin_inst& SkinInst = m_Skin;
            SkinInst.Render( &GetL2W(),
                            pBone, 
                            nBones, 
                            Flags | render::CLIPPED | render::DISABLE_PROJ_LIGHT | render::FIRST_PERSON, 
                            SkinInst.GetLODMask(GetL2W()),
                            Ambient );
        }
        else
        if( !GetCurrentWeaponPtr() )
        {
            // KSS -- FIXME -- HACK -- This will cause no weapon on moving platforms to now work.
            // PREVIOUSLY, you would get locked in and were not able to YAW at all.
            const matrix4& mat = GetL2W();
            (void)mat;
        }

        if ( m_CurrentAnimState == ANIM_STATE_CHANGE_MUTATION && ( m_AnimStage > 1 ) && ( m_AnimStage < 3 ) ) //stage 1 is the switch from
        {
            //special case
            return;
        }

        //render weapon
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if ( pWeapon )
        {
            pWeapon->SetRenderState( new_weapon::RENDER_STATE_PLAYER );
            //AttachWeapon();
            pWeapon->RenderWeapon( TRUE, Ambient, FALSE );
        }
    }
    else
    {
        actor::OnRender();
        new_weapon* pWeapon = GetCurrentWeaponPtr();
        if( pWeapon )
            pWeapon->SetRenderState( new_weapon::RENDER_STATE_PLAYER );
    }

}

void player::OnRenderShadowCast( u64 ProjMask )
{
    if( IsAvatar() )
    {
        actor::OnRenderShadowCast( ProjMask );
    }
}

xbool player::RenderSniperZoom( void )
{
    return (m_CurrentWeaponItem == INVEN_WEAPON_SNIPER_RIFLE)
        && m_bActivePlayer
        && (   (m_CurrentAnimState == ANIM_STATE_ZOOM_IDLE)
            || (m_CurrentAnimState == ANIM_STATE_ZOOM_RUN)
            || (m_CurrentAnimState == ANIM_STATE_ZOOM_FIRE));
}

#ifdef X_EDITOR

void player::OnEditorRender( void )
{
    const view* ActiveView = eng_GetView();

    if ( ActiveView && ((ActiveView->GetPosition() - GetRenderView().GetPosition()).LengthSquared() > 0.5f) )
    {
        //
        // Draw the player orientation axes
        //
        vector3 Z           ( 0.0f,    0.0f,   150.0f  );
        vector3 EyesPosition( GetPosition() + vector3( 0.0f, 172.5f, 0.0f ) + m_EyesOffset );
        matrix4 L2W;
        L2W.Identity();
        L2W.Rotate( radian3( GetPitch(), GetYaw(), 0.0f ) );
        L2W.Translate( EyesPosition );
        Z = L2W.Transform( Z );

        const render::primitive_draw_desc Material( NULL,
                                                    render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                    render::PRIMITIVE_BLEND_OPAQUE,
                                                    render::PRIMITIVE_DEPTH_READ_WRITE,
                                                    render::PRIMITIVE_RASTER_SOLID,
                                                    render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);
        render::PrimitiveBatch Batch( Material );
        // draw a cone
        // draw each vertex in the z plane then transform it
        static const s32    nVertex    = 25;
        Batch.Reserve( nVertex * 6, nVertex * 6 );
        static const f32    Radius  = 10.0f;
        static const radian Step    = R_360 / nVertex;
        radian              Angle   = Step;
        s32                 i;
        const vector3       Center      ( L2W.Transform( vector3( 0.0f, Radius, 0.0f ) ) );
        vector3             LastVertex  ( Center );

        for ( i = 0; i < nVertex; ++i )
        {
            vector3 Vertex( 0.0f, Radius, 0.0f );
            Vertex.RotateZ( Angle );
            Vertex = L2W.Transform( Vertex );

            plane Plane( LastVertex, Vertex, Z );
            s32 ColorShade = (s32)((Plane.Normal.GetY() + 1.0f) * 127);
            const xcolor OuterColor( 0, (u8)ColorShade, (u8)ColorShade );
            Batch.AddTriangle( render::primitive_vertex( LastVertex, vector2( 0.0f, 0.0f ), OuterColor ),
                               render::primitive_vertex( Vertex, vector2( 0.0f, 0.0f ), OuterColor ),
                               render::primitive_vertex( Z, vector2( 0.0f, 0.0f ), OuterColor ) );

            Plane.Setup( Vertex, LastVertex, Center );
            ColorShade = (s32)((Plane.Normal.GetY() + 1.0f) * 127);
            const xcolor InnerColor( 0, (u8)ColorShade, (u8)ColorShade );
            Batch.AddTriangle( render::primitive_vertex( Vertex, vector2( 0.0f, 0.0f ), InnerColor ),
                               render::primitive_vertex( LastVertex, vector2( 0.0f, 0.0f ), InnerColor ),
                               render::primitive_vertex( Center, vector2( 0.0f, 0.0f ), InnerColor ) );

            LastVertex = Vertex;
            Angle += Step;
        }

        matrix4 Identity;
        Identity.Identity();
        Batch.Submit( Identity );
    }
}

#endif // X_EDITOR

const vector3& player::GetCurrentWeaponCollisionOffset( void ) const
{
    return m_WeaponCollisionOffset;
}

#ifndef X_RETAIL

void player::OnColRender( xbool bRenderHigh )
{    
    (void)bRenderHigh;
    m_Physics.RenderCollision();
} 

#endif // X_RETAIL
