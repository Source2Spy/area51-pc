//==============================================================================
//
//  hud_Scanner.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "hud_Scanner.hpp"
#include "HudObject.hpp"
#include "UI/ui_renderer.hpp"
#include "WeaponScanner.hpp"
#include "Characters/Character.hpp"
#include "Corpse.hpp"
#include "StringMgr/StringMgr.hpp"
#include "NetworkMgr/NetworkMgr.hpp"
#include "NetworkMgr/Messages/MsgMgr.hpp"

#ifndef X_EDITOR
#include "../../Apps/GameApp/Config.hpp"
#endif

//==============================================================================
//  STORAGE
//==============================================================================

rhandle<texture>            hud_scanner::m_InnerBmp;
rhandle<texture>            hud_scanner::m_OuterBmp;
rhandle<texture>            hud_scanner::m_OuterBmpFlipped;

//#define BAR_LENGTH          84.0f
//#define BAR_HEIGHT          4.0f

#define NUM_CONTROL_POINTS 5

xcolor  g_ScannerColors[ NUM_CONTROL_POINTS ] = 
{   
xcolor( 100, 255, 148 ),
xcolor( 100, 255, 148 ),
xcolor( 240, 150,  40 ),
xcolor( 230, 40,   40 ),
xcolor( 255, 40,   40 ) 
};

extern tweak_handle Lore_Max_Detect_DistanceTweak;
extern tweak_handle Lore_Min_Detect_DistanceTweak;

// our random that we can seed
static xrandom Random;

//==============================================================================

