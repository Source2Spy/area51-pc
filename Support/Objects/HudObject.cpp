//==============================================================================
//
//  hud_Object.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "HudObject.hpp"
#include "e_View.hpp"
#include "Entropy.hpp"
#include "x_math.hpp"
#include "../MiscUtils/SimpleUtils.hpp"
#include "Objects/WeaponSniper.hpp"
#include "GameTextMgr/GameTextMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "GameLib/RenderContext.hpp"
#include "StringMgr/StringMgr.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"

#ifndef X_EDITOR
#include "StateMgr/StateMgr.hpp"
#endif

//=========================================================================
// GLOBALS
//=========================================================================

// Screen margins given in pixels
#if defined( TARGET_DESKTOP ) || defined( TARGET_MOBILE ) || defined( X_EDITOR )
#define LEFTMARGIN              16
#define TOPMARGIN               16
#define RIGHTMARGIN             16
#define BOTTOMMARGIN            16
#endif

f32     hud_object::m_PulseAlpha;
f32     hud_object::m_PulseRate;

xbool g_RenderFrameRateInfo = FALSE;

xcolor g_HudColor( 80, 150, 150, 255);

extern xbool g_first_person;
extern xbool g_game_running;


//==============================================================================
// FUNCTIONS
//==============================================================================

void RenderLine( const ui_text_data& Text,
                 irect& Rect,
                 u8 Alpha,
                 xcolor& Color,
                 s32 FontNum,
                 s32 Flags,
                 xbool Shadow,
                 f32 FlareAmount )
{
#ifndef X_EDITOR
    ui_font* pFont = g_UiMgr->GetFont( FontNum );
    xcolor   Tint  = Color;

    Tint.A = (u8)(Alpha * 0.8f);

    if( Shadow )
    {
        ui_text_data ShadowText = Text;

        for( s32 i = 0; i < ShadowText.Runs.GetCount(); i++ )
        {
            ShadowText.Runs[i].Color = XCOLOR_WHITE;
        }

        irect Bounds = Rect;
        Bounds.Translate( 0, 2 );

        pFont->RenderText( Bounds, Flags, xcolor( 0, 0, 0, Tint.A ), ShadowText, FALSE );
    }

    pFont->RenderText( Rect, Flags, Tint, Text, TRUE, FlareAmount );
#endif
}

//==============================================================================
// OBJECT DESCRIPTION
//==============================================================================
static struct hud_object_desc : public object_desc
{
    hud_object_desc( void ) : object_desc( 
        object::TYPE_HUD_OBJECT, 
        "Hud Object", 
        "HUD",

        object::ATTR_DRAW_2D                     |
        object::ATTR_NEEDS_LOGIC_TIME            |
        object::ATTR_RENDERABLE,

        FLAGS_GENERIC_EDITOR_CREATE | 
        FLAGS_IS_DYNAMIC ) {}

        //---------------------------------------------------------------------

        virtual object* Create          ( void )
        {
            return new hud_object;
        }

        //---------------------------------------------------------------------

} s_HudObject_Desc;

//==============================================================================

const object_desc&  hud_object::GetTypeDesc( void ) const
{
    return s_HudObject_Desc;
}

//==============================================================================

const object_desc&  hud_object::GetObjectType( void )
{
    return s_HudObject_Desc;
}


//==============================================================================
// FUNCTIONS
//==============================================================================

