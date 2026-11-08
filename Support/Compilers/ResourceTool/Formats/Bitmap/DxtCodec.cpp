//=========================================================================
//
//  DxtCodec.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "DxtCodec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace resource_tool::bitmap::dxt
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

u16 ReadU16( const u8* pData )
{
    return static_cast<u16>( pData[0] | ( static_cast<u16>( pData[1] ) << 8 ) );
}

//=========================================================================

void WriteU16( u8* pData, u16 Value )
{
    pData[0] = static_cast<u8>( Value & 0xff );
    pData[1] = static_cast<u8>( Value >> 8 );
}

//=========================================================================

u32 ReadU32( const u8* pData )
{
    return static_cast<u32>( pData[0] ) | ( static_cast<u32>( pData[1] ) << 8 ) |
           ( static_cast<u32>( pData[2] ) << 16 ) | ( static_cast<u32>( pData[3] ) << 24 );
}

//=========================================================================

void WriteU32( u8* pData, u32 Value )
{
    pData[0] = static_cast<u8>( Value & 0xff );
    pData[1] = static_cast<u8>( ( Value >> 8 ) & 0xff );
    pData[2] = static_cast<u8>( ( Value >> 16 ) & 0xff );
    pData[3] = static_cast<u8>( ( Value >> 24 ) & 0xff );
}

//=========================================================================

u16 Pack565( pixel Value )
{
    return static_cast<u16>( ( ( static_cast<u16>( Value.R ) * 31 + 127 ) / 255 ) << 11 |
                             ( ( static_cast<u16>( Value.G ) * 63 + 127 ) / 255 ) << 5 |
                             ( ( static_cast<u16>( Value.B ) * 31 + 127 ) / 255 ) );
}

//=========================================================================

pixel Unpack565( u16 Value )
{
    pixel Result;
    Result.R = static_cast<u8>( ( ( Value >> 11 ) & 31 ) * 255 / 31 );
    Result.G = static_cast<u8>( ( ( Value >> 5 ) & 63 ) * 255 / 63 );
    Result.B = static_cast<u8>( ( Value & 31 ) * 255 / 31 );
    Result.A = 255;
    return Result;
}

//=========================================================================

std::array<pixel, 4> MakeColorPalette( u16 Color0, u16 Color1, xbool bThreeColor )
{
    std::array<pixel, 4> Colors;
    Colors[0] = Unpack565( Color0 );
    Colors[1] = Unpack565( Color1 );
    if( bThreeColor )
    {
        Colors[2].R = static_cast<u8>( ( Colors[0].R + Colors[1].R ) / 2 );
        Colors[2].G = static_cast<u8>( ( Colors[0].G + Colors[1].G ) / 2 );
        Colors[2].B = static_cast<u8>( ( Colors[0].B + Colors[1].B ) / 2 );
        Colors[2].A = 255;
        Colors[3] = { 0, 0, 0, 0 };
    }
    else
    {
        Colors[2].R = static_cast<u8>( ( 2 * Colors[0].R + Colors[1].R + 1 ) / 3 );
        Colors[2].G = static_cast<u8>( ( 2 * Colors[0].G + Colors[1].G + 1 ) / 3 );
        Colors[2].B = static_cast<u8>( ( 2 * Colors[0].B + Colors[1].B + 1 ) / 3 );
        Colors[2].A = 255;
        Colors[3].R = static_cast<u8>( ( Colors[0].R + 2 * Colors[1].R + 1 ) / 3 );
        Colors[3].G = static_cast<u8>( ( Colors[0].G + 2 * Colors[1].G + 1 ) / 3 );
        Colors[3].B = static_cast<u8>( ( Colors[0].B + 2 * Colors[1].B + 1 ) / 3 );
        Colors[3].A = 255;
    }
    return Colors;
}

//=========================================================================

s32 ColorDistance( pixel A, pixel B, xbool bIgnoreAlpha )
{
    const s32 DR = static_cast<s32>( A.R ) - B.R;
    const s32 DG = static_cast<s32>( A.G ) - B.G;
    const s32 DB = static_cast<s32>( A.B ) - B.B;
    const s32 DA = bIgnoreAlpha ? 0 : static_cast<s32>( A.A ) - B.A;
    return DR * DR + DG * DG + DB * DB + DA * DA;
}

//=========================================================================