hud_scanner::hud_scanner ( void )
{
    //
    // Initialize bar to default values.
    //
    m_Bar.m_CurrentVal    = 0.0f; 
    m_Bar.m_TargetVal     = 0.0f; 
    m_Bar.m_StartVal      = 0.0f;

    m_Bar.m_CurrentPhase  = 0.0f;
    m_Bar.m_PhaseSign     = 1.0f;

    m_Bar.m_OuterColor    = XCOLOR_PURPLE;
    m_Bar.m_InnerColor    = XCOLOR_PURPLE;

    m_Bar.m_MaxVal        = 1.0f;

    m_Bar.m_CurrentDisplayPercentage = 0.0f;

    //
    // Initialize the scanner bar.
    //
    {
        m_Bar.m_OuterColor = g_HudColor;
        m_Bar.m_bFlip      = FALSE;
    }
    //m_InnerBmp.SetName( PRELOAD_FILE( "HUD/GENERAL/HUD_HEALTH_BAR.XBMP" ) );
    //m_OuterBmp.SetName( PRELOAD_FILE( "HUD/GENERAL/HUD_HEALTH_BAR_OUTLINE.XBMP" ) );
    //m_OuterBmpFlipped.SetName( PRELOAD_FILE( "HUD/GENERAL/HUD_HEALTH_BAR_OUTLINE02.XBMP" ) );

    m_ScannerBarBmp.SetName(PRELOAD_FILE("HUD/CAMPAIGN/HUD_CAMPAIGN_SCAN.XBMP"));
    m_ScannerBmpAlpha = 0.0f;
    m_ScanPeakUpdateTime = 0.0f;
    m_ScanPeak = 0;
    m_ScanPeakUpdate = TRUE;

    m_bRenderScanData = FALSE;

    m_ScanDataTimeout = 0.0f;
    m_bJustAirSample = FALSE;

    m_LoreDetected = FALSE;
    m_LoreBarAmount = 0;
    m_LoreDistance = 0.0f;
    m_LoreMaxDistance = 1.0f;
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
static xbool g_RenderGeiger = TRUE;
void hud_scanner::OnRender( player* pPlayer )
{


    if( pPlayer->GetCurrentWeapon2() == INVEN_WEAPON_SCANNER )
    {
        //RenderBar( );
        
        // should we render scan data
        if( m_bRenderScanData )
        {
            RenderScanInfo(pPlayer);
        }
    }

    // do we render the gieger?
    if( g_RenderGeiger )
    {
#ifndef X_EDITOR
        if( !g_NetworkMgr.IsOnline() )
#endif // X_EDITOR
            RenderGeiger(pPlayer);
    }    
}

#define MAX_SCAN_CHAR 256
char g_ScanAirInfo[MAX_SCAN_LINE][MAX_SCAN_CHAR] = { "Air Sample:",
                                                      "----------------",
                                                      "  ",                                            
                                                      "28.0% - Oxygen",
                                                      "71.0% - Nitrogen",
                                                      "01.0% - Trace",
                                                      "00.0% - Unknown" };

struct scan_ID_table_entry
{
    s32         Item;
    const char* pIdentifier;
};

static scan_ID_table_entry s_ScanIDTable[] =
{
    { SSID_DNA          ,    "CS_DNA_"              },
    { SSID_TITLE        ,    "CS_TITLE_"            },
    { SSID_JOB          ,    "CS_JOB_"              },
    { SSID_SPECIES      ,    "CS_SPECIES_"          },
    { SSID_BLOOD        ,    "CS_BLOOD_"            },
    { SSID_HEART        ,    "CS_HEARTNORMAL_"      },
    { SSID_HEART_FIGHT  ,    "CS_HEARTFIGHT_"       },
    { SSID_EQUIP        ,    "CS_EQUIP_"            },
};

s32 g_ScanFontNum       = 0;
s32 g_ScanLineOffY      = 25;
s32 g_ScanInfoBGSize    = 500;
s32 g_ScanInfoSize      = 185;
f32 g_ScanOffX          = 10.0f;
f32 g_ScanOffY          = -200.0f;
f32 g_ScanBGAlpha       = 0.65f;
//==============================================================================
void hud_scanner::RenderScanInfo( player *pPlayer )
{
    (void)pPlayer;
}

//==============================================================================
f32 g_ScanDataRenderTimeout = 5.0f;
void hud_scanner::NotifyScanComplete( player* pPlayer, guid TheGuid )
{
    (void)TheGuid;
    m_ScanDataTimeout = g_ScanDataRenderTimeout;

    m_bJustAirSample = ( TheGuid == NULL_GUID );

    if( !m_bJustAirSample )
    {
        BuildDataLines( pPlayer, TheGuid );
    }
    else
    {
#ifndef X_EDITOR
        BuildAirSample( pPlayer );
#endif
    }
}

//==============================================================================
void hud_scanner::BuildAirSample( player *pPlayer )
{
#ifndef X_EDITOR
    
    SetAirSampleRandSeed(pPlayer);

    f32 TotalPct = 100.0f;
    
    xwstring theText0( g_StringTableMgr("scan", "CS_AIR_TITLE") );

    // Oxygen
    f32 Oxygen = 21.0f + Random.frand( -1.0f, 1.0f );
    TotalPct -= Oxygen;
    xwstring theText3( xfs( "%05.2f%", Oxygen ) );
    theText3 += g_StringTableMgr("scan", "CS_AIR_OXYGEN");

    // Nitrogen
    f32 Nitrogen = TotalPct + Random.frand( -5.0f, 0.0f );
    TotalPct -= Nitrogen;
    xwstring theText4( xfs( "%02.2f", Nitrogen) );
    theText4 += g_StringTableMgr("scan", "CS_AIR_NITROGEN");

    // trace materials
    f32 TraceMat = TotalPct;
    if( TraceMat >= 1.0f )
        TraceMat += Random.frand( -1.0f, 0.0f );
    else
        TraceMat = 0.0f;

    TotalPct -= TraceMat;    
    xwstring theText5( xfs( "%02.2f", TraceMat) );
    theText5 += g_StringTableMgr("scan", "CS_AIR_TRACE");

    // whatever is left
    xwstring theText6( xfs( "%02.2f", TotalPct ) );
    theText6 += g_StringTableMgr("scan", "CS_AIR_UNKNOWN");

    // now print them
    xwstring AirScan(theText0);
    AirScan += '\n';
    AirScan += theText3;
    AirScan += '\n';
    AirScan += theText4;
    AirScan += '\n';
    AirScan += theText5;
    AirScan += '\n';
    AirScan += theText6;

    MsgMgr.Message( MSG_STRING, 0, (saddr)((const xwchar*)AirScan));
#endif
}

//==============================================================================
void hud_scanner::BuildDataLines( player* pPlayer, guid TheGuid )
{
    s32 TheLine = 0;
    char pIdentifier[256] = {'\0'};

    // DNA Line
    {
        BuildScanIdentifier(pIdentifier, pPlayer, SSID_DNA);
        m_DataLine[TheLine]     = g_StringTableMgr("scan", pIdentifier);
    }

    // CS_TITLE_
    {
        BuildScanIdentifier(pIdentifier, pPlayer, SSID_TITLE);
        m_DataLine[TheLine] += " ";
        m_DataLine[TheLine] += g_StringTableMgr("scan", pIdentifier);
        TheLine++;
    }

    // CS_JOB_
    {
        BuildScanIdentifier(pIdentifier, pPlayer, SSID_JOB);
        m_DataLine[TheLine]     = g_StringTableMgr("scan", pIdentifier);
        TheLine++;
    }


    // CS_SPECIES_
    {
        BuildScanIdentifier(pIdentifier, pPlayer, SSID_SPECIES);
        m_DataLine[TheLine]     = g_StringTableMgr("scan", pIdentifier);
        TheLine++;
    }

    // CS_BLOOD_
    {
        // now get person specific blood type "O-"
        m_DataLine[TheLine] = g_StringTableMgr("scan", "CS_BLOOD_TYPE" );
        BuildScanIdentifier(pIdentifier, pPlayer, SSID_BLOOD);

        // append
        s32 PosNeg = (s32) Random.irand( 1, 2 );
        
        m_DataLine[TheLine] += " ";
        m_DataLine[TheLine] +=  g_StringTableMgr("scan", pIdentifier);

        // generic NPCs such as scientists and technicians create their own bloodtypes, add +/-.
        if( x_stristr( pIdentifier, "GENERIC" ) )
        {
            // put positive negative sign on
            if( PosNeg == 1 )
                m_DataLine[TheLine] += "+";
            else
                m_DataLine[TheLine] += "-";
        }

        m_DataLine[TheLine] += " @ ";
    }

    // CS_HEARTNORMAL_
    // CS_HEARTFIGHT_
    {
        // now get person specific heartbeat "73bpm"
        BuildScanIdentifier(pIdentifier, pPlayer, SSID_HEART);

        // if this is a character, prepend the number instead of reading whole value from table
        PrependHeartbeat( pPlayer, TheLine );

        m_DataLine[TheLine]     +=  g_StringTableMgr("scan", pIdentifier);        
        TheLine++;
    }

    // CS_EQUIP_
    {
        // get the generic title string "Equipment: "
        m_DataLine[TheLine]     = g_StringTableMgr("scan", "CS_EQUIPMENT");

        // now get person specific equipment "Hazmat suit, field issue rifle"
        BuildScanIdentifier(pIdentifier, pPlayer, SSID_EQUIP);
        
        m_DataLine[TheLine]     += " ";
        m_DataLine[TheLine]     +=  g_StringTableMgr("scan", pIdentifier);

        // generic NPCs such as scientists and technicians get keys and wallets.
        if( x_stristr( pIdentifier, "GENERIC" ) )
        {            
            // Keys
            m_DataLine[TheLine]     += ", ";
            m_DataLine[TheLine]     += g_StringTableMgr("scan", "CS_EQUIP_GENERIC_COMMON_1");

            // wallet
            m_DataLine[TheLine]     += ", ";
            m_DataLine[TheLine]     += g_StringTableMgr("scan", "CS_EQUIP_GENERIC_COMMON_2");
        }

        actor *pActor = (actor*)g_ObjMgr.GetObjectByGuid( TheGuid );

        // actor with a weapon, get it.
        if( pActor && 
            ( pActor->IsKindOf(player::GetRTTI()) || 
              pActor->IsKindOf(character::GetRTTI()) ) )
        {
            if( pActor->GetCurrentWeaponPtr() )
            {
                inven_item Item = pActor->GetCurrentWeaponPtr()->GetInvenItem();

                // weapon
                m_DataLine[TheLine]     += ", ";
                m_DataLine[TheLine]     += pActor->GetInventory2().ItemToDisplayName( Item );
            }
        }

        TheLine++;
    }

#ifndef X_EDITOR
    //xwstring TotalScanString(m_DataLine[0]);
    //TotalScanString += '\n';
    //TotalScanString += m_DataLine[1];
    //TotalScanString += '\n';
    //TotalScanString += m_DataLine[2];
    //TotalScanString += '\n';
    //TotalScanString += m_DataLine[3];
    //TotalScanString += '\n';
    //TotalScanString += m_DataLine[4];
    //MsgMgr.Message( MSG_STRING, 0, (saddr)((const xwchar*)TotalScanString));

    MsgMgr.Message( MSG_STRING, 0, (saddr)((const xwchar*)m_DataLine[4]));
    MsgMgr.Message( MSG_STRING, 0, (saddr)((const xwchar*)m_DataLine[3]));
    MsgMgr.Message( MSG_STRING, 0, (saddr)((const xwchar*)m_DataLine[2]));
    MsgMgr.Message( MSG_STRING, 0, (saddr)((const xwchar*)m_DataLine[1]));
    MsgMgr.Message( MSG_STRING, 0, (saddr)((const xwchar*)m_DataLine[0]));
#endif
}

//==============================================================================
xbool hud_scanner::IsGenericCharacter( object *pObject )
{
    if( pObject->IsKindOf(character::GetRTTI()) )
    {
        character *pCharacter = (character*)pObject;
        const char* pScanIdentifier = pCharacter->GetScanIdentifier();

        // this is a generic NPC, probably a technician or a scientist
        if( x_stristr( pScanIdentifier, "GENERIC" ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================
void hud_scanner::PrependHeartbeat( player *pPlayer, s32 TheLine )
{
    weapon_scanner *pWeapon = ((weapon_scanner*)pPlayer->GetCurrentWeaponPtr());

    guid ScannedGuid = pWeapon->GetScannedGuid();

    object *pObject = g_ObjMgr.GetObjectByGuid(ScannedGuid);

    // for generic peeps, give them random heartbeat
    if( IsGenericCharacter( pObject ) )
    {
        SetCharacterRandSeed(pObject);

        // get random seeded heartbeat for generic characters
        s32 HBPM = (s32) Random.irand( 63, 75 );

        character *pCharacter = (character*)pObject;
        if( pCharacter->WasRecentlyInCombat() )
        {
            // recently in combat, add 45
            HBPM += 45;
        }

        xwstring theText(xfs( "%d", HBPM ));
        m_DataLine[TheLine] += theText;
    }
}

//==============================================================================
void hud_scanner::SetCharacterRandSeed( object *pObject )
{
    // build character's slot ID
    s32 SlotID = (s32)pObject->GetSlot();
    s32 RandID = SlotID | (SlotID << 7);

    Random.srand( RandID );
}

//==============================================================================
void hud_scanner::SetAirSampleRandSeed( player *pPlayer )
{
    s32 MapID = g_ActiveConfig.GetLevelID();
    s32 ZoneID = (s32)pPlayer->GetZone1();

    s32 RandID = (MapID << 7) | ZoneID;

    Random.srand( RandID );
}

//==============================================================================
xbool hud_scanner::BuildScanIdentifier( char *pIdentifier, player* pPlayer, eScanSectionID Ident )
{    
    weapon_scanner *pWeapon = ((weapon_scanner*)pPlayer->GetCurrentWeaponPtr());

    guid ScannedGuid = pWeapon->GetScannedGuid();

    object *pObject = g_ObjMgr.GetObjectByGuid(ScannedGuid);

    const char* pScanIdentifier = NULL;

    xbool bIsMutant = FALSE;

    // we've scanned ourselves :)
    if( ScannedGuid == pPlayer->GetGuid() )
    {
        if( pPlayer->GetInventory2().HasItem( INVEN_WEAPON_MUTATION ) )
        {
            bIsMutant = TRUE;
            pScanIdentifier = "COLE_MUT";
        }
        else
        {
            if( Ident == SSID_SPECIES ||
                Ident == SSID_DNA )
            {
                pScanIdentifier = "GENERIC";
            }
            else
            {
                pScanIdentifier = "COLE";
            }
        }
    }
    else
    // character
    if( pObject->IsKindOf(character::GetRTTI()) )
    {
        character *pCharacter = (character*)pObject;
        pScanIdentifier = pCharacter->GetScanIdentifier();

        // special excited elevated heartbeat
        if( Ident == SSID_HEART && pCharacter->WasRecentlyInCombat() )
        {
            // recently in combat, different heartbeat
            x_sprintf(pIdentifier, "%s%s", s_ScanIDTable[SSID_HEART_FIGHT].pIdentifier, pScanIdentifier );

            // handled here, done
            return TRUE;        
        }
        else
        if( Ident == SSID_EQUIP )
        {
            // for generic peeps, give them random items
            if( x_stristr( pScanIdentifier, "GENERIC" ) )
            {
                // set the seed
                SetCharacterRandSeed(pObject);

                // get a random equipment index
                s32 ID = (s32) Random.irand( 1, 16 );

                x_sprintf(pIdentifier, "%sGENERIC_%d", s_ScanIDTable[Ident].pIdentifier, ID);

                return TRUE;
            }
        }
        else
        if( Ident == SSID_BLOOD )
        {
            // for generic peeps, give them random blood types
            if( x_stristr( pScanIdentifier, "GENERIC" ) )
            {
                // set the seed
                SetCharacterRandSeed(pObject);

                // get a random equipment index
                s32 ID = (s32) Random.irand( 1, 4 );
                
                x_sprintf(pIdentifier, "%sGENERIC_%d", s_ScanIDTable[Ident].pIdentifier, ID);

                return TRUE;
            }
        }
    }
    else
    if( pObject->IsKindOf(corpse::GetRTTI()) )
    {
        corpse *pCorpse = (corpse*)pObject;
        pScanIdentifier = pCorpse->GetScanIdentifier();

        // no heartbeat if dead
        if( Ident == SSID_HEART )
        {
            x_sprintf(pIdentifier, "CS_HEARTBEAT_CORPSE" );
            return TRUE;
        }
        else
        if( Ident == SSID_EQUIP )
        {
            // for generic peeps, give them random items
            if( x_stristr( pScanIdentifier, "GENERIC" ) )
            {
                // set the seed
                SetCharacterRandSeed(pObject);

                // get a random equipment index
                s32 ID = (s32) Random.irand( 1, 16 );

                x_sprintf(pIdentifier, "%sGENERIC_%d", s_ScanIDTable[Ident].pIdentifier, ID);

                return TRUE;
            }
        }
        else
        if( Ident == SSID_BLOOD )
        {
            // for generic peeps, give them random blood types
            if( x_stristr( pScanIdentifier, "GENERIC" ) )
            {
                // set the seed
                SetCharacterRandSeed(pObject);

                // get a random equipment index
                s32 ID = (s32) Random.irand( 1, 4 );

                x_sprintf(pIdentifier, "%sGENERIC_%d", s_ScanIDTable[Ident].pIdentifier, ID);

                return TRUE;
            }
        }
    }
    else
    {
        // don't know what this is (fall through)
        x_sprintf(pIdentifier, "%sUNKNOWN", s_ScanIDTable[Ident].pIdentifier);
        return TRUE;
    }

    // check to see if we are some form of mutant since we have our own descriptors
    if( x_stristr( pScanIdentifier, "_MUT" )   || 
        x_stristr( pScanIdentifier, "THETA" )  || 
        x_stristr( pScanIdentifier, "BO" )     || 
        x_stristr( pScanIdentifier, "MUTANT" ) )
    {
        bIsMutant = TRUE;
    }

    // Now put it together
    if( Ident == SSID_DNA )
    {
        // use COLE's mutant line for now
        if( x_stristr( pScanIdentifier, "_MUT" ) )
        {
            pScanIdentifier = "COLE_MUT";
        }
        
        if( bIsMutant )
        {
            // mutants, thetas and cole have their own descriptor
            x_sprintf(pIdentifier, "%s%s", s_ScanIDTable[Ident].pIdentifier, pScanIdentifier );
        }
        else
        {
            x_sprintf(pIdentifier, "%sGENERIC", s_ScanIDTable[Ident].pIdentifier);
        }
    }
    else
    if( Ident == SSID_SPECIES )
    {
        // if we're a mutant we don't know what species we are
        if( bIsMutant )
        {
            x_sprintf(pIdentifier, "%sUNKNOWN", s_ScanIDTable[Ident].pIdentifier);
        }
        else
        {
            x_sprintf(pIdentifier, "%sGENERIC", s_ScanIDTable[Ident].pIdentifier );
        }
    }    
    else
    {
        // fell through, pull full descriptor from table
        x_sprintf(pIdentifier, "%s%s", s_ScanIDTable[Ident].pIdentifier, pScanIdentifier );
    }
        
    return TRUE;
}

//==============================================================================

f32 g_GeigerSize    = 70.0f;
f32 g_GeigerBGSize  = 6.0f;

void hud_scanner::RenderGeiger( player* pPlayer )
{
    (void)pPlayer;

    // Keep rendering the last reading until the fade-out has finished.
    if( !m_LoreDetected && (m_ScannerBmpAlpha == 0) )
        return;

    // Draw Scanner Boarder
    texture* pTexture = m_ScannerBarBmp.GetPointer();
    if( pTexture == NULL )
        return;

    const xbitmap& Bitmap = pTexture->m_bitmap;
    s32 BitmapWidth  = Bitmap.GetWidth();
    s32 BitmapHeight = Bitmap.GetHeight();
    vector2 WH( (f32)(BitmapWidth), (f32)(BitmapHeight) );
    vector2 Pos;
    static s32 HUD_SCANNER_X = -128;
    static s32 HUD_SCANNER_Y = -180;//-132;
    Pos.X = m_XPos+HUD_SCANNER_X;
    Pos.Y = m_YPos+HUD_SCANNER_Y;
    static xcolor HUD_LORE_PIC_COLOR = g_HudColor;
    HUD_LORE_PIC_COLOR.A = (u8)m_ScannerBmpAlpha;
    g_UIRenderer.DrawImage( *pTexture, Pos, WH,
                            vector2( 0.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                            HUD_LORE_PIC_COLOR, 0.0f, UI_BLEND_ALPHA, UI_SAMPLER_POINT_CLAMP );

    if( m_LoreDetected )
    {
        const f32 Proximity = 1.0f - (m_LoreDistance / m_LoreMaxDistance);
        m_LoreBarAmount = (s32)x_clamp( Proximity * (g_GeigerSize + x_frand( -2.0f, 2.0f )),
                                      0.0f, g_GeigerSize );
    }
    RenderLoreBar( m_LoreBarAmount );
}

//==============================================================================
void hud_scanner::RenderLoreBar( s32 AmountPercent )
{
    irect R;
    static s32 HUD_S_X = -23;
    static s32 HUD_S_Y = -64;//-16;
    static s32 HUD_S_W = 8;

    s32 X = (s32)m_XPos + HUD_S_X;
    s32 Y = (s32)m_YPos + HUD_S_Y;

    s32 WorkingX = (s32)X;
    s32 BarLength = AmountPercent;

    // Render Bar
    R.Set( 
        WorkingX, 
        Y-BarLength,
        WorkingX+HUD_S_W, 
        Y
        );

    static xcolor HUD_LORE_BAR = xcolor(80, 150, 150, 30);
    static xcolor HUD_LORE_BAR_HIGH = xcolor(200, 80, 80, 200);
    xcolor bar = Interpolate(HUD_LORE_BAR, HUD_LORE_BAR_HIGH, AmountPercent / g_GeigerSize);
    bar.A = (u8)(bar.A * (m_ScannerBmpAlpha / 255.0f));
    g_UIRenderer.DrawGradientRect( R, bar, bar, bar, bar );

    if( Y - BarLength < m_ScanPeak )
        m_ScanPeak = Y - BarLength;

    if( m_ScanPeakUpdate )
    {
        m_ScanPeak = Y - BarLength;
        m_ScanPeakUpdate = FALSE;
    }

    // Render Peak
    R.Set(
        WorkingX,
        m_ScanPeak-2,
        WorkingX+HUD_S_W,
        m_ScanPeak
        );

    xcolor PeakColor = g_HudColor;
    PeakColor.A = (u8)(PeakColor.A * (m_ScannerBmpAlpha / 255.0f));
    g_UIRenderer.DrawRect( R, PeakColor );
    
}
//==============================================================================

void hud_scanner::RenderBar( void )
{
    static s32 TweakY = 15;
    f32 YOffset = (16.0f) + TweakY;

    f32 BarPercentage = ((f32)m_Bar.m_TargetVal /  m_Bar.m_MaxVal);

    texture* pTexture;

    if( !m_Bar.m_bFlip )
        pTexture = m_OuterBmp.GetPointer();
    else
        pTexture = m_OuterBmpFlipped.GetPointer();

    //
    // Draw the border.
    //
    if( pTexture )
    {
        const xbitmap& Bitmap = pTexture->m_bitmap;
        vector2 WH( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() );

        vector2 Pos;

        Pos.X  = m_XPos + 5.0f;
        Pos.Y  = m_YPos - YOffset;

        xcolor DisplayColor = m_Bar.m_OuterColor;
        vector2 TL = vector2( 0.0f, 0.0f );
        vector2 BR = vector2( 1.0f, 1.0f );

        // check for pulsing
        if( m_bPulsing )
        {
            xcolor PulseColor( DisplayColor );
            PulseColor.A = (u8)(((f32)PulseColor.A / 255) * hud_object::m_PulseAlpha);
            DisplayColor = PulseColor;
        }
        g_UIRenderer.DrawImage( *pTexture, Pos, WH, TL, BR, DisplayColor,
                                0.0f, UI_BLEND_ALPHA, UI_SAMPLER_LINEAR_CLAMP );
    }

    pTexture = m_InnerBmp.GetPointer();

    // Image constants
    static f32 BarWidth      = 115.0f;
    static f32 BarHeight     = 8.0f;
    static f32 BarXOffset    = 3.0f;
    static f32 BarYOffset    = 5.0f;
    static f32 BarYOffsetF   = 6.0f; // Flipped offset.

    //
    // Draw the recently lost amount.
    //
    if( x_abs( m_Bar.m_TargetVal - m_Bar.m_CurrentVal ) > 0.01f )
    {
        pTexture = m_InnerBmp.GetPointer();
        if( pTexture )
        {
            const xbitmap& Bitmap = pTexture->m_bitmap;
            f32 CurrWidth = ((f32)m_Bar.m_CurrentVal / (f32)m_Bar.m_MaxVal );
            vector2 WH( BarWidth * CurrWidth, BarHeight );

            vector2 Pos;

            Pos.X = m_XPos + BarXOffset + 5.0f;
            Pos.Y = m_YPos + (m_Bar.m_bFlip ? BarYOffsetF : BarYOffset) - YOffset;

            vector2 UV0( (1.0f - CurrWidth) * ( BarWidth / Bitmap.GetWidth()), 0.0f );
            vector2 UV1( BarWidth / Bitmap.GetWidth(), BarHeight / Bitmap.GetHeight() );

            xcolor ScannerColor = Interpolate(XCOLOR_WHITE, XCOLOR_BLUE, (f32)(m_Bar.m_StartVal - m_Bar.m_CurrentVal) / (f32)(m_Bar.m_StartVal - m_Bar.m_TargetVal));

            // check for pulsing
            if( m_bPulsing )
            {
                xcolor PulseColor( ScannerColor );
                PulseColor.A = (u8)(((f32)PulseColor.A / 255) * hud_object::m_PulseAlpha);       
                ScannerColor = PulseColor;
            }
            g_UIRenderer.DrawImage( *pTexture, Pos, WH, UV0, UV1, ScannerColor,
                                    0.0f, UI_BLEND_ALPHA, UI_SAMPLER_LINEAR_CLAMP );
        }
    }


    //
    // Draw the current value.
    //
    if( pTexture )
    {
        const xbitmap& Bitmap = pTexture->m_bitmap;
        for( s32 i = 0; i < 2; i++ )
        {
            vector2 WH( x_floor( BarPercentage * BarWidth ) + i, BarHeight );
            vector2 Pos;

            Pos.X = m_XPos + BarXOffset + 5.0f;
            Pos.Y = m_YPos + (m_Bar.m_bFlip ? BarYOffsetF : BarYOffset) - YOffset;

            vector2 UV0( (1.0f - BarPercentage) * ( BarWidth / Bitmap.GetWidth()) + (i / (f32)Bitmap.GetWidth()), 0.0f );
            vector2 UV1( BarWidth / Bitmap.GetWidth(), BarHeight / Bitmap.GetHeight() );

            xcolor DisplayColor = m_Bar.m_InnerColor;

            // The first pass is always at full opacity, but the second pass is a percentage.
            if( i != 0 )
            {
                DisplayColor.A = (u8)(x_fmod( BarPercentage * BarWidth, 1.0f ) * DisplayColor.A);
            }

            // Check for pulsing.
            if( m_bPulsing )
            {
                xcolor PulseColor( DisplayColor );
                PulseColor.A = (u8)(((f32)PulseColor.A / 255) * hud_object::m_PulseAlpha);
                DisplayColor = PulseColor;
            }
            g_UIRenderer.DrawImage( *pTexture, Pos, WH, UV0, UV1, DisplayColor,
                                    0.0f, UI_BLEND_ALPHA, UI_SAMPLER_LINEAR_CLAMP );
        }
    }
}

//==============================================================================

void hud_scanner::OnAdvanceSimulation( player* pPlayer, f32 DeltaTime )
{
    m_ScanPeakUpdateTime -= DeltaTime;
    if( m_ScanPeakUpdateTime <= 0.0f )
    {
        m_ScanPeakUpdateTime += 1.5f;
        m_ScanPeakUpdate = TRUE;
    }

    m_ScanDataTimeout -= DeltaTime;
    if( m_ScanDataTimeout <= F32_MIN )
        m_bRenderScanData = FALSE;

    f32 ClosestDistance = 0.0f;
    const f32 MaxDistance = MAX( Lore_Max_Detect_DistanceTweak.GetF32(), F32_MIN );
    const xbool HasScanner = pPlayer->GetInventory2().HasItem( INVEN_WEAPON_SCANNER ) &&
                             !pPlayer->IsMutated();
    const xbool WasDetected = m_LoreDetected;

    m_LoreDetected = HasScanner &&
                     pPlayer->GetClosestLoreObjectDist( ClosestDistance ) &&
                     (ClosestDistance < MaxDistance);

    if( m_LoreDetected && !WasDetected )
        g_AudioMgr.Play( "Scanner_HUD_Appear", TRUE );

    if( m_LoreDetected )
    {
        m_LoreDistance = ClosestDistance;
        m_LoreMaxDistance = MaxDistance;
    }

    const f32 FadeDelta = 960.0f * DeltaTime;
    if( m_LoreDetected )
        m_ScannerBmpAlpha = MIN( m_ScannerBmpAlpha + FadeDelta, 255.0f );
    else
        m_ScannerBmpAlpha = MAX( m_ScannerBmpAlpha - FadeDelta, 0.0f );

    if( !m_LoreDetected && (m_ScannerBmpAlpha == 0) )
    {
        m_LoreBarAmount = 0;
    }
}

//==============================================================================

void hud_scanner::AdvanceBar( player* pPlayer, f32 DeltaTime )
{
    // scanner not in blueprint bag
    if( !pPlayer->GetCurrentWeaponPtr() )
    {
        ASSERT(0 && "Scanner is not in blueprint bag!");
        return;
    }

    f32 PlayerValue = 0.0f;

    s32 NumSections = (NUM_CONTROL_POINTS - 1);

    m_Bar.m_InnerColor = g_ScannerColors[ 0 ];

    // Get scanner update time
    PlayerValue  = ((weapon_scanner*)pPlayer->GetCurrentWeaponPtr())->GetCurrentScanTime();
    m_Bar.m_MaxVal = ((weapon_scanner*)pPlayer->GetCurrentWeaponPtr())->GetMaxScanTime();

    m_Bar.m_CurrentDisplayPercentage = 
        ((DeltaTime * 1.5f)          * (m_Bar.m_TargetVal / m_Bar.m_MaxVal)) + 
        ((1.0f - (DeltaTime * 1.5f)) * m_Bar.m_CurrentDisplayPercentage); 

    m_Bar.m_CurrentDisplayPercentage = MINMAX( 0.0f, m_Bar.m_CurrentDisplayPercentage, 1.0f );

    s32 i;
    for( i = 1; i <= NumSections; i++ )
    {
        f32 SectionTop    = (1.0f - ((f32)(i - 1)     / (f32)NumSections));
        f32 SectionBottom = (1.0f - ((f32)(i) / (f32)NumSections));

        if( IN_RANGE( SectionBottom, m_Bar.m_CurrentDisplayPercentage, SectionTop ) )
        {
            f32 ColorPoint = NumSections * (m_Bar.m_CurrentDisplayPercentage - 
                (((f32)NumSections - (f32)(i)) / (f32)NumSections));

            m_Bar.m_InnerColor = Interpolate( 
                g_ScannerColors[ i ], 
                g_ScannerColors[ i - 1], 
                ColorPoint );
        }
    }

    // First, look for damage or scanner increases.
    if( PlayerValue != m_Bar.m_TargetVal )
    {
        m_Bar.m_TargetVal  = PlayerValue;
        m_Bar.m_StartVal   = m_Bar.m_CurrentVal;
    }

    // Now, advance towards the target value.
    if( m_Bar.m_CurrentVal > m_Bar.m_TargetVal )
    {        
        m_Bar.m_CurrentVal    -= (50.0f + (m_Bar.m_CurrentVal - m_Bar.m_TargetVal)) * DeltaTime;
    }

    m_Bar.m_CurrentVal = x_max( m_Bar.m_CurrentVal, m_Bar.m_TargetVal );
    m_Bar.m_StartVal   = x_max( m_Bar.m_CurrentVal, m_Bar.m_StartVal  );
}

//==============================================================================

xbool hud_scanner::OnProperty( prop_query& rPropQuery )
{
    (void) rPropQuery;
    /*
    if( rPropQuery.IsVar( "Hud\\ScannerBarOutline\\BitmapResource" ) )
    {
    if( rPropQuery.IsRead() )
    {
    rPropQuery.SetVarExternal( m_OuterBmp.GetName(), RESOURCE_NAME_SIZE );
    }
    else            
    {
    const char* pStr = rPropQuery.GetVarExternal();
    m_OuterBmp.SetName( pStr );
    }
    return TRUE;
    }    

    if( rPropQuery.IsVar( "Hud\\ScannerBar\\BitmapResource" ) )
    {
    if( rPropQuery.IsRead() )
    {
    rPropQuery.SetVarExternal( m_InnerBmp.GetName(), RESOURCE_NAME_SIZE );
    }
    else            
    {
    const char* pStr = rPropQuery.GetVarExternal();
    m_InnerBmp.SetName( pStr ); 
    }
    return TRUE;
    } 
    */

    return FALSE;
}

//==============================================================================

void hud_scanner::OnEnumProp( prop_enum&  List )
{
    (void)List;
    return;

    //----------------------------------------------------------------------
    // scanner Bar.
    //----------------------------------------------------------------------
    List.PropEnumExternal( "Hud\\ScannerBarOutline\\BitmapResource",   "Resource\0xbmp\0", "Bitmap resource for the scanner bars outline.", PROP_TYPE_MUST_ENUM  );
    List.PropEnumExternal( "Hud\\ScannerBar\\BitmapResource",          "Resource\0xbmp\0", "Bitmap resource for the scanner bars.", PROP_TYPE_MUST_ENUM );
}

//==============================================================================
