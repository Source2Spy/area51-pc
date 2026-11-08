//=========================================================================
//
//  ui_listbox.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"

#include "ui_listbox.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"
#include "StateMgr/StateMgr.hpp"

//=========================================================================
//  Defines
//=========================================================================

#define SPACE_TOP       4
#define SPACE_BOTTOM    4
#define LINE_HEIGHT     16
#define HEADER_HEIGHT   22

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

//=========================================================================
//  Factory function
//=========================================================================

ui_win* ui_listbox_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_listbox* pcombo = new ui_listbox;
    pcombo->Create( UserID, pManager, Position, pParent, Flags );

    return (ui_win*)pcombo;
}

//=========================================================================
//  ui_listbox
//=========================================================================

ui_listbox::ui_listbox( void )
{
}

//=========================================================================

ui_listbox::~ui_listbox( void )
{
}

//=========================================================================

xbool ui_listbox::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Set default text flags
    m_LabelFlags = ui_font::h_center|ui_font::v_center|ui_font::clip_ellipsis|ui_font::clip_l_justify;
    m_BackgroundColor = xcolor(0,0,0,0);
    m_HeaderBarColor  = xcolor(0,0,0,0);
    m_HeaderColor     = XCOLOR_WHITE;

    // Initialize data
    m_iElementFrame = m_pManager->FindElement( "sb_frame" );
    ASSERT( m_iElementFrame != -1 );
    Success &= m_ScrollBar.Create( m_pManager );

    m_LineHeight            = LINE_HEIGHT;
    m_ExitOnSelect          = TRUE;
    m_ExitOnBack            = FALSE;
    m_AcceptOnRepeatClick   = FALSE;
    m_LastPointerSelection  = -1;
    m_iSelection            = -1;
    m_iSelectionBackup      = -1;
    m_iFirstVisibleItem     = 0;
    m_ShowBorders           = TRUE;
    m_ShowFrame             = TRUE;
    m_ShowHeaderBar         = FALSE;
    m_AllowParentNavigate   = FALSE;
    m_DisableCursor         = FALSE;
    UpdateVisibleItemCount();
    m_Font                  = m_pManager->FindFont("small");
    m_HoveredItem           = -1;

    return Success;
}

//=========================================================================