void GatherBlock( const mip_level& Source, u32 BlockX, u32 BlockY, std::array<pixel, 16>& Pixels )
{
    for( u32 Y = 0; Y < 4; ++Y )
    {
        for( u32 X = 0; X < 4; ++X )
        {
            const u32 SX = x_min( Source.Width - 1, BlockX * 4 + X );
            const u32 SY = x_min( Source.Height - 1, BlockY * 4 + Y );
            const usize Offset = ( static_cast<usize>( SY ) * Source.Width + SX ) * 4;
            Pixels[Y * 4 + X] = { Source.Pixels[Offset + 0], Source.Pixels[Offset + 1], Source.Pixels[Offset + 2],
                                  Source.Pixels[Offset + 3] };
        }
    }
}

//=========================================================================

void FindEndpoints( const std::array<pixel, 16>& Pixels, u16& Color0, u16& Color1 )
{
    pixel Minimum = Pixels[0];
    pixel Maximum = Pixels[0];
    for( pixel Value : Pixels )
    {
        Minimum.R = x_min( Minimum.R, Value.R );
        Minimum.G = x_min( Minimum.G, Value.G );
        Minimum.B = x_min( Minimum.B, Value.B );
        Maximum.R = x_max( Maximum.R, Value.R );
        Maximum.G = x_max( Maximum.G, Value.G );
        Maximum.B = x_max( Maximum.B, Value.B );
    }
    Color0 = Pack565( Maximum );
    Color1 = Pack565( Minimum );
    if( Color0 == Color1 )
    {
        if( Color0 < 0xffff )
            ++Color0;
        else
            --Color1;
    }
}

//=========================================================================

u32 MakeColorIndices( const std::array<pixel, 16>& Pixels, u16 Color0, u16 Color1, xbool bThreeColor,
                      xbool bPunchThrough, u8 AlphaThreshold )
{
    const std::array<pixel, 4> Palette = MakeColorPalette( Color0, Color1, bThreeColor );
    u32 Indices = 0;
    for( usize Index = 0; Index < Pixels.size(); ++Index )
    {
        s32 BestIndex = 0;
        if( bPunchThrough && Pixels[Index].A < AlphaThreshold )
        {
            BestIndex = 3;
        }
        else
        {
            const s32 PaletteCount = bThreeColor ? 3 : 4;
            s32 BestDistance = std::numeric_limits<s32>::max();
            for( s32 Candidate = 0; Candidate < PaletteCount; ++Candidate )
            {
                const s32 Distance = ColorDistance( Pixels[Index], Palette[Candidate], TRUE );
                if( Distance < BestDistance )
                {
                    BestDistance = Distance;
                    BestIndex = Candidate;
                }
            }
        }
        Indices |= static_cast<u32>( BestIndex ) << ( Index * 2 );
    }
    return Indices;
}

//=========================================================================

void EncodeColorBlock( const std::array<pixel, 16>& Pixels, xbool bPunchThrough, u8 AlphaThreshold, u8* pOutput )
{
    u16 Color0 = 0;
    u16 Color1 = 0;
    FindEndpoints( Pixels, Color0, Color1 );
    if( bPunchThrough )
    {
        if( Color0 > Color1 )
            std::swap( Color0, Color1 );
    }
    else if( Color0 < Color1 )
    {
        std::swap( Color0, Color1 );
    }
    const xbool bThreeColor = bPunchThrough;
    WriteU16( pOutput + 0, Color0 );
    WriteU16( pOutput + 2, Color1 );
    WriteU32( pOutput + 4, MakeColorIndices( Pixels, Color0, Color1, bThreeColor, bPunchThrough, AlphaThreshold ) );
}

//=========================================================================

void EncodeDxt3Alpha( const std::array<pixel, 16>& Pixels, u8* pOutput )
{
    u64 Alpha = 0;
    for( usize Index = 0; Index < Pixels.size(); ++Index )
    {
        const u64 Value = ( static_cast<u64>( Pixels[Index].A ) * 15 + 127 ) / 255;
        Alpha |= Value << ( Index * 4 );
    }
    for( s32 Byte = 0; Byte < 8; ++Byte )
        pOutput[Byte] = static_cast<u8>( ( Alpha >> ( Byte * 8 ) ) & 0xff );
}

//=========================================================================

