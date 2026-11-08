//=========================================================================
//
//  GlyphAtlas.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GlyphAtlas.hpp"
#include "e_Engine.hpp"

#include <map>
#include <tuple>
#include "x_math.hpp"

//=========================================================================
//  STRUCTURES
//=========================================================================

struct GlyphAtlas::Data
{
    enum
    {
        PageSize = 1024,
        MaxPages = 16
    };

    typedef std::tuple<FontFace*, u32, s32, s32, s32, s32, s32, s32> Key;

    //-------------------------------------------------------------------------

    struct Page
    {
        vram_texture Texture;
        xarray<u8>   Pixels;
        s32          X         = 1;
        s32          Y         = 1;
        s32          RowHeight = 0;
        u64          LastFrame = 0;
        irect        Dirty     = irect( 0, 0, 0, 0 );
    };

    //-------------------------------------------------------------------------

    struct Entry
    {
        xref<FontFace>  Face;
        s32            PageIndex = -1;
        s32            X         = 0;
        s32            Y         = 0;
        s32            Width     = 0;
        s32            Height    = 0;
        s32            Left      = 0;
        s32            Top       = 0;
    };

    //-------------------------------------------------------------------------

    xarray<Page*>         Pages;
    std::map<Key, Entry>  Entries;
    xarray<u8>            UploadPixels;
    u64                  Serial      = ~u64( 0 );
    u64                  RasterCount = 0;
};

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

GlyphAtlas::GlyphAtlas( void ) :
    m_pData( new Data )
{
}

//=========================================================================

GlyphAtlas::~GlyphAtlas( void )
{
    Kill();

    delete m_pData;
}

//=========================================================================

void GlyphAtlas::BeginFrame( u64 Serial )
{
    m_pData->Serial = Serial;
}

//=========================================================================