void ui_listbox::Render( s32 ox, s32 oy )
{
    s32     i;

    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        xcolor  TextColor2  = XCOLOR_BLACK;
        // Calculate rectangle
        irect r = GetListRect();
        r.Translate( ox, oy );

        if( m_Flags & WF_DISABLED )
        {
            TextColor2  = xcolor(0,0,0,0);
        }

        m_pManager->PushClipWindow( r );

        irect   rb = r;
        
        if (m_ShowFrame)
            rb.Deflate( 1, 1 );

        // render header bar
        if( m_ShowHeaderBar )
        {
            irect hb = rb;
            hb.SetHeight( HEADER_HEIGHT );          

            m_pManager->RenderRect( hb, m_HeaderBarColor, FALSE );
            RenderHeader( hb );


            rb.t += 22;
        }

        // Render background color
        m_pManager->RenderRect( rb, m_BackgroundColor, FALSE );

        // Render Text & Selection Marker
        irect rl = rb;
        rl.SetHeight( m_LineHeight );
        rl.Deflate( 2, 0 );
        rl.r -= 2;
        rl.Translate( 0, SPACE_TOP );

        if( m_Items.GetCount() )
        {
            for( i=0 ; i<m_nVisibleItems ; i++ )
            {
                s32 iItem = m_iFirstVisibleItem + i;

                if( (iItem >= 0) && (iItem < m_Items.GetCount()) )
                {
                    // Render Selection Rectangle
                    if( (iItem == m_iSelection)  &&
                        (m_ShowBorders)          &&
                        (m_DisableCursor == FALSE) )
                    {
                        RenderSelection( rl );
                    }
                    // Pointer hover is independent from the persistent selection.
                    if( (iItem == m_HoveredItem) &&
                        (iItem != m_iSelection) && m_Items[iItem].Enabled )
                    {
                        RenderHighlightRect( rl, HS_HOVERED );
                    }

                    // Render Text
                    xcolor c1 = m_Items[iItem].Color;
                    xcolor c2 = TextColor2;
                    if( !m_Items[iItem].Enabled )
                    {
                        c1 = XCOLOR_GREY;
                        c2 = xcolor(0,0,0,0);
                    }
                    else if( (iItem == m_iSelection)     &&
                             (m_DisableCursor == FALSE) )
                    {
                        if( IsActive() )
                        {
                            c1 = xcolor(0,0,0,255);
                            c2 = xcolor(0,0,0,0);
                        }
                        else
                        {
                            c1 = xcolor(126,220,60,255);
                        }
                    }
                    irect rl2 = rl;

                    RenderItem( rl2, m_Items[iItem], c1, c2 );
                }
                rl.Translate( 0, m_LineHeight );
            }
        }

        m_pManager->PopClipWindow();

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

//=========================================================================

void ui_listbox::RenderHeader( irect r )
{
    irect Shadow = r;
    Shadow.Translate( 1, 1 );

    m_pManager->RenderText( m_Font, Shadow, ui_font::h_center | ui_font::v_center, XCOLOR_BLACK, m_Label );
    m_pManager->RenderText( m_Font, r, ui_font::h_center | ui_font::v_center, m_HeaderColor, m_Label );
}

//=========================================================================

void ui_listbox::RenderSelection( irect r )
{
    RenderHighlightRect( r, IsActive() ? HS_ACTIVE : HS_SELECTED );
}

//=========================================================================

void ui_listbox::RenderItem( irect r, const item& Item, const xcolor& c1, const xcolor& c2 )
{
    r.Deflate( 4, 0 );

    irect Shadow = r;
    Shadow.Translate( 1, 1 );

    m_pManager->RenderText( m_Font, Shadow, m_LabelFlags, c2, Item.Label );
    m_pManager->RenderText( m_Font, r, m_LabelFlags, c1, Item.Label );
}

//=========================================================================

void ui_listbox::SetPosition( const irect& Position )
{
    m_Position = Position;
    UpdateVisibleItemCount();
}

//=========================================================================

void ui_listbox::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    s32 Direction = 0;

    // check for cursor enabled
    if ( m_DisableCursor )
    {
        g_AudioMgr.Play( "InvalidEntry" );
        return;
    }

    // Determine movement required
    switch( Code )
    {
        case ui_navigation::Up:
            Direction = -1;
            break;

        case ui_navigation::Down:
            Direction = 1;
            break;

        default:
            break;
    }

    if( IsActive() && (Direction != 0) )
    {
        s32 const NextItem = FindEnabledItem( m_iSelection + Direction, Direction );
        if( NextItem != -1 )
        {
            SelectUserItem( NextItem );
            g_AudioMgr.Play( "Cusor_Norm" );
            return;
        }

        if( Presses > 0 )
        {
            if( m_AllowParentNavigate && m_pParent )
            {
                m_pParent->OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
            }
            else
            {
                g_AudioMgr.Play( "InvalidEntry" );
            }
        }
        return;
    }

    ui_win::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
}

void ui_listbox::OnPage( ui_win* pWin, s32 Direction )
{
    (void)pWin;

    if( (Direction == 0) || (m_iSelection == -1) )
    {
        return;
    }

    s32 const LastItem = m_Items.GetCount() - 1;
    s32 const PageStep = MAX( 1, m_nVisibleItems - 1 );
    s32 const DesiredItem = x_clamp( m_iSelection + PageStep * Direction, 0, LastItem );
    s32 NextItem = FindEnabledItem( DesiredItem, Direction );
    if( NextItem == -1 )
    {
        NextItem = FindEnabledItem( DesiredItem, -Direction );
    }

    if( SelectUserItem( NextItem ) )
    {
        g_AudioMgr.Play( "Cusor_Norm" );
    }
}

