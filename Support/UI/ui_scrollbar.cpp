//==============================================================================
//
//  ui_scrollbar.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "ui_scrollbar.hpp"
#include "ui_control.hpp"
#include "ui_manager.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

static s32 const MOUSE_WHEEL_DELTA_PER_NOTCH = 120;
static s32 const SCROLLBAR_ARROW_SIZE         = 16;
static s32 const SCROLLBAR_MIN_THUMB_SIZE     = 16;
static f32 const SCROLLBAR_REPEAT_DELAY       = 0.35f;
static f32 const SCROLLBAR_REPEAT_INTERVAL    = 0.06f;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_scrollbar::ui_scrollbar( void )
{
    m_pManager              = NULL;
    m_iElementArrowDown     = -1;
    m_iElementArrowUp       = -1;
    m_iElementContainer     = -1;
    m_iElementThumb         = -1;
    m_ItemCount             = 0;
    m_PageSize              = 0;
    m_Position              = 0;
    m_MaxPosition           = 0;
    m_PointerX              = 0;
    m_PointerY              = 0;
    m_DragOffset            = 0;
    m_WheelRemainder        = 0;
    m_RepeatTimer           = 0.0f;
    m_PressedPart           = PART_NONE;
    m_HoveredPart           = PART_NONE;
}

//==============================================================================

xbool ui_scrollbar::Create( ui_manager* pManager )
{
    ASSERT( pManager );

    m_pManager          = pManager;
    m_iElementArrowDown = m_pManager->FindElement( "sb_arrowdown" );
    m_iElementArrowUp   = m_pManager->FindElement( "sb_arrowup" );
    m_iElementContainer = m_pManager->FindElement( "sb_container" );
    m_iElementThumb     = m_pManager->FindElement( "sb_thumb" );

    ASSERT( m_iElementArrowDown != -1 );
    ASSERT( m_iElementArrowUp   != -1 );
    ASSERT( m_iElementContainer != -1 );
    ASSERT( m_iElementThumb     != -1 );

    return    (m_iElementArrowDown != -1)
           && (m_iElementArrowUp   != -1)
           && (m_iElementContainer != -1)
           && (m_iElementThumb     != -1);
}

//==============================================================================

void ui_scrollbar::SetBounds( irect const& Bounds )
{
    m_Bounds = Bounds;
    UpdateGeometry();
}

//==============================================================================

void ui_scrollbar::SetRange( s32 ItemCount, s32 PageSize, s32 Position )
{
    m_ItemCount   = MAX( 0, ItemCount );
    m_PageSize    = MAX( 1, PageSize );
    m_MaxPosition = MAX( 0, m_ItemCount - m_PageSize );
    m_Position    = x_clamp( Position, 0, m_MaxPosition );
    UpdateGeometry();
}

//==============================================================================

void ui_scrollbar::Render( xbool IsEnabled ) const
{
    if( !m_pManager )
    {
        return;
    }

    irect ContainerBounds = m_Bounds;
    ContainerBounds.t = m_DecrementBounds.b;
    ContainerBounds.b = m_IncrementBounds.t;

    const s32 ContainerState = IsEnabled ? ui_control::CS_NORMAL
                                         : ui_control::CS_DISABLED;
    m_pManager->RenderElement( m_iElementContainer, ContainerBounds, ContainerState );
    m_pManager->RenderElement( m_iElementArrowUp,
                               m_DecrementBounds,
                               GetPartVisualState( PART_DECREMENT, IsEnabled ) );
    m_pManager->RenderElement( m_iElementArrowDown,
                               m_IncrementBounds,
                               GetPartVisualState( PART_INCREMENT, IsEnabled ) );
    m_pManager->RenderRect( m_TrackBounds, xcolor( 20, 80, 13, 128 ), FALSE );

    part TrackPart = PART_NONE;
    if( (m_PressedPart == PART_PAGE_DECREMENT) ||
        (m_PressedPart == PART_PAGE_INCREMENT) )
    {
        TrackPart = m_PressedPart;
    }
    else if( (m_HoveredPart == PART_PAGE_DECREMENT) ||
             (m_HoveredPart == PART_PAGE_INCREMENT) )
    {
        TrackPart = m_HoveredPart;
    }

    if( IsEnabled && (TrackPart != PART_NONE) )
    {
        irect HoverBounds = m_TrackBounds;
        if( TrackPart == PART_PAGE_DECREMENT )
        {
            HoverBounds.b = m_ThumbBounds.t;
        }
        else
        {
            HoverBounds.t = m_ThumbBounds.b;
        }

        const u8 Alpha = (TrackPart == m_PressedPart) ? 112 : 48;
        m_pManager->RenderRect( HoverBounds, xcolor( 79, 214, 60, Alpha ), FALSE );
    }

    m_pManager->RenderElement( m_iElementThumb,
                               m_ThumbBounds,
                               GetPartVisualState( PART_THUMB, IsEnabled ) );
}