hud_object::hud_object( void ) 
{
    x_DebugMsg( "Initializing HUD\n" );

    m_LogicRunning  = FALSE;
    m_Initialized   = FALSE;
    m_PulseAlpha    = 255;
    m_PulseRate     = 512.0f;
    m_NumHuds       = 0;


    m_bLetterBoxOn       = FALSE;
    m_LetterBoxCurrTime  = 1.0f;
    m_LetterBoxTotalTime = 1.0f;

    // No point in waiting for InitHud to be called, since the array and the
    // components exist regardless of how many players there are.  Also,
    // this makes message creation before everything is completely set up less
    // problematic.
    s32 i; 
    for( i = 0; i < NET_MAX_PER_CLIENT; i++ )
    {
        // Initialize component pointer array.
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_MUTANT_VISION     ]  = &m_PlayerHuds[ i ].m_MutantVision;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_CONTAGIOUS_VISION ]  = &m_PlayerHuds[ i ].m_ContagiousVision;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_HEALTH_BAR        ]  = &m_PlayerHuds[ i ].m_Health; 
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_AMMO_BAR          ]  = &m_PlayerHuds[ i ].m_Ammo;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_RETICLE           ]  = &m_PlayerHuds[ i ].m_Reticle;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_DAMAGE            ]  = &m_PlayerHuds[ i ].m_Damage;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_SNIPER            ]  = &m_PlayerHuds[ i ].m_Sniper;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_ICON              ]  = &m_PlayerHuds[ i ].m_Icon;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_INFO_BOX          ]  = &m_PlayerHuds[ i ].m_InfoBox;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_TEXT_BOX          ]  = &m_PlayerHuds[ i ].m_Text;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_VOTE              ]  = &m_PlayerHuds[ i ].m_Vote;
        m_PlayerHuds[ i ].m_HudComponents[ HUD_ELEMENT_SCANNER           ]  = &m_PlayerHuds[ i ].m_Scanner;
    
        m_PlayerHuds[ i ].m_Active = FALSE;

        // Initialize element pulse.
        for( s32 j = 0; j < HUD_ELEMENT_NUM_ELEMENTS; j++ )
        {
            m_PlayerHuds[ i ].m_HudComponents[ j ]->m_bPulsing = FALSE;
        }    
    }

    // Timer
    m_TimerTriggerGuid = 0;
    m_RenderTimer = FALSE;
    m_TimerActive = FALSE;
    m_TimerTime   = 30.0f;
    m_TimerAdd    = 0.0f;
    m_TimerSub    = 0.0f;
    m_TimerWarning = FALSE;
    m_TimerCritical = FALSE;
    m_TimerCriticalStarted = FALSE;
    m_TimerWarningStarted = FALSE;

    // Objective Strings
    m_ObjectiveTableNameIndex=-1;
    m_ObjectiveTitleStringIndex=-1;
    m_ObjectiveTime = 0.0f;
}

//==============================================================================

hud_object::~hud_object( void ) 
{
}

//==============================================================================

