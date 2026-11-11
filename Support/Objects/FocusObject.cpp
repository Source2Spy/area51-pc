//==============================================================================
// focus OBJECT
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "FocusObject.hpp"
#include "e_View.hpp"
#include "Entropy.hpp"
#include "x_math.hpp"
#include "../Support/TriggerEx/TriggerEx_Object.hpp"

#include "InputMgr/GamePad.hpp"
#include "Objects/HudObject.hpp"
#include "UI/ui_renderer.hpp"

#include "GameTextMgr/GameTextMgr.hpp"
#include "StringMgr/StringMgr.hpp"

#ifndef X_EDITOR
#include "GameLib/RenderContext.hpp"
#include "NetworkMgr/Messages/MsgMgr.hpp"
#endif

//=========================================================================
// GLOBALS
//=========================================================================

#ifndef CONFIG_RETAIL 
xbool g_bDrawFODebug = FALSE;
#endif

f32 g_FO_OffsetY = 5.0f;

rhandle<texture>            focus_object::m_Bracket;

//=========================================================================
// OBJECT DESCRIPTION
//=========================================================================

//=========================================================================
static struct focus_object_desc : public object_desc
{
    focus_object_desc( void ) : object_desc( 
            object::TYPE_FOCUS_OBJECT, 
            "Focus Object",
            "HUD",
            object::ATTR_DRAW_2D             |
            object::ATTR_RENDERABLE          |
            object::ATTR_NEEDS_LOGIC_TIME,

            FLAGS_GENERIC_EDITOR_CREATE | 
            FLAGS_IS_DYNAMIC            |
            FLAGS_TARGETS_OBJS          ) {}

    //---------------------------------------------------------------------

    virtual object* Create( void )
    {
        return new focus_object;
    }

    //---------------------------------------------------------------------

#ifdef X_EDITOR

    virtual s32 OnEditorRender( object& Object ) const
    { 
        focus_object FO = focus_object::GetSafeType( Object );
        {             
            bbox BBox = FO.m_PosState.m_FocusBox;
            BBox.Translate( Object.GetPosition() );
            render::debug::Box( BBox, XCOLOR_GREEN );
        }
        {   
            bbox BBox = FO.m_NegState.m_FocusBox;
            BBox.Translate( Object.GetPosition() );
            render::debug::Box( BBox, XCOLOR_RED );
        }
        render::debug::Marker( Object.GetPosition() );

        object_desc::OnEditorRender( Object );
        return static_cast<s32>( EditorIcon::FocusObject );
    }

#endif // X_EDITOR

} s_focusObject_Desc;

//=========================================================================

const object_desc&  focus_object::GetTypeDesc( void ) const
{
    return s_focusObject_Desc;
}

//=========================================================================

const object_desc&  focus_object::GetObjectType( void )
{
    return s_focusObject_Desc;
}


//=========================================================================
// FUNCTIONS
//=========================================================================

//=========================================================================

inline xcolor Interpolate( xcolor Color1, xcolor Color2, f32 Percentage )
{
    xcolor TmpColor( 
        (u8)(Color2.R * Percentage + Color1.R * (1.0f - Percentage)),
        (u8)(Color2.G * Percentage + Color1.G * (1.0f - Percentage)),
        (u8)(Color2.B * Percentage + Color1.B * (1.0f - Percentage)),
        (u8)(Color2.A * Percentage + Color1.A * (1.0f - Percentage))
        );
    return TmpColor;
}
//=========================================================================

