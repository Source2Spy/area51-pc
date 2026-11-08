//==============================================================================
//  DebugMenuPageMultiplayer.cpp
//==============================================================================
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu multiplayer page.
//  
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "DebugMenu2.hpp"
#include "NetworkMgr/Game/GameMgr.hpp"
#include "e_Network.hpp"
#include "NetworkMgr/Connection/GameClient.hpp"
#include "NetworkMgr/Connection/GameServer.hpp"
#include "NetworkMgr/Connection/ClientProxy.hpp"
#include "InputMgr/GamePad.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"

//==============================================================================
#if defined( ENABLE_DEBUG_MENU )
//==============================================================================

#define A51_XWSTRING_FORMAT( ... ) xwstring( (const char*)xfs( __VA_ARGS__ ) )

//==============================================================================
//  DATA
//==============================================================================

extern f32 HumanSpeedFactor;
extern f32 MutantSpeedFactor;

extern f32 TweakHeadPain;
extern f32 TweakBodyPain;
extern f32 TweakLimbPain;

extern f32 TweakCostParasite;
extern f32 TweakCostContagion;
extern f32 MPParasiteHealth;

extern f32 MPMutagenBurn;

//==============================================================================
//  FUNCTIONS
//==============================================================================

debug_menu_page_multiplayer::debug_menu_page_multiplayer( void ) 
{
    m_pTitle         = "Multiplayer";
    m_AvatarSkin     = -1;
    m_bToggleNetInfo = FALSE;
    m_iClientIndex   = 0;
    
    AddItemFloat( "Speed Factor - Human",  HumanSpeedFactor,  0.1f, 2.0f, 0.01f );
    AddItemFloat( "Speed Factor - Mutant", MutantSpeedFactor, 0.1f, 2.0f, 0.01f );
    AddItemFloat( "Damage Factor - Head",  TweakHeadPain,     0.1f, 2.0f, 0.01f );
    AddItemFloat( "Damage Factor - Body",  TweakBodyPain,     0.1f, 2.0f, 0.01f );
    AddItemFloat( "Damage Factor - Limb",  TweakLimbPain,     0.1f, 2.0f, 0.01f );
    AddItemFloat( "Parasite - Health Return", MPParasiteHealth,   0.0f,  10.0f, 1.0f );
    AddItemFloat( "Parasite - Cost",          TweakCostParasite,  0.0f, 100.0f, 1.0f );
    AddItemFloat( "Contagion - Cost",         TweakCostContagion, 0.0f, 100.0f, 1.0f );
    AddItemFloat( "Burn Factor - Mutagen",    MPMutagenBurn,      0.1f, 2.0f, 0.05f );

    AddItemSeperator( "" );
    AddItemInt( "% Packet Sends Lost",       ISettings.PercSendsLost,      0, 100 );
    AddItemInt( "% Packet Received Lost",    ISettings.PercReceivesLost,   0, 100 );
    AddItemSeperator( "" );
    m_pPlayerCount =
        AddItemInt( "Player count (for zoning)", GameMgr.m_Score.NPlayers,     1,  32 );
    m_pSkinItem =
        AddItemInt( "Force Player Skin",         m_AvatarSkin,                 -1,  SKIN_END_PLAYERS );
    
    // Spawn point teleport
    m_iSpawnPoint    = 0;
    s32 iLastSpawnPoint = g_ObjMgr.GetNumInstances( object::TYPE_SPAWN_POINT ) -1;
    iLastSpawnPoint = MAX( 0, iLastSpawnPoint );
    m_pSpawnPoint = AddItemInt( "Teleport to spawn point", m_iSpawnPoint, 0, iLastSpawnPoint );

    // Net info
    AddItemSeperator( "" );
    AddItemBool( "Toggle NetInfo",   m_bToggleNetInfo );
    m_pClientIndex =
        AddItemInt ( "ClientToWatch",    m_iClientIndex, 0, 31 );


}

//==============================================================================