void hud_object::LayoutPlayerHud( player_hud& PlayerHud, const rect& ViewDimensions )
{
    PlayerHud.m_ViewDimensions = ViewDimensions;

    xbool UseLeftMargin   = FALSE;
    xbool UseTopMargin    = FALSE;
    xbool UseRightMargin  = FALSE;
    xbool UseBottomMargin = FALSE;

    switch( m_NumHuds )
    {
        case 1:
            UseLeftMargin   = TRUE;
            UseTopMargin    = TRUE;
            UseRightMargin  = TRUE;
            UseBottomMargin = TRUE;
            break;

        case 2:
            ASSERT( (PlayerHud.m_LocalSlot >= 0) && (PlayerHud.m_LocalSlot < 2) );
            UseLeftMargin   = TRUE;
            UseRightMargin  = TRUE;
            UseTopMargin    = (PlayerHud.m_LocalSlot == 0);
            UseBottomMargin = (PlayerHud.m_LocalSlot == 1);
            break;

        case 3:
            ASSERT( (PlayerHud.m_LocalSlot >= 0) && (PlayerHud.m_LocalSlot < 3) );
            UseLeftMargin   = (PlayerHud.m_LocalSlot != 1);
            UseTopMargin    = (PlayerHud.m_LocalSlot != 2);
            UseRightMargin  = (PlayerHud.m_LocalSlot != 0);
            UseBottomMargin = (PlayerHud.m_LocalSlot == 2);
            break;

        case 4:
            ASSERT( (PlayerHud.m_LocalSlot >= 0) && (PlayerHud.m_LocalSlot < 4) );
            UseLeftMargin   = ((PlayerHud.m_LocalSlot & 1) == 0);
            UseTopMargin    = (PlayerHud.m_LocalSlot < 2);
            UseRightMargin  = ((PlayerHud.m_LocalSlot & 1) != 0);
            UseBottomMargin = (PlayerHud.m_LocalSlot >= 2);
            break;

        default:
            ASSERT( FALSE );
            return;
    }

    const f32 LeftInset   = UseLeftMargin   ? (f32)LEFTMARGIN   : 0.0f;
    const f32 TopInset    = UseTopMargin    ? (f32)TOPMARGIN    : 0.0f;
    const f32 RightInset  = UseRightMargin  ? (f32)RIGHTMARGIN  : 0.0f;
    const f32 BottomInset = UseBottomMargin ? (f32)BOTTOMMARGIN : 0.0f;

    PlayerHud.m_XPos   = ViewDimensions.Min.X + LeftInset + 2.0f;
    PlayerHud.m_YPos   = ViewDimensions.Min.Y + TopInset  + 2.0f;
    PlayerHud.m_Width  = ViewDimensions.GetWidth()  - LeftInset - RightInset  - 4.0f;
    PlayerHud.m_Height = ViewDimensions.GetHeight() - TopInset  - BottomInset - 4.0f;

    PlayerHud.m_CenterX = ViewDimensions.GetCenter().X;
    PlayerHud.m_CenterY = ViewDimensions.GetCenter().Y;

    PlayerHud.m_Reticle.m_XPos = PlayerHud.m_CenterX;
    PlayerHud.m_Reticle.m_YPos = PlayerHud.m_CenterY;
    PlayerHud.m_Damage.m_XPos  = PlayerHud.m_CenterX;
    PlayerHud.m_Damage.m_YPos  = PlayerHud.m_CenterY;
    PlayerHud.m_Icon.m_XPos    = PlayerHud.m_CenterX;
    PlayerHud.m_Icon.m_YPos    = PlayerHud.m_CenterY;

    PlayerHud.m_Text.m_XPos   = PlayerHud.m_XPos;
    PlayerHud.m_Text.m_YPos   = PlayerHud.m_YPos;
    PlayerHud.m_Health.m_XPos = PlayerHud.m_XPos;
    PlayerHud.m_Health.m_YPos = PlayerHud.m_YPos + PlayerHud.m_Height;

    PlayerHud.m_Vote.m_XPos = PlayerHud.m_CenterX - 46.0f;
    PlayerHud.m_Vote.m_YPos = PlayerHud.m_CenterY + 108.0f;

    PlayerHud.m_Ammo.m_XPos    = PlayerHud.m_XPos + PlayerHud.m_Width;
    PlayerHud.m_Ammo.m_YPos    = PlayerHud.m_YPos + PlayerHud.m_Height;
    PlayerHud.m_Scanner.m_XPos = PlayerHud.m_XPos + PlayerHud.m_Width;
    PlayerHud.m_Scanner.m_YPos = PlayerHud.m_YPos + PlayerHud.m_Height;

    PlayerHud.m_InfoBox.m_XPos = PlayerHud.m_XPos + PlayerHud.m_Width;
    PlayerHud.m_InfoBox.m_YPos = PlayerHud.m_YPos;

    PlayerHud.m_Sniper.m_ViewDimensions = ViewDimensions;
    PlayerHud.UpdateTextWidth();
}

//==============================================================================

void hud_object::InitHud( void )
{
#ifdef X_EDITOR
    m_NumHuds = 1;
#else
    m_NumHuds = g_NetworkMgr.GetLocalPlayerCount();
#endif


    s32 i = 0;
    slot_id PlayerSlot = g_ObjMgr.GetFirst( object::TYPE_PLAYER );

    // If a player hasn't been created yet, abort.
    if( PlayerSlot == SLOT_NULL )
        return;

    while( PlayerSlot != SLOT_NULL )
    {
        ASSERT( i < m_NumHuds );

        // Get the players view port.
        player* pPlayer = (player*)g_ObjMgr.GetObjectBySlot( PlayerSlot );

        if( 
            (pPlayer != NULL)                     && // Is the player valid?
            (pPlayer->GetLocalSlot() != -1) )         // Is the player local?
        {
            player_hud& PlayerHud   = m_PlayerHuds[ i ]; 
            PlayerHud.m_PlayerSlot  = PlayerSlot;
            PlayerHud.m_LocalSlot   = pPlayer->GetLocalSlot();
#ifndef X_EDITOR
            PlayerHud.m_NetSlot     = pPlayer->net_GetSlot();            
#else
            PlayerHud.m_NetSlot     = 0;
#endif
            PlayerHud.m_Active      = TRUE;

            i++;
        }

        // Get the next player.
        PlayerSlot = g_ObjMgr.GetNext( PlayerSlot ) ;
    }

    m_Initialized = TRUE;
}

