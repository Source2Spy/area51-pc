//==============================================================================
//  DebugMenuPageAdvCheckPoints.hpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This class is the specific page for advance checkpoints
//  
//==============================================================================

#ifndef DEBUG_MENU_PAGE_ADV_CHECKPOINTS_HPP
#define DEBUG_MENU_PAGE_ADV_CHECKPOINTS_HPP

#include "CheckPointMgr/CheckPointMgr.hpp"

//==============================================================================
// Defines

//==============================================================================

class debug_menu_page_adv_checkpoints : public debug_menu_page
{
public:
                                        debug_menu_page_adv_checkpoints     ( );
    virtual                             ~debug_menu_page_adv_checkpoints    ( ) { };

    virtual void                        OnFocus     ( void );
    virtual void                        OnChangeItem( debug_menu_item* pItem );

protected:
    xstring                             m_Names[MAX_CHECKPOINTS];
    debug_menu_item*                    m_Checkpoints[MAX_CHECKPOINTS];
};

//==============================================================================

#endif  // DEBUG_MENU_PAGE_ADV_CHECKPOINTS_HPP