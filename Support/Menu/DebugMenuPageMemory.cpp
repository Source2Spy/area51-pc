//==============================================================================
//  DebugMenuPageMemory.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu memory page.
//  
//==============================================================================

#include "DebugMenu2.hpp"
#include "Configuration/GameConfig.hpp"
#include "Obj_mgr/obj_mgr.hpp"

//==============================================================================

#if defined( ENABLE_DEBUG_MENU )

//==============================================================================

static
xbool FileExists( const xstring& PathName )
{
    X_FILE* pFile = x_fopen( PathName, "rb" );
    if( pFile )
        x_fclose( pFile );
    return( pFile != NULL );
}

//==============================================================================

static
xstring FindNextFileInSequence( const char* pFile, const char* pExtension )
{
    s32 Index = 0;

    while( 1 )
    {
        xstring Name = pFile;
        if( Index > 0 )
            Name.AddFormat( "_%04d", Index );
        Name.AddFormat( ".%s", pExtension );

        if( !FileExists( Name ) )
            return Name;

        Index++;
    }
}

//==============================================================================

debug_menu_page_memory::debug_menu_page_memory( ) : debug_menu_page()
{
    m_pTitle = "Memory";
    m_pItemMemoryDump          = AddItemButton   ( "COMPLETE Memory dump to file" );
}

//==============================================================================

void debug_menu_page_memory::OnChangeItem( debug_menu_item* pItem )
{
    if( pItem == m_pItemMemoryDump )
    {
        xstring PathName = FindNextFileInSequence( xfs("MemoryDump_%d", g_ActiveConfig.GetLevelID()), "csv" );
        x_MemDump( PathName, TRUE );
    }
}

//==============================================================================

#endif // defined( ENABLE_DEBUG_MENU )