void ui_listbox::OnJump( ui_win* pWin, s32 Direction )
{
    (void)pWin;

    if( (Direction == 0) || (m_Items.GetCount() == 0) )
    {
        return;
    }

    s32 const Start = (Direction < 0) ? 0 : m_Items.GetCount() - 1;
    if( SelectUserItem( FindEnabledItem( Start, Direction ) ) )
    {
        g_AudioMgr.Play( "Cusor_Norm" );
    }
}

//=========================================================================

void ui_listbox::OnAccept( ui_win* pWin )
{
    (void)pWin;

    // Check if Exit on Select is disabled
    if( !m_ExitOnSelect && (IsActive()) )
    {
        if( m_pParent )
            Notify( ui_notification_type::ListAccepted, static_cast<s32>( m_iSelection  ) );
    }
    else
    {
        if( (IsActive()) || (GetNumEnabledItems() > 0) )
        {
            // Toggle Selected
            SetActive( !IsActive() );

            if( IsActive() )
            {
                m_iSelectionBackup = m_iSelection;
            }
            else
            {
                if( m_pParent )
                    Notify( ui_notification_type::ListAccepted, static_cast<s32>( m_iSelection  ) );
            }
        }
    }

    if ( m_pParent )
        m_pParent->OnAccept( pWin );
}

//=========================================================================

void ui_listbox::OnCancel( ui_win* pWin )
{
    (void)pWin;

    if( ( IsActive() ) && ( !m_ExitOnBack ) )
    {
        // Clear selected
        SetActive( FALSE );

        // Clamp the saved selection when items disappeared while the list was active.
        if( m_iSelectionBackup >= GetItemCount() )
        {
            m_iSelectionBackup = GetItemCount()-1;
        }
        SetSelection( m_iSelectionBackup );

        if( m_pParent )
            Notify( ui_notification_type::ListCancelled, static_cast<s32>( m_iSelection  ) );
    }
    else
    {
        if( m_pParent )
            m_pParent->OnCancel( pWin );
    }
}

//=========================================================================

void ui_listbox::SetLineHeight( s32 Height )
{
    ASSERT( Height > 0 );
    m_LineHeight = MAX( 1, Height );
    UpdateVisibleItemCount();
}

//=========================================================================
void ui_listbox::SetExitOnSelect( xbool State )
{
    m_ExitOnSelect = State;
}

//=========================================================================

s32 ui_listbox::AddItem( const xwstring& Label, uaddr Data, uaddr Data2, xbool State, u32 Flags )
{
    item& Item = m_Items.Append();
    Item.Enabled = State;
    Item.Label   = Label;
    Item.Data[0] = Data;
    Item.Data[1] = Data2;
    Item.Color   = xcolor(255,252,204,255);
    Item.Flags   = Flags;
    return m_Items.GetCount()-1;
}

//=========================================================================

s32 ui_listbox::AddItem( const xwchar* Label, uaddr Data, uaddr Data2, xbool State, u32 Flags )
{
    item& Item = m_Items.Append();
    Item.Enabled = State;
    Item.Label   = Label;
    Item.Data[0] = Data;
    Item.Data[1] = Data2;
    Item.Color   = xcolor(255,252,204,255);
    Item.Flags   = Flags;
    return m_Items.GetCount()-1;
}

//=========================================================================

void ui_listbox::DeleteAllItems( void )
{
    m_LastPointerSelection = -1;

    s32 const OldSelection = m_iSelection;
    m_iSelection        = -1;
    m_iFirstVisibleItem = 0;

    m_Items.Delete( 0, m_Items.GetCount() );

    if( OldSelection != m_iSelection )
        Notify( ui_notification_type::ListSelectionChanged, m_iSelection );
}

//=========================================================================

