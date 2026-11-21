//==============================================================================
//
//  ui_textbox.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"

#include "ui_textbox.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"
#include "StateMgr/StateMgr.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define SPACE_TOP       4
#define SPACE_BOTTOM    4
#define TEXT_OFFSET     -2

//==============================================================================
//  REGISTRATION
//==============================================================================

ui_win* ui_textbox_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_textbox* pcombo = new ui_textbox;
    pcombo->Create( UserID, pManager, Position, pParent, Flags );

    return pcombo;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_textbox::ui_textbox( void )
{
}

//==============================================================================

ui_textbox::~ui_textbox( void )
{
}

//==============================================================================

xbool ui_textbox::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Set default text flags
    m_LabelFlags = ui_font::h_center|ui_font::v_center|ui_font::clip_character|ui_font::clip_l_justify;
    m_BackgroundColor = xcolor(0,0,0,0);

    // Initialize data
    m_iElementFrame = m_pManager->FindElement( "sb_frame" );
    ASSERT( m_iElementFrame != -1 );
    Success &= m_ScrollBar.Create( m_pManager );

    m_ExitOnSelect      = FALSE;
    m_ExitOnBack        = FALSE;
    m_Font              = m_pManager->FindFont( "small" );
    m_LineHeight        = m_pManager->GetLineHeight( m_Font );
    m_iFirstVisibleLine = 0;
    m_nLines            = 0;
    m_ShowBorders       = TRUE;
    m_ShowFrame         = TRUE;
    m_nVisibleLines     = MAX( 1, (m_Position.GetHeight()-SPACE_TOP-SPACE_BOTTOM) / m_LineHeight );

    return Success;
}

//==============================================================================

void ui_textbox::SetLabel( const xwstring& Text )
{
    // Create wordwrap string
    irect r = m_Position;
    r.r -= 13;
    r.Deflate( 4, 4 );
    xwstring Wrapped;
    m_pManager->WordWrapString( m_Font, r, Text, Wrapped );

    m_Label  = Wrapped;
    m_pManager->TextSize( m_Font, m_TextRect, static_cast<const xwchar*>( m_Label ), -1 );
    m_nLines = m_TextRect.GetHeight() / m_LineHeight;
    m_iFirstVisibleLine = 0;
}

//==============================================================================

void ui_textbox::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Calculate rectangle
        irect   r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );
        r.r -= 14;

        // Render background color
        irect   rb = r;
        
        if (m_ShowFrame)
        {
            rb.Deflate( 1, 1 );
        }

        m_pManager->RenderRect( rb, m_BackgroundColor, FALSE );
        if( !(m_Flags & WF_DISABLED) )
        {
            if( IsFocused() || IsActive() )
            {
                m_pManager->RenderRect( rb, xcolor( 79, 214, 60, 48 ), FALSE );
            }

            if( IsHovered() && !m_ScrollBar.IsHovered() )
            {
                m_pManager->RenderRect( rb, xcolor( 79, 214, 60, 32 ), FALSE );
            }
        }

        // Render text offset by first visible line
        irect rt = r;
        rt.t += SPACE_TOP;
        rt.b -= SPACE_BOTTOM;
        rt.Deflate( 4, 0 );

        // Skip to the first visible line in the wrapped text by counting newlines.
        const xwchar* pVisibleText = static_cast<const xwchar*>( m_Label );
        for( s32 skipLines = m_iFirstVisibleLine; skipLines > 0 && *pVisibleText; )
        {
            if( *pVisibleText == '\n' )
            {
                skipLines--;
            }
            pVisibleText++;
        }

        // Force v_top and clipping so the textbox viewport owns its visible area.
        u32 renderFlags = (m_LabelFlags & ~(ui_font::v_center | ui_font::v_bottom)) |
                          ui_font::v_top |
                          ui_font::clip_character;
        xcolor const TextColor = (m_Flags & WF_DISABLED)
                               ? XCOLOR_GREY
                               : xcolor( 255, 252, 204, 255 );
        m_pManager->RenderText( m_Font, rt, renderFlags, TextColor, pVisibleText );

        if (m_ShowBorders)
        {
            // Render Frame
            if (m_ShowFrame)
            {
                m_pManager->RenderElement( m_iElementFrame, r, 0 );
            }

            UpdateScrollBar();
            m_ScrollBar.Render( (m_Flags & WF_DISABLED) == 0 );
        }

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//==============================================================================

void ui_textbox::SetPosition( const irect& Position )
{
    m_Position      = Position;
    m_nVisibleLines = MAX( 1, (m_Position.GetHeight()-SPACE_TOP-SPACE_BOTTOM) / m_LineHeight );
    SetFirstVisibleLine( m_iFirstVisibleLine );
}

//==============================================================================

void ui_textbox::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    s32 Direction = 0;

    // Determine movement required
    switch( Code )
    {
        case ui_navigation::Up:
        {
            Direction = -1;
        }
        break;

        case ui_navigation::Down:
        {
            Direction = 1;
        }
        break;

        default:
        {
        }
        break;
    }

    if( IsActive() && (Direction != 0) )
    {
        if( SetFirstVisibleLine( m_iFirstVisibleLine + Direction ) )
        {
            g_AudioMgr.Play( "Cusor_Norm" );
        }
        else if( Presses > 0 )
        {
            g_AudioMgr.Play( "InvalidEntry" );
        }
        return;
    }

    ui_win::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

