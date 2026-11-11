//==============================================================================
//
//  hud_InfoBox.cpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "hud_InfoBox.hpp"
#include "HudObject.hpp"  

#include "NetworkMgr/Game/GameMgr.hpp"

#include "GameLib/RenderContext.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"
#include "UI/ui_renderer.hpp"
#include "Objects/Flag.hpp"


static xcolor SCORE_RECT_COLOR_GREEN = xcolor( 0,31,0,127 );

//==============================================================================
// FUNCTIONS
//==============================================================================

hud_info_box::hud_info_box( void ) 
{
    m_RenderCTFFlag      = FALSE;
    m_HasCTFFlag         = FALSE;
    m_CTFFlagAlpha       = 0.0f;
    m_CTFFlag.SetName    ( PRELOAD_FILE( "HUD/MULTIPLAYER/HUD_MULTIPLAYER_FLAGICON.XBMP") );
    m_CTFFlagRing.SetName( PRELOAD_FILE( "HUD/MULTIPLAYER/HUD_MULTIPLAYER_FLAGICON_BACKGND.XBMP"));

}

//==============================================================================

s32 hud_info_box::MeasureScoreLayout( f32& Segment1Width,
                                      f32& Segment2Width,
                                      f32& Segment3Width,
                                      s32& NumBoxes ) const
{
    Segment1Width = 12.0f;
    Segment2Width = 12.0f;
    Segment3Width = 12.0f;
    NumBoxes      = 0;

    for( s32 BoxNum = 0; BoxNum < 2; ++BoxNum )
    {
        irect TextRect;
        f32   ColumnWidths[3] = { 8.0f, 8.0f, 8.0f };

        if( m_ScoreString_Col1[BoxNum].Text.GetLength() )
        {
            g_UiMgr->TextSize( 1, TextRect, m_ScoreString_Col1[BoxNum].Text, -1 );
            ColumnWidths[0] += (f32)TextRect.GetWidth();
        }

        if( m_ScoreString_Col2[BoxNum].Text.GetLength() )
        {
            NumBoxes++;
            g_UiMgr->TextSize( 1, TextRect, m_ScoreString_Col2[BoxNum].Text, -1 );
            ColumnWidths[1] += (f32)TextRect.GetWidth();
        }

        if( m_ScoreString_Col3[BoxNum].Text.GetLength() )
        {
            g_UiMgr->TextSize( 1, TextRect, m_ScoreString_Col3[BoxNum].Text, -1 );
            ColumnWidths[2] += (f32)TextRect.GetWidth();
        }

        Segment1Width = x_max( Segment1Width, ColumnWidths[0] );
        Segment2Width = x_max( Segment2Width, ColumnWidths[1] );
        Segment3Width = x_max( Segment3Width, ColumnWidths[2] );
    }

    return (s32)(Segment1Width + Segment2Width + Segment3Width);
}

//==============================================================================

s32 hud_info_box::GetWidth( void ) const
{
#ifdef X_EDITOR
    return 0;
#else
    f32 Segment1Width;
    f32 Segment2Width;
    f32 Segment3Width;
    s32 NumBoxes;
    return MeasureScoreLayout( Segment1Width, Segment2Width, Segment3Width, NumBoxes );
#endif
}

//==============================================================================

