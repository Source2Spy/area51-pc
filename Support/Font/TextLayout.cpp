//=========================================================================
//
//  TextLayout.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Font.hpp"
#include "FontFace_Private.hpp"

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uscript.h>
#include <unicode/uchar.h>
#include <unicode/utf16.h>
#include <unicode/ustring.h>

#include <algorithm>
#include "x_math.hpp"
#include <vector>

//=========================================================================
//  FUNCTIONS
//=========================================================================

void TextLayout::Clear( void )
{
    Owner.Clear();
    Text.Clear();
    Glyphs.Clear();
    Lines.Clear();
    Size.Set( 0.0f, 0.0f );
}

//=========================================================================

xbool DecodeFontText( const char* pText, xwstring& Result )
{
    Result.Clear();

    if( !pText )
    {
        return FALSE;
    }

    UErrorCode Error  = U_ZERO_ERROR;
    s32        Length = 0;

    u_strFromUTF8( NULL, 0, &Length, pText, -1, &Error );

    if( Error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE( Error ) )
    {
        return FALSE;
    }

    std::vector<UChar> Buffer( Length + 1 );
    Error = U_ZERO_ERROR;

    u_strFromUTF8( Buffer.data(), Length + 1, NULL, pText, -1, &Error );

    if( U_FAILURE( Error ) )
    {
        return FALSE;
    }

    Result = (const xwchar*)Buffer.data();
    return TRUE;
}

//=========================================================================
//  STRUCTURES
//=========================================================================

struct Segment
{
    s32         Start;
    s32         End;
    s32         Face;
    UScriptCode Script;
    s32         Style;
};

//-------------------------------------------------------------------------

struct BreakIterator
{
    UBreakIterator* Value;

    ~BreakIterator( void )
    {
        if( Value )
        {
            ubrk_close( Value );
        }
    }
};

//-------------------------------------------------------------------------

struct BidiIterator
{
    UBiDi* Value;

    ~BidiIterator( void )
    {
        if( Value )
        {
            ubidi_close( Value );
        }
    }
};

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
s32 FindStyle( const TextLayoutOptions& Options, s32 Position )
{
    for( s32 I = 0; I < Options.Runs.GetCount(); I++ )
    {
        if( Position >= Options.Runs[I].Start && Position < Options.Runs[I].Start + Options.Runs[I].Length )
        {
            return I;
        }
    }

    return -1;
}

//=========================================================================

