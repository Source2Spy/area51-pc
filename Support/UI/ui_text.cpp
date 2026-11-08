//=========================================================================
//
//  ui_text.cpp
//
//=========================================================================
#include "Entropy.hpp"
#include "ui_text.hpp"
#include "ui_manager.hpp"
#include "ui_font.hpp"
#include "ui_renderer.hpp"

//=========================================================================
//  Structs
//=========================================================================

//=========================================================================
//  Data
//=========================================================================

//=========================================================================
//  Factory function
//=========================================================================

ui_win* ui_text_factory( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    ui_text* pButton = new ui_text;
    pButton->Create( UserID, pManager, Position, pParent, Flags );

    return (ui_win*)pButton;
}

//=========================================================================
//  ui_text
//=========================================================================

ui_text::ui_text( void )
{
    m_LabelColor = XCOLOR_WHITE;
}

//=========================================================================

ui_text::~ui_text( void )
{
}

//=========================================================================

xbool ui_text::Create( s32 UserID, ui_manager* pManager, const irect& Position, ui_win* pParent, s32 Flags )
{
    xbool   Success;

    Success = ui_control::Create( UserID, pManager, Position, pParent, Flags );

    // Initialize Data
    m_UseSmallText = FALSE;
    m_UseShadow = FALSE;

    return Success;
}

//=========================================================================

void ui_text::Render( s32 ox, s32 oy )
{
    s32     FontID;

    // Only render is visible
    if( m_Flags & WF_VISIBLE )
    {
        const xbool HasInputGlyphs = (m_LabelFlags & ui_font::input_glyphs) != 0;
        if( HasInputGlyphs && (m_pManager->GetInputDevice( m_UserID ) != ui_input_device::Gamepad) )
        {
            return;
        }

        xcolor TextColor = (m_Flags & WF_DISABLED) ? XCOLOR_GREY : GetLabelColor();

        // Calculate rectangle
        irect r;
        r.Set( (m_Position.l+ox), (m_Position.t+oy), (m_Position.r+ox), (m_Position.b+oy) );

        // Render Text
        if (m_UseSmallText)
        {
            FontID = m_pManager->FindFont("small");
        }
        else
        {
            FontID = m_pManager->FindFont("large");
        }

        if( m_UseShadow )
        {
            irect Shadow = r;
            Shadow.Translate( 1, 1 );
            xcolor ShadowColor( 0, 0, 0, TextColor.A );
            if( HasInputGlyphs )
            {
                m_pManager->RenderInputText( FontID, Shadow, m_LabelFlags & ~ui_font::input_glyphs,
                    ShadowColor, m_Label, m_pManager->GetInputPlatform(m_UserID) );
            }
            else
            {
                m_pManager->RenderText( FontID, Shadow, m_LabelFlags, ShadowColor, m_Label );
            }
        }

        if( HasInputGlyphs )
        {
            m_pManager->RenderInputText( FontID,
                                         r,
                                         m_LabelFlags & ~ui_font::input_glyphs,
                                         TextColor,
                                         m_Label,
                                         m_pManager->GetInputPlatform( m_UserID ) );
        }
        else
        {
            m_pManager->RenderText( FontID, r, m_LabelFlags, TextColor, m_Label );
        }

        // Render children
        for( s32 i=0 ; i<m_Children.GetCount() ; i++ )
        {
            m_Children[i]->Render( m_Position.l+ox, m_Position.t+oy );
        }
    }
}
