//==============================================================================
//
//  ui_check.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"
#include "ui_check.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

ui_win* ui_check_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_check* pcheck = new ui_check;
    pcheck->Create( UserID, pManager, Position, pParent, Flags );

    return pcheck;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_check::ui_check( void )
{
    m_bIsChecked = FALSE;
}

//==============================================================================

ui_check::~ui_check( void )
{
}

//==============================================================================

xbool ui_check::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Initialize Data
    m_iElement = m_pManager->FindElement( "button_check" );
    ASSERT( m_iElement != -1 );

    return Success;
}

//==============================================================================

void ui_check::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Calculate rectangle
        irect    r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        s32 const State = GetVisualState( m_bIsChecked );
        m_pManager->RenderElement( m_iElement, r, State );

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//==============================================================================

void ui_check::OnAccept( ui_win* pWin )
{
    if( pWin == this )
    {
        m_bIsChecked = !m_bIsChecked;

        // Notify Parent
        if( m_pParent )
        {
            Notify( ui_notification_type::CheckChanged, static_cast<s32>( m_bIsChecked  ) );
        }

    }
}

//==============================================================================

void ui_check::SetChecked( xbool State )
{
    m_bIsChecked = State;
}

//==============================================================================

xbool ui_check::IsChecked( void ) const
{
    return m_bIsChecked;
}