xbool GlyphAtlas::RequestGlyph( const TextGlyph& Glyph,
                                const FontDefinition& Definition,
                                f32 Scale,
                                f32 Outline,
                                f32 Blur,
                                GlyphImage& Image )
{
    Image = GlyphImage();

    if( !Glyph.Face.Get() || !x_isvalid( Scale ) || Scale <= 0 )
    {
        return FALSE;
    }

    if( !x_isvalid( Definition.WidthScale + Definition.HeightScale ) ||
        Definition.WidthScale <= 0 || Definition.HeightScale <= 0 ||
        Definition.Size * Scale > 4096.0f ||
        Definition.Size * Definition.WidthScale * Scale > 4096.0f ||
        Definition.Size * Definition.HeightScale * Scale > 4096.0f ||
        !x_isvalid( Outline + Blur ) || Outline < 0 || Blur < 0 ||
        Outline * Scale > 1024.0f || Blur * Scale > 1024.0f )
    {
        return FALSE;
    }

    u32 PixelSize   = (u32)MAX( 1.0f, x_floor( Definition.Size * Scale + 0.5f ) );

    if( PixelSize * Definition.WidthScale > 4096.0f || PixelSize * Definition.HeightScale > 4096.0f )
    {
        return FALSE;
    }

    s32 PixelWidth  = (s32)MAX( 1.0f, x_floor( PixelSize * Definition.WidthScale * 64.0f + 0.5f ) );
    s32 PixelHeight = (s32)MAX( 1.0f, x_floor( PixelSize * Definition.HeightScale * 64.0f + 0.5f ) );
    f32 RasterScale = PixelSize / Definition.Size;
    s32 OutlineSize = (s32)x_floor( Outline * RasterScale * 64.0f + 0.5f );
    s32 BlurSize    = (s32)x_floor( Blur * RasterScale * 64.0f + 0.5f );

    Data::Key Key( Glyph.Face.Get(),
                   Glyph.GlyphIndex,
                   PixelWidth,
                   PixelHeight,
                   (s32)Definition.Hinting,
                   Definition.Antialias,
                   OutlineSize,
                   BlurSize );

    auto Found = m_pData->Entries.find( Key );

    if( Found == m_pData->Entries.end() )
    {
        GlyphBitmap Bitmap;

        if( !Glyph.Face->Rasterize( Glyph.GlyphIndex,
                                    PixelSize,
                                    Definition,
                                    OutlineSize / 64.0f,
                                    BlurSize / 64.0f,
                                    Bitmap ) )
        {
            return FALSE;
        }

        m_pData->RasterCount++;

        Data::Entry Entry;

        Entry.Face   = Glyph.Face;
        Entry.Width  = Bitmap.Width;
        Entry.Height = Bitmap.Height;
        Entry.Left   = Bitmap.Left;
        Entry.Top    = Bitmap.Top;

        if( Bitmap.Width && Bitmap.Height )
        {
            s32 Chosen = -1;

            for( s32 I = 0; I < m_pData->Pages.GetCount(); I++ )
            {
                const Data::Page& Page = *m_pData->Pages[I];
                s32 Y = Page.Y;

                if( Page.X + Bitmap.Width + 1 > Data::PageSize )
                {
                    Y += Page.RowHeight + 1;
                }

                if( Y + Bitmap.Height + 1 <= Data::PageSize )
                {
                    Chosen = I;
                    break;
                }
            }

            if( Chosen < 0 && m_pData->Pages.GetCount() < Data::MaxPages )
            {
                Data::Page* pPage = new Data::Page;

                vram_texture_desc Desc;

                Desc.Width      = Data::PageSize;
                Desc.Height     = Data::PageSize;
                Desc.Format     = VRAM_TEXTURE_FORMAT_R8;
                Desc.pDebugName = "GlyphAtlas";

                if( !vram_CreateTexture( pPage->Texture, Desc ) )
                {
                    delete pPage;
                    return FALSE;
                }

                pPage->Pixels.SetCount( Data::PageSize * Data::PageSize );
                x_memset( pPage->Pixels.GetPtr(), 0, pPage->Pixels.GetCount() );
                pPage->Dirty.Set( 0, 0, Data::PageSize, Data::PageSize );

                Chosen = m_pData->Pages.GetCount();
                m_pData->Pages.Append() = pPage;
            }

            if( Chosen < 0 )
            {
                u64 Oldest = ~u64( 0 );

                for( s32 I = 0; I < m_pData->Pages.GetCount(); I++ )
                {
                    const Data::Page& Page = *m_pData->Pages[I];

                    if( Page.LastFrame != m_pData->Serial && Page.LastFrame < Oldest &&
                        eng_IsFrameComplete( Page.LastFrame ) )
                    {
                        Chosen = I;
                        Oldest = Page.LastFrame;
                    }
                }

                if( Chosen < 0 )
                {
                    return FALSE;
                }

                for( auto I = m_pData->Entries.begin(); I != m_pData->Entries.end(); )
                {
                    if( I->second.PageIndex == Chosen )
                    {
                        I = m_pData->Entries.erase( I );
                    }
                    else
                    {
                        ++I;
                    }
                }

                Data::Page& Page = *m_pData->Pages[Chosen];

                Page.X         = 1;
                Page.Y         = 1;
                Page.RowHeight = 0;

                x_memset( Page.Pixels.GetPtr(), 0, Page.Pixels.GetCount() );
                Page.Dirty.Set( 0, 0, Data::PageSize, Data::PageSize );
            }

            Data::Page& Page = *m_pData->Pages[Chosen];

            if( Page.X + Bitmap.Width + 1 > Data::PageSize )
            {
                Page.X         = 1;
                Page.Y        += Page.RowHeight + 1;
                Page.RowHeight = 0;
            }

            Entry.PageIndex = Chosen;
            Entry.X         = Page.X;
            Entry.Y         = Page.Y;

            for( s32 Y = 0; Y < Bitmap.Height; Y++ )
            {
                x_memcpy( Page.Pixels.GetPtr() + (Page.Y + Y) * Data::PageSize + Page.X,
                          Bitmap.Pixels.GetPtr() + Y * Bitmap.Width, Bitmap.Width );
            }

            if( Page.Dirty.l >= Page.Dirty.r || Page.Dirty.t >= Page.Dirty.b )
            {
                Page.Dirty.Set( Page.X - 1, Page.Y - 1, Page.X + Bitmap.Width + 1, Page.Y + Bitmap.Height + 1 );
            }
            else
            {
                Page.Dirty.l = MIN( Page.Dirty.l, Page.X - 1 );
                Page.Dirty.t = MIN( Page.Dirty.t, Page.Y - 1 );
                Page.Dirty.r = MAX( Page.Dirty.r, Page.X + Bitmap.Width + 1 );
                Page.Dirty.b = MAX( Page.Dirty.b, Page.Y + Bitmap.Height + 1 );
            }

            Page.X        += Bitmap.Width + 1;
            Page.RowHeight = MAX( Page.RowHeight, Bitmap.Height );
        }

        Found = m_pData->Entries.emplace( Key, Entry ).first;
    }

    const Data::Entry& Entry = Found->second;

    if( Entry.PageIndex < 0 )
    {
        return TRUE;
    }

    Data::Page& Page = *m_pData->Pages[Entry.PageIndex];

    Page.LastFrame = m_pData->Serial;
    Image.Texture  = *vram_GetShaderResource( Page.Texture );

    Image.Offset.Set( Entry.Left / RasterScale, -Entry.Top / RasterScale );
    Image.Size.Set( Entry.Width / RasterScale, Entry.Height / RasterScale );
    Image.UV0.Set( (f32)Entry.X / Data::PageSize, (f32)Entry.Y / Data::PageSize );
    Image.UV1.Set( (f32)(Entry.X + Entry.Width) / Data::PageSize, (f32)(Entry.Y + Entry.Height) / Data::PageSize );

    return TRUE;
}

