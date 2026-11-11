//=========================================================================
//
//  ui_edit.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "ui_edit.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"
#include "ui_dlg_vkeyboard.hpp"

//=========================================================================
//  Defines
//=========================================================================

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

//=========================================================================
//  Factory function
//=========================================================================

ui_win* ui_edit_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_edit* pedit = new ui_edit;
    pedit->Create( UserID, pManager, Position, pParent, Flags );

    return (ui_win*)pedit;
}

//=========================================================================
//  ui_edit
//=========================================================================

ui_edit::ui_edit( void )
{
    m_bName = TRUE;
}

//=========================================================================

ui_edit::~ui_edit( void )
{
}

//=========================================================================

xbool ui_edit::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    m_iElement1 = m_pManager->FindElement( "button_edit" );
    ASSERT( m_iElement1 != -1 );

    m_LabelWidth    = 0;
    m_BufferSize    = -1;

    m_Font = m_pManager->FindFont("small");

    return Success;
}

//=========================================================================

void ui_edit::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        s32 const State = GetVisualState( IsActive() );
        xcolor const TextColor = (m_Flags & WF_DISABLED)
                               ? XCOLOR_GREY
                               : xcolor( 255, 252, 204, 255 );

        // Calculate rectangle
        irect    r, r2;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );
        r2 = r;
        m_pManager->RenderElement( m_iElement1, r2, State );

        // Set a clip window to render the text
        r2.Deflate( 4, 1 );
        m_pManager->PushClipWindow( r2 );

        // Render Text
        irect rt = r2;
        rt.l += 1;
        rt.r -= 3;
        m_pManager->RenderText( m_Font, rt, ui_font::h_center|ui_font::v_center|ui_font::clip_ellipsis|ui_font::clip_l_justify,
                               TextColor, m_Label.GetLength() ? m_Label : m_Placeholder );

        // Clear the clip window
        m_pManager->PopClipWindow();

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//=========================================================================

void ui_edit::OnAccept( ui_win* pWin )
{
    (void)pWin;

    irect   r = m_pManager->GetUserBounds( m_UserID );

    // Open virtual keyboard dialog and connect it to the edit string
    ui_dlg_vkeyboard* pVKeyboard = (ui_dlg_vkeyboard*)m_pManager->OpenDialog( m_UserID, "ui_vkeyboard", r, NULL, ui_win::WF_VISIBLE|ui_win::WF_INPUTMODAL );
    if( !pVKeyboard )
    {
        return;
    }

    pVKeyboard->Configure( m_bName );
    pVKeyboard->ConnectString( &m_Label, m_BufferSize );
    pVKeyboard->SetLabel( m_VirtualKeyboardTitle );
}

//=========================================================================

void ui_edit::SetLabelWidth( s32 Width )
{
    m_LabelWidth = Width;
}

//=========================================================================

void ui_edit::SetBufferSize( s32 BufferSize )
{
    m_BufferSize = BufferSize;
}

//=========================================================================

void ui_edit::SetVirtualKeyboardTitle( const xwstring& Title )
{
    m_VirtualKeyboardTitle = Title;
}