//==============================================================================

void hud_object::OnAdvanceSimulation( f32 DeltaTime )
{
    // Currently, the HUD needs to initialize before it can do the logic,
    // and it needs to attempt to do the logic before it can initialize.
    // This is due to the orders in which the game and the editor create the
    // objects. It should only take a couple of frames to work itself out.
    m_LogicRunning = TRUE;
    if( !m_Initialized )
    {
        return;
    }

    // Timer
    if( m_TimerWarning && m_TimerWarningStarted == FALSE )
    {
        g_AudioMgr.Play("Objective_Timer_Reset", TRUE );
        m_TimerWarningStarted = TRUE;
    }
    else if( m_TimerWarning == FALSE)
        m_TimerWarningStarted = FALSE;
        
    if( m_TimerCritical && m_TimerCriticalStarted == FALSE )
    {
        g_AudioMgr.Play("Objective_Timer_Reset", TRUE );
        m_TimerCriticalStarted = TRUE;
    }
    else if( m_TimerCritical == FALSE )
        m_TimerCriticalStarted = FALSE;

    if( m_TimerAdd > 0.0f )
    {
        m_TimerTime += m_TimerAdd;
        m_TimerAdd = 0.0f;
    }

    if( m_TimerSub < 0.0f )
    {
        m_TimerTime -= m_TimerSub;
        m_TimerSub = 0.0f;
    }

    if(m_TimerActive)
    {
        if(m_TimerTime<=0.0f)
        {
            // Trigger Guid
            object* pObject = NULL;
            pObject = g_ObjMgr.GetObjectByGuid(m_TimerTriggerGuid);
            if( pObject )
            {                
                pObject->OnActivate(TRUE);               
            }

            // Time is at zero stop the clock.
            m_TimerActive = FALSE;
        }
        m_TimerTime-=DeltaTime;
    }

    // Objective text has a finite display lifetime.  Keep this timer in the
    // simulation path so it cannot remain latched after the message expires.
    if( m_ObjectiveTime > 0.0f )
        m_ObjectiveTime -= DeltaTime;

    // Update pulsing HUD elements.
    m_PulseAlpha += m_PulseRate * DeltaTime;
    if( m_PulseAlpha > 255.0f )
    {
        m_PulseAlpha = 255.0f;
        m_PulseRate  = -m_PulseRate;
    }
    if( m_PulseAlpha < 64.0f )
    {
        m_PulseAlpha = 64.0f;
        m_PulseRate  = -m_PulseRate;
    }

    // Do logic for all components.
    s32 i;
    for( i = 0; i < m_NumHuds; i++ )
    {
        m_PlayerHuds[ i ].OnAdvanceSimulation( DeltaTime );
    }

    hud_mutant_vision::UpdateEffects( DeltaTime );
    hud_contagious_vision::UpdateEffects( DeltaTime );

    // Advance cinematic bars independently of player HUD components.
    m_LetterBoxCurrTime = MINMAX( 0.0f,
                                  m_LetterBoxCurrTime + DeltaTime,
                                  m_LetterBoxTotalTime );

}

//==============================================================================

void hud_object::BeginIconSnapshot( void )
{
    for( s32 i = 0; i < NET_MAX_PER_CLIENT; i++ )
    {
        m_PlayerHuds[i].m_Icon.BeginSimulationSnapshot();
    }
}

//==============================================================================

void hud_object::CommitIconSnapshot( void )
{
    for( s32 i = 0; i < NET_MAX_PER_CLIENT; i++ )
    {
        m_PlayerHuds[i].m_Icon.CommitSimulationSnapshot();
    }
}