void debug_menu_page_multiplayer::OnFocus( void )
{
    m_pSpawnPoint->SetIntLimits( 0, MAX( 0, g_ObjMgr.GetNumInstances( object::TYPE_SPAWN_POINT ) - 1 ) );
    const s32 ClientCount = g_NetworkMgr.IsServer() ? g_NetworkMgr.GetServerObject().GetMaxClients() : 0;
    m_pClientIndex->SetIntLimits( 0, MAX( 0, ClientCount - 1 ) );
}

//==============================================================================

void debug_menu_page_multiplayer::OnChangeItem( debug_menu_item* pItem )
{
    if( pItem == m_pSkinItem )
    {
        if( IN_RANGE( 0, pItem->GetValueInt(), SKIN_END_PLAYERS ) )
        {
            GameMgr.DebugSetSkin( pItem->GetValueInt() );
        }
    }
    
    if( pItem == m_pPlayerCount )
    {
        GameMgr.m_ZoneTimer = 3.0f;
    }

    m_iClientIndex = MAX( m_iClientIndex,  0 );
    m_iClientIndex = MIN( m_iClientIndex, 31 );
    
    // Teleport to spawn point?
    if( pItem == m_pSpawnPoint )
    {
        // Update range
        s32 iLastSpawnPoint = g_ObjMgr.GetNumInstances( object::TYPE_SPAWN_POINT ) -1;
        m_pSpawnPoint->SetIntLimits( 0, MAX( 0, iLastSpawnPoint ) );
    
        // Lookup spawn point ID
        slot_id SpawnPointID = g_ObjMgr.GetFirst( object::TYPE_SPAWN_POINT );
        s32     iSpawnPoint  = 0;
        while( ( iSpawnPoint < m_iSpawnPoint ) && ( SpawnPointID != SLOT_NULL ) )
        {
            // Goto next spawn point
            iSpawnPoint++;
            SpawnPointID = g_ObjMgr.GetNext( SpawnPointID );
        }
        
        // Lookup spawn point and player
        spawn_point* pSpawnPoint = (spawn_point*)g_ObjMgr.GetObjectBySlot( SpawnPointID );
        player*      pPlayer     = SMP_UTIL_GetActivePlayer();
        
        // Teleport?
        if( pSpawnPoint && pPlayer )
        {
            // Update item text
            m_SpawnPointName.Format( "Spawn: %s", 
                                     (const char*)guid_ToString( pSpawnPoint->GetGuid() ) );
            m_pSpawnPoint->SetName( (const char*)m_SpawnPointName );                                     
            
            //  Lookup spawn point info
            vector3 Position;
            radian3 Rotation;
            u16     Zone1;
            u16     Zone2;
            pSpawnPoint->GetSpawnInfo( pPlayer->GetGuid(), Position, Rotation, Zone1, Zone2 );

            // Teleport the player just like in "logic_base::MoveToSpawnPt"
            pPlayer->Teleport( Position,
                               Rotation.Pitch,
                               Rotation.Yaw,
                               static_cast<zone_mgr::zone_id>( Zone1 ),
                               static_cast<zone_mgr::zone_id>( Zone2 ),
                               PlayerTeleportVelocityPolicy::Clear,
                               FALSE,
                               FALSE );

            // Flush stale input at the discontinuity. The next simulation
            // update aplies the new authoritative view. Debug UI must never
            // advance the whole world
            g_GameInput.ClearPlayerActions( pPlayer->GetActivePlayerPad() );
        }
        else
        {
            // Invalid spawn point - clear info
            m_pSpawnPoint->SetName( "Teleport to spawn point" );
        }
    }
}

//==============================================================================

