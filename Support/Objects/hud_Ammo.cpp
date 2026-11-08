//==============================================================================
//
//  hud_Ammo.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================
//==============================================================================
// INCLUDES
//==============================================================================

#include "hud_Ammo.hpp"
#include "UI/ui_renderer.hpp"
#include "HudObject.hpp"
#include "GameLib/RenderContext.hpp"
#include "UI/ui_font.hpp"

//==============================================================================
// STORAGE
//==============================================================================

xcolor  s_AmmoColor         (  0, 255, 255, 255 );

extern xcolor  s_ShadowColor;//       ( 49,  49,  49, 255 );
extern s32     s_ShadowOffsetX;//     = 2;
extern s32     s_ShadowOffsetY;//     = 2;

xcolor                      hud_ammo::m_FragGrenadeColor;
xcolor                      hud_ammo::m_GravGrenadeColor;

static s32 LMAG = 64;
static s32 TMAG = 22;
static s32 LRES = 27;
static s32 TRES = 19;

//==============================================================================
// FUNCTIONS
//==============================================================================
hud_ammo::hud_ammo( void )
{
    m_AmmoHudInited             = FALSE;
    m_CurrentWeaponAmmo         = 0;
    m_GrenadeAmmoJBean          = 0;
    m_GrenadeAmmoFrag           = 0;        

    m_MultiPlayerHud            = FALSE;


    for( s32 a = 0 ; a < NUM_SHADOW_AMMO_RENDERS ; a ++ )
        m_WeaponAmmoQAmounts[a] = 0;

    // initialize ammo hud color
    m_AmmoHudColor              = g_HudColor;

    m_WarningClip = FALSE;
    m_WarningResv = FALSE;
    m_WarningNads = FALSE;
    m_CriticalClip = FALSE;
    m_CriticalResv = FALSE;
    m_CriticalNads = FALSE;
    m_WarningClipALPHA = 0;
    m_WarningResvALPHA = 0;
    m_WarningNadsALPHA = 0;
    m_CriticalSoundID = -1;
}

//==============================================================================

