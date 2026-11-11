//=========================================================================
//
//  Font.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Font.hpp"
#include "FontMgr.hpp"
#include "x_profile.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

Font::Font( const FontDefinition& Definition, const xref<FontFace>& Face ) :
    m_Definition( Definition )
{
    m_Faces.Append() = Face;
}

//=========================================================================

Font::~Font( void )
{
}

//=========================================================================

const FontDefinition& Font::GetDefinition( void ) const
{
    return m_Definition;
}

//=========================================================================

const xarray<xref<FontFace>>& Font::GetFaces( void ) const
{
    return m_Faces;
}

//=========================================================================

FontMetrics Font::GetMetrics( void ) const
{
    FontMetrics Metrics = m_Faces[0]->GetMetrics( m_Definition.Size );

    Metrics.Ascender  *= m_Definition.HeightScale;
    Metrics.Descender *= m_Definition.HeightScale;
    Metrics.LineHeight = ( Metrics.LineHeight + m_Definition.LineSpacing ) * m_Definition.HeightScale;
    return Metrics;
}

//=========================================================================

xbool Font::BuildLayout( const xwchar* pText, const TextLayoutOptions& Options, TextLayout& Result )
{
    X_PROFILE_SCOPE_CATEGORY( "UI", "Font/BuildLayoutUTF16" );

    return BuildTextLayout( *this, pText, Options, Result );
}

//=========================================================================

xbool Font::BuildLayout( const char* pText, const TextLayoutOptions& Options, TextLayout& Result )
{
    X_PROFILE_SCOPE_CATEGORY( "UI", "Font/BuildLayoutUTF8" );

    Result.Clear();

    xwstring Decoded;
    return DecodeFontText( pText, Decoded ) && BuildLayout( Decoded, Options, Result );
}

//=========================================================================

void Font::AddFallback( const xref<FontFace>& Face )
{
    m_Faces.Append() = Face;
}

//=========================================================================

xbool Font::PrepareText( const TextLayout& Layout, f32 Scale, PreparedText& Result )
{
    X_PROFILE_SCOPE_CATEGORY( "UI", "Font/PrepareText" );

    Result.Glyphs.Clear();

    if( Layout.Owner.Get() != this )
    {
        return FALSE;
    }

    Result.Owner = this;

    for( s32 Layer = 0; Layer < 3; Layer++ )
    {
        if( Layer == 0 && m_Definition.ShadowColor.A == 0 )
        {
            continue;
        }

        if( Layer == 1 && ( m_Definition.OutlineWidth <= 0 || m_Definition.OutlineColor.A == 0 ) )
        {
            continue;
        }

        for( s32 I = 0; I < Layout.Glyphs.GetCount(); I++ )
        {
            const TextGlyph& Glyph = Layout.Glyphs[I];

            if( Glyph.InlineId >= 0 )
            {
                if( Layer != 2 )
                {
                    continue;
                }

                PreparedGlyph& Output = Result.Glyphs.Append();
                Output.Position       = Glyph.Position;
                Output.Image.Size     = Glyph.InlineSize;
                Output.Layer          = 2;
                Output.InlineId       = Glyph.InlineId;
                Output.Cluster        = Glyph.Cluster;
                Output.Color          = Glyph.Color;
                continue;
            }

            GlyphImage  Image;
            f32         Outline = Layer == 1 ? m_Definition.OutlineWidth : 0.0f;
            f32         Blur    = Layer == 0 ? m_Definition.ShadowBlur : 0.0f;

            if( !g_FontMgr.GetAtlas().RequestGlyph( Glyph, m_Definition, Scale, Outline, Blur, Image ) )
            {
                return FALSE;
            }

            if( Image.Size.X <= 0 || Image.Size.Y <= 0 )
            {
                continue;
            }

            PreparedGlyph& Output = Result.Glyphs.Append();
            Output.Image          = Image;
            Output.Position       = Glyph.Position + Image.Offset;
            Output.Cluster        = Glyph.Cluster;
            Output.Layer          = Layer;
            Output.Color          = Glyph.Color;

            if( Layer == 0 )
            {
                Output.Color     = m_Definition.ShadowColor;
                Output.Position += m_Definition.ShadowOffset;
            }
            else if( Layer == 1 )
            {
                Output.Color = m_Definition.OutlineColor;
            }
        }
    }

    return TRUE;
}