void debug_menu_page_multiplayer::OnPreRender( void )
{
#if !defined(X_RETAIL) || defined(X_QA)
    if( m_bToggleNetInfo )
    {
        s32 font = g_UiMgr->FindFont("small");
        const irect Bounds = g_UIRenderer.GetViewport().GetLogicalClipBounds();

        irect Rect;
        s32 x = Bounds.l + 10;
        s32 y = Bounds.t + 40;

        xwstring strPackets, strUpdates, strGameMgr, strVoiceMgr, strUpdateMgr;
        xwstring strPlayerBits, strGhostBits, strOtherBits, strTotalBits;
        xwstring strPains, strPainQueue;

        if( g_NetworkMgr.IsServer() )
        {
            s32          Index      = m_iClientIndex;
            if( Index < 0 || Index >= g_NetworkMgr.GetServerObject().GetMaxClients() )
            {
                RenderText( xwstring( "No client at selected index" ), x, y, font, Rect );
                return;
            }
            conn_mgr&    ConnMgr    = g_NetworkMgr.GetServerObject().m_pClients[Index].m_ConnMgr;
            conn_stats&  Stats      = ConnMgr.m_StatsDebug;
            f32          fStatsTime = ConnMgr.m_StatsClockDebug;
            f32          RateScale  = fStatsTime > 0.0f ? 1.0f / fStatsTime : 0.0f;
            s32          Packets    = (s32)(Stats.Packets * RateScale);
            f32          PacketScale = Stats.Packets > 0 && fStatsTime > 0.0f ? fStatsTime / Stats.Packets : 0.0f;

            strPackets    = A51_XWSTRING_FORMAT( "%04d Packets per second", Packets);
            strUpdates    = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Updates",
                                (s32) (Stats.Updates * RateScale),
                                (s32)((Stats.Updates * RateScale) * PacketScale),
                                Stats.MaxUpdates );
            strGameMgr    = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] GameMgr Bytes",
                                (s32) ((Stats.BitsGameMgr    >> 3) * RateScale),
                                (s32)(((Stats.BitsGameMgr    >> 3) * RateScale) * PacketScale),
                                (s32)  (Stats.MaxBitsGameMgr >> 3));
            strVoiceMgr   = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] VoiceMgr Bytes",
                                (s32) ((Stats.BitsVoiceMgr    >> 3) * RateScale),
                                (s32)(((Stats.BitsVoiceMgr    >> 3) * RateScale) * PacketScale),
                                (s32)  (Stats.MaxBitsVoiceMgr >> 3));
            strUpdateMgr  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] UpdateMgr Bytes",
                                (s32) ((Stats.BitsUpdateMgr    >> 3) * RateScale),
                                (s32)(((Stats.BitsUpdateMgr    >> 3) * RateScale) * PacketScale),
                                (s32)  (Stats.MaxBitsUpdateMgr >> 3));
            strPlayerBits = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Update(player) Bytes",
                                (s32) ((Stats.BitsPlayerUpdates    >> 3) * RateScale),
                                (s32)(((Stats.BitsPlayerUpdates    >> 3) * RateScale) * PacketScale),
                                (s32)  (Stats.MaxBitsPlayerUpdates >> 3));
            strGhostBits  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Update(ghost) Bytes",
                                (s32) ((Stats.BitsGhostUpdates    >> 3) * RateScale),
                                (s32)(((Stats.BitsGhostUpdates    >> 3) * RateScale) * PacketScale),
                                (s32)  (Stats.MaxBitsGhostUpdates >> 3));
            strOtherBits  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Update(other) Bytes",
                                (s32) ((Stats.BitsOtherUpdates    >> 3) * RateScale),
                                (s32)(((Stats.BitsOtherUpdates    >> 3) * RateScale) * PacketScale),
                                (s32)  (Stats.MaxBitsOtherUpdates >> 3));
            strTotalBits  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Total Packet Bytes",
                                (s32) ((Stats.BitsTotal    >> 3) * RateScale),
                                (s32)(((Stats.BitsTotal    >> 3) * RateScale) * PacketScale),
                                (s32)  (Stats.MaxBitsTotal >> 3));

            Rect.Set( x - 4, y, x + 320, y + g_UiMgr->GetLineHeight(font) * 10 );
            g_UIRenderer.DrawRect( Rect, xcolor(0,0,0,128) );
        }
        
        if( g_NetworkMgr.IsClient() )
        {
            f32 fStatsTime = g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsClockDebug;
            f32 RateScale = fStatsTime > 0.0f ? 1.0f / fStatsTime : 0.0f;
            s32 Packets = (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.Packets * RateScale);
            const s32 PacketCount = g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.Packets;
            f32 PacketScale = PacketCount > 0 && fStatsTime > 0.0f ? fStatsTime / PacketCount : 0.0f;
            strPackets    = A51_XWSTRING_FORMAT( "%04d Packets per second", Packets);
            strUpdates    = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Updates",
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.Updates * RateScale),
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.Updates * RateScale)* PacketScale),
                g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxUpdates);
            strGameMgr    = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] GameMgr Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsGameMgr >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsGameMgr >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsGameMgr >> 3));
            strVoiceMgr   = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] VoiceMgr Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsVoiceMgr >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsVoiceMgr >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsVoiceMgr >> 3));
            strUpdateMgr  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] UpdateMgr Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsUpdateMgr >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsUpdateMgr >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsUpdateMgr >> 3));
            strPlayerBits = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Update(player) Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsPlayerUpdates >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsPlayerUpdates >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsPlayerUpdates >> 3));
            strGhostBits  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Update(ghost) Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsGhostUpdates >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsGhostUpdates >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsGhostUpdates >> 3));
            strOtherBits  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Update(other) Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsOtherUpdates >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsOtherUpdates >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsOtherUpdates >> 3));
            strTotalBits  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Total Packet Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsTotal >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsTotal >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsTotal >> 3));
            strPains      = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Pains sent",
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.Pains * RateScale),
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.Pains * RateScale)* PacketScale),
                g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxPains);
            strPainQueue  = A51_XWSTRING_FORMAT( "%04d P%03d [%03d] Pain Queue Bytes",
                (s32)((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsPainQueue >> 3) * RateScale),
                (s32)(((g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.BitsPainQueue >> 3) * RateScale)* PacketScale),
                (s32)(g_NetworkMgr.GetClientObject().m_ConnMgr.m_StatsDebug.MaxBitsPainQueue >> 3));

            Rect.Set( x - 4, y, x + 320, y + g_UiMgr->GetLineHeight(font) * 12 );
            g_UIRenderer.DrawRect( Rect, xcolor(0,0,0,128) );
        }

        if( !g_NetworkMgr.IsServer() && !g_NetworkMgr.IsClient() )
        {
            RenderText( xwstring( "Network offline" ), x, y, font, Rect );
            return;
        }

        RenderText( strPackets   , x, y, font, Rect );   y += 10;
        RenderText( strUpdates   , x, y, font, Rect );
        RenderText( strGameMgr   , x, y, font, Rect );
        RenderText( strVoiceMgr  , x, y, font, Rect );
        RenderText( strUpdateMgr , x, y, font, Rect );
        RenderText( strPlayerBits, x, y, font, Rect );
        RenderText( strGhostBits , x, y, font, Rect );
        RenderText( strOtherBits , x, y, font, Rect );

        if( g_NetworkMgr.IsClient() )
        {
            RenderText( strPains    , x, y, font, Rect );
            RenderText( strPainQueue, x, y, font, Rect );
        }

        RenderText( strTotalBits , x, y, font, Rect );   
    }
#endif
}


//==============================================================================

void debug_menu_page_multiplayer::RenderText ( const xwstring& strOutput, s32& x, s32& y, const s32& font, irect& Rect )
{
    g_UiMgr->TextSize( font, Rect, strOutput, strOutput.GetLength());
    Rect.Translate(x, y);
    g_UiMgr->RenderText( font, Rect, ui_font::h_left|ui_font::v_top, xcolor(XCOLOR_WHITE), strOutput );
    y += g_UiMgr->GetLineHeight(font);
}

//==============================================================================
#endif // defined( ENABLE_DEBUG_MENU )
//==============================================================================
