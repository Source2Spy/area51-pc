//=========================================================================
//
//  DxtCodec.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_DXT_CODEC_HPP
#define RESOURCE_TOOL_BITMAP_DXT_CODEC_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "BitmapImage.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace resource_tool::bitmap::dxt
{

//=========================================================================
//  TYPES
//=========================================================================

enum class format
{
    DXT1,
    DXT3,
    DXT5,
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

usize CompressedSize( u32 Width, u32 Height, format Format );

xbool EncodeMip( const mip_level& Source, format Format, alpha_mode Alpha, u8 AlphaThreshold, std::vector<u8>& Output,
                 std::string& Error );

xbool DecodeMip( u32 Width, u32 Height, format Format, const std::vector<u8>& Input, mip_level& Output,
                 std::string& Error );

} // namespace resource_tool::bitmap::dxt

#endif // RESOURCE_TOOL_BITMAP_DXT_CODEC_HPP
//=========================================================================
