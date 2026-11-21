//==============================================================================
//
//  ui_frame.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "ui_frame.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

ui_win* ui_frame_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_frame* pframe = new ui_frame;
    pframe->Create( UserID, pManager, Position, pParent, Flags );

    return pframe;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_frame::ui_frame( void )
{
}

//==============================================================================

ui_frame::~ui_frame( void )
{
}

//==============================================================================

xbool ui_frame::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Initialize Data
    m_iElement = m_pManager->FindElement( "frame" );
    ASSERT( m_iElement != -1 );
    m_BackgroundColor = xcolor(0,0,0,0);

    if( m_Flags & WF_TITLE )
    {
        m_Flags &= ~WF_TITLE;
    }

    return Success;
}

//==============================================================================

void ui_frame::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Calculate rectangle
        irect r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        // Render background rectangle first
        irect rb;
        rb = r;
        rb.Deflate( 1, 1 );
        if( m_BackgroundColor.A > 0 )
        {
            m_pManager->RenderRect( rb, m_BackgroundColor, FALSE );
        }

        // Render title
        if( m_Flags & WF_TITLE )
        {
            irect rect = r;
            if( m_BigTitle )
            {
                rect.SetHeight( 40 );
            }
            else
            {
                rect.SetHeight( 22 );
            }
            LocalToScreen(rb);
            rect.Deflate  ( 1, 0 );
            rect.Translate( 0, 1 );
            rect.Deflate( 8, 0 );
            rect.Translate( 0, -2 );
            rect.Translate( 1, 1 );
            
            // Check what size font we want.
            s32 TextSize;

            if( m_BigTitle )
            {
                TextSize = 2;
            }
            else
            {
                TextSize = 1;
            }

            m_pManager->RenderText( TextSize, rect, ui_font::h_left|ui_font::v_center, xcolor(XCOLOR_BLACK), m_Title );
            rect.Translate( -1, -1 );
            m_pManager->RenderText( TextSize, rect, ui_font::h_left|ui_font::v_center, xcolor(XCOLOR_WHITE), m_Title );
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

void ui_frame::SetBackgroundColor( xcolor Color )
{
    m_BackgroundColor = Color;
}

//==============================================================================

xcolor ui_frame::GetBackgroundColor( void ) const
{
    return m_BackgroundColor;
}

//==============================================================================

void ui_frame::EnableTitle ( const xwstring&   Text, xbool BigTitle )
{
    m_Flags |= WF_TITLE;
    m_Title = Text;
    m_BigTitle = BigTitle;
}

//==============================================================================

void ui_frame::EnableTitle ( const xwchar*     Text, xbool BigTitle )
{
    m_Flags |= WF_TITLE;
    m_Title = Text;
    m_BigTitle = BigTitle;
}

//==============================================================================

void ui_frame::ChangeElement ( const char* element )
{
    
    m_iElement = m_pManager->FindElement( element );

}

//==============================================================================