//==============================================================================

xbool hud_object::IsLetterBoxOn( void ) const
{
    // Draw the cinematic bars?
    if( ( ( m_LetterBoxCurrTime > 0.0f ) && m_bLetterBoxOn ) ||
        ( ( m_LetterBoxCurrTime < m_LetterBoxTotalTime ) && !m_bLetterBoxOn ) )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

//==============================================================================

f32 hud_object::GetLetterBoxAmount( void ) const
{
    f32 Amount = m_LetterBoxCurrTime / m_LetterBoxTotalTime;
    if( !m_bLetterBoxOn )
    {
        Amount = 1.0f - Amount;
    }
    
    return Amount;
}

//==============================================================================

void hud_object::RenderLetterBox( const irect& VP, f32 Amount )
{
    // Compute percentage of bar coverage in physical pixels
    const f32 ClampedAmount = x_clamp( Amount, 0.0f, 1.0f );
    const s32 ViewportHeight = VP.GetHeight();
    const s32 BarHeight = iMin( ViewportHeight,
                                iMax( 0, (s32)x_round( (f32)ViewportHeight * 0.2f * ClampedAmount, 1.0f ) ) );

    // Compute top bar
    const irect TopRect( VP.l,
                         VP.t,
                         VP.r,
                         VP.t + BarHeight );

    // Compute bottom bar
    const irect BottomRect( VP.l,
                            VP.b - BarHeight,
                            VP.r,
                            VP.b );

#ifdef X_EDITOR
    const xcolor BarColor = XCOLOR_PURPLE;
#else
    const xcolor BarColor = XCOLOR_BLACK;
#endif
    g_UIRenderer.DrawRect( TopRect, BarColor );
    g_UIRenderer.DrawRect( BottomRect, BarColor );
}

//==============================================================================
xbool g_RenderHUD = TRUE;
void hud_object::OnRender( void )
{
    // If _Debug_ tirgger is set dont render any HUD.   
#if !defined(X_RETAIL) || defined(X_QA)
    if( g_RenderHUD == FALSE )
        return;
#endif

// SB: Do not render if the game isn't running in the editor otherwise the editor
//     will crash when a new player is created and placed.
#ifdef X_EDITOR
    if( !g_game_running )
        return;
#endif

// SB: I commented this out for the editor so that the letter box bars
//     are drawn when you pause the game in the editor.
#ifndef X_EDITOR
    if( m_LogicRunning == FALSE )
        return;
#endif

    // Try to initialize the hud every frame until it works.
    if( !m_Initialized )
    {
        InitHud();

        // Check and see if it worked, if not, come back later.
        if( !m_Initialized )
        {
            return;
        }
    }

#ifdef X_EDITOR
    InitHud();
#endif

#ifndef X_EDITOR
    if( !( ( g_first_person ) && ( IsLetterBoxOn() ) ) &&
        ( g_StateMgr.GetState() != SM_PLAYING_GAME ) )
    {
        return;
    }
#endif

    player* pActivePlayer = SMP_UTIL_GetActivePlayer();
    if( !pActivePlayer )
        return;

    rect ScreenViewDimensions;
    pActivePlayer->GetRenderView().GetViewport( ScreenViewDimensions );

    player_hud& PlayerHud = GetPlayerHud( g_RenderContext.LocalPlayerIndex );
    const irect ScreenViewport( (s32)ScreenViewDimensions.Min.X,
                                (s32)ScreenViewDimensions.Min.Y,
                                (s32)ScreenViewDimensions.Max.X,
                                (s32)ScreenViewDimensions.Max.Y );
    const rect HudViewDimensions = g_UIRenderer.GetViewport().GetHudBounds( ScreenViewport );
    if( (PlayerHud.m_ViewDimensions.Min.X != HudViewDimensions.Min.X) ||
        (PlayerHud.m_ViewDimensions.Min.Y != HudViewDimensions.Min.Y) ||
        (PlayerHud.m_ViewDimensions.Max.X != HudViewDimensions.Max.X) ||
        (PlayerHud.m_ViewDimensions.Max.Y != HudViewDimensions.Max.Y) )
    {
        LayoutPlayerHud( PlayerHud, HudViewDimensions );
    }

    // Draw the cinematic bars?
    if( ( g_first_person ) && ( IsLetterBoxOn() ) )
    {
        g_UIRenderer.PushScreenSpace( ScreenViewport );
        RenderLetterBox( ScreenViewport, GetLetterBoxAmount() );
        g_UIRenderer.PopScreenSpace();

        g_UIRenderer.PushHudSpace( ScreenViewport );
        ((hud_renderable*)PlayerHud.m_HudComponents[ HUD_ELEMENT_TEXT_BOX ])->OnRender( pActivePlayer );
        g_UIRenderer.PopHudSpace();
    }
    else
    {
        g_UIRenderer.PushHudSpace( ScreenViewport );
        PlayerHud.OnRender();

        // Timer
        if( m_RenderTimer )
            RenderTimer( HudViewDimensions );

        g_UIRenderer.PopHudSpace();
    }

    g_UIRenderer.PushScreenSpace( ScreenViewport );
    g_UIRenderer.PopScreenSpace();

}
//==============================================================================
void hud_object::RenderTimer( const rect& ViewDimensions )
{
    irect TimerRect( (s32)x_floor( ViewDimensions.Min.X ),
                     300,
                     (s32)x_ceil( ViewDimensions.Max.X ),
                     (s32)x_ceil( ViewDimensions.Max.Y ) );
    xwchar TimeStr[32];

    s32 Seconds1 = ((s32)(m_TimerTime) % 60) / 10;
    s32 Seconds2 = ((s32)(m_TimerTime) % 60) % 10;
    s32 Minutes1 = (((s32)(m_TimerTime)) / 60) / 10;
    s32 Minutes2 = (((s32)(m_TimerTime)) / 60) % 10;

    s32 upper1 = (s32)(m_TimerTime);
    f32 strip = m_TimerTime - (f32)upper1;
    s32 HthsSeconds2 = (s32)((strip /  10)*1000);

    Minutes1 = MAX( Minutes1, 0 );
    Minutes2 = MAX( Minutes2, 0 );
    Seconds1 = MAX( Seconds1, 0 );
    Seconds2 = MAX( Seconds2, 0 );
    HthsSeconds2 = MAX( HthsSeconds2, 0 );

    xcolor TextColor;
    TextColor = XCOLOR_WHITE;
    x_wstrcpy( TimeStr, (const xwchar*)((xwstring)xfs( "%d%d:%d%d:%02d", Minutes1, Minutes2, Seconds1, Seconds2, HthsSeconds2)) );  

#ifndef X_EDITOR
    if( m_TimerWarning )
        TextColor = xcolor( 200,0,0,255);

    if( m_TimerCritical )
    {
        TextColor = xcolor( 200,0,0,255 );
        RenderLine( TimeStr, TimerRect, (s32)m_PulseAlpha, TextColor, 2, ui_font::h_center|ui_font::v_center, TRUE );
    }
    else
        RenderLine( TimeStr, TimerRect, 255, TextColor, 2, ui_font::h_center|ui_font::v_center, TRUE );
#else
    x_printfxy( 1, 5, "Timer\n%d%d:%d%d:%02d", Minutes1, Minutes2, Seconds1, Seconds2, HthsSeconds2 );
#endif
}

//==============================================================================
void hud_object::OnEnumProp( prop_enum&  List )
{
    object::OnEnumProp      ( List );

    List.PropEnumHeader( "Hud", "Stats for this item", PROP_TYPE_HEADER );

    // Enum all components.
    s32 j;
    for( j = 0; j < HUD_ELEMENT_NUM_ELEMENTS; j++ )
    {
        m_PlayerHuds[ 0 ].m_HudComponents[ j ]->OnEnumProp( List );
    }


    //----------------------------------------------------------------------
    // Localized text resources.
    //----------------------------------------------------------------------
    List.PropEnumHeader( "Hud\\Binary Text Resource", "Localized string tables.", 0 );
    s32 i;
    for( i = 0; i < MAX_BIN_TXT_RSC; i++ )
        List.PropEnumExternal( xfs("Hud\\Binary Text Resource\\Binary Text Rsc [%d]", i ), 
        "Resource\0string\0",
        "Loads a localized string table.", 0 );


    //----------------------------------------------------------------------
    // Timer
    //----------------------------------------------------------------------
    List.PropEnumHeader ("Hud\\Timer",                  "Properties for setting up and useing a on screen timer",   0 );
    List.PropEnumGuid   ("Hud\\Timer\\Trigger Guid",    "Guid that is activated when the timer hits Zero.",         PROP_TYPE_EXPOSE|PROP_TYPE_MUST_ENUM);
    List.PropEnumFloat  ("Hud\\Timer\\Time",            "Time in seconds",                                          PROP_TYPE_EXPOSE);
    List.PropEnumBool   ("Hud\\Timer\\Render Timer",    "Render the Timer",                                         PROP_TYPE_EXPOSE);
    List.PropEnumBool   ("Hud\\Timer\\Start Timer",     "true = Start, False = Stop",                               PROP_TYPE_EXPOSE);
    List.PropEnumFloat  ("Hud\\Timer\\Add Time",        "Add Time",                                                 PROP_TYPE_EXPOSE|PROP_TYPE_DONT_SHOW);
    List.PropEnumFloat  ("Hud\\Timer\\Sub Time",        "Sub Time",                                                 PROP_TYPE_EXPOSE|PROP_TYPE_DONT_SHOW);
    List.PropEnumBool   ("Hud\\Timer\\Timer In Warning State", "Set Timer to a Warning State. Turn Timer Text RED",                     PROP_TYPE_EXPOSE);
    List.PropEnumBool   ("Hud\\Timer\\Timer In Critical State","Set Timer to a Warning State. Flash Timer Text and render it RED",      PROP_TYPE_EXPOSE);
}

//==============================================================================

xbool hud_object::OnProperty( prop_query& rPropQuery )
{
    //----------------------------------------------------------------------
    // Base Object.
    //----------------------------------------------------------------------
    if( object::OnProperty( rPropQuery ) )
    {
        return TRUE;
    }

    // Cycle through all elements.

    s32 i;
    for( i = 0; i < HUD_ELEMENT_NUM_ELEMENTS; i++ )
    {
        if( m_PlayerHuds[ 0 ].m_HudComponents[ i ]->OnProperty( rPropQuery ) )
        {
            return TRUE;
        }
    }

    //----------------------------------------------------------------------
    // Binary text resource.    
    //----------------------------------------------------------------------
    if( rPropQuery.IsVar( "Hud\\Binary Text Resource\\Binary Text Rsc []" ) )
    {
        if( rPropQuery.IsRead() )
        {
            rPropQuery.SetVarExternal( m_hBinaryTextRsc[ rPropQuery.GetIndex(0) ].GetName(), RESOURCE_NAME_SIZE );
        }
        else
        {
            const char* pStr = rPropQuery.GetVarExternal();

            if( x_strlen( pStr ) )
            {
                xstring FileName( pStr );

                if( FileName.Find( '.' ) == -1 )
                    FileName += ".string";

                m_hBinaryTextRsc[ rPropQuery.GetIndex(0) ].SetName( (const char*)FileName );

                // Load the binary text resource.
                if( m_hBinaryTextRsc[ rPropQuery.GetIndex(0) ].IsLoaded() == FALSE )
                    m_hBinaryTextRsc[ rPropQuery.GetIndex(0) ].GetPointer();
            }
        }

        return TRUE;
    }

    //----------------------------------------------------------------------
    // Timer
    //----------------------------------------------------------------------
    if( rPropQuery.VarGUID("Hud\\Timer\\Trigger Guid",      m_TimerTriggerGuid) )
        return TRUE;

    if( rPropQuery.VarFloat("Hud\\Timer\\Time",             m_TimerTime)        )
        return TRUE;

    if( rPropQuery.VarBool("Hud\\Timer\\Render Timer",      m_RenderTimer)      )
        return TRUE;

    if( rPropQuery.VarBool("Hud\\Timer\\Start Timer",       m_TimerActive)      )
        return TRUE;

    if( rPropQuery.VarFloat("Hud\\Timer\\Add Time",         m_TimerAdd )        )
        return TRUE;

    if( rPropQuery.VarFloat("Hud\\Timer\\Sub Time",         m_TimerSub )        )
        return TRUE;

    if( rPropQuery.VarBool("Hud\\Timer\\Timer In Warning State", m_TimerWarning ))
        return TRUE;
    if( rPropQuery.VarBool("Hud\\Timer\\Timer In Critical State", m_TimerCritical ) )
        return TRUE;

    return FALSE;
}

//==============================================================================

void hud_object::GetBinaryResourceName( xstring& String )
{
    for( s32 i = 0; i < MAX_BIN_TXT_RSC; i++ )
    {
        if( x_strlen( m_hBinaryTextRsc[i].GetName() ) )
        {
            xstring tStr( m_hBinaryTextRsc[i].GetName() );

            // Get rid of the the extension.
            s32 DotIndex = tStr.Find('.');
            if( DotIndex != -1 )
                tStr = tStr.Left( DotIndex );

            String += tStr;
            String += '\0';
        }
    }

    String += '\0';
}

//==============================================================================

// Start or stop a HUD element pulsing 
void hud_object::SetElementPulseState( s32 ElementID, xbool DoPulse )
{
    ASSERTS( IN_RANGE( 0, ElementID, HUD_ELEMENT_NUM_ELEMENTS - 1), "Element ID out of range" );

    // This can only be used on the first HUD.
    m_PlayerHuds[ 0 ].m_HudComponents[ ElementID ]->m_bPulsing = DoPulse;
}                   

//==============================================================================

// Just because the editor wants this.
bbox hud_object::GetLocalBBox( void ) const
{
    return bbox( GetPosition(), 50.0f );
}

//==============================================================================

player_hud& hud_object::GetPlayerHud( s32 LocalSlot )
{
    s32 i = 0;
    for( i = 0; i < m_NumHuds; i++ )
    {
        if( m_PlayerHuds[ i ].m_LocalSlot == LocalSlot )
        {
            return m_PlayerHuds[ i ];
        }
    }

    ASSERTS( FALSE, "No valid hud for player found!" );
    return m_PlayerHuds[ 0 ];
}

//==============================================================================

void    hud_object::SetupLetterBox( xbool On, f32 SlideTime )
{
    // We don't want any nasty division by zero bugs!
    if( SlideTime == 0.0f )
    {
        m_LetterBoxCurrTime  = 1.0f;
        m_LetterBoxTotalTime = 1.0f;
    }

    f32 CurrPercentage = m_LetterBoxCurrTime / m_LetterBoxTotalTime;

    // If its on already, just change the effective speed of sliding.
    if( On == m_bLetterBoxOn )
    {
        m_LetterBoxCurrTime  = SlideTime * CurrPercentage;
        m_LetterBoxTotalTime = SlideTime;
    }

    // Looks like we have to reverse its direction too.
    else 
    {
        CurrPercentage = 1.0f - CurrPercentage;
    }

    m_LetterBoxCurrTime  = SlideTime * CurrPercentage;
    m_LetterBoxTotalTime = SlideTime;
    m_bLetterBoxOn = On;
}

//==============================================================================

void hud_object::SetObjectiveText( s32 TableNameIndex, s32 TitleStringIndex )
{
    m_ObjectiveTableNameIndex = TableNameIndex;
    m_ObjectiveTitleStringIndex = TitleStringIndex;
}

//==============================================================================

void hud_object::RenderObjectiveText( void )
{
    if( m_ObjectiveTime > 0.0f )
        return;

    if( (m_ObjectiveTableNameIndex != -1) &&
        (m_ObjectiveTitleStringIndex != -1) )
    {
        g_GameTextMgr.DisplayMessage( g_StringMgr.GetString( m_ObjectiveTableNameIndex ),
                                       g_StringMgr.GetString( m_ObjectiveTitleStringIndex ) );
        m_ObjectiveTime = 6.0f;
    }
}

//==============================================================================
