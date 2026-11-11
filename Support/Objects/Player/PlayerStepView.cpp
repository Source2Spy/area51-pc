//=========================================================================
//
//  PlayerStepView.cpp
//
//=========================================================================

#include "PlayerView.hpp"

//=========================================================================

PlayerStepView::PlayerStepView( void ) :
    m_Offset( 0.0f )
{
}

//=========================================================================

void PlayerStepView::Clear( void )
{
    m_Offset = 0.0f;
}

//=========================================================================

void PlayerStepView::Advance( f32 GroundOffset, f32 DeltaTime, f32 StepHeight )
{
    const f32 Speed = 200.0f;
    m_Offset = x_clamp( m_Offset - GroundOffset, -StepHeight, StepHeight );
    const f32 Distance = Speed * DeltaTime;
    if( m_Offset > 0.0f )
        m_Offset = x_max( 0.0f, m_Offset - Distance );
    else
        m_Offset = x_min( 0.0f, m_Offset + Distance );
}

//=========================================================================

f32 PlayerStepView::GetOffset( void ) const
{
    return m_Offset;
}

//=========================================================================

