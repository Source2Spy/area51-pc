//==============================================================================
//  DebugMenuPageAdvCheckpoints.cpp
//==============================================================================

#include "DebugMenu2.hpp"
#include "CheckPointMgr/CheckPointMgr.hpp"

#if defined( ENABLE_DEBUG_MENU )

//==============================================================================

debug_menu_page_adv_checkpoints::debug_menu_page_adv_checkpoints( void )
{
    m_pTitle = "Advance Checkpoints";
    OnFocus();
}

//==============================================================================

void debug_menu_page_adv_checkpoints::OnFocus( void )
{
    for( s32 i = 0; i < m_Items.GetCount(); i++ )
        delete m_Items[i];
    m_Items.Clear();
    m_iActiveItem = 0;
    x_memset( m_Checkpoints, 0, sizeof( m_Checkpoints ) );
    for( s32 i = 0; i < g_CheckPointMgr.m_Level.nValidCheckPoints && i < MAX_CHECKPOINTS; i++ )
    {
        if( !g_CheckPointMgr.m_Level.CheckPoints[i].bIsValid )
            continue;
        m_Names[i].Format( "Restore checkpoint %d%s", i + 1,
                           i == g_CheckPointMgr.GetCheckPointIndex() ? " (current)" : "" );
        m_Checkpoints[i] = AddItemButton( m_Names[i] );
    }
    if( m_Items.GetCount() == 0 )
        AddItemSeperator( "No checkpoints available" );
}

//==============================================================================

void debug_menu_page_adv_checkpoints::OnChangeItem( debug_menu_item* pItem )
{
    if( !SMP_UTIL_GetActivePlayer() )
        return;
    for( s32 i = 0; i < MAX_CHECKPOINTS; i++ )
    {
        if( m_Checkpoints[i] == pItem && i < g_CheckPointMgr.m_Level.nValidCheckPoints &&
            g_CheckPointMgr.m_Level.CheckPoints[i].bIsValid )
        {
            g_CheckPointMgr.SetCheckPointIndex( i );
            g_DebugMenu.Disable();
            g_CheckPointMgr.Restore( TRUE );
            return;
        }
    }
}

#endif // ENABLE_DEBUG_MENU