//==============================================================================

void ui_textbox::OnPage( ui_win* pWin, s32 Direction )
{
    (void)pWin;

    s32 const PageStep = MAX( 1, m_nVisibleLines - 1 );
    if( SetFirstVisibleLine( m_iFirstVisibleLine + PageStep * Direction ) )
    {
        g_AudioMgr.Play( "Cusor_Norm" );
    }
}

//==============================================================================

void ui_textbox::OnJump( ui_win* pWin, s32 Direction )
{
    (void)pWin;

    s32 const FirstVisible = (Direction < 0) ? 0 : MAX( 0, m_nLines - m_nVisibleLines );
    if( SetFirstVisibleLine( FirstVisible ) )
    {
        g_AudioMgr.Play( "Cusor_Norm" );
    }
}

//==============================================================================

void ui_textbox::OnAccept( ui_win* pWin )
{
    if ( ( IsActive() ) && ( m_ExitOnSelect ) )
    {
        if( m_pParent )
        {
            m_pParent->OnAccept( pWin );
        }
    }
    else
    {
        // Toggle Selected
        SetActive( !IsActive() );

    }

}

//==============================================================================

void ui_textbox::OnCancel( ui_win* pWin )
{
    if( ( IsActive() ) && ( !m_ExitOnBack ) )
    {
        // Clear selected
        SetActive( FALSE );
    }
    else
    {
        if( m_pParent )
        {
            m_pParent->OnCancel( pWin );
        }
    }
}

//==============================================================================

void ui_textbox::SetBackgroundColor( xcolor Color )
{
    m_BackgroundColor = Color;
}

//==============================================================================

xcolor ui_textbox::GetBackgroundColor( void ) const
{
    return m_BackgroundColor;
}

//==============================================================================

s32 ui_textbox::GetLineCount( void ) const
{
    return m_nLines;
}

//==============================================================================

void ui_textbox::EnsureVisible( s32 iLine )
{
    if( iLine < m_iFirstVisibleLine )
    {
        SetFirstVisibleLine( iLine );
    }
    else if( iLine >= (m_iFirstVisibleLine + m_nVisibleLines) )
    {
        SetFirstVisibleLine( iLine - m_nVisibleLines + 1 );
    }
}

//==============================================================================

xbool ui_textbox::SetFirstVisibleLine( s32 FirstVisibleLine )
{
    s32 const MaxFirstVisibleLine = MAX( 0, m_nLines - m_nVisibleLines );
    FirstVisibleLine = x_clamp( FirstVisibleLine, 0, MaxFirstVisibleLine );
    if( FirstVisibleLine == m_iFirstVisibleLine )
    {
        return FALSE;
    }

    m_iFirstVisibleLine = FirstVisibleLine;
    return TRUE;
}

//==============================================================================

void ui_textbox::UpdateScrollBar( void )
{
    irect Bounds( GetWidth() - 14, 0, GetWidth(), GetHeight() );
    LocalToScreen( Bounds );
    m_ScrollBar.SetBounds( Bounds );
    m_ScrollBar.SetRange( m_nLines, m_nVisibleLines, m_iFirstVisibleLine );
}

//==============================================================================

void ui_textbox::OnPointerMove ( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    UpdateScrollBar();
    if( m_ShowBorders && m_ScrollBar.OnPointerMove( x, y ) )
    {
        SetFirstVisibleLine( m_ScrollBar.GetPosition() );
    }
    else if( !m_ShowBorders )
    {
        m_ScrollBar.OnPointerLeave();
    }
}

//==============================================================================

void ui_textbox::OnPointerLeave( ui_win* pWin )
{
    (void)pWin;
    m_ScrollBar.OnPointerLeave();
}

//==============================================================================

void ui_textbox::OnPointerWheel( ui_win* pWin, s32 Delta )
{
    UpdateScrollBar();
    if( m_ScrollBar.OnWheel( Delta ) )
    {
        SetFirstVisibleLine( m_ScrollBar.GetPosition() );
        return;
    }

    ui_win::OnPointerWheel( pWin, Delta );
}

//==============================================================================

void ui_textbox::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;

    UpdateScrollBar();
    if( m_ShowBorders && m_ScrollBar.OnPointerDown( x, y ) )
    {
        SetFirstVisibleLine( m_ScrollBar.GetPosition() );
        if( m_ScrollBar.IsInteracting() )
        {
            m_pManager->SetCapture( m_UserID, this );
        }
    }
}

//==============================================================================

void ui_textbox::OnPointerUp( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    (void)x;
    (void)y;

    if( m_ScrollBar.OnPointerUp() )
    {
        m_pManager->ReleaseCapture( m_UserID );
    }
}

//==============================================================================

void ui_textbox::OnUpdate ( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    UpdateScrollBar();
    if( m_ScrollBar.OnUpdate( DeltaTime ) )
    {
        SetFirstVisibleLine( m_ScrollBar.GetPosition() );
    }
}

//==============================================================================

void ui_textbox::OnFocusLost ( ui_win* pWin )
{
    xbool const HadScrollCapture = m_ScrollBar.IsInteracting();
    m_ScrollBar.CancelInteraction();
    if( HadScrollCapture )
    {
        m_pManager->ReleaseCapture( m_UserID );
    }

    ui_win::OnFocusLost( pWin );
}

//==============================================================================
