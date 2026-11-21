//==============================================================================
//
//  ui_win.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"

#include "ui_win.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_win::ui_win( void )
{
    m_pManager   = NULL;
    m_pParent    = NULL;
    m_UserID     = -1;
    m_ID         = -1;
    m_Flags      = 0;
    m_Font       = 0;
    m_IsActive   = FALSE;
    m_IsFocused  = FALSE;
    m_IsHovered  = FALSE;
    m_IsPressed  = FALSE;
    m_LabelColor = XCOLOR_WHITE;
}

//==============================================================================

ui_win::~ui_win( void )
{
    Destroy();
}

//==============================================================================

xbool ui_win::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success = TRUE;

    m_UserID         = UserID;
    m_pManager       = pManager;
    m_pParent        = pParent;
    m_Position       = Position;
    m_Flags          = Flags;
    m_LabelFlags     = ui_font::h_center|ui_font::v_center;
    m_LabelColor     = XCOLOR_WHITE;
    m_IsActive       = FALSE;
    m_IsFocused      = FALSE;
    m_IsHovered      = FALSE;
    m_IsPressed      = FALSE;

    // Add child entry to parent window if we have a parent
    if( m_pParent )
    {
        m_pParent->m_Children.Append() = this;
    }

    return Success;
}

//==============================================================================

void ui_win::Destroy( void )
{
    s32     i;
    s32     iFound = -1;

    // Kill Children
    while( m_Children.GetCount() > 0 )
    {
        ui_win* pChild = m_Children[0];
        delete pChild;
    }

    if( m_pParent )
    {
        // Find in parents child list and remove
        for( i=0 ; i<m_pParent->m_Children.GetCount() ; i++ )
        {
            if( m_pParent->m_Children[i] == this )
            {
                iFound = i;
            }
        }
        if( iFound != -1 )
        {
            m_pParent->m_Children.Delete( iFound );
        }

        // Clear Parent pointer
        m_pParent = NULL;
    }
}

//==============================================================================

void ui_win::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( ox, oy );
        }
    }
}

//==============================================================================

void ui_win::UpdateTree( f32 DeltaTime )
{
    OnUpdate( this, DeltaTime );

    for( s32 i = 0; i < m_Children.GetCount(); i++ )
    {
        m_Children[i]->UpdateTree( DeltaTime );
    }
}

//==============================================================================

void ui_win::SetPosition( const irect& Position )
{
    m_Position = Position;
}

//==============================================================================

const irect& ui_win::GetPosition( void ) const
{
    return m_Position;
}

//==============================================================================

s32 ui_win::GetWidth( void ) const
{
    return m_Position.GetWidth();
}

//==============================================================================

s32 ui_win::GetHeight( void ) const
{
    return m_Position.GetHeight();
}

//==============================================================================

ui_win* ui_win::GetWindowAtXY( s32 x, s32 y ) const
{
    ui_win* pFound = NULL;

    // Don't process for STATIC, DISABLED or INVISIBLE windows
    if( !(m_Flags & WF_STATIC) && !(m_Flags & WF_DISABLED) && (m_Flags & WF_VISIBLE) )
    {
        // Check if the coordinates hit our rectangle
        if( ((x >= 0) && (x < m_Position.GetWidth())) &&
            ((y >= 0) && (y < m_Position.GetHeight())) )
        {
            // Loop through all children testing
            for( s32 i=m_Children.GetCount()-1 ; (i>=0) && !pFound ; i-- )
            {
                irect r = m_Children[i]->GetPosition();
                pFound = m_Children[i]->GetWindowAtXY( x - r.l, y - r.t );
            }

            // If no child found then return this window
            if( pFound == NULL )
            {
                pFound = const_cast<ui_win*>( this );
            }
        }
    }

    // Return window found
    return pFound;
}

//==============================================================================

void ui_win::SetFlags( s32 Flags )
{
    m_Flags = Flags;
}

//==============================================================================

s32 ui_win::GetFlags( void ) const
{
    return m_Flags;
}

//==============================================================================

void ui_win::SetFlag( s32 Flag, s32 State )
{
    if( State )
    {
        m_Flags |= Flag;
    }
    else
    {
        m_Flags &= ~Flag;
    }
}

//==============================================================================

s32 ui_win::GetFlags( s32 Flag ) const
{
    return m_Flags & Flag;
}

//==============================================================================

void ui_win::SetActive( xbool State )
{
    m_IsActive = State;
}

//==============================================================================

xbool ui_win::IsActive( void ) const
{
    return m_IsActive;
}

//==============================================================================

xbool ui_win::IsFocused( void ) const
{
    return m_IsFocused;
}

//==============================================================================

xbool ui_win::IsHovered( void ) const
{
    return m_IsHovered;
}

//==============================================================================

xbool ui_win::IsPressed( void ) const
{
    return m_IsPressed;
}

//==============================================================================

xbool ui_win::CanFocus( void ) const
{
    return (m_Flags & (WF_VISIBLE | WF_STATIC | WF_DISABLED)) == WF_VISIBLE;
}

