//==============================================================================
//  
//  ui_listbox.hpp
//  
//==============================================================================

#ifndef UI_LISTBOX_HPP
#define UI_LISTBOX_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#include "x_math.hpp"
#endif

#include "ui_control.hpp"
#include "ui_scrollbar.hpp"

//==============================================================================
//  ui_listbox
//==============================================================================

#define LISTBOX_DATA_FIELDS     2

extern ui_win* ui_listbox_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags );

class ui_listbox : public ui_control
{
public:
    struct item
    {
        xbool       Enabled;
        xwstring    Label;
        uaddr       Data[LISTBOX_DATA_FIELDS];
        xcolor      Color;
        u32         Flags;
    };

                    ui_listbox              ( void );
    virtual        ~ui_listbox              ( void );

    xbool           Create                  ( s32           UserID,
                                              ui_manager*   pManager,
                                              const irect&  Position,
                                              ui_win*       pParent,
                                              s32           Flags );

    virtual void    Render                  ( s32 ox=0, s32 oy=0 );
    virtual void    RenderHeader            ( irect r );
    virtual void    RenderSelection         ( irect r );
    virtual void    RenderItem              ( irect r, const item& Item, const xcolor& c1, const xcolor& c2 );

    virtual void    SetPosition             ( const irect& Position );

    virtual void    OnNavigate              ( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX = FALSE, xbool WrapY = FALSE );
    virtual void    OnPage                  ( ui_win* pWin, s32 Direction );
    virtual void    OnJump                  ( ui_win* pWin, s32 Direction );
    virtual void    OnAccept                ( ui_win* pWin );
    virtual void    OnCancel                ( ui_win* pWin );
    virtual void    OnPointerMove           ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnPointerLeave          ( ui_win* pWin );
    virtual void    OnPointerWheel          ( ui_win* pWin, s32 Delta );
    virtual void    OnPointerDown           ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnPointerUp             ( ui_win* pWin, s32 x, s32 y );
    virtual void    OnUpdate                ( ui_win* pWin, f32 DeltaTime );
    virtual void    OnFocusGained           ( ui_win* pWin );
    virtual void    OnFocusLost             ( ui_win* pWin );

    void            SetLineHeight           ( s32 Height );

    void            SetExitOnSelect         ( xbool State );
    void            SetExitOnBack           ( xbool State )                             { m_ExitOnBack = State; }
    void            SetAcceptOnRepeatClick  ( xbool State )                             { m_AcceptOnRepeatClick = State; }

    s32             AddItem                 ( const xwstring& Item, uaddr Data = 0, uaddr Data2 = 0 , xbool State = TRUE, u32 Flags = 0 );
    s32             AddItem                 ( const xwchar*   Item, uaddr Data = 0, uaddr Data2 = 0 , xbool State = TRUE, u32 Flags = 0 );
    void            DeleteAllItems          ( void );
    void            DeleteItem              ( s32 iItem );
    void            DeleteSelectedItem      ( void );
    void            EnableItem              ( s32 iItem, xbool State );
    u32             GetItemFlags            ( s32 iItem ) const;

    void            EnableBorders           ( void )                                    { m_ShowBorders = TRUE; }
    void            DisableBorders          ( void )                                    { m_ShowBorders = FALSE; }

    void            EnableFrame             ( void )                                    { m_ShowFrame = TRUE; }
    void            DisableFrame            ( void )                                    { m_ShowFrame = FALSE; }

    void            EnableHeaderBar         ( void );
    void            DisableHeaderBar        ( void );

    void            SetHeaderBarColor       ( xcolor Color )                            { m_HeaderBarColor = Color; }
    void            SetHeaderColor          ( xcolor Color )                            { m_HeaderColor = Color; }

    void            EnableParentNavigation  ( void )                                    { m_AllowParentNavigate = TRUE;  }
    void            DisableParentNavigation ( void )                                    { m_AllowParentNavigate = FALSE; }

    void            EnableCursor            ( void )                                    { m_DisableCursor = FALSE;  }
    void            DisableCursor           ( void )                                    { m_DisableCursor = TRUE; }

    item&           GetItem                 ( s32 Index )                               { return m_Items[Index]; }
    s32             GetItemCount            ( void ) const;
    const xwstring& GetItemLabel            ( s32 iItem ) const;
    void            SetItemLabel            ( s32 iItem, const xwstring& Label );
    uaddr           GetItemData             ( s32 iItem, s32 Index = 0 ) const;
    const xwstring& GetSelectedItemLabel    ( void ) const;
    uaddr           GetSelectedItemData     ( s32 Index = 0 ) const;
    void            SetItemColor            ( s32 iItem, const xcolor& Color );
    xcolor          GetItemColor            ( s32 iItem ) const;

    s32             FindItemByLabel         ( const xwstring& Label ) const;
    s32             FindItemByData          ( uaddr Data, s32 Index = 0 ) const;

    s32             GetSelection            ( void ) const;
    void            SetSelection            ( s32 iSelection );
    void            ClearSelection          ( void );

    void            EnsureVisible           ( s32 iItem );
    s32             GetNumEnabledItems      ( void ) const;
    
    s32             GetCursorOffset         ( void ) const;
    void            SetSelectionWithOffset  ( s32 iSelection, s32 Offset );

    void            SetBackgroundColor      ( xcolor Color );
    xcolor          GetBackgroundColor      ( void ) const;

    void            AlphaSortList           ( void );
    virtual irect   GetListRect             ( void ) const;

protected:
    s32             GetMaxFirstVisibleItem  ( void ) const;
    xbool           SetFirstVisibleItem     ( s32 FirstVisibleItem );
    xbool           ScrollItems             ( s32 ItemDelta );
    s32             GetItemAt               ( s32 x, s32 y ) const;
    s32             FindEnabledItem         ( s32 Start, s32 Direction ) const;
    xbool           SelectUserItem          ( s32 iItem );
    virtual void    UpdateScrollBar         ( void );
    void            UpdateVisibleItemCount  ( void );

    //-------------------------------------------------------------------------

    xbool           m_ExitOnSelect;
    xbool           m_ExitOnBack;
    xbool           m_AcceptOnRepeatClick;
    s32             m_LastPointerSelection;
    xbool           m_ShowBorders;
    xbool           m_ShowFrame;
    xbool           m_ShowHeaderBar;
    xbool           m_AllowParentNavigate;
    xbool           m_DisableCursor;
    s32             m_iElementFrame;
    ui_scrollbar    m_ScrollBar;
    s32             m_HoveredItem;

    xarray<item>    m_Items;
    s32             m_iSelection;
    s32             m_iSelectionBackup;
    s32             m_iFirstVisibleItem;
    s32             m_nVisibleItems;
    s32             m_LineHeight;

    xcolor          m_BackgroundColor;
    xcolor          m_HeaderBarColor;
    xcolor          m_HeaderColor;
};

//==============================================================================
#endif // UI_LISTBOX_HPP
//==============================================================================
