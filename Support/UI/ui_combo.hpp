//==============================================================================
//  
//  ui_combo.hpp
//  
//==============================================================================

#ifndef UI_COMBO_HPP
#define UI_COMBO_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_control.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define COMBO_DATA_FIELDS   2

//==============================================================================
//  REGISTRATION
//==============================================================================

extern ui_win* ui_combo_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

//==============================================================================
//  CLASSES
//==============================================================================

class ui_combo : public ui_control
{
    struct item
    {
        xwstring    Label;
        xbool       Enabled;
        xcolor      Color;
        s32         BitmapID;
        uaddr       Data[COMBO_DATA_FIELDS];
    };

public:

    enum flags
    {
        CB_CHANGE_ON_NAV    = 0x00000001,   // Change selection on pad left or right
        CB_CHANGE_ON_SELECT = 0x00000002,   // Change selection on pad select
        CB_NOTIFY_PARENT    = 0x00000004,   // Pass message to the parent even if handled
    };

                    ui_combo                ( void );
    virtual        ~ui_combo                ( void );

    xbool           Create                  ( s32           UserID,
                                              ui_manager*   pManager,
                                              const irect&  Position,
                                              ui_win*       pParent,
                                              s32           Flags );

    virtual void    Render                  ( s32 ox=0, s32 oy=0 );

    virtual void    OnNavigate              ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void    OnAccept                ( ui_win* pWin );
    virtual void    OnPage                  ( ui_win* pWin, s32 Direction );
    virtual void    OnPointerDown           ( ui_win* pWin, s32 x, s32 y );


    void            SetLabelWidth           ( s32 Width );

    s32             AddItem                 ( const xwstring& Item, uaddr Data1 = 0, uaddr Data2 = 0 );
    s32             AddItem                 ( const xwchar*   Item, uaddr Data1 = 0, uaddr Data2 = 0 );
    void            SetItemEnabled          ( s32 iItem, xbool State );
    void            SetItemColor            ( s32 iItem, xcolor Color );
    void            SetItemBitmap           ( s32 iItem, s32 ID );
    void            DeleteAllItems          ( void );
    void            DeleteItem              ( s32 iItem );

    s32             GetItemCount            ( void ) const;
    const xwstring& GetItemLabel            ( s32 iItem ) const;
    s32             GetItemBitmap           ( s32 iItem ) const;
    uaddr           GetItemData             ( s32 iItem, s32 Index = 0 ) const;
    const xwstring& GetSelectedItemLabel    ( void ) const;
    uaddr           GetSelectedItemData     ( s32 Index = 0 ) const;
    xbool           GetItemEnabled          ( s32 iItem ) const;
    xbool           GetSelectedItemEnabled  ( void ) const;

    s32             FindItemByLabel         ( const xwstring& Label ) const;
    s32             FindItemByData          ( uaddr Data, s32 Index = 0 ) const;

    s32             GetSelection            ( void ) const;
    void            SetSelection            ( s32 iSelection );
    void            ClearSelection          ( void );

    void            SetNavFlags             ( u32 flags )    { m_NavFlags = flags; };

protected:
    s32             m_iElement1;
    s32             m_LabelWidth;
    xarray<item>    m_Items;
    s32             m_iSelection;
    u32             m_NavFlags;
};

//==============================================================================
#endif // UI_COMBO_HPP
//==============================================================================
