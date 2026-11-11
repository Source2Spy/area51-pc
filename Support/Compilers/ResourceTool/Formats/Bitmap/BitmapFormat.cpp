//=========================================================================
//
//  BitmapFormat.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapFormat.hpp"

#include "DxtCodec.hpp"
#include "AstcCodec.hpp"
#include "Core/ResourceUtilities.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

namespace resource_tool::bitmap::format_io
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

using byte = u8;

xbool ReadFile( const std::filesystem::path& Path, std::vector<byte>& Bytes, std::string& Error )
{
    std::ifstream File( Path, std::ios::binary );
    if( !File )
    {
        Error = "Unable to open bitmap source " + Path.generic_string();
        return FALSE;
    }
    File.seekg( 0, std::ios::end );
    const std::streamoff Size = File.tellg();
    if( Size < 0 || static_cast<u64>( Size ) > std::numeric_limits<usize>::max() )
    {
        Error = "Unable to determine bitmap source size";
        return FALSE;
    }
    File.seekg( 0, std::ios::beg );
    Bytes.resize( static_cast<usize>( Size ) );
    if( !Bytes.empty() )
        File.read( reinterpret_cast<char*>( Bytes.data() ), static_cast<std::streamsize>( Bytes.size() ) );
    if( !File )
    {
        Error = "Unable to read bitmap source " + Path.generic_string();
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

u16 ReadU16( const byte* pData )
{
    return static_cast<u16>( pData[0] | ( static_cast<u16>( pData[1] ) << 8 ) );
}

//=========================================================================

u32 ReadU32( const byte* pData )
{
    return static_cast<u32>( pData[0] ) | ( static_cast<u32>( pData[1] ) << 8 ) |
           ( static_cast<u32>( pData[2] ) << 16 ) | ( static_cast<u32>( pData[3] ) << 24 );
}

//=========================================================================

s32 ReadS32( const byte* pData )
{
    return static_cast<s32>( ReadU32( pData ) );
}

//=========================================================================

void WriteU16( std::vector<byte>& Bytes, usize Offset, u16 Value )
{
    Bytes[Offset + 0] = static_cast<byte>( Value & 0xff );
    Bytes[Offset + 1] = static_cast<byte>( Value >> 8 );
}

//=========================================================================

void WriteU32( std::vector<byte>& Bytes, usize Offset, u32 Value )
{
    Bytes[Offset + 0] = static_cast<byte>( Value & 0xff );
    Bytes[Offset + 1] = static_cast<byte>( ( Value >> 8 ) & 0xff );
    Bytes[Offset + 2] = static_cast<byte>( ( Value >> 16 ) & 0xff );
    Bytes[Offset + 3] = static_cast<byte>( ( Value >> 24 ) & 0xff );
}

//=========================================================================

xbool IsConsoleFlags( u32 Flags )
{
    constexpr u32 PS2ClutSwizzled = 1u << 8;
    constexpr u32 NibblesFlipped = 1u << 9;
    constexpr u32 GcnDataSwizzled = 1u << 11;
    constexpr u32 XboxDataSwizzled = 1u << 12;
    constexpr u32 XboxPreRegistered = 1u << 13;
    return ( Flags & ( PS2ClutSwizzled | NibblesFlipped | GcnDataSwizzled | XboxDataSwizzled | XboxPreRegistered ) ) !=
           0;
}

//=========================================================================

xbool IsSupportedFormat( s32 Format )
{
    switch( Format )
    {
    case 1:  // RGBA8888
    case 2:  // RGBU8888
    case 3:  // ARGB8888
    case 4:  // URGB8888
    case 5:  // RGB888
    case 6:  // RGBA4444
    case 8:  // RGBA5551
    case 9:  // RGBU5551
    case 11: // URGB1555
    case 10: // ARGB1555
    case 12: // RGB565
    case 13: // BGRA8888
    case 14: // BGRU8888
    case 15: // ABGR8888
    case 16: // UBGR8888
    case 17: // BGR888
    case 18: // BGRA4444
    case 20: // BGRA5551
    case 21: // BGRU5551
    case 22: // ABGR1555
    case 23: // UBGR1555
    case 24: // BGR565
    case 7:  // ARGB4444
    case 19: // ABGR4444
    case 73: // DXT1
    case 75: // DXT3
    case 77: // DXT5
    case 78: // A8
        return TRUE;
    default:
    {
        u32 Width = 0;
        u32 Height = 0;
        return astc::BlockSize( Format, Width, Height );
    }
    }
}

//=========================================================================

xbool IsDxtFormat( s32 Format )
{
    return Format == 73 || Format == 75 || Format == 77;
}

//=========================================================================

usize RawBytesPerPixel( s32 Format )
{
    switch( Format )
    {
    case 1:  // RGBA8888
    case 2:  // RGBU8888
    case 3:  // ARGB8888
    case 4:  // URGB8888
    case 13: // BGRA8888
    case 14: // BGRU8888
    case 15: // ABGR8888
    case 16: // UBGR8888
        return 4;
    case 5:  // RGB888
    case 17: // BGR888
        return 3;
    case 6:  // RGBA4444
    case 7:  // ARGB4444
    case 8:  // RGBA5551
    case 9:  // RGBU5551
    case 10: // ARGB1555
    case 11: // URGB1555
    case 12: // RGB565
    case 18: // BGRA4444
    case 19: // ABGR4444
    case 20: // BGRA5551
    case 21: // BGRU5551
    case 22: // ABGR1555
    case 23: // UBGR1555
    case 24: // BGR565
        return 2;
    case 78: // A8
        return 1;
    default:
        return 0;
    }
}

//=========================================================================

xbool CheckedImageBytes( u32 Width, u32 Height, usize BytesPerPixel, usize& Result )
{
    if( Width == 0 || Height == 0 || BytesPerPixel == 0 )
        return FALSE;
    const usize W = Width;
    const usize H = Height;
    if( W > std::numeric_limits<usize>::max() / H )
        return FALSE;
    const usize Pixels = W * H;
    if( Pixels > std::numeric_limits<usize>::max() / BytesPerPixel )
        return FALSE;
    Result = Pixels * BytesPerPixel;
    return TRUE;
}

//=========================================================================

byte ExpandBits( u32 Value, u32 Bits )
{
    const u32 Maximum = ( 1u << Bits ) - 1u;
    return static_cast<byte>( ( Value * 255u + Maximum / 2u ) / Maximum );
}

//=========================================================================

pixel DecodePacked16( u16 Value, s32 Format )
{
    pixel Result;
    switch( Format )
    {
    case 6: // RGBA4444
        Result.R = ExpandBits( ( Value >> 12 ) & 0xf, 4 );
        Result.G = ExpandBits( ( Value >> 8 ) & 0xf, 4 );
        Result.B = ExpandBits( ( Value >> 4 ) & 0xf, 4 );
        Result.A = ExpandBits( Value & 0xf, 4 );
        break;
    case 7: // ARGB4444
        Result.R = ExpandBits( ( Value >> 8 ) & 0xf, 4 );
        Result.G = ExpandBits( ( Value >> 4 ) & 0xf, 4 );
        Result.B = ExpandBits( Value & 0xf, 4 );
        Result.A = ExpandBits( ( Value >> 12 ) & 0xf, 4 );
        break;
    case 10: // ARGB1555
        Result.R = ExpandBits( ( Value >> 10 ) & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 5 ) & 0x1f, 5 );
        Result.B = ExpandBits( Value & 0x1f, 5 );
        Result.A = ( Value & 0x8000 ) ? 255 : 0;
        break;
    case 8: // RGBA5551
        Result.R = ExpandBits( ( Value >> 11 ) & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 6 ) & 0x1f, 5 );
        Result.B = ExpandBits( ( Value >> 1 ) & 0x1f, 5 );
        Result.A = ( Value & 0x1 ) ? 255 : 0;
        break;
    case 9: // RGBU5551
        Result.R = ExpandBits( ( Value >> 11 ) & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 6 ) & 0x1f, 5 );
        Result.B = ExpandBits( ( Value >> 1 ) & 0x1f, 5 );
        Result.A = 255;
        break;
    case 11: // URGB1555
        Result.R = ExpandBits( ( Value >> 10 ) & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 5 ) & 0x1f, 5 );
        Result.B = ExpandBits( Value & 0x1f, 5 );
        Result.A = 255;
        break;
    case 12: // RGB565
        Result.R = ExpandBits( ( Value >> 11 ) & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 5 ) & 0x3f, 6 );
        Result.B = ExpandBits( Value & 0x1f, 5 );
        Result.A = 255;
        break;
    case 19: // ABGR4444
        Result.R = ExpandBits( Value & 0xf, 4 );
        Result.G = ExpandBits( ( Value >> 4 ) & 0xf, 4 );
        Result.B = ExpandBits( ( Value >> 8 ) & 0xf, 4 );
        Result.A = ExpandBits( ( Value >> 12 ) & 0xf, 4 );
        break;
    case 18: // BGRA4444
        Result.R = ExpandBits( ( Value >> 4 ) & 0xf, 4 );
        Result.G = ExpandBits( ( Value >> 8 ) & 0xf, 4 );
        Result.B = ExpandBits( ( Value >> 12 ) & 0xf, 4 );
        Result.A = ExpandBits( Value & 0xf, 4 );
        break;
    case 20: // BGRA5551
        Result.R = ExpandBits( ( Value >> 1 ) & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 6 ) & 0x1f, 5 );
        Result.B = ExpandBits( ( Value >> 11 ) & 0x1f, 5 );
        Result.A = ( Value & 0x1 ) ? 255 : 0;
        break;
    case 21: // BGRU5551
        Result.R = ExpandBits( ( Value >> 1 ) & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 6 ) & 0x1f, 5 );
        Result.B = ExpandBits( ( Value >> 11 ) & 0x1f, 5 );
        Result.A = 255;
        break;
    case 22: // ABGR1555
        Result.R = ExpandBits( Value & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 5 ) & 0x1f, 5 );
        Result.B = ExpandBits( ( Value >> 10 ) & 0x1f, 5 );
        Result.A = ( Value & 0x8000 ) ? 255 : 0;
        break;
    case 23: // UBGR1555
        Result.R = ExpandBits( Value & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 5 ) & 0x1f, 5 );
        Result.B = ExpandBits( ( Value >> 10 ) & 0x1f, 5 );
        Result.A = 255;
        break;
    case 24: // BGR565
        Result.R = ExpandBits( Value & 0x1f, 5 );
        Result.G = ExpandBits( ( Value >> 5 ) & 0x3f, 6 );
        Result.B = ExpandBits( ( Value >> 11 ) & 0x1f, 5 );
        Result.A = 255;
        break;
    default:
        break;
    }
    return Result;
}

