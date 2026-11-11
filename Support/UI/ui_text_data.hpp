//==============================================================================
//
//  ui_text_data.hpp
//
//==============================================================================

#ifndef UI_TEXT_DATA_HPP
#define UI_TEXT_DATA_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Font/TextLayout.hpp"

//==============================================================================
//  STRUCTURES
//==============================================================================

struct ui_text_data
{
    void                    Append          ( const xwchar* pText,
                                               const xcolor& Color = XCOLOR_WHITE );
    void                    AppendButton    ( s32 ButtonCode,
                                               const vector2& Size,
                                               f32 Spacing = 0.0f,
                                               const xcolor& Color = XCOLOR_WHITE );
    ui_text_data            Mid             ( s32 Start, s32 Length ) const;

    xwstring                Text;
    xarray<TextStyleRun>     Runs;
};

//==============================================================================
#endif // UI_TEXT_DATA_HPP
//==============================================================================