void hud_ammo::Init( void )
{
    slot_id SlotID  = g_ObjMgr.GetFirst( object::TYPE_HUD_OBJECT );

    if( SlotID != SLOT_NULL )
    {
        object* pObj    = g_ObjMgr.GetObjectBySlot( SlotID );
        hud_object& Hud = hud_object::GetSafeType( *pObj );

        if( Hud.m_Initialized )
        {
            if( Hud.GetNumPlayers() > 2 )
                m_MultiPlayerHud = TRUE;
            else
                m_MultiPlayerHud = FALSE;
        }
    }

    // Load Art
    if( m_MultiPlayerHud )
    {
        m_AmmoBoxBMP.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX_Q.XBMP"));     // frame
        m_AmmoBoxBMP_Clip.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX_000_Q.XBMP"));
        m_AmmoBoxBMP_Resv.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX_002_Q.XBMP"));
        m_AmmoBoxBMP_Nads.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX_001_Q.XBMP"));

        // initialize nade icons
        m_FragAmmoIcon.SetName  (PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_GRENADE_FRAG.XBMP"));    
        m_JBeanAmmoIcon.SetName (PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_GRENADE_JELLYBEAN.XBMP"));
        m_JBeanXAmmoIcon.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_GRENADE_JELLYBEANX.XBMP"));
    }
    else
    {    
        m_AmmoBoxBMP.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX.XBMP"));     // frame
        m_AmmoBoxBMP_Clip.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX_000.XBMP"));
        m_AmmoBoxBMP_Resv.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX_002.XBMP"));
        m_AmmoBoxBMP_Nads.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_AMMOBOX_001.XBMP"));

        // initialize nade icons
        m_FragAmmoIcon.SetName  (PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_GRENADE_FRAG.XBMP"));    
        m_JBeanAmmoIcon.SetName (PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_GRENADE_JELLYBEAN.XBMP"));
        m_JBeanXAmmoIcon.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_GRENADE_JELLYBEANX.XBMP"));
    }

    // Set the offsets for the ammo text to appear.
    if( m_MultiPlayerHud )
    {    
        m_LeftMag       = LMAG; 
        m_TopMag        = TMAG;                          
        m_LeftReserve   = LRES; 
        m_TopReserve    = TRES;
        m_LeftNade      = 0;
        m_TopNade       = 0;
    }
    else
    {
        m_LeftMag       = 81; 
        m_TopMag        = 24;
        m_LeftReserve   = 32; 
        m_TopReserve    = 19;
        m_LeftNade      = 71;
        m_TopNade       = 18;
    }

    m_AmmoHudInited = TRUE;
}

//==============================================================================

void hud_ammo::OnRender( player* pPlayer )
{
    // Don't render any ammo stuff if the player is mutated.
    if( 
        pPlayer->GetCurrentWeapon2() == INVEN_WEAPON_MUTATION ||
        pPlayer->GetCurrentWeapon2() == INVEN_WEAPON_SCANNER || 
        pPlayer->GetCurrentWeapon2() == INVEN_WEAPON_TRA
      )
    {
        return;
    }

    RenderAmmoBar       ( pPlayer );
    RenderNades         ( pPlayer, pPlayer->GetCurrentGrenadeType2() );
    RenderAmmoString    ( pPlayer );
}

//==============================================================================

void hud_ammo::OnAdvanceSimulation( player* pPlayer, f32 DeltaTime )
{
    // First pass we need to setup some stuff based on the number
    // of players.
    if( !m_AmmoHudInited )
        Init();

    UpdateAmmoState( pPlayer, DeltaTime );
    UpdateWarningAlpha( DeltaTime );
}

//==============================================================================

void hud_ammo::UpdateAmmoState( player* pPlayer, f32 DeltaTime )
{
    (void)DeltaTime;
    new_weapon* pWeapon = pPlayer->GetCurrentWeaponPtr();
    const xbool ShowAmmo = pWeapon &&
                           (pPlayer->GetCurrentWeapon2() != INVEN_WEAPON_MUTATION) &&
                           (pPlayer->GetCurrentWeapon2() != INVEN_WEAPON_SCANNER) &&
                           (pPlayer->GetCurrentWeapon2() != INVEN_WEAPON_TRA);

    if( !ShowAmmo )
    {
        m_WarningClip = FALSE;
        m_WarningResv = FALSE;
        m_CriticalClip = FALSE;
        m_CriticalResv = FALSE;
        m_WarningClipALPHA = 0;
        m_WarningResvALPHA = 0;
        m_CurrentWeaponAmmo = 0;
        m_WeaponAmmoQAmounts[ 0 ] = 0;

        if( m_CriticalSoundID != -1 )
        {
            g_AudioMgr.Release( m_CriticalSoundID, 0.0f );
            m_CriticalSoundID = -1;
        }
        return;
    }

    s32 AmmoMax;
    s32 ClipMax;
    pWeapon->GetAmmoState( new_weapon::AMMO_PRIMARY,
                           m_CurrentWeaponAmmo,
                           AmmoMax,
                           ClipMax,
                           m_WeaponAmmoQAmounts[0] );

    const s32 AmmoResv = m_CurrentWeaponAmmo - m_WeaponAmmoQAmounts[0];
    const s32 AmmoClip = m_WeaponAmmoQAmounts[0];
    const xbool WarningsOverRide = (AmmoMax == ClipMax) ||
                                    (pPlayer->GetCurrentWeapon2() == INVEN_WEAPON_MESON_CANNON);
    const s32 ClipWarning  = MAX( 4, (s32)(0.25f * (f32)ClipMax) );
    const s32 ResvWarning  = (s32)(0.25f * (f32)AmmoMax);

    if( (AmmoClip < ClipWarning) && !m_WarningClip )
    {
        g_AudioMgr.Play( "Ammo_Low", TRUE );
        m_WarningClip      = TRUE;
        m_WarningClipALPHA = 0;
    }
    else if( AmmoClip >= ClipWarning )
    {
        m_WarningClip  = FALSE;
        m_CriticalClip = FALSE;
    }

    if( WarningsOverRide )
    {
        m_WarningResv  = FALSE;
        m_CriticalResv = FALSE;
    }
    else
    {
        m_CriticalResv = (AmmoResv == 0);
        if( (AmmoResv < ResvWarning) && !m_WarningResv )
        {
            g_AudioMgr.Play( "Ammo_Low", TRUE );
            m_WarningResv      = TRUE;
            m_WarningResvALPHA = 0;
        }
        else if( AmmoResv >= ResvWarning )
        {
            m_WarningResv  = FALSE;
            m_CriticalResv = FALSE;
        }
    }

    if( (AmmoClip == 0) && (AmmoResv == 0) )
    {
        if( m_CriticalSoundID == -1 )
            m_CriticalSoundID = g_AudioMgr.Play( "Ammo_Out_Loop", TRUE );
        m_CriticalClip = TRUE;
    }
    else
    {
        if( m_CriticalSoundID != -1 )
        {
            g_AudioMgr.Release( m_CriticalSoundID, 0.0f );
            m_CriticalSoundID = -1;
        }
        m_CriticalClip = FALSE;
    }

}

//==============================================================================

void hud_ammo::UpdateWarningAlpha( f32 DeltaTime )
{
    const f32 AlphaStep = 960.0f * DeltaTime;
    m_WarningClipALPHA = m_WarningClip
                       ? MIN( m_WarningClipALPHA + AlphaStep, 100.0f )
                       : MAX( m_WarningClipALPHA - AlphaStep,   0.0f );
    m_WarningResvALPHA = m_WarningResv
                       ? MIN( m_WarningResvALPHA + AlphaStep, 100.0f )
                       : MAX( m_WarningResvALPHA - AlphaStep,   0.0f );
}

//==============================================================================

xbool hud_ammo::OnProperty( prop_query& rPropQuery )
{
    (void)rPropQuery;
    return FALSE;
}

//==============================================================================

void hud_ammo::OnEnumProp( prop_enum&  List )
{
    (void)List;
}

//==============================================================================
static s32 HUD_AMMO_X = -134;
static s32 HUD_AMMO_Y = -43;
static s32 HUD_AMMO_X_G = -40;
static s32 HUD_AMMO_Y_G = -39;
static s32 HUD_AMMO_X_G_Q = -36;
static s32 HUD_AMMO_Y_G_Q = -39;
void hud_ammo::RenderNades( player* pPlayer, inven_item NadType )
{
    (void)pPlayer;
    (void)NadType;

    texture*    pGrenadeTexture = NULL;
    s32         GrnCount    = 0;
    xcolor      GrnColor;

    static xcolor FRAG_GRENADE_COLOR = g_HudColor;
    
    switch( NadType )
    {
        case INVEN_GRENADE_FRAG:
            pGrenadeTexture = m_FragAmmoIcon.GetPointer();
        break;
        case INVEN_GRENADE_JBEAN:
            pGrenadeTexture = m_JBeanAmmoIcon.GetPointer();
        break;
        case INVEN_GRENADE_JBEAN_ENHANCE:
            pGrenadeTexture = m_JBeanXAmmoIcon.GetPointer();
        break;

        default:
            ASSERT(0);
        break;
    }

    GrnCount    = (s32)pPlayer->GetInventory2().GetAmount( NadType );
    GrnColor    = XCOLOR_WHITE;

    if( pGrenadeTexture == NULL )
        return;

    const xbitmap& GrenadeBitmap = pGrenadeTexture->m_bitmap;
    vector2 WH( (f32)GrenadeBitmap.GetWidth(), (f32)GrenadeBitmap.GetHeight() );
    vector3 Pos(0,0,0.01f);
    if( m_MultiPlayerHud )
    {
        Pos.GetX() = m_XPos + HUD_AMMO_X_G_Q;
        Pos.GetY() = m_YPos + HUD_AMMO_Y_G_Q;
    }
    else
    {    
        Pos.GetX() = m_XPos + HUD_AMMO_X_G;
        Pos.GetY() = m_YPos + HUD_AMMO_Y_G;
    }
    g_UIRenderer.DrawImage( *pGrenadeTexture,
                            vector2( Pos.GetX(), Pos.GetY() ),
                            WH,
                            vector2( 0.0f, 0.0f ),
                            vector2( 1.0f, 1.0f ),
                            GrnColor );

    static xstring GrnCountString;
    GrnCountString = (const char *)xfs("%d", GrnCount);
    irect IRECT;
    s32 SomeR = -12;
    s32 SomeB = -22;

    if( m_MultiPlayerHud )
        SomeR = -9;

    IRECT.Set( (s32)m_XPos, (s32)m_YPos, SomeR+(s32)m_XPos, SomeB+(s32)m_YPos );
    xcolor white = XCOLOR_WHITE;
    RenderLine( (const xwchar*)(xwstring((const char*)GrnCountString)), IRECT, 255, white, 1, ui_font::h_right|ui_font::v_bottom, FALSE );
}

//==============================================================================
static xcolor AMMO_INSIDE_COLOR = xcolor(0,31,0,204);
void hud_ammo::RenderAmmoBar( player* pPlayer )
{
    (void)pPlayer;
    static xcolor AMMO_FRAME_COLOR = g_HudColor;

    texture* pFrameTexture = m_AmmoBoxBMP.GetPointer();
    if( pFrameTexture == NULL )
        return;

    const xbitmap& FrameBitmap = pFrameTexture->m_bitmap;
    s32 BitmapWidth  = FrameBitmap.GetWidth();
    s32 BitmapHeight = FrameBitmap.GetHeight();

    // Draw the ammo bar.
    vector2 WH( (f32)(BitmapWidth), (f32)(BitmapHeight) );

    vector3 Pos;

    Pos.GetX() = m_XPos+HUD_AMMO_X; 
    Pos.GetY() = m_YPos+HUD_AMMO_Y; 
   
    const vector2 ScreenPos( Pos.GetX(), Pos.GetY() );
    const vector2 UV0( 0.0f, 0.0f );
    const vector2 UV1( 1.0f, 1.0f );

    const auto DrawLayer = [&]( texture* pTexture, const xcolor& Color )
    {
        if( pTexture )
            g_UIRenderer.DrawImage( *pTexture, ScreenPos, WH, UV0, UV1, Color,
                                    0.0f, UI_BLEND_ALPHA, UI_SAMPLER_LINEAR_CLAMP );
    };

    // Clip Box
    texture* pClipTexture = m_AmmoBoxBMP_Clip.GetPointer();
    DrawLayer( pClipTexture, AMMO_INSIDE_COLOR );
    if( m_WarningClip )
    {
        xcolor warning = xcolor( 200,50,0,(u8)m_WarningClipALPHA);
        if( m_CriticalClip )
        {
            warning.A = (u8)MIN( hud_object::m_PulseAlpha, 100.0f );
            DrawLayer( pClipTexture, warning );
        }
        else
            DrawLayer( pClipTexture, warning );
    }    
    else
    {
        xcolor warning = xcolor( 200,50,0,(u8)m_WarningClipALPHA);
        DrawLayer( pClipTexture, warning );
    }

    // Box 2
    texture* pReserveTexture = m_AmmoBoxBMP_Resv.GetPointer();
    DrawLayer( pReserveTexture, AMMO_INSIDE_COLOR );
    if( m_WarningResv )
    {
        xcolor warning = xcolor( 200,50,0,(u8)m_WarningResvALPHA);
        if( m_CriticalResv )
        {
            warning.A = (u8)MIN( hud_object::m_PulseAlpha, 100.0f );
            DrawLayer( pReserveTexture, warning );
        }
        else
            DrawLayer( pReserveTexture, warning );
    }    
    else
    {
        xcolor warning = xcolor( 200,50,0,(u8)m_WarningResvALPHA);
        DrawLayer( pReserveTexture, warning );
    }

    // Box 3
    texture* pGrenadesTexture = m_AmmoBoxBMP_Nads.GetPointer();
    if( m_WarningNads )
        DrawLayer( pGrenadesTexture, xcolor(180,50,0,100) );
    else
        DrawLayer( pGrenadesTexture, AMMO_INSIDE_COLOR );


    // Frame
    DrawLayer( pFrameTexture, AMMO_FRAME_COLOR );
}

//==============================================================================

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

//==============================================================================

void hud_ammo::RenderAmmoString( player* pPlayer )
{
    new_weapon* pWeapon = pPlayer->GetCurrentWeaponPtr();
    if( pWeapon == NULL  )
        return;

    s32 AmmoMax;
    s32 ClipMax;
    s32 AmmoResv;
    s32 AmmoClip;
    pWeapon->GetAmmoState(new_weapon::AMMO_PRIMARY,m_CurrentWeaponAmmo,AmmoMax,ClipMax,m_WeaponAmmoQAmounts[0]);

    AmmoResv = m_CurrentWeaponAmmo - m_WeaponAmmoQAmounts[0];
    AmmoClip = m_WeaponAmmoQAmounts[0];

    //
    // Render the ammo counts.
    //
#ifndef X_EDITOR
    {
        static xstring AmmoReserveString;
        static xstring AmmoMagazineString;
        AmmoReserveString  = (const char *)xfs( "%d", AmmoResv );
        AmmoMagazineString = (const char *)xfs( "%d", AmmoClip );

        // Current loaded ammo.
        irect MagazineRect;
        MagazineRect.l = (s32)m_XPos - m_LeftMag;
        MagazineRect.t = (s32)m_YPos - m_TopMag;
        MagazineRect.SetHeight( 50 );
        MagazineRect.SetWidth ( 20 );

        xcolor cAmmoColor = g_HudColor;     
        if( m_MultiPlayerHud )
            RenderLine( (const xwchar*)(xwstring((const char*)AmmoMagazineString)), MagazineRect, 255, cAmmoColor, 1, ui_font::h_right|ui_font::v_top, FALSE );
        else
            RenderLine( (const xwchar*)(xwstring((const char*)AmmoMagazineString)), MagazineRect, 255, cAmmoColor, 2, ui_font::h_right|ui_font::v_top, FALSE );

        // Ammo in reserve.
        irect ReserveRect;
        ReserveRect.l = (s32)m_XPos - m_LeftReserve;
        ReserveRect.t = (s32)m_YPos - m_TopReserve;
        ReserveRect.SetHeight( 50 );
        ReserveRect.SetWidth ( 20 );
        RenderLine( (const xwchar*)(xwstring((const char*)AmmoReserveString)), ReserveRect, 255, g_HudColor, 1, ui_font::h_right|ui_font::v_top, FALSE );
    }
#endif
}

//==============================================================================

void hud_ammo::RenderFlashlight( player* pPlayer )
{
    (void)pPlayer;
}

//==============================================================================