void ui_listbox::DeleteItem( s32 iItem )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );

    m_LastPointerSelection = -1;

    s32 const OldSelection = m_iSelection;
    xbool const DeletedSelection = (iItem == OldSelection);

    m_Items.Delete( iItem );

    if( m_Items.GetCount() == 0 )
        m_iSelection = -1;
    else if( iItem < OldSelection )
        m_iSelection = OldSelection - 1;
    else if( DeletedSelection )
    {
        s32 const Candidate = MIN( iItem, m_Items.GetCount() - 1 );
        m_iSelection = FindEnabledItem( Candidate, 1 );
        if( m_iSelection == -1 )
            m_iSelection = FindEnabledItem( Candidate, -1 );
    }

    EnsureVisible( m_iSelection );

    if( (m_iSelection != OldSelection) || DeletedSelection )
        Notify( ui_notification_type::ListSelectionChanged, m_iSelection );
}

//=========================================================================

void ui_listbox::DeleteSelectedItem( void )
{
    // ensure that we have something to delete!
    if ( m_iSelection < 0 )
        return;

    DeleteItem( m_iSelection );
}


//=========================================================================

u32 ui_listbox::GetItemFlags( s32 iItem ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    return m_Items[iItem].Flags;
}

//=========================================================================

void ui_listbox::EnableItem( s32 iItem, xbool State )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );

    s32 OldSelection = m_iSelection;
    m_Items[iItem].Enabled = State;

    // If the selected item was just disabled then search for new item to select
    if( (iItem == m_iSelection) && !State )
    {
        s32 iFound = FindEnabledItem( iItem - 1, -1 );
        if( iFound == -1 )
            iFound = FindEnabledItem( iItem + 1, 1 );

        m_iSelection = iFound;
        EnsureVisible( m_iSelection );

        if( m_iSelection != OldSelection )
            Notify( ui_notification_type::ListSelectionChanged, m_iSelection );
    }
}

//=========================================================================
    
void ui_listbox::EnableHeaderBar( void )
{ 
    m_ShowHeaderBar = TRUE;
    UpdateVisibleItemCount();
}

//=========================================================================

void ui_listbox::DisableHeaderBar( void )
{ 
    m_ShowHeaderBar = FALSE;
    UpdateVisibleItemCount();
}

//=========================================================================

s32 ui_listbox::GetItemCount( void ) const
{
    return m_Items.GetCount();
}

//=========================================================================

const xwstring& ui_listbox::GetItemLabel( s32 iItem ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );

    return m_Items[iItem].Label;
}

//=========================================================================

void ui_listbox::SetItemLabel( s32 iItem, const xwstring& Label )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );

    m_Items[iItem].Label = Label;
}

//=========================================================================

uaddr ui_listbox::GetItemData( s32 iItem, s32 Index ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );
    ASSERT( (Index >= 0) && (Index < LISTBOX_DATA_FIELDS) );

    return m_Items[iItem].Data[Index];
}

//=========================================================================

const xwstring& ui_listbox::GetSelectedItemLabel( void ) const
{
    ASSERT( (m_iSelection >= 0) && (m_iSelection < m_Items.GetCount()) );

    return m_Items[m_iSelection].Label;
}

//=========================================================================

uaddr ui_listbox::GetSelectedItemData( s32 Index ) const
{
    ASSERT( (m_iSelection >= 0) && (m_iSelection < m_Items.GetCount()) );
    ASSERT( (Index >= 0) && (Index < LISTBOX_DATA_FIELDS) );

    return m_Items[m_iSelection].Data[Index];
}

//=========================================================================

void ui_listbox::SetItemColor( s32 iItem, const xcolor& Color )
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );

    m_Items[iItem].Color = Color;
}

//=========================================================================

xcolor ui_listbox::GetItemColor( s32 iItem ) const
{
    ASSERT( (iItem >= 0) && (iItem < m_Items.GetCount()) );

    return m_Items[iItem].Color;
}

//=========================================================================

s32 ui_listbox::FindItemByLabel( const xwstring& Label ) const
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

//=========================================================================

