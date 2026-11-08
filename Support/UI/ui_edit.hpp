//==============================================================================
//  
//  ui_edit.hpp
//  
//==============================================================================

#ifndef UI_EDIT_HPP
#define UI_EDIT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_control.hpp"

//==============================================================================
//  ui_edit
//==============================================================================

extern ui_win* ui_edit_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

class ui_edit : public ui_control
{
public:
                            ui_edit                 ( void );
    virtual                ~ui_edit                 ( void );

    xbool                   Create                  ( s32           UserID,
                                                    ui_manager*   pManager,
                                                    const irect&  Position,
                                                    ui_win*       pParent,
                                                    s32           Flags );
    void                    Configure               ( xbool bName ) { m_bName = bName; }

    virtual void            Render                  ( s32 ox=0, s32 oy=0 );

    virtual void            OnAccept               ( ui_win* pWin );
    void                    SetLabelWidth           ( s32 Width );
    void                    SetBufferSize           ( s32 BufferSize );
    void                    SetVirtualKeyboardTitle ( const xwstring& Title );
    void                    SetPlaceholder          ( const xwstring& Text ) { m_Placeholder = Text; }

protected:
    s32             m_iElement1;
    s32             m_LabelWidth;
    s32             m_BufferSize;
    xwstring        m_VirtualKeyboardTitle;
    xwstring        m_Placeholder;
    xbool           m_bName;  // Whether this dialogue exists to enter in a name (as opposed to a password).

};

//==============================================================================
#endif // UI_EDIT_HPP
//==============================================================================
