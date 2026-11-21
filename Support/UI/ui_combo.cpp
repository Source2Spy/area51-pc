//==============================================================================
//
//  ui_combo.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"

#include "ui_combo.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

ui_win* ui_combo_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_combo* pcombo = new ui_combo;
    pcombo->Create( UserID, pManager, Position, pParent, Flags );

    return pcombo;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_combo::ui_combo( void )
{
}

//==============================================================================

ui_combo::~ui_combo( void )
{
}

//==============================================================================

xbool ui_combo::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Initialize data
    
    m_iElement1 = m_pManager->FindElement( "button_combo1" );
    ASSERT( m_iElement1 != -1 );

    m_NavFlags          = 0;
    m_iSelection        = -1;
    m_LabelWidth        = 0;
    m_Font              = m_pManager->FindFont("small");

    return Success;
}

//==============================================================================

void ui_combo::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        xcolor  TextColor1  = XCOLOR_WHITE;
        s32 const State     = GetVisualState( IsActive() );

        // Calculate rectangle
        irect    r, r2;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );
        r2 = r;
        r.r = r.l + m_LabelWidth;
        r2.l = r.r;

        // set item color
        if( m_iSelection != -1 )
        {
            TextColor1  = m_Items[m_iSelection].Color; 
        }
        else
        {
            TextColor1  = xcolor(255,252,204,255);
        }

        // Use the disabled color for either a disabled control or item.
        if( m_Flags & WF_DISABLED )
        {
            TextColor1  = XCOLOR_GREY;
        }

        if( m_iSelection != -1 )
        {
            if( !m_Items[m_iSelection].Enabled )
            {
                TextColor1  = XCOLOR_GREY;
            }

        }

        // Render bitmap (if any)
        if( m_iSelection != -1 )
        {
            if( m_Items[m_iSelection].BitmapID != -1 )
            {
                m_pManager->RenderBitmap( m_Items[m_iSelection].BitmapID, r2 );
            }
            else
            {
                // Render single field Combo
                m_pManager->RenderElement( m_iElement1, r2, State );
            }
        }
        else
        {
            // Render single field Combo
            m_pManager->RenderElement( m_iElement1, r2, State );
        }

        // Render Selection Text
        if( m_iSelection != -1 )
        {
            m_pManager->RenderText( m_Font, r2, ui_font::h_center|ui_font::v_center, TextColor1, m_Items[m_iSelection].Label );
        }

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//==============================================================================

void ui_combo::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    xbool bHandled = FALSE;

    if ( m_NavFlags & CB_CHANGE_ON_NAV )
    {
        // check 
        if (Code == ui_navigation::Left)
        {
            // Move Back in List
            OnPage( pWin, -1 );
            bHandled = TRUE;
        }
        else if (Code == ui_navigation::Right)
        {
            // Move Forward in List
            OnPage( pWin, 1 );
            bHandled = TRUE;
        }

        // Pass up chain
        if( m_pParent )
        {
            if( ( m_NavFlags & CB_NOTIFY_PARENT ) || ( !bHandled ) )
            {
                m_pParent->OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
            }
        }
    }
    else
    {
        // Pass up chain
        if( m_pParent )
        {
            m_pParent->OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
        }
    }
}

//==============================================================================

void ui_combo::OnAccept( ui_win* pWin )
{
    if ( m_NavFlags & CB_CHANGE_ON_SELECT )
    {
        // Move Forward in List
        OnPage( pWin, 1 );

        // Pass up chain
        if( m_pParent )
        {
            if( m_NavFlags & CB_NOTIFY_PARENT ) 
            {
                m_pParent->OnAccept( pWin );
            }
        }
    }
    else
    {
        if ( m_pParent )
        {
            m_pParent->OnAccept ( pWin );
        }
    }
}

//==============================================================================

void ui_combo::OnPage( ui_win* pWin, s32 Direction )
{
    (void)pWin;

    if( (Direction == 0) || (m_Items.GetCount() == 0) )
    {
        return;
    }

    s32 const OldSelection = m_iSelection;
    s32 iSelection = (m_iSelection == -1)
                   ? ((Direction > 0) ? 0 : m_Items.GetCount() - 1)
                   : m_iSelection;

    for( s32 i = 0; i < m_Items.GetCount(); i++ )
    {
        if( (OldSelection != -1) || (i > 0) )
        {
            iSelection += Direction;
            if( iSelection < 0 )
            {
                iSelection = m_Items.GetCount() - 1;
            }
            else if( iSelection >= m_Items.GetCount() )
            {
                iSelection = 0;
            }
        }

        if( m_Items[iSelection].Enabled )
        {
            m_iSelection = iSelection;
            if( m_iSelection != OldSelection )
            {
                Notify( ui_notification_type::ComboSelectionChanged, m_iSelection );
                g_AudioMgr.Play( "Toggle" );
            }
            return;
        }
    }

    m_iSelection = OldSelection;
    g_AudioMgr.Play( "InvalidEntry" );
}

//==============================================================================

void ui_combo::SetLabelWidth( s32 Width )
{
    m_LabelWidth = Width;
}

//==============================================================================

s32 ui_combo::AddItem( const xwstring& Label, uaddr Data1, uaddr Data2 )
{
    item& Item      = m_Items.Append();
    Item.Label      = Label;
    Item.Enabled    = TRUE;
    Item.Data[0]    = Data1;
    Item.Data[1]    = Data2;
    Item.BitmapID   = -1;
    Item.Color      = xcolor(255,252,204,255);

    return m_Items.GetCount()-1;
}