focus_object::focus_object( void ) 
{
    // Set the initial state.
    m_State     = 0;
    m_bActive   = TRUE;
    m_bActivationChanged = FALSE;
    m_ActivationTickDelay = 0;

    m_pCurrState = &m_PosState;

    // Positive state stuff.
    {
        m_PosState.m_TriggerGuid   = 0;
        m_PosState.m_SpatialGuid   = 0;

        m_PosState.m_ViewDist      = 800;
        m_PosState.m_TextDist      = 500;
        m_PosState.m_InteractDist  = 200;
        m_PosState.m_bInteractLOS  = TRUE;

        m_PosState.m_InteractStrTable = -1;
        m_PosState.m_InteractStrTitle = -1;

        m_PosState.m_DisplayStrTable  = -1;
        m_PosState.m_DisplayStrTitle  = -1;

        m_PosState.m_pInteractAudio = "FocusObject_Positive";

        m_PosState.m_Color1             = xcolor( 0,   128, 0, 200 );
        m_PosState.m_Color2             = xcolor( 0,   240, 0, 200 );
        m_PosState.m_SelectedColor1     = xcolor( 0,   255, 0, 200 );
        m_PosState.m_SelectedColor2     = xcolor( 255, 255, 0, 200 );

        vector3 Corner( 50.0f, 50.0f, 50.0f );
        m_PosState.m_FocusBox.Set( Corner, -Corner );

        m_PosState.m_ViewType = VIEW_CIRCLE;
    }

    // Negative state stuff.
    {
        m_NegState.m_TriggerGuid   = 0;
        m_NegState.m_SpatialGuid   = 0;

        m_NegState.m_ViewDist      = 800;
        m_NegState.m_TextDist      = 500;
        m_NegState.m_InteractDist  = 200;
        m_NegState.m_bInteractLOS  = TRUE;

        m_NegState.m_InteractStrTable = -1;
        m_NegState.m_InteractStrTitle = -1;

        m_NegState.m_DisplayStrTable  = -1;
        m_NegState.m_DisplayStrTitle  = -1;

        m_NegState.m_pInteractAudio = "FocusObject_Negative";

        m_NegState.m_Color1             = xcolor( 180, 0, 0, 200 );
        m_NegState.m_Color2             = xcolor( 180, 0, 0, 200 );
        m_NegState.m_SelectedColor1     = xcolor( 180, 0, 0, 200 );
        m_NegState.m_SelectedColor2     = xcolor( 180, 0, 0, 200 );

        vector3 Corner( 50.0f, 50.0f, 50.0f );
        m_NegState.m_FocusBox.Set( Corner, -Corner );

        m_NegState.m_ViewType = VIEW_CIRCLE;
    }

    // Internal state stuff.
    m_AnimState         = 1.0f;
    m_TextAlphaState    = 0.0f;
    m_bLookingAt        = FALSE;
    m_bHasLOS           = FALSE;
    m_bInViewRange      = FALSE;
    m_bInTextRange      = FALSE;
    m_bInInteractRange  = FALSE;
    m_bStandingIn       = FALSE;

    m_ColorPhase        = 0.0f;
    m_MaxAlpha          = 0;

    m_Bracket.SetName( PRELOAD_FILE("Bracket.xbmp") );

    m_CircleFocusObj[0].BMP.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_LORERETICLE_1.XBMP"));
    m_CircleFocusObj[1].BMP.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_LORERETICLE_2.XBMP"));
    m_CircleFocusObj[2].BMP.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_LORERETICLE_3.XBMP"));

    m_CircleFocusObj[0].Rotation = 0.0f;
    m_CircleFocusObj[1].Rotation = 0.0f;
    m_CircleFocusObj[2].Rotation = 0.0f;


}

//=========================================================================

focus_object::~focus_object( void ) 
{
}

//=========================================================================


#ifndef X_RETAIL
void focus_object::OnDebugRender( void )
{

}
#endif // X_RETAIL

//=========================================================================

