//==============================================================================
//  
//  ui_font.hpp
//  
//==============================================================================

#ifndef UI_FONT_HPP
#define UI_FONT_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Texture.hpp"
#include "e_Input.hpp"
#include "ui_renderer.hpp"
#include "Font/Font.hpp"

class ui_manager;
struct ui_text_data;

//==============================================================================
//  ui_font
//==============================================================================

typedef struct _CustomRenderStruct
{
    s32 m_State;
    f32 m_Value;
    s32 m_Frame;
}CustomRenderStruct;


class ui_font
{
public:
                        ui_font             ( void );

    enum Flags
    {
        h_left          = 0x0001,
        h_center        = 0x0002,
        h_right         = 0x0004,
        v_top           = 0x0008,
        v_center        = 0x0010,
        v_bottom        = 0x0020,
        clip_character  = 0x0040,
        clip_l_justify  = 0x0080,
        clip_r_justify  = 0x0100,
        clip_ellipsis   = 0x0200,
        input_glyphs    = 0x0400,
        blend_additive  = 0x1000,
    };

    enum CUSTOM_RENDER_STATES
    {
        s_render        = 0x0001,
        s_no_render     = 0x0002,
        s_fade_in       = 0x0004,
        s_fade_out      = 0x0008
    };

protected:

    void                RenderLayout        ( const irect& Rect,
                                              u32 Flags,
                                              const xcolor& Color,
                                              const TextLayout& Layout,
                                              xbool UseGradient,
                                              f32 FlareAmount,
                                              const CustomRenderStruct* pStateData = NULL ) const;

protected:

    FontHandle          m_Font;
    ui_manager*         m_pManager;
    mutable xwstring    m_ClippedText;

public:

    xbool               Load                ( ui_manager* pManager, const char* pPathName, const char* pName );
    void                Kill                ( void );

    void                TextSize            ( irect& Rect, const   char* pString, s32 Count = -1 ) const;
    void                TextSize            ( irect& Rect, const xwchar* pString, s32 Count = -1 ) const;
    s32                 TextWidth           (              const xwchar* pString, s32 Count = -1 ) const;
    s32                 TextHeight          (              const xwchar* pString, s32 Count = -1 ) const;
    f32                 GetGlyphAdvance     ( u32 Codepoint ) const;
    s32                 GetLineHeight       ( void ) const;

    const xwchar*        ClipEllipsis        ( const xwchar* pString, const irect& Rect ) const;
    void                TextWrap            ( const xwchar* pString, const irect& Rect, xwstring& WrappedString );
    void                TextWrap            ( const ui_text_data& Text, const irect& Rect, ui_text_data& Wrapped );

    void                RenderText          ( const irect& R, u32 Flags, const xcolor& Color, const ui_text_data& Text, xbool UseGradient = TRUE, f32 FlareAmount = 0.0f ) const;
    void                RenderText          ( const irect& R, u32 Flags, const xcolor& Color, const   char* pString, xbool IgnoreEmbeddedColor = TRUE, xbool UseGradient = TRUE, f32 FlareAmount = 0.0f ) const;
    void                RenderText          ( const irect& R, u32 Flags, const xcolor& Color, const xwchar* pString, xbool IgnoreEmbeddedColor = TRUE, xbool UseGradient = TRUE, f32 FlareAmount = 0.0f ) const;
    void                RenderText          ( const irect& R, u32 Flags,       s32     Alpha, const xwchar* pString, xbool IgnoreEmbeddedColor = TRUE, xbool UseGradient = TRUE, f32 FlareAmount = 0.0f ) const;
    void                RenderInputText     ( const irect& R, u32 Flags, const xcolor& Color, const xwchar* pString, input_platform Platform ) const;

    void                RenderStateControlledText( const irect& Rect, u32 Flags, const xcolor& Color, const xwchar* pString, void* StateData) const;
};

//==============================================================================
#endif // UI_FONT_HPP
//==============================================================================