s32 ui_listbox::FindItemByData( uaddr Data, s32 Index ) const
{
    ASSERT( (Index >= 0) && (Index < LISTBOX_DATA_FIELDS) );

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

//=========================================================================

s32 ui_listbox::GetSelection( void ) const
{
    return m_iSelection;
}

//=========================================================================

void ui_listbox::SetSelection( s32 iSelection )
{
    ASSERT( (iSelection >= -1) && (iSelection < m_Items.GetCount()) );

    m_LastPointerSelection = -1;

    if( iSelection < -1 )
        iSelection = -1;
    if( iSelection > (m_Items.GetCount()-1) )
        iSelection = m_Items.GetCount()-1;

    if( (iSelection == -1) || m_Items[iSelection].Enabled )
    {
        if( iSelection == m_iSelection )
            return;

        m_iSelection = iSelection;
        EnsureVisible( m_iSelection );
        Notify( ui_notification_type::ListSelectionChanged, m_iSelection );
    }
}

//=========================================================================

void ui_listbox::ClearSelection( void )
{
    SetSelection( -1 );
}

//=========================================================================

void ui_listbox::EnsureVisible( s32 iItem )
{
    ASSERT( (iItem >= -1) && (iItem < m_Items.GetCount()) );

    if( iItem != -1 )
    {
        if( iItem < m_iFirstVisibleItem )
        {
            SetFirstVisibleItem( iItem );
        }
        if( iItem >= (m_iFirstVisibleItem+m_nVisibleItems) )
        {
            SetFirstVisibleItem( iItem - (m_nVisibleItems-1) );
        }
    }
}

//=========================================================================

s32 ui_listbox::GetNumEnabledItems( void ) const
{
    s32 i;
    s32 Count = 0;
    for( i=0 ; i<m_Items.GetCount() ; i++ )
    {
        if( m_Items[i].Enabled )
            Count++;
    }
    return Count;
}

//=========================================================================

s32 ui_listbox::GetCursorOffset( void ) const
{
    s32 Offset;

    Offset = m_iSelection - m_iFirstVisibleItem;
    if( Offset >= m_nVisibleItems )
        Offset = m_nVisibleItems-1;
    if( Offset < 0 )
        Offset = 0;

    return Offset;
}

//=========================================================================

void ui_listbox::SetSelectionWithOffset( s32 iSelection, s32 Offset )
{
    ASSERT( (iSelection >= -1) && (iSelection < m_Items.GetCount()) );

    m_LastPointerSelection = -1;

    if( (iSelection == -1) || m_Items[iSelection].Enabled )
    {
        s32 const OldSelection = m_iSelection;
        m_iSelection = iSelection;

        if( m_iSelection != -1 )
        {
            Offset = x_clamp( Offset, 0, MAX( 0, m_nVisibleItems - 1 ) );
            SetFirstVisibleItem( m_iSelection - Offset );
        }

        if( m_iSelection != OldSelection )
            Notify( ui_notification_type::ListSelectionChanged, m_iSelection );
    }
}

//=========================================================================

void ui_listbox::SetBackgroundColor( xcolor Color )
{
    m_BackgroundColor = Color;
}

//=========================================================================

xcolor ui_listbox::GetBackgroundColor( void ) const
{
    return m_BackgroundColor;
}

//=========================================================================

class listbox_sort_compare : public x_compare_functor<const ui_listbox::item&>
{
public:
    s32 operator()( const ui_listbox::item& A, const ui_listbox::item& B )
    {
        return x_wstrcmp( A.Label, B.Label );
    }
};


//=========================================================================
void ui_listbox::AlphaSortList( void )
{
    if( m_Items.GetCount() < 2 )
    {
        return;
    }

    s32 const OldSelection = m_iSelection;
    item SelectedItem;
    xbool const HasSelection = (m_iSelection >= 0) && (m_iSelection < m_Items.GetCount());
    if( HasSelection )
    {
        SelectedItem = m_Items[m_iSelection];
    }

    x_qsort( &m_Items[0], m_Items.GetCount(), listbox_sort_compare() );

    if( HasSelection )
    {
        m_iSelection = -1;
        for( s32 iItem = 0; iItem < m_Items.GetCount(); iItem++ )
        {
            item const& Item = m_Items[iItem];
            if( (Item.Label == SelectedItem.Label) &&
                (Item.Data[0] == SelectedItem.Data[0]) &&
                (Item.Data[1] == SelectedItem.Data[1]) )
            {
                m_iSelection = iItem;
                break;
            }
        }

        EnsureVisible( m_iSelection );
        if( m_iSelection != OldSelection )
        {
            Notify( ui_notification_type::ListSelectionChanged, m_iSelection );
        }
    }
}

//=========================================================================

s32 ui_listbox::GetMaxFirstVisibleItem( void ) const
{
    return MAX( 0, m_Items.GetCount() - m_nVisibleItems );
}

//=========================================================================

xbool ui_listbox::SetFirstVisibleItem( s32 FirstVisibleItem )
{
    FirstVisibleItem = x_clamp( FirstVisibleItem, 0, GetMaxFirstVisibleItem() );
    if( FirstVisibleItem == m_iFirstVisibleItem )
    {
        return FALSE;
    }

    m_iFirstVisibleItem = FirstVisibleItem;
    m_HoveredItem = -1;
    return TRUE;
}

//=========================================================================

xbool ui_listbox::ScrollItems( s32 ItemDelta )
{
    return SetFirstVisibleItem( m_iFirstVisibleItem + ItemDelta );
}

//=========================================================================

s32 ui_listbox::GetItemAt( s32 x, s32 y ) const
{
    ScreenToLocal( x, y );
    irect Bounds = GetListRect();
    Bounds.Translate( -m_Position.l, -m_Position.t );
    if( (x < Bounds.l) || (x >= Bounds.r) )
        return -1;

    s32 const ContentTop = Bounds.t + SPACE_TOP + (m_ShowHeaderBar ? HEADER_HEIGHT : 0);
    s32 const RelativeY = y - ContentTop;
    if( (RelativeY < 0) || (RelativeY >= m_nVisibleItems * m_LineHeight) )
    {
        return -1;
    }

    s32 const iItem = m_iFirstVisibleItem + (RelativeY / m_LineHeight);
    return ((iItem >= 0) && (iItem < m_Items.GetCount())) ? iItem : -1;
}

//=========================================================================

s32 ui_listbox::FindEnabledItem( s32 Start, s32 Direction ) const
{
    if( Direction == 0 )
    {
        return -1;
    }

    for( s32 iItem = Start;
         (iItem >= 0) && (iItem < m_Items.GetCount());
         iItem += Direction )
    {
        if( m_Items[iItem].Enabled )
        {
            return iItem;
        }
    }

    return -1;
}

//=========================================================================

xbool ui_listbox::SelectUserItem( s32 iItem )
{
    m_LastPointerSelection = -1;

    if( (iItem < 0) ||
        (iItem >= m_Items.GetCount()) ||
        !m_Items[iItem].Enabled ||
        (iItem == m_iSelection) )
    {
        return FALSE;
    }

    m_iSelection = iItem;
    EnsureVisible( m_iSelection );
    Notify( ui_notification_type::ListSelectionChanged, m_iSelection );
    return TRUE;
}

//=========================================================================

irect ui_listbox::GetListRect( void ) const
{
    irect Bounds = m_Position;
    Bounds.r -= 14;
    return Bounds;
}

//=========================================================================

void ui_listbox::UpdateScrollBar( void )
{
    irect Bounds( GetWidth() - 14,
                  m_ShowHeaderBar ? HEADER_HEIGHT : 0,
                  GetWidth(),
                  GetHeight() );
    LocalToScreen( Bounds );
    m_ScrollBar.SetBounds( Bounds );
    m_ScrollBar.SetRange( m_Items.GetCount(), m_nVisibleItems, m_iFirstVisibleItem );
}

//=========================================================================

void ui_listbox::UpdateVisibleItemCount( void )
{
    s32 const HeaderHeight = m_ShowHeaderBar ? HEADER_HEIGHT : 0;
    s32 const ContentHeight = GetListRect().GetHeight()
                            - SPACE_TOP
                            - SPACE_BOTTOM
                            - HeaderHeight;
    m_nVisibleItems = MAX( 1, ContentHeight / MAX( 1, m_LineHeight ) );
    SetFirstVisibleItem( m_iFirstVisibleItem );
}

//=========================================================================

void ui_listbox::OnPointerMove( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    UpdateScrollBar();
    if( m_ShowBorders && m_ScrollBar.OnPointerMove( x, y ) )
    {
        SetFirstVisibleItem( m_ScrollBar.GetPosition() );
    }
    else if( !m_ShowBorders )
    {
        m_ScrollBar.OnPointerLeave();
    }

    if( m_ShowBorders &&
        (m_ScrollBar.IsHovered() || m_ScrollBar.IsInteracting()) )
    {
        m_HoveredItem = -1;
        return;
    }

    m_HoveredItem = GetItemAt( x, y );
}

//=========================================================================

void ui_listbox::OnPointerLeave( ui_win* pWin )
{
    (void)pWin;
    m_ScrollBar.OnPointerLeave();
    m_HoveredItem = -1;
}

//=========================================================================

void ui_listbox::OnPointerWheel( ui_win* pWin, s32 Delta )
{
    UpdateScrollBar();
    if( m_ScrollBar.OnWheel( Delta ) )
    {
        SetFirstVisibleItem( m_ScrollBar.GetPosition() );
        return;
    }

    ui_win::OnPointerWheel( pWin, Delta );
}

//=========================================================================

void ui_listbox::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    UpdateScrollBar();
    if( m_ShowBorders && m_ScrollBar.OnPointerDown( x, y ) )
    {
        m_LastPointerSelection = -1;
        m_HoveredItem = -1;
        SetFirstVisibleItem( m_ScrollBar.GetPosition() );
        if( m_ScrollBar.IsInteracting() )
        {
            m_pManager->SetCapture( m_UserID, this );
        }
        return;
    }

    s32 const iItem = GetItemAt( x, y );
    if( iItem == -1 )
    {
        m_LastPointerSelection = -1;
        return;
    }

    if( !m_Items[iItem].Enabled )
    {
        m_LastPointerSelection = -1;
        g_AudioMgr.Play( "InvalidEntry" );
        return;
    }

    xbool const Accept = !m_AcceptOnRepeatClick || (m_LastPointerSelection == iItem);
    SelectUserItem( iItem );

    if( Accept )
    {
        m_LastPointerSelection = -1;
        OnAccept( pWin );
    }
    else
    {
        m_LastPointerSelection = iItem;
    }
}

//=========================================================================

void ui_listbox::OnPointerUp( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    (void)x;
    (void)y;

    xbool const HadScrollCapture = m_ScrollBar.OnPointerUp();
    if( HadScrollCapture )
    {
        m_pManager->ReleaseCapture( m_UserID );
    }
}

//=========================================================================

void ui_listbox::OnUpdate( ui_win* pWin, f32 DeltaTime )
{
    (void)pWin;

    UpdateScrollBar();
    if( m_ScrollBar.OnUpdate( DeltaTime ) )
    {
        SetFirstVisibleItem( m_ScrollBar.GetPosition() );
    }
}

//=========================================================================

void ui_listbox::OnFocusGained( ui_win* pWin )
{
    SetActive( TRUE );
    ui_win::OnFocusGained( pWin );
}

//=========================================================================

void ui_listbox::OnFocusLost ( ui_win* pWin )
{
    xbool const HadScrollCapture = m_ScrollBar.IsInteracting();
    m_LastPointerSelection = -1;
    m_ScrollBar.CancelInteraction();
    SetActive( FALSE );
    if( HadScrollCapture )
    {
        m_pManager->ReleaseCapture( m_UserID );
    }

    ui_win::OnFocusLost( pWin );
}

//=========================================================================