std::array<u8, 8> MakeAlphaPalette( u8 Alpha0, u8 Alpha1 )
{
    std::array<u8, 8> Palette{};
    Palette[0] = Alpha0;
    Palette[1] = Alpha1;
    if( Alpha0 > Alpha1 )
    {
        for( s32 Index = 1; Index < 7; ++Index )
            Palette[Index + 1] = static_cast<u8>( ( ( 7 - Index ) * Alpha0 + Index * Alpha1 + 3 ) / 7 );
    }
    else
    {
        for( s32 Index = 1; Index < 5; ++Index )
            Palette[Index + 1] = static_cast<u8>( ( ( 5 - Index ) * Alpha0 + Index * Alpha1 + 2 ) / 5 );
        Palette[6] = 0;
        Palette[7] = 255;
    }
    return Palette;
}

//=========================================================================

void EncodeDxt5Alpha( const std::array<pixel, 16>& Pixels, u8* pOutput )
{
    u8 Minimum = 255;
    u8 Maximum = 0;
    for( pixel Value : Pixels )
    {
        Minimum = x_min( Minimum, Value.A );
        Maximum = x_max( Maximum, Value.A );
    }
    u8 Alpha0 = Maximum;
    u8 Alpha1 = Minimum;
    if( Alpha0 == Alpha1 )
    {
        if( Alpha0 < 255 )
            Alpha0 = static_cast<u8>( Alpha0 + 1 );
        else
            Alpha1 = static_cast<u8>( Alpha1 - 1 );
    }
    pOutput[0] = Alpha0;
    pOutput[1] = Alpha1;
    const std::array<u8, 8> Palette = MakeAlphaPalette( Alpha0, Alpha1 );
    u64 Indices = 0;
    for( usize PixelIndex = 0; PixelIndex < Pixels.size(); ++PixelIndex )
    {
        s32 BestIndex = 0;
        s32 BestDistance = std::numeric_limits<s32>::max();
        for( s32 Candidate = 0; Candidate < 8; ++Candidate )
        {
            const s32 Distance = std::abs( static_cast<s32>( Pixels[PixelIndex].A ) - Palette[Candidate] );
            if( Distance < BestDistance )
            {
                BestDistance = Distance;
                BestIndex = Candidate;
            }
        }
        Indices |= static_cast<u64>( BestIndex ) << ( PixelIndex * 3 );
    }
    for( s32 Byte = 0; Byte < 6; ++Byte )
        pOutput[2 + Byte] = static_cast<u8>( ( Indices >> ( Byte * 8 ) ) & 0xff );
}

//=========================================================================

