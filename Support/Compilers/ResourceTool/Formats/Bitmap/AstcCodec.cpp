#include "AstcCodec.hpp"
#include "BitmapBuildProfile.hpp"
#include "astcenc.h"

#include <limits>
#include <memory>

namespace resource_tool::bitmap::astc
{

namespace
{

constexpr u32 Blocks[][2] = {
    {4, 4}, {5, 4}, {5, 5}, {6, 5}, {6, 6}, {8, 5}, {8, 6},
    {8, 8}, {10, 5}, {10, 6}, {10, 8}, {10, 10}, {12, 10}, {12, 12}
};

using context = std::unique_ptr<astcenc_context, decltype( &astcenc_context_free )>;

xbool Status( astcenc_error Result, std::string& Error )
{
    if( Result == ASTCENC_SUCCESS )
        return TRUE;
    Error = std::string( "ASTC: " ) + astcenc_get_error_string( Result );
    return FALSE;
}

xbool CreateContext( u32 BlockWidth, u32 BlockHeight, xbool bSrgb, float Quality, xbool bDecode,
                     context& Context, std::string& Error )
{
    astcenc_config Config;
    const auto Profile = bSrgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR;
    const u32 Flags = bDecode ? ASTCENC_FLG_DECOMPRESS_ONLY : 0;
    if( !Status( astcenc_config_init( Profile, BlockWidth, BlockHeight, 1, Quality, Flags, &Config ), Error ) )
        return FALSE;
    astcenc_context* pContext = NULL;
    if( !Status( astcenc_context_alloc( &Config, 1, &pContext, NULL ), Error ) )
        return FALSE;
    Context.reset( pContext );
    return TRUE;
}

xbool PixelBytes( u32 Width, u32 Height, usize& Bytes, std::string& Error )
{
    if( !Width || !Height || (usize)Width > std::numeric_limits<usize>::max() / Height / 4 )
    {
        Error = "Invalid ASTC image dimensions";
        return FALSE;
    }
    Bytes = (usize)Width * Height * 4;
    return TRUE;
}

}

s32 FormatId( u32 BlockWidth, u32 BlockHeight )
{
    for( s32 i = 0; i < (s32)(sizeof(Blocks) / sizeof(Blocks[0])); i++ )
    {
        if( Blocks[i][0] == BlockWidth && Blocks[i][1] == BlockHeight )
            return 79 + i;
    }
    return 0;
}

xbool BlockSize( s32 Format, u32& BlockWidth, u32& BlockHeight )
{
    if( Format < 79 || Format >= 79 + (s32)(sizeof(Blocks) / sizeof(Blocks[0])) )
        return FALSE;
    BlockWidth = Blocks[Format - 79][0];
    BlockHeight = Blocks[Format - 79][1];
    return TRUE;
}

usize CompressedSize( u32 Width, u32 Height, u32 BlockWidth, u32 BlockHeight )
{
    if( !Width || !Height || !FormatId( BlockWidth, BlockHeight ) )
        return 0;
    const usize X = ((usize)Width + BlockWidth - 1) / BlockWidth;
    const usize Y = ((usize)Height + BlockHeight - 1) / BlockHeight;
    if( X > std::numeric_limits<usize>::max() / Y / 16 )
        return 0;
    return X * Y * 16;
}

xbool EncodeMip( const mip_level& Source, const build_profile& Profile, std::vector<byte>& Output,
                 std::string& Error )
{
    usize Bytes = 0;
    const usize Size = CompressedSize( Source.Width, Source.Height, Profile.AstcBlockWidth, Profile.AstcBlockHeight );
    if( !PixelBytes( Source.Width, Source.Height, Bytes, Error ) )
        return FALSE;
    if( !Size || Profile.AstcQuality > 100 || Source.Pixels.size() != Bytes )
    {
        Error = "Invalid ASTC encoding input or profile";
        return FALSE;
    }
    context Context( NULL, astcenc_context_free );
    if( !CreateContext( Profile.AstcBlockWidth, Profile.AstcBlockHeight, Profile.Color == color_space::SRGB,
                        (float)Profile.AstcQuality, FALSE, Context, Error ) )
        return FALSE;
    std::vector<byte> Pixels = Source.Pixels;
    if( Profile.Alpha == alpha_mode::Mask || Profile.Alpha == alpha_mode::Opaque )
    {
        for( usize i = 3; i < Pixels.size(); i += 4 )
            Pixels[i] = Profile.Alpha == alpha_mode::Opaque || Pixels[i] >= Profile.AlphaThreshold ? 255 : 0;
    }
    void* pSlice = Pixels.data();
    astcenc_image Image = {Source.Width, Source.Height, 1, ASTCENC_TYPE_U8, &pSlice};
    const astcenc_swizzle Swizzle = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
    Output.resize( Size );
    return Status( astcenc_compress_image( Context.get(), &Image, &Swizzle, Output.data(), Output.size(), 0 ), Error );
}

xbool DecodeMip( u32 Width, u32 Height, u32 BlockWidth, u32 BlockHeight, xbool bSrgb,
                 const std::vector<byte>& Source, mip_level& Output, std::string& Error )
{
    usize Bytes = 0;
    const usize Size = CompressedSize( Width, Height, BlockWidth, BlockHeight );
    if( !PixelBytes( Width, Height, Bytes, Error ) )
        return FALSE;
    if( !Size || Source.size() != Size )
    {
        Error = "Invalid ASTC payload size";
        return FALSE;
    }
    context Context( NULL, astcenc_context_free );
    if( !CreateContext( BlockWidth, BlockHeight, bSrgb, ASTCENC_PRE_FASTEST, TRUE, Context, Error ) )
        return FALSE;
    Output.Width = Width;
    Output.Height = Height;
    Output.Pixels.resize( Bytes );
    void* pSlice = Output.Pixels.data();
    astcenc_image Image = {Width, Height, 1, ASTCENC_TYPE_U8, &pSlice};
    const astcenc_swizzle Swizzle = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
    return Status( astcenc_decompress_image( Context.get(), Source.data(), Source.size(), &Image, &Swizzle, 0 ), Error );
}

}