void focus_object::OnRender( void )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "focus_object::OnRender" );

    if( !m_bActive || (m_AnimState == 1.0f) )
    {
        return;
    }
    bbox BBox = m_pCurrState->m_FocusBox;
    player* pPlayer = SMP_UTIL_GetActivePlayer();
    if( !pPlayer )
        return;

    const view& rView = pPlayer->GetRenderView();
    const vector3 RenderPosition = GetL2W().GetTranslation();
    f32 DistToBox = (RenderPosition - rView.GetPosition()).Length();

    // Get the longest dimension of the bbox.
    f32 LongestDim = 1;
    LongestDim = x_max( LongestDim, x_abs( BBox.Max.GetX() - BBox.Min.GetX() ) );
    LongestDim = x_max( LongestDim, x_abs( BBox.Max.GetY() - BBox.Min.GetY() ) );
    LongestDim = x_max( LongestDim, x_abs( BBox.Max.GetZ() - BBox.Min.GetZ() ) );

    // If the ratio of the distance from the eye to the object is less 
    // than 2x the longest dim, start scaling.
    f32 ScaleFactor = x_min( 1.0f, DistToBox / (LongestDim * 2.0f) ); 

    vector3 scale( ScaleFactor, ScaleFactor, ScaleFactor );
    radian3 rotation(0.0f, 0.0f, 0.0f);
    vector3 translation = RenderPosition;
    BBox.Transform( matrix4( scale, rotation, translation ) );

    rect ViewDimensions;

    rView.GetViewport( ViewDimensions );

    // Draw the FO
    const vector3 CenterOnScreen = rView.PointToScreen( GetBBox().GetCenter() );
    if( CenterOnScreen.GetZ() <= 0.001f )
        return;

    const irect ScreenViewport( (s32)ViewDimensions.Min.X,
                                (s32)ViewDimensions.Min.Y,
                                (s32)ViewDimensions.Max.X,
                                (s32)ViewDimensions.Max.Y );
    const rect HudViewDimensions = g_UIRenderer.GetViewport().GetHudBounds( ScreenViewport );
    g_UIRenderer.PushHudSpace( ScreenViewport );

    //
    // Brackets.
    //
    switch( m_pCurrState->m_ViewType )
    {
    case VIEW_CIRCLE:
        {
            const vector2 CirclePosition = g_UIRenderer.GetViewport().ScreenToHud(
                vector2( CenterOnScreen.GetX(), CenterOnScreen.GetY() ),
                ScreenViewport );

            // scale the FO circle with the focus box's size on screen
            const f32 CircleSize = rView.CalcScreenSize( GetBBox().GetCenter(), LongestDim * 0.5f ) /
                                   g_UIRenderer.GetViewport().GetHudScale( ScreenViewport );

            if( m_MaxAlpha > 0 )
            {
                // render all of the 3 circle pc of art to make up the FO
                for( s32 circle = 0 ; circle < NUM_CIRCLE_SECTIONS ; circle++ )
                {
                    texture* pTexture = NULL;

                    if( circle != CIRCLE_FLOATER ) // FLOATER not rendered
                        pTexture = m_CircleFocusObj[circle].BMP.GetPointer();

                    if( pTexture )
                    {
                        const xbitmap& Bitmap = pTexture->m_bitmap;

                        f32 H = f32( Bitmap.GetHeight() );
                        f32 W = f32( Bitmap.GetWidth () );

                        vector2 UV0(0,0);
                        vector2 UV1(1,1);  
                        vector2 WH (W,H);

                        WH.Scale( 0.5f * CircleSize / x_max( W, H ) );

                        xcolor FO_Color = m_pCurrState->m_SelectedColor2;

                        if( HasFocus() )
                            FO_Color = m_pCurrState->m_SelectedColor1;

                        // fade the FO circle using the same alpha as the brackets
                        FO_Color.A = x_min( m_MaxAlpha,
                                            pPlayer->IsMutated() ? m_pCurrState->m_SelectedColor1.A : (u8)255 );

                        g_UIRenderer.DrawImage( *pTexture,
                                                CirclePosition - WH * 0.5f,
                                                WH,
                                                UV0,
                                                UV1,
                                                FO_Color,
                                                DEG_TO_RAD(m_CircleFocusObj[circle].Rotation) );

                    }
                }
            }
        }
        break;

    case VIEW_BRACKETS:
        {
            vector3 P[8];

            P[0].GetX() = BBox.Min.GetX();    P[0].GetY() = BBox.Min.GetY();    P[0].GetZ() = BBox.Min.GetZ();
            P[1].GetX() = BBox.Min.GetX();    P[1].GetY() = BBox.Min.GetY();    P[1].GetZ() = BBox.Max.GetZ();
            P[2].GetX() = BBox.Min.GetX();    P[2].GetY() = BBox.Max.GetY();    P[2].GetZ() = BBox.Min.GetZ();
            P[3].GetX() = BBox.Min.GetX();    P[3].GetY() = BBox.Max.GetY();    P[3].GetZ() = BBox.Max.GetZ();
            P[4].GetX() = BBox.Max.GetX();    P[4].GetY() = BBox.Min.GetY();    P[4].GetZ() = BBox.Min.GetZ();
            P[5].GetX() = BBox.Max.GetX();    P[5].GetY() = BBox.Min.GetY();    P[5].GetZ() = BBox.Max.GetZ();
            P[6].GetX() = BBox.Max.GetX();    P[6].GetY() = BBox.Max.GetY();    P[6].GetZ() = BBox.Min.GetZ();
            P[7].GetX() = BBox.Max.GetX();    P[7].GetY() = BBox.Max.GetY();    P[7].GetZ() = BBox.Max.GetZ();

            f32 MaxX = -10000.0f;
            f32 MinX =  10000.0f;

            f32 MaxY = -10000.0f;
            f32 MinY =  10000.0f;

            s32 i;
            for( i = 0; i < 8; i++ )
            {
                vector3 P2D = rView.PointToScreen( P[i] );
                MaxX = x_max( MaxX, P2D.GetX() );
                MinX = x_min( MinX, P2D.GetX() );

                MaxY = x_max( MaxY, P2D.GetY() );
                MinY = x_min( MinY, P2D.GetY() );
            }

            const vector2 HudMin = g_UIRenderer.GetViewport().ScreenToHud(
                vector2( MinX, MinY ), ScreenViewport );
            const vector2 HudMax = g_UIRenderer.GetViewport().ScreenToHud(
                vector2( MaxX, MaxY ), ScreenViewport );
            MinX = HudMin.X;
            MinY = HudMin.Y;
            MaxX = HudMax.X;
            MaxY = HudMax.Y;

            vector3 P3D[4];
            P3D[ 0 ].Set( MinX, MinY, 0.0f );
            P3D[ 1 ].Set( MaxX, MinY, 0.0f );
            P3D[ 2 ].Set( MaxX, MaxY, 0.0f );
            P3D[ 3 ].Set( MinX, MaxY, 0.0f );
#ifndef CONFIG_RETAIL 
            if( g_bDrawFODebug )
            {
                g_UIRenderer.DrawRect( rect( MinX, MinY, MaxX, MaxY ), XCOLOR_GREEN, TRUE );

                render::debug::Marker( GetPosition(), XCOLOR_BLUE );
                s32 u;
                for( u = 0; u < 8; u++ )
                {
                    render::debug::Marker( P[ u ], XCOLOR_RED );
                }
            }
#endif

            vector3 UpOne   ( 0.0f, -1.0f, 0.0f );
            vector3 RightOne( 1.0f,  0.0f, 0.0f );

            /*
            Focus Objects:
            -If it is a bracket, make color always purple RGB(255,0,255).
            -If it is a circle, keep it the way it currently is.
            -These colors should not be editable in properties, and should override anything already set through the property system.
            */
            xcolor Color1 = xcolor(255, 0, 255, 200); // light purple
            xcolor Color2 = xcolor(175, 0, 175, 200); // dark purple

            xcolor RenderColor = Interpolate( Color1, Color2, 
                (m_ColorPhase > 1.0f) ? (2.0f - m_ColorPhase) : m_ColorPhase );

            RenderColor.A = x_min( RenderColor.A, m_MaxAlpha );

            vector2 TL( 0.0f, 0.0f );
            vector2 BR( 1.0f, 1.0f );

            f32 BracketSize = x_sqrt( x_min( MaxX - MinX, MaxY - MinY ) ) / 30.0f;
            f32 CornerOffset = 100.0f;

            texture* pTexture = m_Bracket.GetPointer();
            if( !pTexture )
                break;
            const xbitmap& Bitmap = pTexture->m_bitmap;
            vector2 WH( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() );

            WH.Scale( BracketSize );

            vector3 Displacement[ 4 ] = { P3D[ 0 ] + m_AnimState * CornerOffset * (-RightOne + UpOne),
                P3D[ 1 ] + m_AnimState * CornerOffset * (UpOne + RightOne),
                P3D[ 2 ] + m_AnimState * CornerOffset * (RightOne - UpOne),
                P3D[ 3 ] + m_AnimState * CornerOffset * (-UpOne - RightOne) };

            radian Rotation = R_0;

            for( s32 i = 0; i < 4; i++ )
            {
                // Check to make sure it's actually on the screen so we don't get any weird wrap around glitches.
                if( IN_RANGE( HudViewDimensions.Min.X - Bitmap.GetWidth(),
                              Displacement[i].GetX(),
                              HudViewDimensions.Max.X + Bitmap.GetWidth() ) &&
                    IN_RANGE( HudViewDimensions.Min.Y - Bitmap.GetHeight(),
                              Displacement[i].GetY(),
                              HudViewDimensions.Max.Y + Bitmap.GetHeight() ) )
                {
                    g_UIRenderer.DrawImage( *pTexture,
                                            vector2( Displacement[i].GetX(), Displacement[i].GetY() ),
                                            WH,
                                            TL,
                                            BR,
                                            RenderColor,
                                            Rotation );
                }

                Rotation -= R_90;
            }

            // Draw the Label.
#ifndef X_EDITOR
            if( (m_pCurrState->m_DisplayStrTitle >= 0) && (m_pCurrState->m_DisplayStrTable >= 0) )
            {
                irect LabelPos( (s32)P3D[ 0 ].GetX(), (s32)P3D[ 0 ].GetY(), (s32)P3D[ 1 ].GetX(), (s32)P3D[ 1 ].GetY() );
                xcolor White            = XCOLOR_WHITE;
                const xwchar* pDisplayStr     = 
                    g_StringTableMgr( g_StringMgr.GetString( m_pCurrState->m_DisplayStrTable ),
                    g_StringMgr.GetString( m_pCurrState->m_DisplayStrTitle ) );

#if !defined(CONFIG_RETAIL) | defined(CONFIG_QA)
                if ( x_wstrcmp( pDisplayStr, (const xwchar*)xwstring( "<null>" ) ) == 0 )
                {
                    White                  = XCOLOR_RED;
                    pDisplayStr = (const xwchar*)xwstring( "Focus Object text goes here." );
                }
#endif

                if( IN_RANGE( HudViewDimensions.Min.X - HudViewDimensions.GetWidth(),
                              LabelPos.GetCenter().X,
                              HudViewDimensions.Max.X + HudViewDimensions.GetWidth() ) &&
                    IN_RANGE( HudViewDimensions.Min.Y - HudViewDimensions.GetHeight(),
                              LabelPos.GetCenter().Y,
                              HudViewDimensions.Max.Y + HudViewDimensions.GetHeight() ) )
                {
                    RenderLine( pDisplayStr, LabelPos, (u8)(255 * m_TextAlphaState), White, 1, ui_font::h_center|ui_font::v_bottom, TRUE );
                }
            }

#endif
        }
        break;
    case VIEW_NONE:
        break;
    default:
        break;
    }

    g_UIRenderer.PopHudSpace();
}

