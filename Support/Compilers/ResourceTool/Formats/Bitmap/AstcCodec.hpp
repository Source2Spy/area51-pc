#ifndef RESOURCE_TOOL_BITMAP_ASTC_CODEC_HPP
#define RESOURCE_TOOL_BITMAP_ASTC_CODEC_HPP

#include "BitmapImage.hpp"

#include <string>
#include <vector>

namespace resource_tool::bitmap { struct build_profile; }

namespace resource_tool::bitmap::astc
{

constexpr u32 SrgbFlag = 1u << 14;

s32 FormatId( u32 BlockWidth, u32 BlockHeight );
xbool BlockSize( s32 Format, u32& BlockWidth, u32& BlockHeight );
usize CompressedSize( u32 Width, u32 Height, u32 BlockWidth, u32 BlockHeight );
xbool EncodeMip( const mip_level& Source, const build_profile& Profile, std::vector<byte>& Output,
                 std::string& Error );
xbool DecodeMip( u32 Width, u32 Height, u32 BlockWidth, u32 BlockHeight, xbool bSrgb,
                 const std::vector<byte>& Source, mip_level& Output, std::string& Error );

}

#endif