//=========================================================================

xbool GlyphAtlas::Prepare( void )
{
    for( s32 I = 0; I < m_pData->Pages.GetCount(); I++ )
    {
        Data::Page& Page = *m_pData->Pages[I];

        if( Page.Dirty.l >= Page.Dirty.r || Page.Dirty.t >= Page.Dirty.b )
        {
            continue;
        }

        if( Page.Dirty.l < 0 || Page.Dirty.t < 0 ||
            Page.Dirty.r > Data::PageSize || Page.Dirty.b > Data::PageSize )
        {
            return FALSE;
        }

        vram_texture_upload_desc Upload;

        s32 Width  = Page.Dirty.GetWidth();
        s32 Height = Page.Dirty.GetHeight();

        m_pData->UploadPixels.SetCount( Width * Height );

        for( s32 Y = 0; Y < Height; Y++ )
        {
            x_memcpy( m_pData->UploadPixels.GetPtr() + Y * Width,
                      Page.Pixels.GetPtr() + (Page.Dirty.t + Y) * Data::PageSize + Page.Dirty.l,
                      Width );
        }

        Upload.Region.X      = Page.Dirty.l;
        Upload.Region.Y      = Page.Dirty.t;
        Upload.Region.Width  = Width;
        Upload.Region.Height = Height;
        Upload.RowPitch      = Width;
        Upload.pData         = m_pData->UploadPixels.GetPtr();
        Upload.Size          = Width * Height;

        if( !vram_UploadTexture( Page.Texture, Upload ) )
        {
            return FALSE;
        }

        Page.Dirty.Set( 0, 0, 0, 0 );
    }

    return TRUE;
}

//=========================================================================

void GlyphAtlas::Kill( void )
{
    for( s32 I = 0; I < m_pData->Pages.GetCount(); I++ )
    {
        vram_DestroyTexture( m_pData->Pages[I]->Texture );
        delete m_pData->Pages[I];
    }

    m_pData->Pages.Clear();
    m_pData->Entries.clear();
    m_pData->UploadPixels.Clear();
}

//=========================================================================

u32 GlyphAtlas::GetPageCount( void ) const
{
    return m_pData->Pages.GetCount();
}

//=========================================================================

u64 GlyphAtlas::GetRasterCount( void ) const
{
    return m_pData->RasterCount;
}