//=========================================================================

xbool DecodeRawMip( s32 Format, u32 Width, u32 Height, const byte* pData, usize DataSize, mip_level& Result,
                    std::string& Error )
{
    const usize BytesPerPixel = RawBytesPerPixel( Format );
    usize ExpectedBytes = 0;
    if( !CheckedImageBytes( Width, Height, BytesPerPixel, ExpectedBytes ) || DataSize != ExpectedBytes )
    {
        Error = "Invalid uncompressed XBMP mip payload size";
        return FALSE;
    }
    Result.Width = Width;
    Result.Height = Height;
    Result.Pixels.assign( static_cast<usize>( Width ) * Height * 4, 0 );
    for( u32 Y = 0; Y < Height; ++Y )
    {
        for( u32 X = 0; X < Width; ++X )
        {
            const usize SourceOffset = ( static_cast<usize>( Y ) * Width + X ) * BytesPerPixel;
            pixel Value;
            if( BytesPerPixel == 4 )
            {
                switch( Format )
                {
                case 1:
                    Value = { pData[SourceOffset + 3], pData[SourceOffset + 2], pData[SourceOffset + 1],
                              pData[SourceOffset + 0] };
                    break;
                case 2:
                    Value = { pData[SourceOffset + 3], pData[SourceOffset + 2], pData[SourceOffset + 1], 255 };
                    break;
                case 3:
                    Value = { pData[SourceOffset + 2], pData[SourceOffset + 1], pData[SourceOffset + 0],
                              pData[SourceOffset + 3] };
                    break;
                case 4:
                    Value = { pData[SourceOffset + 2], pData[SourceOffset + 1], pData[SourceOffset + 0], 255 };
                    break;
                case 13:
                    Value = { pData[SourceOffset + 1], pData[SourceOffset + 2], pData[SourceOffset + 3],
                              pData[SourceOffset + 0] };
                    break;
                case 14:
                    Value = { pData[SourceOffset + 1], pData[SourceOffset + 2], pData[SourceOffset + 3], 255 };
                    break;
                case 15:
                    Value = { pData[SourceOffset + 0], pData[SourceOffset + 1], pData[SourceOffset + 2],
                              pData[SourceOffset + 3] };
                    break;
                case 16:
                    Value = { pData[SourceOffset + 0], pData[SourceOffset + 1], pData[SourceOffset + 2], 255 };
                    break;
                default:
                    break;
                }
            }
            else if( BytesPerPixel == 3 )
            {
                if( Format == 5 )
                    Value = { pData[SourceOffset + 2], pData[SourceOffset + 1], pData[SourceOffset + 0], 255 };
                else
                    Value = { pData[SourceOffset + 0], pData[SourceOffset + 1], pData[SourceOffset + 2], 255 };
            }
            else if( BytesPerPixel == 2 )
            {
                Value = DecodePacked16( ReadU16( pData + SourceOffset ), Format );
            }
            else
            {
                Value = { 255, 255, 255, pData[SourceOffset] };
            }
            const usize DestinationOffset = ( static_cast<usize>( Y ) * Width + X ) * 4;
            Result.Pixels[DestinationOffset + 0] = Value.R;
            Result.Pixels[DestinationOffset + 1] = Value.G;
            Result.Pixels[DestinationOffset + 2] = Value.B;
            Result.Pixels[DestinationOffset + 3] = Value.A;
        }
    }
    return TRUE;
}

