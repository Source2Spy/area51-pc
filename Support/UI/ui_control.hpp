//==============================================================================
//  
//  ui_control.hpp
//  
//==============================================================================

#ifndef UI_CONTROL_HPP
#define UI_CONTROL_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_win.hpp"

//==============================================================================
//  ui_control
//==============================================================================

class ui_control : public ui_win
{
public:
    enum visual_state
    {
        CS_NORMAL = 0,
        CS_HIGHLIGHTED,
        CS_ACTIVE,
        CS_HIGHLIGHTED_ACTIVE,
        CS_DISABLED,
    };

                    ui_control          ( void );
    virtual        ~ui_control          ( void );

    xbool           Create              ( s32           UserID,       
                                          ui_manager*   pManager,
                                          const irect&  Position,
                                          ui_win*       pParent,
                                          s32           Flags );

    virtual void    Render              ( s32 ox=0, s32 oy=0 );
    virtual void    OnPointerDown       ( ui_win* pWin, s32 x, s32 y );

    const irect&    GetNavPos           ( void ) const;
    void            SetNavPos           ( const irect& r );

protected:
    enum highlight_state
    {
        HS_NONE,
        HS_HOVERED,
        HS_SELECTED,
        HS_ACTIVE,
        HS_PRESSED,
    };

    xbool              ShouldRenderHighlight( void ) const;
    visual_state        GetVisualState      ( xbool Active ) const;
    void                RenderHighlightRect ( const irect& Bounds, highlight_state State ) const;

    irect               m_NavPos;
};

//==============================================================================
#endif // UI_CONTROL_HPP
//==============================================================================