void DecodeColorBlock( const u8* pInput, xbool bThreeColor, pixel* pPixels )
{
    const u16 Color0 = ReadU16( pInput + 0 );
    const u16 Color1 = ReadU16( pInput + 2 );
    const std::array<pixel, 4> Palette = MakeColorPalette( Color0, Color1, bThreeColor );
    u32 Indices = ReadU32( pInput + 4 );
    for( s32 Index = 0; Index < 16; ++Index )
    {
        pPixels[Index] = Palette[Indices & 3];
        Indices >>= 2;
    }
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

usize CompressedSize( u32 Width, u32 Height, format Format )
{
    const usize BlockBytes = Format == format::DXT1 ? 8 : 16;
    const usize BlocksX = ( static_cast<usize>( Width ) + 3 ) / 4;
    const usize BlocksY = ( static_cast<usize>( Height ) + 3 ) / 4;
    return BlocksX * BlocksY * BlockBytes;
}

//=========================================================================

xbool EncodeMip( const mip_level& Source, format Format, alpha_mode Alpha, u8 AlphaThreshold, std::vector<u8>& Output,
                 std::string& Error )
{
    if( Source.Width == 0 || Source.Height == 0 ||
        Source.Pixels.size() != static_cast<usize>( Source.Width ) * Source.Height * 4 )
    {
        Error = "Unable to encode invalid bitmap mip";
        return FALSE;
    }
    try
    {
        Output.assign( CompressedSize( Source.Width, Source.Height, Format ), 0 );
    }
    catch( ... )
    {
        Error = "Unable to allocate DXT mip payload";
        return FALSE;
    }

    const u32 BlocksX = ( Source.Width + 3 ) / 4;
    const u32 BlocksY = ( Source.Height + 3 ) / 4;
    const usize BlockBytes = Format == format::DXT1 ? 8 : 16;
    const xbool bPunchThrough = Format == format::DXT1 && Alpha == alpha_mode::Mask;
    for( u32 BlockY = 0; BlockY < BlocksY; ++BlockY )
    {
        for( u32 BlockX = 0; BlockX < BlocksX; ++BlockX )
        {
            std::array<pixel, 16> Pixels;
            GatherBlock( Source, BlockX, BlockY, Pixels );
            u8* pBlock = Output.data() + ( static_cast<usize>( BlockY ) * BlocksX + BlockX ) * BlockBytes;
            if( Format == format::DXT3 )
            {
                EncodeDxt3Alpha( Pixels, pBlock );
                EncodeColorBlock( Pixels, FALSE, AlphaThreshold, pBlock + 8 );
            }
            else if( Format == format::DXT5 )
            {
                EncodeDxt5Alpha( Pixels, pBlock );
                EncodeColorBlock( Pixels, FALSE, AlphaThreshold, pBlock + 8 );
            }
            else
            {
                EncodeColorBlock( Pixels, bPunchThrough, AlphaThreshold, pBlock );
            }
        }
    }
    return TRUE;
}

//=========================================================================

xbool DecodeMip( u32 Width, u32 Height, format Format, const std::vector<u8>& Input, mip_level& Output,
                 std::string& Error )
{
    const usize ExpectedSize = CompressedSize( Width, Height, Format );
    if( Width == 0 || Height == 0 || Input.size() != ExpectedSize )
    {
        Error = "Invalid DXT mip dimensions or payload size";
        return FALSE;
    }
    Output.Width = Width;
    Output.Height = Height;
    Output.Pixels.assign( static_cast<usize>( Width ) * Height * 4, 0 );
    const u32 BlocksX = ( Width + 3 ) / 4;
    const u32 BlocksY = ( Height + 3 ) / 4;
    const usize BlockBytes = Format == format::DXT1 ? 8 : 16;
    for( u32 BlockY = 0; BlockY < BlocksY; ++BlockY )
    {
        for( u32 BlockX = 0; BlockX < BlocksX; ++BlockX )
        {
            const u8* pBlock = Input.data() + ( static_cast<usize>( BlockY ) * BlocksX + BlockX ) * BlockBytes;
            std::array<pixel, 16> Pixels{};
            const xbool bThreeColor = Format == format::DXT1 && ReadU16( pBlock ) <= ReadU16( pBlock + 2 );
            if( Format == format::DXT3 )
            {
                DecodeColorBlock( pBlock + 8, FALSE, Pixels.data() );
                u64 Alpha = 0;
                for( s32 Byte = 0; Byte < 8; ++Byte )
                    Alpha |= static_cast<u64>( pBlock[Byte] ) << ( Byte * 8 );
                for( s32 Index = 0; Index < 16; ++Index )
                {
                    const u8 Value = static_cast<u8>( Alpha & 0xf );
                    Pixels[Index].A = static_cast<u8>( Value * 17 );
                    Alpha >>= 4;
                }
            }
            else if( Format == format::DXT5 )
            {
                DecodeColorBlock( pBlock + 8, FALSE, Pixels.data() );
                const std::array<u8, 8> Palette = MakeAlphaPalette( pBlock[0], pBlock[1] );
                u64 Indices = 0;
                for( s32 Byte = 0; Byte < 6; ++Byte )
                    Indices |= static_cast<u64>( pBlock[2 + Byte] ) << ( Byte * 8 );
                for( s32 Index = 0; Index < 16; ++Index )
                {
                    Pixels[Index].A = Palette[Indices & 7];
                    Indices >>= 3;
                }
            }
            else
            {
                DecodeColorBlock( pBlock, bThreeColor, Pixels.data() );
            }

            for( u32 Y = 0; Y < 4; ++Y )
            {
                for( u32 X = 0; X < 4; ++X )
                {
                    const u32 DX = BlockX * 4 + X;
                    const u32 DY = BlockY * 4 + Y;
                    if( DX >= Width || DY >= Height )
                        continue;
                    const usize Offset = ( static_cast<usize>( DY ) * Width + DX ) * 4;
                    const pixel Value = Pixels[Y * 4 + X];
                    Output.Pixels[Offset + 0] = Value.R;
                    Output.Pixels[Offset + 1] = Value.G;
                    Output.Pixels[Offset + 2] = Value.B;
                    Output.Pixels[Offset + 3] = Value.A;
                }
            }
        }
    }
    return TRUE;
}

} // namespace resource_tool::bitmap::dxt