//=========================================================================

xbool DecodeTga( const std::vector<byte>& Bytes, image& Result, std::string& Error )
{
    if( Bytes.size() < 18 )
    {
        Error = "TGA header is truncated";
        return FALSE;
    }
    const byte ImageType = Bytes[2];
    const u16 Width = ReadU16( Bytes.data() + 12 );
    const u16 Height = ReadU16( Bytes.data() + 14 );
    const byte PixelDepth = Bytes[16];
    const byte Descriptor = Bytes[17];
    if( Bytes[1] != 0 || ( ImageType != 2 && ImageType != 10 ) || ( PixelDepth != 24 && PixelDepth != 32 ) ||
        Width == 0 || Height == 0 )
    {
        Error = "Unsupported TGA layout; expected 24/32-bit TRUE-color TGA";
        return FALSE;
    }
    const usize BytesPerPixel = PixelDepth / 8;
    usize Position = 18 + Bytes[0];
    if( Position > Bytes.size() )
    {
        Error = "TGA image data starts beyond end of file";
        return FALSE;
    }
    if( !Result.Create( Width, Height, Error ) )
        return FALSE;
    const xbool bTopOrigin = ( Descriptor & 0x20 ) != 0;
    usize PixelIndex = 0;
    while( PixelIndex < static_cast<usize>( Width ) * Height )
    {
        usize RunLength = 1;
        xbool bRun = FALSE;
        if( ImageType == 10 )
        {
            if( Position >= Bytes.size() )
            {
                Error = "TGA RLE packet is truncated";
                return FALSE;
            }
            const byte Packet = Bytes[Position++];
            bRun = ( Packet & 0x80 ) != 0;
            RunLength = ( Packet & 0x7f ) + 1;
        }
        if( PixelIndex + RunLength > static_cast<usize>( Width ) * Height )
        {
            Error = "TGA RLE packet exceeds image dimensions";
            return FALSE;
        }
        std::array<byte, 4> Raw{};
        if( Position + BytesPerPixel > Bytes.size() )
        {
            Error = "TGA pixel data is truncated";
            return FALSE;
        }
        for( usize Run = 0; Run < RunLength; ++Run )
        {
            if( !bRun || Run == 0 )
            {
                if( !bRun && Run != 0 )
                {
                    if( Position + BytesPerPixel > Bytes.size() )
                    {
                        Error = "TGA raw packet is truncated";
                        return FALSE;
                    }
                }
                Raw[0] = Bytes[Position++];
                Raw[1] = Bytes[Position++];
                Raw[2] = Bytes[Position++];
                if( BytesPerPixel == 4 )
                    Raw[3] = Bytes[Position++];
                else
                    Raw[3] = 255;
            }
            const u32 SourceY = static_cast<u32>( PixelIndex / Width );
            const u32 X = static_cast<u32>( PixelIndex % Width );
            const u32 Y = bTopOrigin ? SourceY : Height - 1 - SourceY;
            const usize Offset = ( static_cast<usize>( Y ) * Width + X ) * 4;
            Result.Mip( 0 ).Pixels[Offset + 0] = Raw[2];
            Result.Mip( 0 ).Pixels[Offset + 1] = Raw[1];
            Result.Mip( 0 ).Pixels[Offset + 2] = Raw[0];
            Result.Mip( 0 ).Pixels[Offset + 3] = Raw[3];
            ++PixelIndex;
        }
    }
    Result.AlphaMode = PixelDepth == 32 ? alpha_mode::Straight : alpha_mode::Opaque;
    return TRUE;
}