//==============================================================================

void ui_win::SetLabel( const xwstring& Text )
{
    m_Label = Text;
}

//==============================================================================

void ui_win::SetLabel( const xwchar* Text )
{
    m_Label = Text;
}

//==============================================================================

const xwstring& ui_win::GetLabel( void ) const
{
    return m_Label;
}

//==============================================================================

void ui_win::SetLabelFlags( u32 Flags )
{
    m_LabelFlags = Flags;
}

//==============================================================================

u32 ui_win::GetLabelFlags( void ) const
{
    return m_LabelFlags;
}

//==============================================================================

void ui_win::SetControlID( s32 ID )
{
    m_ID = ID;
}

//==============================================================================

s32 ui_win::GetControlID( void ) const
{
    return m_ID;
}

//==============================================================================
void ui_win::SetLabelColor(const xcolor& color)
{
    m_LabelColor = color;
}

//==============================================================================
const xcolor& ui_win::GetLabelColor(void) const
{
    return m_LabelColor;
}

//==============================================================================

void ui_win::SetParent( ui_win* pParent )
{
    ASSERT( pParent != this );
    ASSERT( !pParent || !pParent->IsChildOf( this ) );

    if( pParent == m_pParent )
    {
        return;
    }

    s32     i;
    s32     iFound = -1;

    // Remove from previous parent
    if( m_pParent )
    {
        // Find in parents child list and remove
        for( i=0 ; i<m_pParent->m_Children.GetCount() ; i++ )
        {
            if( m_pParent->m_Children[i] == this )
            {
                iFound = i;
            }
        }
        if( iFound != -1 )
        {
            m_pParent->m_Children.Delete( iFound );
        }
    }

    // Add to new parent
    m_pParent = pParent;
    if( m_pParent )
    {
        m_pParent->m_Children.Append() = this;
    }
}

//==============================================================================

ui_win* ui_win::GetParent( void ) const
{
    return m_pParent;
}

//==============================================================================

ui_win* ui_win::FindChildByID( s32 ID ) const
{
    for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
    {
        ui_win* pChild = m_Children[i];
        if( pChild->m_ID == ID )
        {
            return pChild;
        }
    }

    // Failed to find the child
    ASSERT( 0 );

    return NULL;
}

//==============================================================================

xbool ui_win::IsChildOf( ui_win* pParent ) const
{
    if( m_pParent )
    {
        if( m_pParent == pParent )
        {
            return TRUE;
        }
        else
        {
            return m_pParent->IsChildOf( pParent );
        }
    }

    return FALSE;
}

//==============================================================================

void ui_win::LocalToScreen( s32& x, s32& y ) const
{
    x += m_Position.l;
    y += m_Position.t;
    if( m_pParent )
    {
        m_pParent->LocalToScreen( x, y );
    }
}

//==============================================================================

void ui_win::ScreenToLocal( s32& x, s32& y ) const
{
    x -= m_Position.l;
    y -= m_Position.t;
    if( m_pParent )
    {
        m_pParent->ScreenToLocal( x, y );
    }
}

//==============================================================================

void ui_win::LocalToScreen( irect& r ) const
{
    r.l += m_Position.l;
    r.r += m_Position.l;
    r.t += m_Position.t;
    r.b += m_Position.t;
    if( m_pParent )
    {
        m_pParent->LocalToScreen( r );
    }
}

//==============================================================================

void ui_win::ScreenToLocal( irect& r ) const
{
    r.l -= m_Position.l;
    r.r -= m_Position.l;
    r.t -= m_Position.t;
    r.b -= m_Position.t;
    if( m_pParent )
    {
        m_pParent->ScreenToLocal( r );
    }
}

//==============================================================================
//  MESSAGE HANDLERS
//==============================================================================

void ui_win::Notify( ui_notification_type Type, s32 Value )
{
    if( m_pParent )
    {
        ui_notification const Event = { Type, this, Value, NULL };
        m_pParent->OnNotify( Event );
    }
}

//==============================================================================

void ui_win::Notify( ui_notification_type Type, xwstring const& Text )
{
    if( m_pParent )
    {
        ui_notification const Event = { Type, this, 0, &Text };
        m_pParent->OnNotify( Event );
    }
}

//==============================================================================

