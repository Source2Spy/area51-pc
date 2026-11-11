//=========================================================================
//
//  BitmapImage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapImage.hpp"

#include <algorithm>
#include <limits>

namespace resource_tool::bitmap
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool CheckedPixelBytes( u32 Width, u32 Height, usize& Bytes )
{
    constexpr usize BytesPerPixel = 4;
    if( Width == 0 || Height == 0 )
        return FALSE;
    const usize W = Width;
    const usize H = Height;
    if( W > std::numeric_limits<usize>::max() / H )
        return FALSE;
    const usize Pixels = W * H;
    if( Pixels > std::numeric_limits<usize>::max() / BytesPerPixel )
        return FALSE;
    Bytes = Pixels * BytesPerPixel;
    return TRUE;
}

//=========================================================================

u8 Average4( u32 Sum, u32 Count )
{
    return static_cast<u8>( ( Sum + Count / 2 ) / Count );
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

usize mip_level::PixelCount( void ) const
{
    return static_cast<usize>( Width ) * Height;
}

//=========================================================================

xbool image::Create( u32 Width, u32 Height, std::string& Error )
{
    Clear();
    return AddMip( Width, Height, Error );
}

//=========================================================================

xbool image::AddMip( u32 Width, u32 Height, std::string& Error )
{
    usize Bytes = 0;
    if( !CheckedPixelBytes( Width, Height, Bytes ) )
    {
        Error = "Bitmap mip dimensions overflow or contain zero";
        return FALSE;
    }

    mip_level Mip;
    Mip.Width = Width;
    Mip.Height = Height;
    try
    {
        Mip.Pixels.assign( Bytes, 0 );
    }
    catch( ... )
    {
        Error = "Unable to allocate bitmap mip pixels";
        return FALSE;
    }
    try
    {
        m_Mips.push_back( std::move( Mip ) );
    }
    catch( ... )
    {
        Error = "Unable to append bitmap mip level";
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

void image::Clear( void )
{
    m_Mips.clear();
    ColorSpace = color_space::SRGB;
    AlphaMode = alpha_mode::Straight;
}

//=========================================================================

xbool image::Validate( std::string& Error ) const
{
    if( m_Mips.empty() )
    {
        Error = "Bitmap image has no mip levels";
        return FALSE;
    }

    u32 ExpectedWidth = m_Mips.front().Width;
    u32 ExpectedHeight = m_Mips.front().Height;
    for( usize Index = 0; Index < m_Mips.size(); ++Index )
    {
        const mip_level& Mip = m_Mips[Index];
        usize ExpectedBytes = 0;
        if( !CheckedPixelBytes( Mip.Width, Mip.Height, ExpectedBytes ) )
        {
            Error = "Bitmap mip dimensions are invalid";
            return FALSE;
        }
        if( Mip.Pixels.size() != ExpectedBytes )
        {
            Error = "Bitmap mip pixel payload has an invalid size";
            return FALSE;
        }
        if( Index > 0 )
        {
            const u32 MinimumWidth = x_max<u32>( 1, ExpectedWidth / 2 );
            const u32 MinimumHeight = x_max<u32>( 1, ExpectedHeight / 2 );
            if( Mip.Width != MinimumWidth || Mip.Height != MinimumHeight )
            {
                Error = "Bitmap mip chain is not a complete halving chain";
                return FALSE;
            }
        }
        ExpectedWidth = Mip.Width;
        ExpectedHeight = Mip.Height;
    }
    return TRUE;
}

//=========================================================================

xbool image::HasAlpha( void ) const
{
    if( AlphaMode == alpha_mode::Opaque )
        return FALSE;
    for( const mip_level& Mip : m_Mips )
    {
        for( usize Offset = 3; Offset < Mip.Pixels.size(); Offset += 4 )
        {
            if( Mip.Pixels[Offset] != 255 )
                return TRUE;
        }
    }
    return FALSE;
}

//=========================================================================

u32 image::Width( usize Mip ) const
{
    return m_Mips.at( Mip ).Width;
}

//=========================================================================

u32 image::Height( usize Mip ) const
{
    return m_Mips.at( Mip ).Height;
}

//=========================================================================

usize image::MipCount( void ) const
{
    return m_Mips.size();
}

//=========================================================================

pixel image::GetPixel( u32 X, u32 Y, usize Mip ) const
{
    const mip_level& Level = m_Mips.at( Mip );
    if( X >= Level.Width || Y >= Level.Height )
        return pixel();
    const usize Offset = ( static_cast<usize>( Y ) * Level.Width + X ) * 4;
    return { Level.Pixels[Offset + 0], Level.Pixels[Offset + 1], Level.Pixels[Offset + 2], Level.Pixels[Offset + 3] };
}

//=========================================================================

xbool image::SetPixel( u32 X, u32 Y, pixel Value, usize Mip )
{
    mip_level& Level = m_Mips.at( Mip );
    if( X >= Level.Width || Y >= Level.Height )
        return FALSE;
    const usize Offset = ( static_cast<usize>( Y ) * Level.Width + X ) * 4;
    Level.Pixels[Offset + 0] = Value.R;
    Level.Pixels[Offset + 1] = Value.G;
    Level.Pixels[Offset + 2] = Value.B;
    Level.Pixels[Offset + 3] = Value.A;
    return TRUE;
}

//=========================================================================

const mip_level& image::Mip( usize Index ) const
{
    return m_Mips.at( Index );
}

//=========================================================================

mip_level& image::Mip( usize Index )
{
    return m_Mips.at( Index );
}

//=========================================================================

const std::vector<mip_level>& image::Mips( void ) const
{
    return m_Mips;
}

//=========================================================================

std::vector<mip_level>& image::Mips( void )
{
    return m_Mips;
}

//=========================================================================

xbool BuildMipChain( const image& Source, usize DesiredMipCount, image& Result, std::string& Error )
{
    if( !Source.Validate( Error ) )
        return FALSE;
    if( DesiredMipCount == 0 )
    {
        Error = "Requested mip chain must contain at least one level";
        return FALSE;
    }

    Result.Clear();
    try
    {
        Result.Mips().reserve( DesiredMipCount );
    }
    catch( ... )
    {
        Error = "Unable to reserve bitmap mip levels";
        return FALSE;
    }
    Result.ColorSpace = Source.ColorSpace;
    Result.AlphaMode = Source.AlphaMode;
    const mip_level& Base = Source.Mip( 0 );
    if( !Result.Create( Base.Width, Base.Height, Error ) )
        return FALSE;
    Result.Mip( 0 ).Pixels = Base.Pixels;

    for( usize Index = 1; Index < Source.MipCount() && Index < DesiredMipCount; ++Index )
    {
        if( !Result.AddMip( Source.Width( Index ), Source.Height( Index ), Error ) )
            return FALSE;
        Result.Mip( Index ).Pixels = Source.Mip( Index ).Pixels;
    }

    while( Result.MipCount() < DesiredMipCount )
    {
        const mip_level& Previous = Result.Mip( Result.MipCount() - 1 );
        const u32 NewWidth = x_max<u32>( 1, Previous.Width / 2 );
        const u32 NewHeight = x_max<u32>( 1, Previous.Height / 2 );
        if( !Result.AddMip( NewWidth, NewHeight, Error ) )
            return FALSE;
        mip_level& Next = Result.Mip( Result.MipCount() - 1 );
        for( u32 Y = 0; Y < NewHeight; ++Y )
        {
            for( u32 X = 0; X < NewWidth; ++X )
            {
                u32 R = 0;
                u32 G = 0;
                u32 B = 0;
                u32 A = 0;
                u32 Count = 0;
                for( u32 DY = 0; DY < 2; ++DY )
                {
                    for( u32 DX = 0; DX < 2; ++DX )
                    {
                        const u32 SX = x_min( Previous.Width - 1, X * 2 + DX );
                        const u32 SY = x_min( Previous.Height - 1, Y * 2 + DY );
                        const usize Offset = ( static_cast<usize>( SY ) * Previous.Width + SX ) * 4;
                        R += Previous.Pixels[Offset + 0];
                        G += Previous.Pixels[Offset + 1];
                        B += Previous.Pixels[Offset + 2];
                        A += Previous.Pixels[Offset + 3];
                        ++Count;
                    }
                }
                const usize Offset = ( static_cast<usize>( Y ) * NewWidth + X ) * 4;
                Next.Pixels[Offset + 0] = Average4( R, Count );
                Next.Pixels[Offset + 1] = Average4( G, Count );
                Next.Pixels[Offset + 2] = Average4( B, Count );
                Next.Pixels[Offset + 3] = Average4( A, Count );
            }
        }
    }
    return TRUE;
}

} // namespace resource_tool::bitmap
