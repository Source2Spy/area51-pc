//==============================================================================
//
//  ui_radio.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "ui_radio.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

ui_win* ui_radio_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_radio* pradio = new ui_radio;
    pradio->Create( UserID, pManager, Position, pParent, Flags );

    return pradio;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_radio::ui_radio( void )
{
}

//==============================================================================

ui_radio::~ui_radio( void )
{
}

//==============================================================================

xbool ui_radio::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Initialize Data
    m_iElement = m_pManager->FindElement( "button_check" );
    ASSERT( m_iElement != -1 );

    return Success;
}

//==============================================================================

void ui_radio::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        s32 const State = GetVisualState( IsActive() );
        xcolor const ShadowColor = (m_Flags & WF_DISABLED)
                                 ? xcolor( 0, 0, 0, 0 )
                                 : XCOLOR_BLACK;

        // Calculate rectangle
        irect    r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        m_pManager->RenderElement( m_iElement, r, State );

        // Render Text
        r.Translate( 1, -1 );
        m_pManager->RenderText( m_pManager->FindFont("large"), r, ui_font::h_center|ui_font::v_center, ShadowColor, m_Label );
        r.Translate( -1, -1 );
        m_pManager->RenderText( m_pManager->FindFont("large"), r, ui_font::h_center|ui_font::v_center, xcolor(150,150,150,255), m_Label );

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//==============================================================================
