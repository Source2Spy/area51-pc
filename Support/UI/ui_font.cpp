//==============================================================================
//
//  ui_font.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy.hpp"
#include "ui_font.hpp"
#include "ui_manager.hpp"
#include "ui_text_data.hpp"
#include "Font/FontResource.hpp"
#include "Font/FontMgr.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static xwstring CopyFontText( const xwchar* pString, s32 Count )
{
    if( Count < 0 )
    {
        return xwstring( pString );
    }

    xwstring Result;
    for( s32 i = 0; i < Count && pString[i]; i++ )
    {
        Result += pString[i];
    }

    if( Result.GetLength() && Result[Result.GetLength() - 1] >= 0xd800 && Result[Result.GetLength() - 1] <= 0xdbff )
    {
        Result = Result.Left( Result.GetLength() - 1 );
    }

    return Result;
}

//==============================================================================

static xcolor MultiplyColor( const xcolor& A, const xcolor& B )
{
    return xcolor( (u8)( (u32)A.R * B.R / 255 ), (u8)( (u32)A.G * B.G / 255 ), (u8)( (u32)A.B * B.B / 255 ),
                   (u8)( (u32)A.A * B.A / 255 ) );
}

//==============================================================================

static void BuildUIText( ui_manager* pManager,
                          const xwchar* pString,
                          ui_text_data& Text,
                          xarray<s32>* pSourceOffsets = NULL )
{
    if( pManager )
    {
        pManager->BuildTextData( pString, Text, pSourceOffsets );
        return;
    }

    Text.Append( pString );

    if( pSourceOffsets )
    {
        pSourceOffsets->SetCount( Text.Text.GetLength() + 1 );

        for( s32 i = 0; i < pSourceOffsets->GetCount(); i++ )
        {
            (*pSourceOffsets)[i] = i;
        }
    }
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ui_font::ui_font( void ) : m_pManager( NULL )
{
}

//==============================================================================

xbool ui_font::Load( ui_manager* pManager, const char* pPathName, const char* pName )
{
    FontHandle Font = LoadFontResource( pPathName, pName );

    if( !Font.Get() )
    {
        return FALSE;
    }

    m_pManager = pManager;
    m_Font     = Font;
    m_ClippedText.Clear();
    return TRUE;
}

//==============================================================================

void ui_font::Kill( void )
{
    m_Font.Clear();
    m_ClippedText.Clear();
    m_pManager = NULL;
}

//==============================================================================

void ui_font::TextSize( irect& Rect, const char* pString, s32 Count ) const
{
    xwstring Text;
    if( !DecodeFontText( pString, Text ) )
    {
        Rect.Clear();
        return;
    }
    TextSize( Rect, (const xwchar*)Text, Count );
}

//==============================================================================

void ui_font::TextSize( irect& Rect, const xwchar* pString, s32 Count ) const
{
    Rect.Clear();

    if( !m_Font.Get() || !pString )
    {
        return;
    }

    ui_text_data Text;
    BuildUIText( m_pManager, CopyFontText( pString, Count ), Text );

    TextLayout Layout;
    TextLayoutOptions Options;

    Options.Language = x_GetLocaleString( XL_LOCALE_CODE_ISO_639_1 );
    Options.Runs = Text.Runs;

    if( !m_Font->BuildLayout( Text.Text, Options, Layout ) )
    {
        x_throw( "Font layout failed" );
        return;
    }

    Rect.Set( 0, 0, (s32)x_ceil( Layout.Size.X ), (s32)x_ceil( Layout.Size.Y ) );
}

//==============================================================================

s32 ui_font::TextWidth( const xwchar* pString, s32 Count ) const
{
    irect Rect;
    TextSize( Rect, pString, Count );
    return Rect.GetWidth();
}

//==============================================================================

s32 ui_font::TextHeight( const xwchar* pString, s32 Count ) const
{
    irect Rect;
    TextSize( Rect, pString, Count );
    return Rect.GetHeight();
}

//==============================================================================

s32 ui_font::GetLineHeight( void ) const
{
    if( !m_Font.Get() )
    {
        return 0;
    }

    return (s32)x_ceil( m_Font->GetMetrics().LineHeight );
}

//==============================================================================

f32 ui_font::GetGlyphAdvance( u32 Codepoint ) const
{
    xwchar Text[3] = { 0, 0, 0 };
    if( Codepoint > 0x10ffff || ( Codepoint >= 0xd800 && Codepoint <= 0xdfff ) )
    {
        return 0;
    }

    if( Codepoint < 0x10000 )
    {
        Text[0] = (xwchar)Codepoint;
    }
    else
    {
        Codepoint -= 0x10000;
        Text[0] = (xwchar)( 0xd800 + ( Codepoint >> 10 ) );
        Text[1] = (xwchar)( 0xdc00 + ( Codepoint & 1023 ) );
    }

    TextLayout Layout;
    return m_Font.Get() && m_Font->BuildLayout( Text, TextLayoutOptions(), Layout ) ? Layout.Size.X : 0;
}

//==============================================================================

const xwchar* ui_font::ClipEllipsis( const xwchar* pString, const irect& Rect ) const
{
    ui_text_data Text;
    xarray<s32> SourceOffsets;
    BuildUIText( m_pManager, pString, Text, &SourceOffsets );

    TextLayoutOptions Options;
    Options.Language = x_GetLocaleString( XL_LOCALE_CODE_ISO_639_1 );
    Options.Width    = (f32)Rect.GetWidth();
    Options.Ellipsis = TRUE;
    Options.Runs     = Text.Runs;

    TextLayout Layout;

    if( !m_Font.Get() || !m_Font->BuildLayout( Text.Text, Options, Layout ) )
    {
        x_throw( "Font ellipsis failed" );
        m_ClippedText.Clear();
    }
    else
    {
        if( Layout.Text == Text.Text )
        {
            m_ClippedText = pString;
        }
        else
        {
            s32 Cut = MAX( 0, Layout.Text.GetLength() - 1 );
            m_ClippedText = xwstring( pString ).Left( SourceOffsets[Cut] );
            m_ClippedText += (xwchar)0x2026;
        }
    }

    return m_ClippedText;
}

//==============================================================================

void ui_font::TextWrap( const xwchar* pString, const irect& Rect, xwstring& WrappedString )
{
    WrappedString.Clear();

    ui_text_data Text;
    xarray<s32> SourceOffsets;
    BuildUIText( m_pManager, pString, Text, &SourceOffsets );

    TextLayoutOptions Options;
    Options.Language = x_GetLocaleString( XL_LOCALE_CODE_ISO_639_1 );
    Options.Width    = (f32)Rect.GetWidth();
    Options.Wrap     = TRUE;
    Options.Runs     = Text.Runs;

    TextLayout Layout;

    if( !m_Font.Get() || !m_Font->BuildLayout( Text.Text, Options, Layout ) )
    {
        x_throw( "Font wrapping failed" );
        return;
    }

    for( s32 i = 0; i < Layout.Lines.GetCount(); i++ )
    {
        if( i )
        {
            WrappedString += '\n';
        }

        s32 Start = SourceOffsets[Layout.Lines[i].Start];
        s32 End   = SourceOffsets[Layout.Lines[i].End];

        for( s32 j = Start; j < End; j++ )
        {
            if( pString[j] != '\n' && pString[j] != '\r' )
            {
                WrappedString += pString[j];
            }
        }
    }
}

//==============================================================================

void ui_font::TextWrap( const ui_text_data& Text, const irect& Rect, ui_text_data& Wrapped )
{
    Wrapped = ui_text_data();

    TextLayoutOptions Options;
    Options.Language = x_GetLocaleString( XL_LOCALE_CODE_ISO_639_1 );
    Options.Width    = (f32)Rect.GetWidth();
    Options.Wrap     = TRUE;
    Options.Runs     = Text.Runs;

    TextLayout Layout;

    if( !m_Font.Get() || !m_Font->BuildLayout( Text.Text, Options, Layout ) )
    {
        x_throw( "Font wrapping failed" );
        return;
    }

    for( s32 i = 0; i < Layout.Lines.GetCount(); i++ )
    {
        if( i )
        {
            Wrapped.Text += '\n';
        }

        const TextLine& Line   = Layout.Lines[i];
        s32             Offset = Wrapped.Text.GetLength();

        for( s32 j = Line.Start; j < Line.End; j++ )
        {
            Wrapped.Text += Text.Text[j];
        }

        for( s32 j = 0; j < Text.Runs.GetCount(); j++ )
        {
            TextStyleRun Run   = Text.Runs[j];
            s32          Start = MAX( Run.Start, Line.Start );
            s32          End   = MIN( Run.Start + Run.Length, Line.End );

            if( Start >= End )
            {
                continue;
            }

            Run.Start             = Offset + Start - Line.Start;
            Run.Length            = End - Start;
            Wrapped.Runs.Append() = Run;
        }
    }
}

//==============================================================================

void ui_font::RenderLayout( const irect& Rect,
                            u32 Flags,
                            const xcolor& Color,
                            const TextLayout& Layout,
                            xbool UseGradient,
                            f32 FlareAmount,
                            const CustomRenderStruct* pStateData ) const
{
    ui_draw_list&      DrawList = g_UIRenderer.GetDrawList();
    const ui_viewport& Viewport = g_UIRenderer.GetViewport();
    f32                Scale    = Viewport.GetScale();
    if( DrawList.GetCoordinateSpace() == UI_COORDINATE_SPACE_SCREEN )
    {
        Scale = 1.0f;
    }
    else if( DrawList.GetCoordinateSpace() == UI_COORDINATE_SPACE_HUD )
    {
        Scale = Viewport.GetHudScale( DrawList.GetCoordinateViewport() );
    }

    g_FontMgr.BeginFrame( eng_GetCurrentFrameSerial() );
    PreparedText Prepared;
    if( !m_Font->PrepareText( Layout, Scale, Prepared ) )
    {
        x_throw( "Font atlas preparation failed" );
        return;
    }

    f32 Y = (f32)Rect.t;
    if( Flags & v_center )
    {
        Y += ( Rect.GetHeight() - Layout.Size.Y ) * 0.5f;
    }
    else if( Flags & v_bottom )
    {
        Y += Rect.GetHeight() - Layout.Size.Y;
    }

    xbool Clip = ( Flags & ( clip_character | clip_l_justify | clip_r_justify | clip_ellipsis ) ) != 0;
    if( Clip )
    {
        g_UIRenderer.PushClipRect( Rect );
    }

    for( s32 i = 0; i < Prepared.Glyphs.GetCount(); i++ )
    {
        PreparedGlyph const& Glyph     = Prepared.Glyphs[i];
        s32                  LineIndex = 0;
        for( s32 j = 1; j < Layout.Lines.GetCount(); j++ )
        {
            if( Glyph.Cluster >= Layout.Lines[j].Start )
            {
                LineIndex = j;
            }
        }

        f32 X = (f32)Rect.l;
        if( Flags & h_center )
        {
            X += ( Rect.GetWidth() - Layout.Lines[LineIndex].Width ) * 0.5f;
        }
        else if( Flags & h_right )
        {
            X += Rect.GetWidth() - Layout.Lines[LineIndex].Width;
        }

        xcolor Top = Glyph.Color;
        if( Glyph.Layer == 2 )
        {
            Top = MultiplyColor( Top, Color );
        }
        else
        {
            Top.A = (u8)( (u32)Top.A * Color.A / 255 );
        }

        if( pStateData )
        {
            const CustomRenderStruct& State = pStateData[Glyph.Cluster];

            if( State.m_State != s_render )
            {
                continue;
            }

            Top.A = (u8)( Top.A * x_clamp( State.m_Value, 0.0f, 255.0f ) / 255.0f );
        }

        vector2 Position( X + Glyph.Position.X, Y + Glyph.Position.Y );
        if( Glyph.InlineId >= 0 )
        {
            texture* Texture = m_pManager->GetButtonTexture( Glyph.InlineId );
            if( Texture )
            {
                g_UIRenderer.DrawImage( *Texture, Position, Glyph.Image.Size, vector2( 0, 0 ), vector2( 1, 1 ), Top );
            }
            continue;
        }

        xcolor Bottom = Top;
        if( UseGradient && Glyph.Layer == 2 )
        {
            if( ( Top.R + Top.G + Top.B ) / 3.0f > 59.0f )
            {
                Top.R = 255;
                Top.G = 255;
                Top.B = 255;
            }
            else
            {
                Bottom.R = 0;
                Bottom.G = 0;
                Bottom.B = 0;
            }
        }

        if( Glyph.Layer == 2 )
        {
            // Tint both ends after the gradient sets its highlight color.
            const xcolor& FontColor = m_Font->GetDefinition().Color;

            Top    = MultiplyColor( Top, FontColor );
            Bottom = MultiplyColor( Bottom, FontColor );
        }

        const vector2&   Size        = Glyph.Image.Size;
        const vector2&   UV0         = Glyph.Image.UV0;
        const vector2&   UV1         = Glyph.Image.UV1;
        ui_vertex        Vertices[4] = { ui_vertex( Position, UV0, Top ),
                                         ui_vertex( Position + vector2( Size.X, 0 ), vector2( UV1.X, UV0.Y ), Top ),
                                         ui_vertex( Position + Size, UV1, Bottom ),
                                         ui_vertex( Position + vector2( 0, Size.Y ), vector2( UV0.X, UV1.Y ), Bottom ) };
        static const u32 Indices[6]  = { 0, 1, 2, 2, 3, 0 };
        ui_material Material( Glyph.Image.Texture, ( Flags & blend_additive ) ? UI_BLEND_ADDITIVE : UI_BLEND_ALPHA );
        Material.Coverage = TRUE;
        if( !DrawList.AddTriangles( Material, Vertices, 4, Indices, 6 ) )
        {
            x_throw( "Font draw submission failed" );
        }

        if( FlareAmount > 0 && Glyph.Layer == 2 )
        {
            Material.Blend = UI_BLEND_ADDITIVE;
            for( s32 j = 0; j < 4; j++ )
            {
                xcolor Flare = Top;
                Flare.A      = (u8)( Flare.A * MIN( 1.0f, FlareAmount ) );
                Vertices[j]  = ui_vertex( Vertices[j].Position, Vertices[j].UV, Flare );
            }

            if( !DrawList.AddTriangles( Material, Vertices, 4, Indices, 6 ) )
            {
                x_throw( "Font flare submission failed" );
            }
        }
    }

    if( Clip )
    {
        g_UIRenderer.PopClipRect();
    }
}

//==============================================================================

void ui_font::RenderText( const irect& Rect, u32 Flags, const xcolor& Color, const ui_text_data& Text,
                          xbool UseGradient, f32 FlareAmount ) const
{
    if( !m_Font.Get() )
    {
        return;
    }

    TextLayoutOptions Options;
    Options.Language = x_GetLocaleString( XL_LOCALE_CODE_ISO_639_1 );
    Options.Width    = (f32)Rect.GetWidth();
    Options.Ellipsis = ( Flags & clip_ellipsis ) != 0;
    Options.Runs     = Text.Runs;

    TextLayout Layout;

    if( !m_Font->BuildLayout( Text.Text, Options, Layout ) )
    {
        x_throw( "Font layout failed" );
        return;
    }

    RenderLayout( Rect, Flags, Color, Layout, UseGradient, FlareAmount );
}

//==============================================================================

void ui_font::RenderText( const irect& Rect, u32 Flags, const xcolor& Color, const xwchar* pString,
                          xbool IgnoreRunColors, xbool UseGradient, f32 FlareAmount ) const
{
    ui_text_data Text;
    BuildUIText( m_pManager, pString, Text );

    if( IgnoreRunColors )
    {
        for( s32 i = 0; i < Text.Runs.GetCount(); i++ )
        {
            Text.Runs[i].Color = XCOLOR_WHITE;
        }
    }
    RenderText( Rect, Flags, Color, Text, UseGradient, FlareAmount );
}

//==============================================================================

void ui_font::RenderText( const irect& Rect, u32 Flags, const xcolor& Color, const char* pString, xbool IgnoreRunColors,
                          xbool UseGradient, f32 FlareAmount ) const
{
    xwstring Text;
    if( !DecodeFontText( pString, Text ) )
    {
        x_throw( "Invalid UTF-8 text" );
        return;
    }
    RenderText( Rect, Flags, Color, (const xwchar*)Text, IgnoreRunColors, UseGradient, FlareAmount );
}

//==============================================================================

void ui_font::RenderText( const irect& Rect, u32 Flags, s32 Alpha, const xwchar* pString, xbool IgnoreRunColors,
                          xbool UseGradient, f32 FlareAmount ) const
{
    RenderText( Rect, Flags, xcolor( 255, 255, 255, (u8)Alpha ), pString, IgnoreRunColors, UseGradient, FlareAmount );
}

//==============================================================================

void ui_font::RenderInputText( const irect& Rect, u32 Flags, const xcolor& Color, const xwchar* pString,
                               input_platform ) const
{
    RenderText( Rect, Flags | input_glyphs, Color, pString );
}

//==============================================================================

void ui_font::RenderStateControlledText( const irect& Rect, u32 Flags, const xcolor& Color, const xwchar* pString,
                                         void* StateData ) const
{
    TextLayout Layout;
    if( !m_Font.Get() || !m_Font->BuildLayout( pString, TextLayoutOptions(), Layout ) )
    {
        x_throw( "Font layout failed" );
        return;
    }
    RenderLayout( Rect, Flags, Color, Layout, FALSE, 0.0f, (CustomRenderStruct const*)StateData );
}
