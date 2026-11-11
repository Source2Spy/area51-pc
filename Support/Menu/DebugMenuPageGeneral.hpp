//==============================================================================
//  DebugMenuPageGeneral.hpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This class is the specific page for general debug optoins.
//  
//==============================================================================

#ifndef DEBUG_MENU_PAGE_GENERAL_HPP
#define DEBUG_MENU_PAGE_GENERAL_HPP

//==============================================================================
class debug_menu_page_general : public debug_menu_page
{
public:
                                debug_menu_page_general     ( );
    virtual                     ~debug_menu_page_general    ( ) { };

    virtual void                OnChangeItem        ( debug_menu_item* pItem );
    virtual void                OnPreRender         ( void );

protected:
    debug_menu_item*            m_pItemStatsLegend;
    debug_menu_item*            m_pItemShowQA;
};

//==============================================================================

#endif  // DEBUG_MENU_PAGE_GENERAL_HPP