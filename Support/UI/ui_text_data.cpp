//==============================================================================
//
//  ui_text_data.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ui_text_data.hpp"

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void ui_text_data::Append( const xwchar* pText, const xcolor& Color )
{
    if( !pText || !pText[0] )
    {
        return;
    }

    TextStyleRun Run;
    Run.Start  = Text.GetLength();
    Run.Length = x_wstrlen( pText );
    Run.Color  = Color;
    Text += pText;
    Runs.Append() = Run;
}

//==============================================================================

void ui_text_data::AppendButton( s32 ButtonCode, const vector2& Size, f32 Spacing, const xcolor& Color )
{
    TextStyleRun Run;
    Run.Start         = Text.GetLength();
    Run.Length        = 1;
    Run.Color         = Color;
    Run.InlineId      = ButtonCode;
    Run.InlineSize    = Size;
    Run.InlineSpacing = Spacing;
    Text += (xwchar)0xfffc;
    Runs.Append() = Run;
}

//==============================================================================

ui_text_data ui_text_data::Mid( s32 Start, s32 Length ) const
{
    ASSERT( Start >= 0 && Length >= 0 && Start <= Text.GetLength() - Length );

    ui_text_data Result;
    Result.Text = Text.Mid( Start, Length );

    for( s32 i = 0; i < Runs.GetCount(); i++ )
    {
        TextStyleRun Run = Runs[i];
        s32 First = MAX( Start, Run.Start );
        s32 Last  = MIN( Start + Length, Run.Start + Run.Length );

        if( First >= Last )
        {
            continue;
        }

        Run.Start  = First - Start;
        Run.Length = Last - First;
        Result.Runs.Append() = Run;
    }

    return Result;
}
