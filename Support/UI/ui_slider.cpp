//==============================================================================
//
//  ui_slider.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "../AudioMgr/AudioMgr.hpp"

#include "ui_slider.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"

//==============================================================================
//  REGISTRATION
//==============================================================================

ui_win* ui_slider_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_slider* pButton = new ui_slider;
    pButton->Create( UserID, pManager, Position, pParent, Flags );

    return pButton;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_slider::ui_slider( void )
{
}

//==============================================================================

ui_slider::~ui_slider( void )
{
}

//==============================================================================

xbool ui_slider::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Initialize Data
    m_iElementBar   = m_pManager->FindElement( "slider_bar"   );
    m_iElementThumb = m_pManager->FindElement( "slider_thumb" );
    ASSERT( m_iElementBar   != -1 );
    ASSERT( m_iElementThumb != -1 );
    m_Min   = 0;
    m_Max   = 100;
    m_Value = 0;
    m_ValueParametric = 0.0f;
    m_IsParametric = FALSE;
    m_Step  = 5;
    m_StepScaler    = 1;
    m_StepScalerMax = 1;
    m_RepeatCount   = 0;
    m_UseSound = TRUE;

    m_IsDragging = FALSE;

    return Success;
}

//==============================================================================

void ui_slider::Render( s32 ox, s32 oy )
{
    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        s32 const State = GetVisualState( IsActive() );

        // Calculate rectangle
        irect    r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        // Determine Bar & Thumb positions and render
        irect r2 = r;

        if( m_Max > m_Min )
        {
            r.l += (r.r-r.l-5) * (m_Value-m_Min) / (m_Max-m_Min);
        }
        r.r = r.l + 6;
        m_pManager->RenderElement( m_iElementBar,   r2, 0     );
        m_pManager->RenderElement( m_iElementThumb, r , State );

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}

//==============================================================================

void ui_slider::OnNavigate( ui_win* pWin, ui_navigation Code, s32 Presses, s32 Repeats, xbool WrapX, xbool WrapY )
{
    s32 Direction = 0;

    // Determine movement required
    switch( Code )
    {
        case ui_navigation::Left:
        {
            Direction = -1;
        }
        break;

        case ui_navigation::Right:
        {
            Direction = 1;
        }
        break;

        default:
        {
        }
        break;
    }

    if( Direction == 0 )
    {
        ui_win::OnNavigate( pWin, Code, Presses, Repeats, WrapX, WrapY );
        return;
    }

    if( Presses > 0 )
    {
        m_RepeatCount = 0;
        m_StepScaler = 1;
    }
    else if( Repeats > 0 )
    {
        m_RepeatCount += Repeats;
        if( m_RepeatCount >= 10 )
        {
            m_StepScaler = MIN( m_StepScaler * 10, m_StepScalerMax );
            m_RepeatCount = 0;
        }
    }

    if( !SetUserValue( m_Value + Direction * m_Step * m_StepScaler ) )
    {
        g_AudioMgr.Play( "InvalidEntry" );
    }
}

//==============================================================================

void ui_slider::OnAccept( ui_win* pWin )
{
    // Pass up to parent
    if( m_pParent )
    {
        m_pParent->OnAccept( pWin );
    }
}

//==============================================================================

void ui_slider::SetRange( s32 Min, s32 Max )
{
    ASSERT( Max >= Min );
    m_Min = Min;
    m_Max = Max;

    if( m_IsParametric )
    {
        m_Value = m_Min + static_cast<s32>( (m_Max-m_Min) * m_ValueParametric + 0.25f );
    }
    else
    {
        if( m_Value < m_Min )
        {
            m_Value = m_Min;
        }
        if( m_Value > m_Max )
        {
            m_Value = m_Max;
        }
    }
}

//==============================================================================

void ui_slider::GetRange( s32& Min, s32& Max ) const
{
    Min = m_Min;
    Max = m_Max;
}

//==============================================================================

void ui_slider::SetStep( s32 Step, s32 StepScalerMax )
{
    ASSERT( Step > 0 );
    ASSERT( StepScalerMax > 0 );
    m_Step          = Step;
    m_StepScaler    = 1;
    m_StepScalerMax = StepScalerMax;
    m_RepeatCount   = 0;
}

//==============================================================================

s32 ui_slider::GetStep( void ) const
{
    return m_Step;
}

//==============================================================================

void ui_slider::SetValue( s32 Value )
{
    if( Value < m_Min )
    {
        Value = m_Min;
    }
    if( Value > m_Max )
    {
        Value = m_Max;
    }

    if( Value != m_Value )
    {
        m_Value = Value;
        if( m_Max > m_Min )
        {
            m_ValueParametric = static_cast<f32>( Value - m_Min ) / (m_Max-m_Min);
        }
        else
        {
            m_ValueParametric = 0.0f;
        }
    }
}

//==============================================================================

xbool ui_slider::SetUserValue( s32 Value )
{
    s32 const OldValue = m_Value;
    SetValue( Value );
    if( m_Value == OldValue )
    {
        return FALSE;
    }

    Notify( ui_notification_type::SliderChanged, m_Value );

    if( m_UseSound )
    {
        static voice_id SliderVoiceID = 0;
        if( !SliderVoiceID || !g_AudioMgr.IsValidVoiceIdCached( SliderVoiceID ) )
        {
            SliderVoiceID = g_AudioMgr.Play( "Slider" );
        }
    }

    return TRUE;
}

//==============================================================================

s32 ui_slider::GetValue( void ) const
{
    return m_Value;
}

//==============================================================================

void ui_slider::OnPointerDown( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    (void)y;

    // Allow dragging when clicking anywhere on the slider bar.
    m_IsDragging = TRUE;
    m_pManager->SetCapture( m_UserID, this );
    SetValueFromPointer( x );
}

//==============================================================================

void ui_slider::OnPointerMove( ui_win* pWin, s32 x, s32 y )
{
    (void) pWin;
    (void)y;

    // We are still dragging the thumb.
    if( m_IsDragging )
    {
        SetValueFromPointer( x );
    }
}

//==============================================================================

void ui_slider::OnPointerUp( ui_win* pWin, s32 x, s32 y )
{
    (void)pWin;
    (void)y;

    // Stop dragging the thumb.
    if( m_IsDragging )
    {
        SetValueFromPointer( x );
        m_IsDragging = FALSE;
        m_pManager->ReleaseCapture( m_UserID );
    }
}

//==============================================================================

void ui_slider::OnFocusLost ( ui_win* pWin )
{
    // Stop dragging the thumb.
    if( m_IsDragging )
    {
        m_IsDragging = FALSE;
        m_pManager->ReleaseCapture( m_UserID );
    }
    // Turn off the highlight.
    ui_win::OnFocusLost( pWin );
}

//==============================================================================

void ui_slider::SetValueFromPointer( s32 x )
{
    s32 y = 0;
    ScreenToLocal( x, y );

    s32 const TrackWidth = MAX( 1, m_Position.GetWidth() - 5 );
    s32 const LocalX = x_clamp( x, 0, TrackWidth );
    s32 const Range = m_Max - m_Min;
    s32 const Value = m_Min + ((LocalX * Range + TrackWidth / 2) / TrackWidth);
    SetUserValue( Value );
}

//==============================================================================

void ui_slider::SetParametric( xbool State )
{
    m_IsParametric = State;
}

//==============================================================================
