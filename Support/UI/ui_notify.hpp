//==============================================================================
//
//  ui_notify.hpp
//
//==============================================================================

#ifndef UI_NOTIFY_HPP
#define UI_NOTIFY_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_win;
class xwstring;

//==============================================================================
//  NOTIFICATION TYPES
//==============================================================================

enum class ui_notification_type
{
    None = 0,
    ComboSelectionChanged,
    SliderChanged,
    CheckChanged,
    ListSelectionChanged,
    ListSortChanged,
    ListAccepted,
    ListCancelled,
    TabChanged,
    TextInput,
    TextRefresh,
};

//==============================================================================
//  ui_notification
//==============================================================================

struct ui_notification
{
    ui_notification() = default;
    ui_notification( ui_notification_type Type, ui_win* pSender, s32 Value, xwstring const* pText )
        : m_Type( Type ), m_pSender( pSender ), m_Value( Value ), m_pText( pText ) {}

    ui_notification_type  m_Type    = ui_notification_type::None;
    ui_win*               m_pSender = NULL;
    s32                   m_Value   = 0;
    xwstring const*       m_pText   = NULL;
};

//==============================================================================
#endif // UI_NOTIFY_HPP
//==============================================================================
