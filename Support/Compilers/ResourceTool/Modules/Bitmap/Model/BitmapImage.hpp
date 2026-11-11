//=========================================================================
//
//  BitmapImage.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_IMAGE_HPP
#define RESOURCE_TOOL_BITMAP_IMAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace resource_tool::bitmap
{

//=========================================================================
//  TYPES
//=========================================================================

enum class color_space
{
    SRGB,
    Linear,
};

//-------------------------------------------------------------------------

enum class alpha_mode
{
    Opaque,
    Straight,
    Premultiplied,
    Mask,
};

//-------------------------------------------------------------------------

struct pixel
{
    u8 R = 0;
    u8 G = 0;
    u8 B = 0;
    u8 A = 255;
};

//-------------------------------------------------------------------------

struct mip_level
{
    u32 Width = 0;
    u32 Height = 0;
    std::vector<u8> Pixels;

    usize PixelCount( void ) const;
};

//-------------------------------------------------------------------------

class image final
{
public:
    image( void ) = default;

    xbool Create( u32 Width, u32 Height, std::string& Error );
    xbool AddMip( u32 Width, u32 Height, std::string& Error );
    void Clear( void );

    xbool Validate( std::string& Error ) const;
    xbool HasAlpha( void ) const;

    u32 Width( usize Mip = 0 ) const;
    u32 Height( usize Mip = 0 ) const;
    usize MipCount( void ) const;

    pixel GetPixel( u32 X, u32 Y, usize Mip = 0 ) const;
    xbool SetPixel( u32 X, u32 Y, pixel Value, usize Mip = 0 );

    const mip_level& Mip( usize Index ) const;
    mip_level& Mip( usize Index );
    const std::vector<mip_level>& Mips( void ) const;
    std::vector<mip_level>& Mips( void );

    color_space ColorSpace = color_space::SRGB;
    alpha_mode AlphaMode = alpha_mode::Straight;

private:
    std::vector<mip_level> m_Mips;
};

xbool BuildMipChain( const image& Source, usize DesiredMipCount, image& Result, std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_IMAGE_HPP
//=========================================================================