void hud_info_box::OnRender( player* pPlayer )
{
    (void)pPlayer;

#ifndef X_EDITOR
    xwchar  ClockStr      [ 8 ];          
    s32 NumBoxes;
    vector3 Pos;

    //
    // Draw the data bars.
    //
    f32 LineWidthSeg1;
    f32 LineWidthSeg2;
    f32 LineWidthSeg3;
    const f32 LineWidth = (f32)MeasureScoreLayout( LineWidthSeg1,
                                                   LineWidthSeg2,
                                                   LineWidthSeg3,
                                                   NumBoxes );

    Pos.GetX() = m_XPos; 
    Pos.GetY() = m_YPos;               

    Pos.GetX() -= LineWidth;

    // check for pulsing
    xcolor PulseColor( g_HudColor );

    if( m_bPulsing )
    {
        PulseColor.A = (u8)(((f32)PulseColor.A / 255) * hud_object::m_PulseAlpha);
    }

    // Now draw both the boxes.
    for( s32 i = 0; (i <= NumBoxes) && (NumBoxes != 0); i++ )
    {
        if( i >= NumBoxes )
        {
            break;
        }

        // Text
        {
            irect Rect;

            // Fade Out
            Rect.Set( (s32)(Pos.GetX() + 2)-8,(s32)(Pos.GetY() + 1), 
                      (s32)(Pos.GetX() + 2),  (s32)(Pos.GetY() + 17) );
            g_UIRenderer.DrawGradientRect( Rect, xcolor(0,31,0,0), xcolor(0,31,0,0), SCORE_RECT_COLOR_GREEN, SCORE_RECT_COLOR_GREEN );

            // Back Drop
            Rect.Set( (s32)(Pos.GetX() + 2),                                                  (s32)(Pos.GetY() + 1), 
                      (s32)(Pos.GetX() + LineWidthSeg1 + LineWidthSeg2 + LineWidthSeg3 - 2),  (s32)(Pos.GetY() + 17) );
            g_UIRenderer.DrawRect( Rect, SCORE_RECT_COLOR_GREEN );

            // Seg 1
            Rect.Set( (s32)(Pos.GetX() + 2),                 (s32)(Pos.GetY() + 1), 
                      (s32)(Pos.GetX() + LineWidthSeg1 - 2)-4, (s32)(Pos.GetY() + 17) );

            xcolor textColor( XCOLOR_WHITE );
            if( m_ScoreString_Col1[i].Text.GetLength() )
            {                
                RenderLine( m_ScoreString_Col1[ i ],  Rect, 255, textColor, 1, ui_font::h_right |ui_font::v_bottom, FALSE );
            }

            // Seg 2
            Rect.Set( (s32)(Pos.GetX() + LineWidthSeg1 + 2),                 (s32)(Pos.GetY() + 1), 
                      (s32)(Pos.GetX() + LineWidthSeg1 + LineWidthSeg2 - 2), (s32)(Pos.GetY() + 17) );
            if( m_ScoreString_Col2[i].Text.GetLength() )
            {                
                RenderLine( m_ScoreString_Col2[ i ],  Rect, 255, textColor, 1, ui_font::h_left|ui_font::v_bottom, FALSE );
            }

            // Seg 3
            Rect.Set( (s32)(Pos.GetX() + LineWidthSeg1 + LineWidthSeg2 + 2),                 (s32)(Pos.GetY() + 1), 
                      (s32)(Pos.GetX() + LineWidthSeg1 + LineWidthSeg2 + LineWidthSeg3 - 2), (s32)(Pos.GetY() + 17) );
            if( m_ScoreString_Col3[i].Text.GetLength() )
            {               
                RenderLine( m_ScoreString_Col3[ i ],  Rect, 255, textColor, 1, ui_font::h_right|ui_font::v_bottom, FALSE );
            }

            // End Flare
            Rect.Set( (s32)(Pos.GetX() + LineWidthSeg1 + LineWidthSeg2 + LineWidthSeg3 - 2),   (s32)(Pos.GetY() + 1), 
                      (s32)(Pos.GetX() + LineWidthSeg1 + LineWidthSeg2 + LineWidthSeg3 - 2)+8, (s32)(Pos.GetY() + 17) );
            g_UIRenderer.DrawGradientRect( Rect, SCORE_RECT_COLOR_GREEN, SCORE_RECT_COLOR_GREEN, xcolor(0,180,0,80), xcolor(0,180,0,80) );

            // Bright side line.
            irect rLine;
            rLine.Set(
                        Rect.l+8, Rect.t,
                        Rect.l+7, Rect.b
                     );
            g_UIRenderer.DrawRect( rLine, g_HudColor, TRUE );

            Pos.GetY()+=16; // Line Feed
        }
    }

    //
    // Draw the Clock Box
    //
    if( GameMgr.GetClockMode() == -1 )
    {
        f32 GameTimeLeft = GameMgr.GetClock();
        GameTimeLeft += 1.0f;        
        GameTimeLeft = MINMAX( 0.0f, GameTimeLeft, (f32)GameMgr.GetClockLimit() );

        s32 Seconds1 = ((s32)(GameTimeLeft) % 60) / 10;
        s32 Seconds2 = ((s32)(GameTimeLeft) % 60) % 10;
        s32 Minutes1 = (((s32)(GameTimeLeft)) / 60) / 10;
        s32 Minutes2 = (((s32)(GameTimeLeft)) / 60) % 10;

        if( Minutes1 != 0 )
            x_wstrcpy( ClockStr, (const xwchar*)((xwstring)xfs( "%d%d:%d%d", Minutes1, Minutes2, Seconds1, Seconds2 )) );
        else
            x_wstrcpy( ClockStr, (const xwchar*)((xwstring)xfs( "%d:%d%d", Minutes2, Seconds1, Seconds2 )) );

        irect TimeStringRect;
        g_UiMgr->TextSize( 1, TimeStringRect, ClockStr, -1);
        s32 TimeStringWidth = (s32)TimeStringRect.GetWidth();       
        TimeStringWidth+=8;

        if( ClockStr[ 0 ] == 0 )
        {
            return;
        }
        
        vector3 ClockBarPos = Pos;
        ClockBarPos.GetX() = (m_XPos-2) - TimeStringWidth; 

        irect Rect;
        xcolor TextColor;

        TextColor = XCOLOR_WHITE;
        if( NumBoxes == 0 )
        {
            Rect.Set( (s32)ClockBarPos.GetX(), (s32)(ClockBarPos.GetY() + 2.0f), (s32)(ClockBarPos.GetX() + TimeStringWidth), (s32)(ClockBarPos.GetY() + 18 + 2.0f) );
        }
        else
        {
            Rect.Set( (s32)ClockBarPos.GetX(), (s32)(ClockBarPos.GetY() + 1.0f), (s32)(ClockBarPos.GetX() + TimeStringWidth), (s32)(ClockBarPos.GetY() + 18 + 1.0f) );
        }

        // Back fill
        g_UIRenderer.DrawRect( Rect, SCORE_RECT_COLOR_GREEN );

        // Text
        RenderLine( ClockStr, Rect, 255, TextColor, 1, ui_font::h_right|ui_font::v_top, FALSE );

        // End Flare ( bright side )
        Rect.Set( (s32)(ClockBarPos.GetX() + TimeStringWidth),    (s32)(ClockBarPos.GetY() + 1.0f), 
                  (s32)(ClockBarPos.GetX() + TimeStringWidth)+8,  (s32)(ClockBarPos.GetY() + 18 + 1.0f) );
        g_UIRenderer.DrawGradientRect( Rect, SCORE_RECT_COLOR_GREEN, SCORE_RECT_COLOR_GREEN, xcolor(0,180,0,80), xcolor(0,180,0,80) );

        // Bright side line.
        irect rLine;
        rLine.Set(Rect.l+8,Rect.t,Rect.l+7,Rect.b);
        g_UIRenderer.DrawRect( rLine, g_HudColor, TRUE );

        // Fade Out
        Rect.Set( (s32)(ClockBarPos.GetX()-8),    (s32)(ClockBarPos.GetY() + 1.0f), 
                  (s32)(ClockBarPos.GetX()),      (s32)(ClockBarPos.GetY() + 18 + 1.0f) );
        g_UIRenderer.DrawGradientRect( Rect, xcolor(0,31,0,0), xcolor(0,31,0,0), SCORE_RECT_COLOR_GREEN, SCORE_RECT_COLOR_GREEN );

        Pos.GetY()+=16; // Line Feed
    }

    // Render the CTF Flag icon
    OnRenderCTF_Flag(Pos);

#endif // X_EDITOR
}