//==============================================================================

xbool ui_scrollbar::OnPointerMove( s32 X, s32 Y )
{
    m_PointerX  = X;
    m_PointerY  = Y;
    m_HoveredPart = GetPartAt( X, Y );

    if( m_PressedPart == PART_THUMB )
    {
        return DragThumb( Y );
    }

    return FALSE;
}

//==============================================================================

void ui_scrollbar::OnPointerLeave( void )
{
    m_HoveredPart = PART_NONE;
}

//==============================================================================

xbool ui_scrollbar::OnPointerDown( s32 X, s32 Y )
{
    OnPointerMove( X, Y );
    if( m_HoveredPart == PART_NONE )
    {
        return FALSE;
    }

    if( m_MaxPosition <= 0 )
    {
        return TRUE;
    }

    m_PressedPart = m_HoveredPart;
    if( m_PressedPart == PART_THUMB )
    {
        m_DragOffset = Y - m_ThumbBounds.t;
    }

    m_RepeatTimer = SCROLLBAR_REPEAT_DELAY;
    if( m_PressedPart == PART_THUMB )
    {
        return TRUE;
    }

    ApplyPressedPart();
    return m_PressedPart != PART_NONE;
}

//==============================================================================

xbool ui_scrollbar::OnPointerUp( void )
{
    xbool const WasInteracting = IsInteracting();
    CancelInteraction();
    return WasInteracting;
}

//==============================================================================

xbool ui_scrollbar::OnWheel( s32 Delta, s32 LinesPerNotch )
{
    if( m_MaxPosition <= 0 )
    {
        return FALSE;
    }

    m_WheelRemainder += Delta;
    s32 const Notches = m_WheelRemainder / MOUSE_WHEEL_DELTA_PER_NOTCH;
    if( Notches == 0 )
    {
        return TRUE;
    }

    m_WheelRemainder -= Notches * MOUSE_WHEEL_DELTA_PER_NOTCH;
    return SetPosition( m_Position - (Notches * MAX( 1, LinesPerNotch )) );
}

//==============================================================================

xbool ui_scrollbar::OnUpdate( f32 DeltaTime )
{
    if( (m_PressedPart == PART_NONE) || (m_PressedPart == PART_THUMB) )
    {
        return FALSE;
    }

    m_RepeatTimer -= DeltaTime;
    xbool Changed = FALSE;
    while( m_RepeatTimer <= 0.0f )
    {
        Changed |= ApplyPressedPart();
        m_RepeatTimer += SCROLLBAR_REPEAT_INTERVAL;
    }

    return Changed;
}

//==============================================================================

void ui_scrollbar::CancelInteraction( void )
{
    m_PressedPart = PART_NONE;
    m_RepeatTimer = 0.0f;
    m_DragOffset  = 0;
}

//==============================================================================

xbool ui_scrollbar::Contains( s32 X, s32 Y ) const
{
    return m_Bounds.PointInRect( X, Y );
}

//==============================================================================

xbool ui_scrollbar::IsHovered( void ) const
{
    return m_HoveredPart != PART_NONE;
}

//==============================================================================

xbool ui_scrollbar::IsInteracting( void ) const
{
    return m_PressedPart != PART_NONE;
}

//==============================================================================

s32 ui_scrollbar::GetPosition( void ) const
{
    return m_Position;
}

//==============================================================================

