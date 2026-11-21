//==============================================================================
//
//  ui_dlg_list.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"

#include "ui_dlg_list.hpp"
#include "ui_manager.hpp"
#include "ui_control.hpp"
#include "ui_font.hpp"
#include "ui_listbox.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum controls
{
    IDC_LIST
};

//==============================================================================
//  DATA
//==============================================================================

ui_manager::control_tem ListControls[] =
{
    { IDC_LIST, 0, "listbox", 0, 0, 0, 0, 0, 0, 1, 1, ui_win::WF_VISIBLE },
};

//==============================================================================

ui_manager::dialog_tem ListDialog =
{
    0,
    1, 1,
    sizeof(ListControls)/sizeof(ui_manager::control_tem),
    &ListControls[0],
    0
};

//==============================================================================
//  REGISTRATION
//==============================================================================

void ui_dlg_list_register( ui_manager* pManager )
{
    pManager->RegisterDialogClass( "ui_list", &ListDialog, &ui_dlg_list_factory );
}

//==============================================================================

ui_win* ui_dlg_list_factory( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData )
{
    ui_dlg_list* pDialog = new ui_dlg_list;
    pDialog->Create( UserID, pManager, pDialogTem, Position, pParent, Flags, pUserData );

    return pDialog;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_dlg_list::ui_dlg_list( void )
{
}

//==============================================================================

ui_dlg_list::~ui_dlg_list( void )
{
}

//==============================================================================

xbool ui_dlg_list::Create( s32                        UserID,
                           ui_manager*                pManager,
                           ui_manager::dialog_tem*    pDialogTem,
                           const irect&               Position,
                           ui_win*                    pParent,
                           s32                        Flags,
                           void*                      pUserData )
{
    xbool   Success = FALSE;

    (void)pUserData;

    ASSERT( pManager );

    // Make it input modal
    Flags |= WF_INPUTMODAL;

    // Do dialog creation
    Success = ui_dialog::Create( UserID, pManager, pDialogTem, Position, pParent, Flags );

    m_pResultPtr        = 0;
    m_BackgroundColor   = xcolor(0, 20, 30, 256);

    m_pList = static_cast<ui_listbox*>( FindChildByID( IDC_LIST ) );

    irect r = Position;
    r.Translate( -r.l, -r.t );
    r.Deflate( 8, 8 );
    m_pList->SetPosition( r );

    m_pList->SetActive( TRUE );

    // Return success code
    return Success;
}

//==============================================================================

void ui_dlg_list::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Get window rectangle
        irect   r;
        r.Set( m_Position.l+ox, m_Position.t+oy, m_Position.r+ox, m_Position.b+oy );

        // Render background color
        if( m_BackgroundColor.A > 0 )
        {
            irect rb = r;
            rb.Deflate( 1, 1 );
            m_pManager->RenderRect( rb, m_BackgroundColor, FALSE );
        }

        // Render frame
        m_pManager->RenderElement( m_iElement, r, 0 );

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//==============================================================================

ui_listbox* ui_dlg_list::GetListBox( void )
{
    return m_pList;
}

//==============================================================================

void ui_dlg_list::SetResultPtr( s32* pResultPtr )
{
    m_pResultPtr = pResultPtr;
}

//==============================================================================

void ui_dlg_list::OnNotify( ui_notification const& Event )
{
    if( Event.m_pSender == m_pList )
    {
        if( Event.m_Type == ui_notification_type::ListAccepted )
        {
            if( m_pResultPtr && (m_pList->GetSelection() != -1) )
            {
                *m_pResultPtr = m_pList->GetSelectedItemData();
            }
            m_pManager->EndDialog( m_UserID, TRUE );
        }
        else if( Event.m_Type == ui_notification_type::ListCancelled )
        {
            m_pManager->EndDialog( m_UserID, TRUE );
        }
    }
}

//==============================================================================

void ui_dlg_list::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;

    if( !m_Position.PointInRect( x, y ) )
    {
        m_pManager->EndDialog( m_UserID );
    }
}

//==============================================================================