//==============================================================================

void hud_info_box::OnAdvanceSimulation( player* pPlayer, f32 DeltaTime )
{
    (void)pPlayer;
    (void)DeltaTime;

    // Update the CTF Flag Icon
    UpdateCFT_Flag( DeltaTime, pPlayer );
}

//==============================================================================

xbool hud_info_box::OnProperty( prop_query& rPropQuery )
{
    (void)rPropQuery;
    return FALSE;
}

//==============================================================================

void hud_info_box::OnEnumProp( prop_enum&  List )
{
    (void)List;
}

//==============================================================================

void hud_info_box::SetScoreInfo( const ui_text_data& Col1,
                                const ui_text_data& Col2,
                                const ui_text_data& Col3,
                                s32 Slot )
{
    s32 Index = MAX( ABS( Slot ) - 1, 0 );

    if( Index >= 2 )
    {
        return;
    }

    if( Slot < 0 )
    {
        m_ScoreString_Col1[Index] = ui_text_data();
        m_ScoreString_Col2[Index] = ui_text_data();
        m_ScoreString_Col3[Index] = ui_text_data();
    }
    else
    {
        m_ScoreString_Col1[Index] = Col1;
        m_ScoreString_Col2[Index] = Col2;
        m_ScoreString_Col3[Index] = Col3;
    }
}