//==============================================================================

s32 ui_combo::AddItem( const xwchar* Label, uaddr Data1, uaddr Data2 )
{
    item& Item      = m_Items.Append();
    Item.Label      = Label;
    Item.Enabled    = TRUE;
    Item.Data[0]    = Data1;
    Item.Data[1]    = Data2;
    Item.BitmapID   = -1;
    Item.Color      = xcolor(255,252,204,255);

    return m_Items.GetCount()-1;
}

//==============================================================================

void ui_combo::SetItemEnabled( s32 iItem, xbool State )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    m_Items[iItem].Enabled = State;
}

//==============================================================================

void ui_combo::SetItemBitmap( s32 iItem, s32 ID )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    m_Items[iItem].BitmapID = ID;
}

//==============================================================================

void ui_combo::SetItemColor( s32 iItem, xcolor Color )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    m_Items[iItem].Color = Color;
}

//==============================================================================

void ui_combo::DeleteAllItems( void )
{
    s32 const OldSelection = m_iSelection;
    m_iSelection = -1;
    m_Items.Delete( 0, m_Items.GetCount() );

    if( OldSelection != m_iSelection )
    {
        Notify( ui_notification_type::ComboSelectionChanged, m_iSelection );
    }
}

//==============================================================================

void ui_combo::DeleteItem( s32 iItem )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    s32 const OldSelection = m_iSelection;
    m_Items.Delete( iItem );

    if( m_Items.GetCount() == 0 )
    {
        m_iSelection = -1;
    }
    else if( iItem < OldSelection )
    {
        m_iSelection = OldSelection - 1;
    }
    else if( iItem == OldSelection )
    {
        m_iSelection = MIN( iItem, m_Items.GetCount() - 1 );
    }

    if( (m_iSelection != OldSelection) || (iItem == OldSelection) )
    {
        Notify( ui_notification_type::ComboSelectionChanged, m_iSelection );
    }
}

//==============================================================================

s32 ui_combo::GetItemCount( void ) const
{
    return m_Items.GetCount();
}

//==============================================================================

const xwstring& ui_combo::GetItemLabel( s32 iItem ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );

    return m_Items[iItem].Label;
}

//==============================================================================

s32 ui_combo::GetItemBitmap( s32 iItem ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    return m_Items[iItem].BitmapID;
}

//==============================================================================

uaddr ui_combo::GetItemData( s32 iItem, s32 Index ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    ASSERT( (Index >= 0) && (Index < COMBO_DATA_FIELDS) );

    return m_Items[iItem].Data[Index];
}

//==============================================================================

const xwstring& ui_combo::GetSelectedItemLabel( void ) const
{
    ASSERT( (m_iSelection >= 0) && (m_iSelection < m_Items.GetCount()) );

    return m_Items[m_iSelection].Label;
}

//==============================================================================

uaddr ui_combo::GetSelectedItemData( s32 Index ) const
{
    ASSERT( (m_iSelection >= 0) && (m_iSelection < m_Items.GetCount()) );
    ASSERT( (Index >= 0) && (Index < COMBO_DATA_FIELDS) );

    return m_Items[m_iSelection].Data[Index];
}

//==============================================================================

xbool ui_combo::GetItemEnabled( s32 iItem ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    return (m_Items[iItem].Enabled);
}

//==============================================================================

xbool ui_combo::GetSelectedItemEnabled( void ) const
{
    ASSERT( (m_iSelection >= 0) && (m_iSelection < m_Items.GetCount()) );
    return (m_Items[m_iSelection].Enabled);
}

//==============================================================================

s32 ui_combo::FindItemByLabel( const xwstring& Label ) const
{
    s32     i;
    s32     iFound = -1;

    for( i=0 ; i<m_Items.GetCount() ; i++ )
    {
        if( m_Items[i].Label == Label )
        {
            iFound = i;
            break;
        }
    }

    return iFound;
}

//==============================================================================

s32 ui_combo::FindItemByData( uaddr Data, s32 Index ) const
{
    ASSERT( (Index >= 0) && (Index < COMBO_DATA_FIELDS) );

    s32     i;
    s32     iFound = -1;

    for( i=0 ; i<m_Items.GetCount() ; i++ )
    {
        if( m_Items[i].Data[Index] == Data )
        {
            iFound = i;
            break;
        }
    }

    return iFound;
}

//==============================================================================

s32 ui_combo::GetSelection( void ) const
{
    return m_iSelection;
}

//==============================================================================

void ui_combo::SetSelection( s32 iSelection )
{
    ASSERT( (iSelection >= -1) && (iSelection < m_Items.GetCount()) );

    if( m_iSelection == iSelection )
    {
        return;
    }

    m_iSelection = iSelection;
    Notify( ui_notification_type::ComboSelectionChanged, m_iSelection );
}

//==============================================================================

void ui_combo::ClearSelection( void )
{
    SetSelection( -1 );
}

//==============================================================================

void ui_combo::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)y;

    // Get combo actual screen bounds
    irect r( 0, 0, m_Position.GetWidth(), m_Position.GetHeight() );
    LocalToScreen( r );

    // Arrow hit zones are square and follow the control height.
    s32 const ArrowWidth = r.GetHeight();

    if( x < r.l + ArrowWidth )
    {
        // Left arrow - previous item
        OnPage( pWin, -1 );
    }
    else if( x >= r.r - ArrowWidth )
    {
        // Right arrow - next item
        OnPage( pWin, 1 );
    }
    else
    {
        OnAccept( pWin );
    }
}

//==============================================================================