//=========================================================================

void focus_object::OnAdvanceSimulation( f32 DeltaTime )
{
    // this will fix the activate on same frame and execute trigger bug
    // if it was activated, set the real flag and return, skipping a frame for sure
    if( m_bActivationChanged )
    {
        if( m_ActivationTickDelay < 3 )
        {
            m_ActivationTickDelay++;
            return;
        }
        else
        {
            m_bActive            = TRUE;
            m_bActivationChanged = FALSE;
            m_ActivationTickDelay = 0;
        }        
    }

    // still not active, get out
    if( !m_bActive )
    {
        return;
    }

    // Set the current state.
    if( m_State == 1 )
    {
        m_pCurrState = &m_PosState;
    }
    else if( m_State == -1 )
    {
        m_pCurrState = &m_NegState;
    }
    else
    {
        // It looks like no one has bothered to update the FO.
        // ASSERTS( FALSE, "State was not set for this focus object!" );
        m_pCurrState = &m_PosState;
    }

    player* pPlayer = SMP_UTIL_GetActivePlayer();
    if( !pPlayer )
        return;
    //
    // Perform LOS collision.
    //
    {
        // Distance checks.
        f32 Dist;

        // if it's above us, check from the eyes
        if( GetPosition().GetY() > pPlayer->GetPosition().GetY() )
        {
            Dist = (GetPosition() - pPlayer->GetEyesPosition()).Length();
        }
        else
            Dist = (GetPosition() - pPlayer->GetPosition()).Length();

        m_bInViewRange     = (Dist < m_pCurrState->m_ViewDist);
        m_bInTextRange     = (Dist < m_pCurrState->m_TextDist);
        m_bInInteractRange = (Dist < m_pCurrState->m_InteractDist);
        m_bStandingIn      = FALSE;

        // Only check collision with trigger and LOS if we're within viewable distance.
        if( m_bInViewRange )
        {
            //
            // Check the spatial trigger, if any.
            //
            {
                if( m_pCurrState->m_SpatialGuid != 0 )
                {
                    object* pObject = g_ObjMgr.GetObjectByGuid( m_pCurrState->m_SpatialGuid );
                    if( pObject )
                    {
                        if( pObject->IsKindOf( trigger_ex_object::GetRTTI() ) )
                        {
                            trigger_ex_object* pTrigger = (trigger_ex_object*)pObject;

                            ASSERTS( pTrigger->GetTriggerType() == trigger_ex_object::TRIGGER_TYPE_SPATIAL, 
                                "Focus Object attached to non spatial trigger!" );
                            if( pTrigger->GetTriggerType() != trigger_ex_object::TRIGGER_TYPE_SPATIAL )
                            {
                                m_bStandingIn = TRUE;
                            }
                            else
                            {
                                bbox TriggerBBox = pTrigger->GetColBBox();
                                bbox PlayerBBox  = pPlayer->GetColBBox();
                                m_bStandingIn = PlayerBBox.Intersect( TriggerBBox );
                            }
                        }
                    }
                }
                else
                {
                    // If there was no spatial trigger guid assigned,
                    // any standing location is valid.
                    m_bStandingIn = TRUE;
                }
            }

            //
            // LOS check.
            //
            // offset it a bit for objects whose positions end up in the floor            
            vector3 Pos = GetBBox().GetCenter() + vector3(0.0f, g_FO_OffsetY, 0.0f);
            
            g_CollisionMgr.LineOfSightSetup( pPlayer->GetGuid(), Pos, pPlayer->GetEyesPosition() );

            // Only need one collision to block LOS.
            g_CollisionMgr.SetMaxCollisions(1);

            g_CollisionMgr.CheckCollisions( object::TYPE_ALL_TYPES, 
                object::ATTR_BLOCKS_PLAYER_LOS,
                (object::object_attr)(object::ATTR_COLLISION_PERMEABLE|object::ATTR_LIVING) );

            m_bHasLOS = (g_CollisionMgr.m_nCollisions == 0);
        } 
        else
        {
            m_bHasLOS = FALSE;
        }
    }

    // Pulse the focus color using simulation time.
    m_ColorPhase += 2.0f * DeltaTime;
    if( m_ColorPhase > 2.0f )
    {
        m_ColorPhase = 0.0f;
    }

    // Update bracket displacement and text opacity.
    if( m_bHasLOS && m_bInViewRange )
    {
        m_AnimState -= 3.0f * DeltaTime;
    }
    else
    {
        m_AnimState += 3.0f * DeltaTime;
    }

    if( (m_AnimState < 0.1f) && m_bInTextRange )
    {
        m_TextAlphaState += DeltaTime * 3.0f;
    }
    else
    {
        m_TextAlphaState -= DeltaTime * 3.0f;
    }

    m_AnimState      = MINMAX( 0.0f, m_AnimState,      1.0f );
    m_TextAlphaState = MINMAX( 0.0f, m_TextAlphaState, 1.0f );
    m_MaxAlpha       = (u8)(255 * ((m_AnimState <= 0.3f) ?
        1.0f :
        (1.0f - ((m_AnimState - 0.3f) / 0.7f))));

    if( (m_pCurrState->m_ViewType == VIEW_CIRCLE) ||
        (m_pCurrState->m_ViewType == VIEW_BRACKETS) )
    {
        bbox FocusBBox = m_pCurrState->m_FocusBox;
        const f32 LongestDim = MAX( 1.0f,
                                    MAX( x_abs( FocusBBox.Max.GetX() - FocusBBox.Min.GetX() ),
                                         MAX( x_abs( FocusBBox.Max.GetY() - FocusBBox.Min.GetY() ),
                                              x_abs( FocusBBox.Max.GetZ() - FocusBBox.Min.GetZ() ) ) ) );
        const f32 Distance = (GetPosition() - pPlayer->GetEyesPosition()).Length();
        const f32 ScaleFactor = MIN( 1.0f, Distance / (LongestDim * 2.0f) );
        FocusBBox.Transform( matrix4( vector3( ScaleFactor, ScaleFactor, ScaleFactor ),
                                      radian3( 0.0f, 0.0f, 0.0f ),
                                      GetPosition() ) );

        const view& SimulationView = pPlayer->GetSimulationView();
        const vector3 RayStart = SimulationView.GetPosition();
        const vector3 RayEnd   = RayStart + SimulationView.GetViewZ() * m_pCurrState->m_ViewDist;
        f32 HitTime;
        m_bLookingAt = FocusBBox.Intersect( HitTime, RayStart, RayEnd );

        if( m_pCurrState->m_ViewType == VIEW_CIRCLE )
        {
            m_bLookingAt = m_bLookingAt &&
                           m_bInInteractRange &&
                           m_bHasLOS &&
                           m_bInViewRange &&
                           m_bStandingIn;
            m_bLocked = m_bLookingAt;
        }
    }
    else
    {
        m_bLookingAt = FALSE;
        m_bLocked = FALSE;
    }

    const f32 RotationScale = DeltaTime * 60.0f;
    m_CircleFocusObj[CIRCLE_INNER].Rotation -= 2.2f * RotationScale;
    m_CircleFocusObj[CIRCLE_FLOATER].Rotation += 1.4f * RotationScale;
    if( m_bLookingAt )
    {
        m_CircleFocusObj[CIRCLE_OUTTER].Rotation += 8.4f * RotationScale;
    }
    else
    {
        m_CircleFocusObj[CIRCLE_OUTTER].Rotation += 2.2f * RotationScale;
    }
}

