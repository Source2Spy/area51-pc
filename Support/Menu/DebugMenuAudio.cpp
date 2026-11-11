//==============================================================================
//  DebugMenuAudio.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu gameplay page.
//  
//==============================================================================

#include "DebugMenu2.hpp"

//==============================================================================

#if defined( ENABLE_DEBUG_MENU )

extern xbool SHOW_STREAM_INFO;
extern xbool DEBUG_ACQUIRE_CHANNEL_FAIL;
#ifndef X_RETAIL
extern xbool AUDIO_TWEAK;
#endif

//==============================================================================

debug_menu_audio::debug_menu_audio( ) : debug_menu_page()
{
    m_pTitle = "Audio";

    AddItemBool( "Stream Debug",                    SHOW_STREAM_INFO    );
#ifndef X_RETAIL
    AddItemBool( "Enable Audio Tweaks",             AUDIO_TWEAK   );
#endif
    AddItemSeperator( );
    AddItemBool( "Debug 'Channel Acquire Failure'", DEBUG_ACQUIRE_CHANNEL_FAIL      );
    AddItemSeperator( );
    m_pDumpLoadedPackages = AddItemButton( "Dump Loaded Packages to TTY" );    
}

//==============================================================================

void debug_menu_audio::OnChangeItem( debug_menu_item* pItem )
{
    if( pItem == m_pDumpLoadedPackages )
    {
        g_AudioMgr.DisplayPackages();
    }
}

#endif // ENABLE_DEBUG_MENU
