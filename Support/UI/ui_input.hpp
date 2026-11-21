//==============================================================================
//
//  ui_input.hpp
//
//==============================================================================

#ifndef UI_INPUT_HPP
#define UI_INPUT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

class ui_win;

//==============================================================================
//  INPUT TYPES
//==============================================================================

enum class ui_input_device
{
    None = 0,
    Mouse,
    Keyboard,
    Gamepad,
};

//------------------------------------------------------------------------------

enum class ui_input_event_type
{
    None = 0,
    Navigate,
    Accept,
    Cancel,
    Delete,
    Alternate,
    Help,
    PointerMove,
    PointerDown,
    PointerUp,
    PointerWheel,
};

//------------------------------------------------------------------------------

enum class ui_navigation
{
    None = 0,
    Up,
    Down,
    Left,
    Right,
    PagePrevious,
    PageNext,
    First,
    Last,
};

//------------------------------------------------------------------------------

enum class ui_pointer_button
{
    None = 0,
    Primary,
};

//==============================================================================
//  TYPES
//==============================================================================

struct ui_input_event
{
    ui_input_event_type m_Type          = ui_input_event_type::None;
    ui_input_device     m_Device        = ui_input_device::None;
    ui_win*             m_pTarget       = NULL;
    ui_navigation       m_Navigation    = ui_navigation::None;
    ui_pointer_button   m_PointerButton = ui_pointer_button::None;
    s32                 m_X             = 0;
    s32                 m_Y             = 0;
    s32                 m_Delta         = 0;
    s32                 m_Presses       = 0;
    s32                 m_Repeats       = 0;
    xbool               m_WrapX         = FALSE;
    xbool               m_WrapY         = FALSE;
};

//==============================================================================
#endif // UI_INPUT_HPP
//==============================================================================