static
xbool ShapeLine( Font& Source,
                 const UChar* Text,
                 s32 Length,
                 s32 Start,
                 s32 End,
                 s32 ParagraphStart,
                 UBiDi* Paragraph,
                 const TextLayoutOptions& Options,
                 f32 Baseline,
                 f32 InlineCenterOffset,
                 s32 LineIndex,
                 xarray<TextGlyph>& Glyphs,
                 f32& Width )
{
    Width = 0.0f;

    if( Start == End )
    {
        return TRUE;
    }

    UErrorCode    Error      = U_ZERO_ERROR;
    BreakIterator Characters = { ubrk_open( UBRK_CHARACTER, Options.Language, Text, Length, &Error ) };

    if( U_FAILURE( Error ) )
    {
        return FALSE;
    }

    const xarray<xref<FontFace>>& Faces = Source.GetFaces();
    std::vector<Segment> Segments;
    UScriptCode Previous = USCRIPT_COMMON;

    for( s32 Position = Start; Position < End; )
    {
        s32 Next = MIN( End, ubrk_following( Characters.Value, Position ) );

        if( Next <= Position )
        {
            return FALSE;
        }

        s32 Chosen = 0;

        for( s32 F = 0; F < Faces.GetCount(); F++ )
        {
            xbool Covered = TRUE;

            for( s32 C = Position; C < Next; )
            {
                UChar32 Code;
                U16_NEXT( Text, C, Next, Code );

                if( !u_hasBinaryProperty( Code, UCHAR_DEFAULT_IGNORABLE_CODE_POINT ) && Code != 0xfffc && Code != '\t' &&
                    !Faces[F]->GetGlyphIndex( Code ) )
                {
                    Covered = FALSE;
                }
            }

            if( Covered )
            {
                Chosen = F;
                break;
            }
        }

        s32     C = Position;
        UChar32 Code;
        U16_NEXT( Text, C, Next, Code );

        UScriptCode Script = uscript_getScript( Code, &Error );

        if( U_FAILURE( Error ) )
        {
            return FALSE;
        }

        if( Script == USCRIPT_COMMON || Script == USCRIPT_INHERITED )
        {
            Script = Previous;
        }
        else
        {
            Previous = Script;
        }

        s32   Style  = FindStyle( Options, Position );
        xbool Inline = Style >= 0 && Options.Runs[Style].InlineId >= 0;
        xbool PreviousInline = !Segments.empty() && Segments.back().Style >= 0 &&
                               Options.Runs[Segments.back().Style].InlineId >= 0;
        xbool Tab = Code == '\t';
        xbool PreviousTab = !Segments.empty() && Text[Segments.back().Start] == '\t';

        if( !Inline && !PreviousInline && !Tab && !PreviousTab && !Segments.empty() && Segments.back().End == Position &&
            Segments.back().Face == Chosen && Segments.back().Script == Script )
        {
            Segments.back().End = Next;
        }
        else
        {
            Segments.push_back( { Position, Next, Chosen, Script, Style } );
        }

        Position = Next;
    }

    BidiIterator Line = { ubidi_openSized( End - Start, 0, &Error ) };

    if( U_FAILURE( Error ) )
    {
        return FALSE;
    }

    ubidi_setLine( Paragraph, Start - ParagraphStart, End - ParagraphStart, Line.Value, &Error );

    s32 RunCount = ubidi_countRuns( Line.Value, &Error );

    if( U_FAILURE( Error ) )
    {
        return FALSE;
    }

    const FontDefinition& Definition = Source.GetDefinition();

    for( s32 Run = 0; Run < RunCount; Run++ )
    {
        s32            RunStart, RunLength;
        UBiDiDirection Direction = ubidi_getVisualRun( Line.Value, Run, &RunStart, &RunLength );
        RunStart += Start;

        std::vector<Segment> Visual;

        for( const Segment& S : Segments )
        {
            if( S.Start < RunStart + RunLength && S.End > RunStart )
            {
                Visual.push_back(
                    { MAX( S.Start, RunStart ), MIN( S.End, RunStart + RunLength ), S.Face, S.Script, S.Style } );
            }
        }

        if( Direction == UBIDI_RTL )
        {
            std::reverse( Visual.begin(), Visual.end() );
        }

        for( const Segment& S : Visual )
        {
            if( Text[S.Start] == '\t' )
            {
                hb_font_t* pFont = Faces[S.Face]->GetData()->CreateShapeFont( Definition.Size );

                f32 Space = hb_font_get_glyph_h_advance( pFont, Faces[S.Face]->GetGlyphIndex( ' ' ) ) / 64.0f;
                f32 TabWidth = MAX( Space + Definition.SpaceSpacing, 1.0f ) * Definition.WidthScale * 4.0f;
                Width = (x_floor( Width / TabWidth ) + 1.0f) * TabWidth;

                hb_font_destroy( pFont );
                continue;
            }

            if( S.Style >= 0 && Options.Runs[S.Style].InlineId >= 0 )
            {
                const TextStyleRun& Style = Options.Runs[S.Style];
                TextGlyph& Glyph = Glyphs.Append();

                Glyph.Cluster    = S.Start;
                Glyph.Line       = LineIndex;
                Glyph.InlineId   = Style.InlineId;
                Glyph.InlineSize = Style.InlineSize;
                Glyph.Position.Set( Width + Style.InlineSpacing,
                                    Baseline + InlineCenterOffset - Style.InlineSize.Y * 0.5f );

                Glyph.Advance = Style.InlineSpacing + Style.InlineSize.X;
                Glyph.Color   = Style.Color;
                Width += Glyph.Advance;
                continue;
            }

            hb_font_t* pShapeFont = Faces[S.Face]->GetData()->CreateShapeFont( Definition.Size );

            hb_buffer_t* pBuffer = hb_buffer_create();

            hb_buffer_set_direction( pBuffer, Direction == UBIDI_RTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR );
            hb_buffer_set_script( pBuffer, hb_script_from_string( uscript_getShortName( S.Script ), -1 ) );
            hb_buffer_set_language( pBuffer, hb_language_from_string( Options.Language, -1 ) );
            hb_buffer_add_utf16( pBuffer, (const u16*)Text, Length, S.Start, S.End - S.Start );

            hb_feature_t Kerning = { HB_TAG( 'k', 'e', 'r', 'n' ), Definition.Kerning ? 1u : 0u, 0, (unsigned)-1 };
            hb_shape( pShapeFont, pBuffer, &Kerning, 1 );

            unsigned Count;
            const hb_glyph_info_t* Infos = hb_buffer_get_glyph_infos( pBuffer, &Count );
            const hb_glyph_position_t* Positions = hb_buffer_get_glyph_positions( pBuffer, NULL );

            for( unsigned I = 0; I < Count; I++ )
            {
                TextGlyph& Glyph = Glyphs.Append();

                Glyph.Face       = Faces[S.Face];
                Glyph.GlyphIndex = Infos[I].codepoint;
                Glyph.Cluster    = Infos[I].cluster;
                Glyph.UnsafeToBreak =
                    ( hb_glyph_info_get_glyph_flags( &Infos[I] ) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK ) != 0;
                Glyph.Line = LineIndex;
                Glyph.Position.Set( Width + Positions[I].x_offset / 64.0f * Definition.WidthScale,
                                    Baseline - Positions[I].y_offset / 64.0f * Definition.HeightScale );
                Glyph.Advance = Positions[I].x_advance / 64.0f;

                s32 Style = FindStyle( Options, Glyph.Cluster );

                if( Style >= 0 )
                {
                    Glyph.Color = Options.Runs[Style].Color;
                }

                if( Text[Glyph.Cluster] == ' ' )
                {
                    Glyph.Advance += Definition.SpaceSpacing;
                }

                if( I + 1 < Count && Infos[I].cluster != Infos[I + 1].cluster &&
                    !( hb_glyph_info_get_glyph_flags( &Infos[I + 1] ) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK ) )
                {
                    Glyph.Advance += Definition.LetterSpacing;
                }

                Glyph.Advance *= Definition.WidthScale;
                Width += Glyph.Advance;
            }

            hb_buffer_destroy( pBuffer );
            hb_font_destroy( pShapeFont );
        }
    }

    return TRUE;
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool BuildTextLayout( Font& Source,
                       const xwchar* String,
                       const TextLayoutOptions& Options,
                       TextLayout& Result )
{
    Result.Clear();

    if( !String || !x_isvalid( Options.Width ) || Options.Direction < 0 || Options.Direction > 2 )
    {
        return FALSE;
    }

    s32 Length = x_wstrlen( String );
    const UChar* Text = (const UChar*)String;

    for( s32 I = 0; I < Length; )
    {
        UChar32 Code;
        U16_NEXT( Text, I, Length, Code );

        if( U_IS_SURROGATE( Code ) )
        {
            return FALSE;
        }
    }

    for( s32 I = 0; I < Options.Runs.GetCount(); I++ )
    {
        const TextStyleRun& Run = Options.Runs[I];

        if( Run.Start < 0 || Run.Length <= 0 || Run.Start > Length - Run.Length )
        {
            return FALSE;
        }

        if( I && Run.Start < Options.Runs[I - 1].Start + Options.Runs[I - 1].Length )
        {
            return FALSE;
        }

        if( Run.InlineId >= 0 &&
            (Run.Length != 1 || Text[Run.Start] != 0xfffc ||
             !x_isvalid( Run.InlineSize.X + Run.InlineSize.Y ) ||
             !x_isvalid( Run.InlineSpacing ) || Run.InlineSpacing < 0 ||
             Run.InlineSize.X <= 0 || Run.InlineSize.Y <= 0) )
        {
            return FALSE;
        }
    }

    Result.Owner = &Source;
    Result.Text  = String;

    FontMetrics Metrics = Source.GetMetrics();

    if( Metrics.LineHeight <= 0 )
    {
        return FALSE;
    }

    const f32 InlineCenterOffset = -( Metrics.Ascender + Metrics.Descender ) * 0.5f;

    UErrorCode    Error      = U_ZERO_ERROR;
    BreakIterator Breaks     = { ubrk_open( UBRK_LINE, Options.Language, Text, Length, &Error ) };
    BreakIterator Characters = { ubrk_open( UBRK_CHARACTER, Options.Language, Text, Length, &Error ) };

    if( U_FAILURE( Error ) )
    {
        return FALSE;
    }

    s32 ParagraphStart = 0;

    do
    {
        s32 ParagraphEnd = ParagraphStart;

        while( ParagraphEnd < Length && Text[ParagraphEnd] != '\n' && Text[ParagraphEnd] != '\r' )
        {
            ParagraphEnd++;
        }

        Error = U_ZERO_ERROR;

        BidiIterator Bidi = { ubidi_openSized( ParagraphEnd - ParagraphStart, 0, &Error ) };
        UBiDiLevel Level = UBIDI_DEFAULT_LTR;

        if( Options.Direction == 1 )
        {
            Level = UBIDI_LTR;
        }
        else if( Options.Direction == 2 )
        {
            Level = UBIDI_RTL;
        }

        ubidi_setPara( Bidi.Value, Text + ParagraphStart, ParagraphEnd - ParagraphStart, Level, NULL, &Error );

        if( U_FAILURE( Error ) )
        {
            return FALSE;
        }

        s32 Start = ParagraphStart;

        do
        {
            xarray<TextGlyph> Trial;
            s32 End      = ParagraphEnd;
            f32 Width    = 0.0f;
            f32 Baseline = Metrics.Ascender + Result.Lines.GetCount() * Metrics.LineHeight;

            if( !ShapeLine( Source, Text, Length, Start, End, ParagraphStart, Bidi.Value, Options, Baseline,
                            InlineCenterOffset, Result.Lines.GetCount(), Trial, Width ) )
            {
                return FALSE;
            }

            if( Options.Width > 0 && Width > Options.Width && Options.Ellipsis && !Options.Wrap )
            {
                // Re-shape each candidate so contextual forms match the final string.
                s32               Cut     = ubrk_preceding( Characters.Value, End );
                TextLayoutOptions Clipped = Options;
                Clipped.Ellipsis          = FALSE;

                while( Cut >= Start )
                {
                    xwstring Candidate;

                    for( s32 I = 0; I < Cut; I++ )
                    {
                        Candidate += String[I];
                    }

                    Candidate += (xwchar)0x2026;
                    Clipped.Runs.Clear();

                    for( s32 I = 0; I < Options.Runs.GetCount(); I++ )
                    {
                        TextStyleRun Run = Options.Runs[I];

                        if( Run.Start >= Cut )
                        {
                            continue;
                        }

                        Run.Length            = MIN( Run.Length, Cut - Run.Start );
                        Clipped.Runs.Append() = Run;
                    }

                    TextLayout CandidateLayout;

                    if( !BuildTextLayout( Source, Candidate, Clipped, CandidateLayout ) )
                    {
                        return FALSE;
                    }

                    if( CandidateLayout.Size.X <= Options.Width || Cut == Start )
                    {
                        Result = CandidateLayout;
                        return TRUE;
                    }

                    Cut = ubrk_preceding( Characters.Value, Cut );
                }
            }

            if( Options.Wrap && Options.Width > 0 && Width > Options.Width )
            {
                s32 Best = Start;

                for( s32 Candidate = ubrk_following( Breaks.Value, Start );
                     Candidate != UBRK_DONE && Candidate < ParagraphEnd;
                     Candidate = ubrk_following( Breaks.Value, Candidate ) )
                {
                    xarray<TextGlyph> Test;
                    f32 TestWidth;

                    if( !ShapeLine( Source, Text, Length, Start, Candidate, ParagraphStart, Bidi.Value, Options,
                                    Baseline, InlineCenterOffset, Result.Lines.GetCount(), Test, TestWidth ) )
                    {
                        return FALSE;
                    }

                    if( TestWidth > Options.Width )
                    {
                        break;
                    }

                    Best = Candidate;
                }

                if( Best == Start )
                {
                    for( s32 Candidate = ubrk_following( Characters.Value, Start );
                         Candidate != UBRK_DONE && Candidate <= ParagraphEnd;
                         Candidate = ubrk_following( Characters.Value, Candidate ) )
                    {
                        xbool Safe = Candidate == ParagraphEnd;

                        for( s32 I = 0; I < Trial.GetCount(); I++ )
                        {
                            if( Trial[I].Cluster == Candidate && !Trial[I].UnsafeToBreak )
                            {
                                Safe = TRUE;
                            }
                        }

                        if( !Safe )
                        {
                            continue;
                        }

                        xarray<TextGlyph> Test;
                        f32 TestWidth;

                        if( !ShapeLine( Source, Text, Length, Start, Candidate, ParagraphStart, Bidi.Value, Options,
                                        Baseline, InlineCenterOffset, Result.Lines.GetCount(), Test, TestWidth ) )
                        {
                            return FALSE;
                        }

                        if( TestWidth > Options.Width && Best > Start )
                        {
                            break;
                        }

                        Best = Candidate;

                        if( TestWidth > Options.Width )
                        {
                            break;
                        }
                    }
                }

                End = Best > Start ? Best : ParagraphEnd;
                Trial.Clear();

                if( !ShapeLine( Source, Text, Length, Start, End, ParagraphStart, Bidi.Value, Options, Baseline,
                                InlineCenterOffset, Result.Lines.GetCount(), Trial, Width ) )
                {
                    return FALSE;
                }
            }

            TextLine& Line  = Result.Lines.Append();

            Line.Start      = Start;
            Line.End        = End;
            Line.FirstGlyph = Result.Glyphs.GetCount();
            Line.GlyphCount = Trial.GetCount();
            Line.Width      = Width;
            Line.Baseline   = Baseline;

            for( s32 I = 0; I < Trial.GetCount(); I++ )
            {
                Result.Glyphs.Append() = Trial[I];
            }

            Result.Size.X = MAX( Result.Size.X, Width );
            Start         = End;
        }
        while( Start < ParagraphEnd );

        if( ParagraphEnd == Length )
        {
            break;
        }

        ParagraphStart = ParagraphEnd + 1;

        if( Text[ParagraphEnd] == '\r' && ParagraphStart < Length && Text[ParagraphStart] == '\n' )
        {
            ParagraphStart++;
        }
    }
    while( ParagraphStart <= Length );

    Result.Size.Y = Result.Lines.GetCount() * Metrics.LineHeight;
    return TRUE;
}