//------------------------------------------------------------------------------

void hud_info_box::UpdateCFT_Flag( f32 DeltaTime, player* pPlayer )
{
    (void)DeltaTime;
    (void)pPlayer;

#ifndef X_EDITOR

    if( GameMgr.GetGameType() == GAME_CAMPAIGN )
    {
        m_HasCTFFlag = FALSE;
        return;
    }

    xbool bAttachedToPlayer = FALSE;

    g_ObjMgr.SelectByAttribute( object::ATTR_COLLIDABLE, object::TYPE_FLAG );
    slot_id aID = g_ObjMgr.StartLoop();
    while( (aID != SLOT_NULL) )
    {
        object* pObject = g_ObjMgr.GetObjectBySlot(aID);

        flag& Flag = flag::GetSafeType( *pObject );

        if( Flag.IsAttached() && (Flag.GetAttachedTo() == pPlayer->net_GetSlot()) )
        {
            bAttachedToPlayer = TRUE;
            break;
        }

        aID = g_ObjMgr.GetNextResult( aID );
    }
    g_ObjMgr.EndLoop();

    // Check to see if we need to show that the player has the flag.
    m_HasCTFFlag = bAttachedToPlayer;
#endif
}

//------------------------------------------------------------------------------

void hud_info_box::OnRenderCTF_Flag( vector3& Pos )
{
    (void)Pos;
#ifndef X_EDITOR
    if( GameMgr.GetGameType() == GAME_CAMPAIGN )
    {
        return;
    }

    if( m_RenderCTFFlag || m_CTFFlagAlpha > 0 )
    {
        s32 FLAG_Y = (s32)(Pos.GetY());
        texture* pCTFTexture = m_CTFFlag.GetPointer();
        if( pCTFTexture )
        {
            const xbitmap& Bitmap = pCTFTexture->m_bitmap;
            xcolor FlagColor = xcolor(200,0,0,255);
            FlagColor.A = (u8)(255.0f*m_CTFFlagAlpha);
            FlagColor.A = (u8)MIN( (f32)FlagColor.A, hud_object::m_PulseAlpha );
            g_UIRenderer.DrawImage( *pCTFTexture,
                                    vector2( m_XPos - (f32)Bitmap.GetWidth(), (f32)(FLAG_Y + 4) ),
                                    vector2( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() ),
                                    vector2( 0.0f, 0.0f ),
                                    vector2( 1.0f, 1.0f ),
                                    FlagColor,
                                    0.0f,
                                    UI_BLEND_ALPHA,
                                    UI_SAMPLER_LINEAR_CLAMP );
        }

        texture* pCTFRingTexture = m_CTFFlagRing.GetPointer();
        if( pCTFRingTexture )
        {
            const xbitmap& Bitmap = pCTFRingTexture->m_bitmap;
            xcolor FlagColor = g_HudColor;
            FlagColor.A = (u8)(255.0f*m_CTFFlagAlpha);
            g_UIRenderer.DrawImage( *pCTFRingTexture,
                                    vector2( m_XPos - (f32)Bitmap.GetWidth(), (f32)(FLAG_Y + 4) ),
                                    vector2( (f32)Bitmap.GetWidth(), (f32)Bitmap.GetHeight() ),
                                    vector2( 0.0f, 0.0f ),
                                    vector2( 1.0f, 1.0f ),
                                    FlagColor,
                                    0.0f,
                                    UI_BLEND_ALPHA,
                                    UI_SAMPLER_LINEAR_CLAMP );
        }

    }
#endif
}

//------------------------------------------------------------------------------