//=========================================================================

xbool DecodeBmp( const std::vector<byte>& Bytes, image& Result, std::string& Error )
{
    if( Bytes.size() < 54 || Bytes[0] != 'B' || Bytes[1] != 'M' )
    {
        Error = "BMP header is invalid or truncated";
        return FALSE;
    }
    const u32 PixelOffset = ReadU32( Bytes.data() + 10 );
    const u32 HeaderSize = ReadU32( Bytes.data() + 14 );
    if( HeaderSize < 40 || Bytes.size() < 14 || static_cast<usize>( HeaderSize ) > Bytes.size() - 14 )
    {
        Error = "Unsupported BMP header size";
        return FALSE;
    }
    const s32 SignedWidth = ReadS32( Bytes.data() + 18 );
    const s32 SignedHeight = ReadS32( Bytes.data() + 22 );
    const u16 Planes = ReadU16( Bytes.data() + 26 );
    const u16 Bits = ReadU16( Bytes.data() + 28 );
    const u32 Compression = ReadU32( Bytes.data() + 30 );
    if( SignedWidth <= 0 || SignedHeight == 0 || Planes != 1 || ( Bits != 8 && Bits != 24 && Bits != 32 ) ||
        Compression != 0 )
    {
        Error = "Unsupported BMP layout; expected uncompressed 8/24/32-bit BMP";
        return FALSE;
    }
    const u32 Width = static_cast<u32>( SignedWidth );
    if( SignedHeight == std::numeric_limits<s32>::min() )
    {
        Error = "BMP height is outside the supported range";
        return FALSE;
    }
    const u32 Height = static_cast<u32>( SignedHeight < 0 ? -SignedHeight : SignedHeight );
    const usize RowBytes = ( ( static_cast<usize>( Width ) * Bits + 31 ) / 32 ) * 4;
    if( PixelOffset > Bytes.size() ||
        RowBytes > 0 && static_cast<usize>( Height ) > ( Bytes.size() - PixelOffset ) / RowBytes )
    {
        Error = "BMP pixel data is truncated";
        return FALSE;
    }
    const u32 ColorsUsed = ReadU32( Bytes.data() + 46 );
    std::array<pixel, 256> Palette{};
    u32 PaletteCount = 0;
    if( Bits == 8 )
    {
        const u32 Count = ColorsUsed ? ColorsUsed : 256;
        const usize PaletteOffset = 14 + HeaderSize;
        if( Count > 256 || PaletteOffset + static_cast<usize>( Count ) * 4 > PixelOffset )
        {
            Error = "BMP palette is invalid";
            return FALSE;
        }
        for( u32 Index = 0; Index < Count; ++Index )
        {
            const byte* pColor = Bytes.data() + PaletteOffset + Index * 4;
            Palette[Index] = { pColor[2], pColor[1], pColor[0], 255 };
        }
        PaletteCount = Count;
    }
    if( !Result.Create( Width, Height, Error ) )
        return FALSE;
    const xbool bTopDown = SignedHeight < 0;
    for( u32 SourceY = 0; SourceY < Height; ++SourceY )
    {
        const u32 Y = bTopDown ? SourceY : Height - 1 - SourceY;
        const byte* pRow = Bytes.data() + PixelOffset + static_cast<usize>( SourceY ) * RowBytes;
        for( u32 X = 0; X < Width; ++X )
        {
            pixel Value;
            if( Bits == 8 )
            {
                if( pRow[X] >= PaletteCount )
                {
                    Error = "BMP pixel references an invalid palette entry";
                    return FALSE;
                }
                Value = Palette[pRow[X]];
            }
            else if( Bits == 24 )
                Value = { pRow[X * 3 + 2], pRow[X * 3 + 1], pRow[X * 3 + 0], 255 };
            else
                Value = { pRow[X * 4 + 2], pRow[X * 4 + 1], pRow[X * 4 + 0], pRow[X * 4 + 3] };
            const usize Offset = ( static_cast<usize>( Y ) * Width + X ) * 4;
            Result.Mip( 0 ).Pixels[Offset + 0] = Value.R;
            Result.Mip( 0 ).Pixels[Offset + 1] = Value.G;
            Result.Mip( 0 ).Pixels[Offset + 2] = Value.B;
            Result.Mip( 0 ).Pixels[Offset + 3] = Value.A;
        }
    }
    Result.AlphaMode = Bits == 32 ? alpha_mode::Straight : alpha_mode::Opaque;
    return TRUE;
}