//=========================================================================

#ifdef X_EDITOR
s32 focus_object::OnValidateProperties( xstring& ErrorMsg )
{
    s32 nErrors = 0;

    if( m_State == 0 )
    {
        nErrors++;
        ErrorMsg += "Initial state has not been set!\nThis Focus Object probably needs to be updated.\n";
    }

    return nErrors;
}
#endif

//=========================================================================

xbool focus_object::TestPress( void )
{
    if( m_bActive           &&
        m_bInInteractRange  &&
        m_bStandingIn       &&
        (m_bLookingAt || !m_pCurrState->m_bInteractLOS) &&
        m_bHasLOS )
    {
        
#ifndef X_EDITOR    
        const xwchar* pString = g_StringTableMgr( g_StringMgr.GetString( m_pCurrState->m_InteractStrTable ),
            g_StringMgr.GetString( m_pCurrState->m_InteractStrTitle ) );

        // AHARP todo - this needs to be removed from any release builds.
        if ( x_wstrcmp( pString, (const xwchar*)xwstring( "<null>" ) ) == 0 )
        {
            //pString = (const xwchar*)xwstring( "You touched me!" );
        }
        else
        {
            MsgMgr.Message( MSG_GOAL_STRING, 0, (saddr)pString );
        }
#endif

        // Fire the trigger.
        if( m_pCurrState->m_TriggerGuid != 0 )
        {
            object* pObject = g_ObjMgr.GetObjectByGuid( m_pCurrState->m_TriggerGuid );
            if( pObject )
            {
                if( pObject->IsKindOf( trigger_ex_object::GetRTTI() ) )
                {
                    trigger_ex_object* pTrigger = (trigger_ex_object*)pObject;
                    pTrigger->OnActivate( TRUE );
                }
            }
        }

        // Play the sound.
        if( m_pCurrState->m_pInteractAudio[ 0 ] != 0 )
        {
            g_AudioMgr.Play( m_pCurrState->m_pInteractAudio );
        }

        // If the auto-off is set then turn the FO off.
        if( m_pCurrState->m_bAutoOff )
        {
            OnActivate( FALSE );
        }

        return TRUE;
    }
    return FALSE;
}