void ui_scrollbar::UpdateGeometry( void )
{
    s32 const ArrowHeight = MIN( SCROLLBAR_ARROW_SIZE,
                                 MAX( 0, m_Bounds.GetHeight() / 2 ) );

    m_DecrementBounds = m_Bounds;
    m_DecrementBounds.b = m_Bounds.t + ArrowHeight;

    m_IncrementBounds = m_Bounds;
    m_IncrementBounds.t = m_Bounds.b - ArrowHeight;

    m_TrackBounds = m_Bounds;
    m_TrackBounds.t = m_DecrementBounds.b;
    m_TrackBounds.b = m_IncrementBounds.t;
    if( m_TrackBounds.GetWidth() > 4 )
    {
        m_TrackBounds.l += 2;
        m_TrackBounds.r -= 2;
    }
    if( m_TrackBounds.GetHeight() > 4 )
    {
        m_TrackBounds.t += 2;
        m_TrackBounds.b -= 2;
    }

    m_ThumbBounds = m_TrackBounds;
    if( (m_ItemCount <= 0) || (m_ItemCount <= m_PageSize) )
    {
        return;
    }

    s32 const TrackHeight = MAX( 0, m_TrackBounds.GetHeight() );
    s32 const ThumbHeight = x_clamp( (TrackHeight * m_PageSize) / m_ItemCount,
                                    MIN( SCROLLBAR_MIN_THUMB_SIZE, TrackHeight ),
                                    TrackHeight );
    s32 const ThumbTravel = TrackHeight - ThumbHeight;
    s32 const ThumbOffset = (m_MaxPosition > 0)
                          ? ((ThumbTravel * m_Position) / m_MaxPosition)
                          : 0;
    m_ThumbBounds.t += ThumbOffset;
    m_ThumbBounds.b = m_ThumbBounds.t + ThumbHeight;
}

//==============================================================================

s32 ui_scrollbar::GetPartVisualState( part Part, xbool IsEnabled ) const
{
    if( !IsEnabled )
    {
        return ui_control::CS_DISABLED;
    }

    if( m_PressedPart == Part )
    {
        return (m_HoveredPart == Part) ? ui_control::CS_HIGHLIGHTED_ACTIVE
                                       : ui_control::CS_ACTIVE;
    }

    return (m_HoveredPart == Part) ? ui_control::CS_HIGHLIGHTED
                                   : ui_control::CS_NORMAL;
}

//==============================================================================

ui_scrollbar::part ui_scrollbar::GetPartAt( s32 X, s32 Y ) const
{
    if( !Contains( X, Y ) )
    {
        return PART_NONE;
    }

    if( m_DecrementBounds.PointInRect( X, Y ) )
    {
        return PART_DECREMENT;
    }

    if( m_IncrementBounds.PointInRect( X, Y ) )
    {
        return PART_INCREMENT;
    }

    if( m_ThumbBounds.PointInRect( X, Y ) )
    {
        return PART_THUMB;
    }

    if( m_TrackBounds.PointInRect( X, Y ) )
    {
        return (Y < m_ThumbBounds.t) ? PART_PAGE_DECREMENT
                                     : PART_PAGE_INCREMENT;
    }

    return PART_NONE;
}

//==============================================================================

xbool ui_scrollbar::SetPosition( s32 Position )
{
    Position = x_clamp( Position, 0, m_MaxPosition );
    if( Position == m_Position )
    {
        return FALSE;
    }

    m_Position = Position;
    UpdateGeometry();
    return TRUE;
}

//==============================================================================

xbool ui_scrollbar::ApplyPressedPart( void )
{
    switch( m_PressedPart )
    {
        case PART_DECREMENT:
        {
            if( m_DecrementBounds.PointInRect( m_PointerX, m_PointerY ) )
            {
                return SetPosition( m_Position - 1 );
            }
        }
        break;

        case PART_INCREMENT:
        {
            if( m_IncrementBounds.PointInRect( m_PointerX, m_PointerY ) )
            {
                return SetPosition( m_Position + 1 );
            }
        }
        break;

        case PART_PAGE_DECREMENT:
        {
            if( m_PointerY < m_ThumbBounds.t )
            {
                return SetPosition( m_Position - MAX( 1, m_PageSize - 1 ) );
            }
        }
        break;

        case PART_PAGE_INCREMENT:
        {
            if( m_PointerY >= m_ThumbBounds.b )
            {
                return SetPosition( m_Position + MAX( 1, m_PageSize - 1 ) );
            }
        }
        break;

        default:
        {
        }
        break;
    }

    return FALSE;
}

//==============================================================================

xbool ui_scrollbar::DragThumb( s32 Y )
{
    s32 const ThumbTravel = m_TrackBounds.GetHeight() - m_ThumbBounds.GetHeight();
    if( (m_MaxPosition <= 0) || (ThumbTravel <= 0) )
    {
        return SetPosition( 0 );
    }

    s32 const ThumbTop = x_clamp( Y - m_DragOffset,
                                  m_TrackBounds.t,
                                  m_TrackBounds.b - m_ThumbBounds.GetHeight() );
    s32 const ThumbOffset = ThumbTop - m_TrackBounds.t;
    return SetPosition( ((ThumbOffset * m_MaxPosition) + (ThumbTravel / 2)) / ThumbTravel );
}

//==============================================================================