//=========================================================================

usize Align32( usize Value )
{
    return ( Value + 31 ) & ~static_cast<usize>( 31 );
}

//=========================================================================

xbool LoadXbmpBytes( const std::vector<byte>& Bytes, image& Result, xbmp_metadata& Metadata, std::string& Error )
{
    if( Bytes.size() < 32 )
    {
        Error = "XBMP header is truncated";
        return FALSE;
    }
    const s32 DataSize = ReadS32( Bytes.data() + 0 );
    const s32 ClutSize = ReadS32( Bytes.data() + 4 );
    const s32 Width = ReadS32( Bytes.data() + 8 );
    const s32 Height = ReadS32( Bytes.data() + 12 );
    const s32 PhysicalWidth = ReadS32( Bytes.data() + 16 );
    const u32 Flags = ReadU32( Bytes.data() + 20 );
    const s32 MipCount = ReadS32( Bytes.data() + 24 );
    const s32 Format = ReadS32( Bytes.data() + 28 );
    if( DataSize < 0 || ClutSize < 0 || Width <= 0 || Height <= 0 || PhysicalWidth <= 0 || MipCount < 0 ||
        MipCount > 32 || !IsSupportedFormat( Format ) || IsConsoleFlags( Flags ) )
    {
        Error = IsConsoleFlags( Flags ) ? "Unsupported console-specific XBMP encoding flags"
                                        : "Invalid or unsupported XBMP header";
        return FALSE;
    }
    if( static_cast<usize>( DataSize ) + static_cast<usize>( ClutSize ) > Bytes.size() - 32 )
    {
        Error = "XBMP payload exceeds file size";
        return FALSE;
    }
    if( ClutSize != 0 )
    {
        Error = "Palette XBMP formats are not supported by the desktop Bitmap module";
        return FALSE;
    }
    Metadata.Flags = Flags;
    Metadata.Width = static_cast<u32>( Width );
    Metadata.Height = static_cast<u32>( Height );
    Metadata.PhysicalWidth = static_cast<u32>( PhysicalWidth );
    u32 AstcBlockWidth = 0;
    u32 AstcBlockHeight = 0;
    const xbool bAstc = astc::BlockSize( Format, AstcBlockWidth, AstcBlockHeight );
    Metadata.Format = Format;
    Metadata.MipCount = MipCount;

    const usize DataBegin = 32;
    const usize TableBytes = MipCount == 0 ? 0 : Align32( static_cast<usize>( MipCount + 1 ) * 8 );
    if( TableBytes > static_cast<usize>( DataSize ) )
    {
        Error = "XBMP mip table exceeds data payload";
        return FALSE;
    }
    struct level_info
    {
        u32 Width;
        u32 Height;
        usize Offset;
    };
    std::vector<level_info> Levels;
    Levels.reserve( static_cast<usize>( MipCount ) + 1 );
    for( s32 Index = 0; Index <= MipCount; ++Index )
    {
        const usize TableOffset = DataBegin + static_cast<usize>( Index ) * 8;
        u32 LevelWidth = static_cast<u32>( Width );
        u32 LevelHeight = static_cast<u32>( Height );
        usize LevelOffset = 0;
        if( MipCount == 0 )
        {
            LevelOffset = 0;
        }
        else
        {
            LevelOffset = static_cast<usize>( ReadU32( Bytes.data() + TableOffset ) );
            LevelWidth = ReadU16( Bytes.data() + TableOffset + 4 );
            LevelHeight = ReadU16( Bytes.data() + TableOffset + 6 );
        }
        if( LevelWidth == 0 || LevelHeight == 0 || LevelOffset < TableBytes ||
            LevelOffset >= static_cast<usize>( DataSize ) )
        {
            Error = "Invalid XBMP mip table entry";
            return FALSE;
        }
        Levels.push_back( { LevelWidth, LevelHeight, LevelOffset } );
    }

    Result.Clear();
    Result.Mips().reserve( Levels.size() );
    const byte* pData = Bytes.data() + DataBegin;
    for( usize Index = 0; Index < Levels.size(); ++Index )
    {
        const level_info& Level = Levels[Index];
        if( Index > 0 )
        {
            const level_info& Previous = Levels[Index - 1];
            if( Level.Width != x_max<u32>( 1, Previous.Width / 2 ) ||
                Level.Height != x_max<u32>( 1, Previous.Height / 2 ) )
            {
                Error = "XBMP mip table is not a complete halving chain";
                return FALSE;
            }
        }
        usize LevelSize = 0;
        if( bAstc )
            LevelSize = astc::CompressedSize( Level.Width, Level.Height, AstcBlockWidth, AstcBlockHeight );
        else if( IsDxtFormat( Format ) )
        {
            const dxt::format DxtFormat = Format == 73   ? dxt::format::DXT1
                                          : Format == 75 ? dxt::format::DXT3
                                                         : dxt::format::DXT5;
            LevelSize = dxt::CompressedSize( Level.Width, Level.Height, DxtFormat );
        }
        else
        {
            if( !CheckedImageBytes( Level.Width, Level.Height, RawBytesPerPixel( Format ), LevelSize ) )
            {
                Error = "Invalid XBMP mip dimensions";
                return FALSE;
            }
        }
        if( Level.Offset > static_cast<usize>( DataSize ) || LevelSize > static_cast<usize>( DataSize ) - Level.Offset )
        {
            Error = "XBMP mip payload exceeds data block";
            return FALSE;
        }
        if( Index + 1 < Levels.size() && Levels[Index + 1].Offset < Level.Offset + LevelSize )
        {
            Error = "XBMP mip payloads overlap";
            return FALSE;
        }

        mip_level Decoded;
        if( bAstc )
        {
            std::vector<byte> Payload( pData + Level.Offset, pData + Level.Offset + LevelSize );
            if( !astc::DecodeMip( Level.Width, Level.Height, AstcBlockWidth, AstcBlockHeight,
                                  (Flags & astc::SrgbFlag) != 0, Payload, Decoded, Error ) )
                return FALSE;
        }
        else if( IsDxtFormat( Format ) )
        {
            const dxt::format DxtFormat = Format == 73   ? dxt::format::DXT1
                                          : Format == 75 ? dxt::format::DXT3
                                                         : dxt::format::DXT5;
            std::vector<byte> Payload( pData + Level.Offset, pData + Level.Offset + LevelSize );
            if( !dxt::DecodeMip( Level.Width, Level.Height, DxtFormat, Payload, Decoded, Error ) )
                return FALSE;
        }
        else if( !DecodeRawMip( Format, Level.Width, Level.Height, pData + Level.Offset, LevelSize, Decoded, Error ) )
        {
            return FALSE;
        }
        Result.Mips().push_back( std::move( Decoded ) );
    }
    if( bAstc )
        Result.ColorSpace = (Flags & astc::SrgbFlag) != 0 ? color_space::SRGB : color_space::Linear;
    Result.AlphaMode = ( Format == 2 || Format == 4 || Format == 5 || Format == 9 || Format == 11 || Format == 12 ||
                         Format == 14 || Format == 16 || Format == 17 || Format == 21 || Format == 23 || Format == 24 )
                           ? alpha_mode::Opaque
                           : alpha_mode::Straight;
    return Result.Validate( Error );
}