//=========================================================================
void focus_object::OnRenderTransparent( void )
{

}

//=========================================================================

bbox focus_object::GetLocalBBox( void ) const
{
    bbox TempBox = m_pCurrState->m_FocusBox;
    
    // Inflate this by a bit so that the FO brackets wait till they
    // are off screen to disappear.
    TempBox.Inflate( 100, 100, 100 );

    return TempBox; 
}

//=========================================================================

void focus_object::OnEnumProp( prop_enum&  List )
{
    // Get string which contains all available string tables.
    xstring TableString;
    TableString.Clear();

    slot_id SlotID = g_ObjMgr.GetFirst( object::TYPE_HUD_OBJECT );

    if( SlotID == SLOT_NULL )
        x_throw( "No Hud Object specified in this project." );

    hud_object& Hud = hud_object::GetSafeType( *g_ObjMgr.GetObjectBySlot( SlotID ) );
    Hud.GetBinaryResourceName(TableString);
    
    object::OnEnumProp      ( List );
         

    List.PropEnumHeader  ( "FocusObject",         "The focus object.", 0 );
    List.PropEnumEnum    ( "FocusObject\\State",  "Positive\0Negative\0", "Initial State",     PROP_TYPE_EXPOSE );
    List.PropEnumBool    ( "FocusObject\\Active", "Does this FO start active?",                               0 );

    const char* Prefixes[]  = { "Pos", "Neg" };
    s32 i;
    for( i = 0; i < 2; i++ )
    {
        List.PropEnumHeader  ( Prefixes[ i ], "State attributes.", 0 );
        List.PropEnumFloat   ( xfs( "%s\\ViewDist",             Prefixes[ i ] ), "Distance the player needs to be within before the effect is drawn.",                 0 );
        List.PropEnumBBox    ( xfs( "%s\\Size",                 Prefixes[ i ] ), "Where this object will appear in the world.",                                        0 );
        List.PropEnumEnum    ( xfs( "%s\\DisplayTable",         Prefixes[ i ] ), (const char*)TableString, "Table for String displayed over FO.",                      0 );
        List.PropEnumString  ( xfs( "%s\\DisplayTitle",         Prefixes[ i ] ), "Title of String displayed over FO.",                                                 0 );
        List.PropEnumFloat   ( xfs( "%s\\TextDist",             Prefixes[ i ] ), "Distance the player needs to be within before the text is drawn.",                   0 );
        List.PropEnumFloat   ( xfs( "%s\\InteractDist",         Prefixes[ i ] ), "Distance the player needs to be within before he can interact with this.",           0 );
        List.PropEnumBool    ( xfs( "%s\\InteractLOS",          Prefixes[ i ] ), "If true, player must look at FO to activate interact.",                              0 );

        List.PropEnumColor   ( xfs( "%s\\Color1",               Prefixes[ i ] ), "What color should this FO pulse to when idle?",                                      0 );
        List.PropEnumColor   ( xfs( "%s\\Color2",               Prefixes[ i ] ), "What color should this FO pulse to when idle?",                                      0 );
        List.PropEnumColor   ( xfs( "%s\\SelectedColor1",       Prefixes[ i ] ), "What color should this FO pulse to when selected?",                                  0 );
        List.PropEnumColor   ( xfs( "%s\\SelectedColor2",       Prefixes[ i ] ), "What color should this FO pulse to when selected?",                                  0 );

        List.PropEnumEnum    ( xfs( "%s\\InteractTable",        Prefixes[ i ] ), (const char*)TableString, "Table for String displayed when FO is used.",              0 );
        List.PropEnumString  ( xfs( "%s\\InteractTitle",        Prefixes[ i ] ), "Title of String displayed when FO is used.",                                         0 );
        
        List.PropEnumGuid    ( xfs( "%s\\TriggerGuid",          Prefixes[ i ] ), "The GUID of the trigger that this FO activates.",                                    0 );
        List.PropEnumGuid    ( xfs( "%s\\SpatialGuid",          Prefixes[ i ] ), "The Spatial Trigger to check the volume of",                                         0 );

        List.PropEnumBool    ( xfs( "%s\\TurnOff",              Prefixes[ i ] ), "Deactivate automatically when used?",                                                0 );
        List.PropEnumEnum    ( xfs( "%s\\Focus Type",           Prefixes[ i ] ), "Brackets\0Nothing\0Circle\0", "What the display should look like for this FO.",              0 );
    } 
}