xbool ui_win::OnInput( ui_input_event& Event )
{
    switch( Event.m_Type )
    {
        case ui_input_event_type::Navigate:
        {
            switch( Event.m_Navigation )
            {
                case ui_navigation::Up:
                {
                    OnNavigate( Event.m_pTarget, ui_navigation::Up, Event.m_Presses, Event.m_Repeats, Event.m_WrapX, Event.m_WrapY );
                }
                break;

                case ui_navigation::Down:
                {
                    OnNavigate( Event.m_pTarget, ui_navigation::Down, Event.m_Presses, Event.m_Repeats, Event.m_WrapX, Event.m_WrapY );
                }
                break;

                case ui_navigation::Left:
                {
                    OnNavigate( Event.m_pTarget, ui_navigation::Left, Event.m_Presses, Event.m_Repeats, Event.m_WrapX, Event.m_WrapY );
                }
                break;

                case ui_navigation::Right:
                {
                    OnNavigate( Event.m_pTarget, ui_navigation::Right, Event.m_Presses, Event.m_Repeats, Event.m_WrapX, Event.m_WrapY );
                }
                break;

                case ui_navigation::PagePrevious:
                {
                    OnPage( Event.m_pTarget, -1 );
                }
                break;

                case ui_navigation::PageNext:
                {
                    OnPage( Event.m_pTarget, 1 );
                }
                break;

                case ui_navigation::First:
                {
                    OnJump( Event.m_pTarget, -1 );
                }
                break;

                case ui_navigation::Last:
                {
                    OnJump( Event.m_pTarget, 1 );
                }
                break;

                default:
                {
                    return FALSE;
                }
            }
        }
        break;

        case ui_input_event_type::Accept:
        {
            OnAccept( Event.m_pTarget );
        }
        break;

        case ui_input_event_type::Cancel:
        {
            OnCancel( Event.m_pTarget );
        }
        break;

        case ui_input_event_type::Delete:
        {
            OnDelete( Event.m_pTarget );
        }
        break;

        case ui_input_event_type::Alternate:
        {
            OnAlternate( Event.m_pTarget );
        }
        break;

        case ui_input_event_type::Help:
        {
            OnHelp( Event.m_pTarget );
        }
        break;

        case ui_input_event_type::PointerMove:
        {
            OnPointerMove( Event.m_pTarget, Event.m_X, Event.m_Y );
        }
        break;

        case ui_input_event_type::PointerDown:
        {
            if( Event.m_PointerButton == ui_pointer_button::Primary )
            {
                OnPointerDown( Event.m_pTarget, Event.m_X, Event.m_Y );
            }
            else
            {
                return FALSE;
            }
        }
        break;

        case ui_input_event_type::PointerUp:
        {
            if( Event.m_PointerButton == ui_pointer_button::Primary )
            {
                OnPointerUp( Event.m_pTarget, Event.m_X, Event.m_Y );
            }
            else
            {
                return FALSE;
            }
        }
        break;

        case ui_input_event_type::PointerWheel:
        {
            OnPointerWheel( Event.m_pTarget, Event.m_Delta );
        }
        break;

        default:
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

void ui_win::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;
    (void)DeltaTime;
}

//==============================================================================

void ui_win::OnNotify( ui_notification const& Event )
{
    if( m_pParent )
    {
        m_pParent->OnNotify( Event );
    }
}

//==============================================================================

void ui_win::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)x;
    (void)y;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnAccept( pWin );
    }
}

//==============================================================================

void ui_win::OnPointerUp( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    (void)x;
    (void)y;
}

//==============================================================================

void ui_win::OnPointerMove( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    (void)x;
    (void)y;
}

//==============================================================================

void ui_win::OnPointerLeave( ui_win* pWin )
{
    (void)pWin;
}

//==============================================================================

void ui_win::OnPointerWheel( ui_win* pWin, s32 Delta )
{
    if( m_pParent )
    {
        m_pParent->OnPointerWheel( pWin, Delta );
    }
}

//==============================================================================

void ui_win::OnFocusGained( ui_win* pWin )
{
    (void)pWin;
    m_IsFocused = TRUE;
}

//==============================================================================

void ui_win::OnFocusLost( ui_win* pWin )
{
    (void)pWin;
    m_IsFocused = FALSE;
    m_IsPressed = FALSE;
}

//==============================================================================

void ui_win::OnFocusWithin( ui_win* pWin )
{
    (void)pWin;
}

//==============================================================================

void ui_win::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    (void)pWin;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
    }
}

//==============================================================================

void ui_win::OnAccept( ui_win* pWin )
{
    (void)pWin;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnAccept( pWin );
    }
}

//==============================================================================

void ui_win::OnCancel( ui_win* pWin )
{
    (void)pWin;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnCancel( pWin );
    }
}

//==============================================================================

void ui_win::OnDelete( ui_win* pWin )
{
    (void)pWin;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnDelete( pWin );
    }
}

//==============================================================================

void ui_win::OnHelp( ui_win* pWin )
{
    (void)pWin;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnHelp( pWin );
    }
}

//==============================================================================

void ui_win::OnAlternate( ui_win* pWin )
{
    (void)pWin;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnAlternate( pWin );
    }
}

//==============================================================================

void ui_win::OnPage( ui_win* pWin, s32 Direction )
{
    (void)pWin;
    (void)Direction;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnPage( pWin, Direction );
    }
}

//==============================================================================

void ui_win::OnJump( ui_win* pWin, s32 Direction )
{
    (void)pWin;
    (void)Direction;

    // Pass up chain to parent
    if( m_pParent )
    {
        m_pParent->OnJump( pWin, Direction );
    }
}

//==============================================================================