//=========================================================================

usize MipPayloadSize( const mip_level& Mip, const build_profile& Profile )
{
    if( Profile.Format == output_format::ASTC )
        return astc::CompressedSize( Mip.Width, Mip.Height, Profile.AstcBlockWidth, Profile.AstcBlockHeight );
    if( Profile.Format == output_format::DXT1 )
        return dxt::CompressedSize( Mip.Width, Mip.Height, dxt::format::DXT1 );
    if( Profile.Format == output_format::DXT3 )
        return dxt::CompressedSize( Mip.Width, Mip.Height, dxt::format::DXT3 );
    if( Profile.Format == output_format::DXT5 )
        return dxt::CompressedSize( Mip.Width, Mip.Height, dxt::format::DXT5 );
    return static_cast<usize>( Mip.Width ) * Mip.Height * 4;
}

//=========================================================================

s32 XbmpFormat( const build_profile& Profile )
{
    switch( Profile.Format )
    {
    case output_format::ARGB8888:
        return 3;
    case output_format::DXT1:
        return 73;
    case output_format::DXT3:
        return 75;
    case output_format::DXT5:
        return 77;
    case output_format::ASTC:
        return astc::FormatId( Profile.AstcBlockWidth, Profile.AstcBlockHeight );
    }
    return 0;
}