//=========================================================================

xbool focus_object::OnProperty( prop_query& rPropQuery )
{                       
    if( object::OnProperty( rPropQuery ) )
    {
        return TRUE;
    }

    s32 iPath = rPropQuery.PushPath( "FocusObject\\" );
           
    if( rPropQuery.IsVar( "State" ) )
    {
        if( rPropQuery.IsRead() )
        {
            if( m_State == 1 )
            {
                rPropQuery.SetVarEnum( "Positive" );
            }
            else if( m_State == -1 )
            {
                rPropQuery.SetVarEnum( "Negative" );
            }
            else
            {
                rPropQuery.SetVarEnum( "Unset" );
            }
            OnMove( GetPosition() );
            return( TRUE );
        }
        else
        {
            m_State = VIEW_NONE;
            const char* pString = rPropQuery.GetVarEnum();
            if( x_stricmp( pString, "Positive"              ) == 0 )    { m_State =  1;    }
            if( x_stricmp( pString, "Negative"              ) == 0 )    { m_State = -1;    }

            // So that the bounding box will update!
            OnMove( GetPosition() );
            return( TRUE );
        }
    }

    if( rPropQuery.VarBool( "Active", m_bActive ) ) return TRUE;

    state_vals* States[]    = { &m_PosState, &m_NegState };
    const char* Prefixes[]  = { "Pos\\",     "Neg\\" };

    s32 i;
    for( i = 0; i < 2; i++ )
    {
        state_vals* pCurrState = States[ i ];

        s32 iPath = rPropQuery.PushPath( Prefixes[ i ] );

        if( rPropQuery.VarFloat     ( "ViewDist",           pCurrState->m_ViewDist            ) ) return TRUE;
        if( rPropQuery.VarBBox      ( "Size",               pCurrState->m_FocusBox            ) ) 
        {
            OnMove( GetPosition() );
            return TRUE;
        }

        if( rPropQuery.VarFloat     ( "TextDist",           pCurrState->m_TextDist            ) ) return TRUE;      
        if( rPropQuery.VarFloat     ( "InteractDist",       pCurrState->m_InteractDist        ) ) return TRUE;  
        if( rPropQuery.VarBool      ( "InteractLOS",        pCurrState->m_bInteractLOS        ) ) return TRUE;

        if( rPropQuery.VarColor     ( "Color1",             pCurrState->m_Color1              ) ) return TRUE;
        if( rPropQuery.VarColor     ( "Color2",             pCurrState->m_Color2              ) ) return TRUE;

        if( rPropQuery.VarColor     ( "SelectedColor1",     pCurrState->m_SelectedColor1      ) ) return TRUE;
        if( rPropQuery.VarColor     ( "SelectedColor2",     pCurrState->m_SelectedColor2      ) ) return TRUE;

        if( rPropQuery.VarGUID      ( "TriggerGuid",        pCurrState->m_TriggerGuid         ) ) return TRUE;
        if( rPropQuery.VarGUID      ( "SpatialGuid",        pCurrState->m_SpatialGuid         ) ) return TRUE;

        if( rPropQuery.VarBool      ( "TurnOff",            pCurrState->m_bAutoOff            ) ) return TRUE;        

        if( rPropQuery.IsVar( "Focus Type" ) )
        {
            if( rPropQuery.IsRead() )
            {
                switch( pCurrState->m_ViewType )
                {
                default:  // Fall through...
                case VIEW_NONE:   
                    rPropQuery.SetVarEnum( "None" );  
                    break;
                case VIEW_BRACKETS:
                    rPropQuery.SetVarEnum( "Brackets" );  
                    break;
                case VIEW_CIRCLE:
                    rPropQuery.SetVarEnum( "Circle" );
                    break;
                }
                return( TRUE );
            }
            else
            {
                pCurrState->m_ViewType = VIEW_NONE;
                const char* pString = rPropQuery.GetVarEnum();
                if( x_stricmp( pString, "None"                   ) == 0 )    { pCurrState->m_ViewType = VIEW_NONE;     }
                if( x_stricmp( pString, "Brackets"               ) == 0 )    { pCurrState->m_ViewType = VIEW_BRACKETS; }
                if( x_stricmp( pString, "Circle"                 ) == 0 )    { pCurrState->m_ViewType = VIEW_CIRCLE; }

                return( TRUE );
            }
        }

        if( rPropQuery.IsVar( "DisplayTable" ) )
        {
            if( rPropQuery.IsRead() )
            {
                if( pCurrState->m_DisplayStrTable >= 0 )
                    rPropQuery.SetVarEnum( g_StringMgr.GetString( pCurrState->m_DisplayStrTable ) );
                else
                    rPropQuery.SetVarEnum( "" );
            }
            else
            {
                pCurrState->m_DisplayStrTable = g_StringMgr.Add( rPropQuery.GetVarEnum() );
            }

            return TRUE;
        }

        if( rPropQuery.IsVar( "DisplayTitle" ) )
        {
            if( rPropQuery.IsRead() )
            {
                if( pCurrState->m_DisplayStrTitle >= 0 )
                {
                    const char* pString = g_StringMgr.GetString( pCurrState->m_DisplayStrTitle );
                    rPropQuery.SetVarString( pString, x_strlen( pString )+1 );
                }
                else
                {
                    rPropQuery.SetVarString( "", 1 );
                }
            }
            else
            {
                pCurrState->m_DisplayStrTitle = g_StringMgr.Add( rPropQuery.GetVarString() );
            }

            return TRUE;
        }

        if( rPropQuery.IsVar( "InteractTable" ) )
        {
            if( rPropQuery.IsRead() )
            {
                if( pCurrState->m_InteractStrTable >= 0 )
                    rPropQuery.SetVarEnum( g_StringMgr.GetString( pCurrState->m_InteractStrTable ) );
                else
                    rPropQuery.SetVarEnum( "" );
            }
            else
            {
                pCurrState->m_InteractStrTable = g_StringMgr.Add( rPropQuery.GetVarEnum() );
            }

            return TRUE;
        }

        if( rPropQuery.IsVar( "InteractTitle" ) )
        {
            if( rPropQuery.IsRead() )
            {
                if( pCurrState->m_InteractStrTitle >= 0 )
                {
                    const char* pString = g_StringMgr.GetString( pCurrState->m_InteractStrTitle );
                    rPropQuery.SetVarString( pString, x_strlen( pString )+1 );
                }
                else
                {
                    rPropQuery.SetVarString( "", 1 );
                }
            }
            else
            {
                pCurrState->m_InteractStrTitle = g_StringMgr.Add( rPropQuery.GetVarString() );
            }

            return TRUE;
        }

        rPropQuery.PopPath( iPath );
    }

    rPropQuery.PopPath( iPath );

    return FALSE;   
}

//=============================================================================

void focus_object::OnActivate( xbool Flag )
{
    if( Flag )
    {
        m_bActivationChanged = TRUE;        
    }
    else
    {
        m_bActive = Flag;
    }
}

//=============================================================================

xbool focus_object::HasFocus( void )
{
    return m_bLookingAt;
}                 

//=============================================================================
