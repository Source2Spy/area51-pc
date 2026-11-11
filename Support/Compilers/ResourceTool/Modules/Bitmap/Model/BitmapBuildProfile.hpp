//=========================================================================
//
//  BitmapBuildProfile.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_BUILD_PROFILE_HPP
#define RESOURCE_TOOL_BITMAP_BUILD_PROFILE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "BitmapImage.hpp"

#include <cstddef>
#include <string>

namespace resource_tool::bitmap
{

//=========================================================================
//  TYPES
//=========================================================================

enum class output_format
{
    ARGB8888,
    DXT1,
    DXT3,
    DXT5,
    ASTC,
};

//-------------------------------------------------------------------------

enum class mip_policy
{
    None,
    FullChain,
    Count,
    Preserve,
};

//-------------------------------------------------------------------------

struct build_profile
{
    output_format Format = output_format::ARGB8888;
    mip_policy Mips = mip_policy::None;
    usize MipCount = 1;
    alpha_mode Alpha = alpha_mode::Straight;
    color_space Color = color_space::SRGB;
    u8 AlphaThreshold = 128;
    u32 AstcBlockWidth = 6;
    u32 AstcBlockHeight = 6;
    u32 AstcQuality = 60;

    static build_profile Default( void );
    std::string Fingerprint( void ) const;
    const char* FormatName( void ) const;
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool ApplyBuildProfile( const image& Source, const build_profile& Profile, image& Result, std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_BUILD_PROFILE_HPP
//=========================================================================