//=========================================================================

xbool EncodeMip( const mip_level& Source, const build_profile& Profile, std::vector<byte>& Output, std::string& Error )
{
    if( Profile.Format == output_format::ARGB8888 )
    {
        Output.resize( static_cast<usize>( Source.Width ) * Source.Height * 4 );
        for( u32 Y = 0; Y < Source.Height; ++Y )
        {
            for( u32 X = 0; X < Source.Width; ++X )
            {
                const usize SourceOffset = ( static_cast<usize>( Y ) * Source.Width + X ) * 4;
                const usize OutputOffset = SourceOffset;
                Output[OutputOffset + 0] = Source.Pixels[SourceOffset + 2];
                Output[OutputOffset + 1] = Source.Pixels[SourceOffset + 1];
                Output[OutputOffset + 2] = Source.Pixels[SourceOffset + 0];
                Output[OutputOffset + 3] = Source.Pixels[SourceOffset + 3];
            }
        }
        return TRUE;
    }
    if( Profile.Format == output_format::ASTC )
        return astc::EncodeMip( Source, Profile, Output, Error );
    const dxt::format Format = Profile.Format == output_format::DXT1   ? dxt::format::DXT1
                               : Profile.Format == output_format::DXT3 ? dxt::format::DXT3
                                                                       : dxt::format::DXT5;
    return dxt::EncodeMip( Source, Format, Profile.Alpha, Profile.AlphaThreshold, Output, Error );
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool LoadSource( const std::filesystem::path& Path, image& Result, std::string& Error )
{
    const xstring Extension = resource_tool::LowerExtension( Path );
    if( Extension == ".xbmp" )
    {
        xbmp_metadata Metadata;
        return LoadXbmp( Path, Result, Metadata, Error );
    }
    std::vector<byte> Bytes;
    if( !ReadFile( Path, Bytes, Error ) )
        return FALSE;
    if( Extension == ".tga" )
        return DecodeTga( Bytes, Result, Error );
    if( Extension == ".bmp" )
        return DecodeBmp( Bytes, Result, Error );
    if( Extension == ".psd" || Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" )
    {
        Error = "Source image format is not implemented by ResourceTool Bitmap yet: " + Extension;
        return FALSE;
    }
    Error = "Unsupported bitmap source extension: " + Extension;
    return FALSE;
}

//=========================================================================

xbool LoadXbmp( const std::filesystem::path& Path, image& Result, xbmp_metadata& Metadata, std::string& Error )
{
    std::vector<byte> Bytes;
    if( !ReadFile( Path, Bytes, Error ) )
        return FALSE;
    return LoadXbmpBytes( Bytes, Result, Metadata, Error );
}

//=========================================================================

xbool SaveXbmp( const std::filesystem::path& Path, const image& Source, const build_profile& Profile,
                std::string& Error )
{
    if( !Source.Validate( Error ) )
        return FALSE;
    if( Source.MipCount() == 0 || Source.MipCount() - 1 > 32 )
    {
        Error = "XBMP mip count is outside the supported range";
        return FALSE;
    }
    for( const mip_level& Mip : Source.Mips() )
    {
        if( Mip.Width > 65535 || Mip.Height > 65535 )
        {
            Error = "XBMP mip dimensions exceed the compiled table limits";
            return FALSE;
        }
    }
    const s32 Format = XbmpFormat( Profile );
    if( Format == 0 )
    {
        Error = "Bitmap profile has no XBMP format";
        return FALSE;
    }
    const s32 MipCount = static_cast<s32>( Source.MipCount() - 1 );
    const usize TableBytes = MipCount == 0 ? 0 : Align32( Source.MipCount() * 8 );
    usize PayloadBytes = TableBytes;
    for( const mip_level& Mip : Source.Mips() )
    {
        const usize MipBytes = MipPayloadSize( Mip, Profile );
        if( MipBytes > std::numeric_limits<usize>::max() - PayloadBytes )
        {
            Error = "XBMP payload size overflow";
            return FALSE;
        }
        PayloadBytes += MipBytes;
    }
    if( PayloadBytes > std::numeric_limits<s32>::max() )
    {
        Error = "XBMP payload is too large for the compiled header";
        return FALSE;
    }
    std::vector<byte> Data( PayloadBytes, 0 );
    usize PayloadOffset = TableBytes;
    for( usize Index = 0; Index < Source.MipCount(); ++Index )
    {
        const mip_level& Mip = Source.Mip( Index );
        if( MipCount != 0 )
        {
            const usize TableOffset = Index * 8;
            WriteU32( Data, TableOffset, static_cast<u32>( PayloadOffset ) );
            WriteU16( Data, TableOffset + 4, static_cast<u16>( Mip.Width ) );
            WriteU16( Data, TableOffset + 6, static_cast<u16>( Mip.Height ) );
        }
        std::vector<byte> Encoded;
        if( !EncodeMip( Mip, Profile, Encoded, Error ) )
            return FALSE;
        std::copy( Encoded.begin(), Encoded.end(), Data.begin() + PayloadOffset );
        PayloadOffset += Encoded.size();
    }

    std::vector<byte> Bytes( 32 + Data.size(), 0 );
    WriteU32( Bytes, 0, static_cast<u32>( Data.size() ) );
    WriteU32( Bytes, 4, 0 );
    WriteU32( Bytes, 8, Source.Width() );
    WriteU32( Bytes, 12, Source.Height() );
    WriteU32( Bytes, 16, Source.Width() );
    const u32 Flags = 1u | (Profile.Format == output_format::ASTC && Profile.Color == color_space::SRGB ? astc::SrgbFlag : 0u);
    WriteU32( Bytes, 20, Flags );
    WriteU32( Bytes, 24, static_cast<u32>( MipCount ) );
    WriteU32( Bytes, 28, static_cast<u32>( Format ) );
    std::copy( Data.begin(), Data.end(), Bytes.begin() + 32 );

    std::ofstream File( Path, std::ios::binary | std::ios::trunc );
    if( !File )
    {
        Error = "Unable to open XBMP output " + Path.generic_string();
        return FALSE;
    }
    File.write( reinterpret_cast<const char*>( Bytes.data() ), static_cast<std::streamsize>( Bytes.size() ) );
    if( !File )
    {
        Error = "Unable to write XBMP output " + Path.generic_string();
        return FALSE;
    }
    return TRUE;
}

//=========================================================================

xbool SaveTga( const std::filesystem::path& Path, const image& Source, std::string& Error )
{
    if( !Source.Validate( Error ) )
        return FALSE;
    const mip_level& Mip = Source.Mip( 0 );
    if( Mip.Width > 65535 || Mip.Height > 65535 )
    {
        Error = "TGA dimensions exceed 16-bit limits";
        return FALSE;
    }
    std::array<byte, 18> Header{};
    Header[2] = 2;
    Header[12] = static_cast<byte>( Mip.Width & 0xff );
    Header[13] = static_cast<byte>( Mip.Width >> 8 );
    Header[14] = static_cast<byte>( Mip.Height & 0xff );
    Header[15] = static_cast<byte>( Mip.Height >> 8 );
    Header[16] = 32;
    Header[17] = 0x28; // 8 alpha bits and top-left origin.
    std::ofstream File( Path, std::ios::binary | std::ios::trunc );
    if( !File )
    {
        Error = "Unable to open TGA output " + Path.generic_string();
        return FALSE;
    }
    File.write( reinterpret_cast<const char*>( Header.data() ), Header.size() );
    for( u32 Y = 0; Y < Mip.Height; ++Y )
    {
        for( u32 X = 0; X < Mip.Width; ++X )
        {
            const usize Offset = ( static_cast<usize>( Y ) * Mip.Width + X ) * 4;
            const byte Pixel[4] = { Mip.Pixels[Offset + 2], Mip.Pixels[Offset + 1], Mip.Pixels[Offset + 0],
                                    Mip.Pixels[Offset + 3] };
            File.write( reinterpret_cast<const char*>( Pixel ), 4 );
        }
    }
    if( !File )
    {
        Error = "Unable to write TGA output " + Path.generic_string();
        return FALSE;
    }
    return TRUE;
}

} // namespace resource_tool::bitmap::format_io
