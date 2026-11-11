//=========================================================================
//
//  ui_control.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "ui_control.hpp"
#include "ui_manager.hpp"

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
//  ui_control
//=========================================================================

ui_control::ui_control( void )
{
    m_NavPos.Set( 0, 0, 0, 0 );
}

//=========================================================================

ui_control::~ui_control( void )
{
}

//=========================================================================

xbool ui_control::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_win::Create( UserID, pManager, Position, pParent, Flags );

    return Success;
}

//=========================================================================

void ui_control::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Render placeholder rectangle
        xcolor  Color = XCOLOR_WHITE;

        // Calculate rectangle
        irect    r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        // Render appropriate state
        if( m_Flags & WF_DISABLED )
        {
            Color = XCOLOR_GREY;
        }
        else if( ShouldRenderHighlight() || IsActive() )
        {
            Color = xcolor(255,0,0,255);
        }
        else
        {
            Color = XCOLOR_WHITE;
        }

        m_pManager->RenderRect( r, Color, TRUE );

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//=========================================================================

void ui_control::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)x;
    (void)y;

    OnAccept( pWin );
}

//=========================================================================

const irect& ui_control::GetNavPos( void ) const
{
    return m_NavPos;
}

//=========================================================================

void ui_control::SetNavPos( const irect& r )
{
    m_NavPos = r;
}

//=========================================================================

ui_control::visual_state ui_control::GetVisualState( xbool Active ) const
{
    if( m_Flags & WF_DISABLED )
    {
        return CS_DISABLED;
    }

    if( ShouldRenderHighlight() )
    {
        return Active ? CS_HIGHLIGHTED_ACTIVE
                      : CS_HIGHLIGHTED;
    }

    return Active ? CS_ACTIVE
                  : CS_NORMAL;
}

//=========================================================================

xbool ui_control::ShouldRenderHighlight( void ) const
{
    return IsFocused() || IsHovered() || IsPressed();
}

//=========================================================================

void ui_control::RenderHighlightRect( const irect& Bounds, highlight_state State ) const
{
    xcolor Color( 79, 214, 60, 51 );

    switch( State )
    {
    case HS_HOVERED:
        break;

    case HS_SELECTED:
        Color = xcolor( 66, 158, 11, 128 );
        break;

    case HS_ACTIVE:
        Color.A = (u8)(57 + m_pManager->GetHighlightAlpha());
        break;

    case HS_PRESSED:
        Color.A = 224;
        break;

    case HS_NONE:
        return;
    }

    m_pManager->RenderRect( Bounds, Color, FALSE );
}

//=========================================================================
