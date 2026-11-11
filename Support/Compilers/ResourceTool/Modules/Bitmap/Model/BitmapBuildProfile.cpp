//=========================================================================
//
//  BitmapBuildProfile.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapBuildProfile.hpp"

#include <algorithm>
#include <sstream>

namespace resource_tool::bitmap
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

build_profile build_profile::Default( void )
{
    return build_profile();
}

//=========================================================================

const char* build_profile::FormatName( void ) const
{
    switch( Format )
    {
    case output_format::ARGB8888:
        return "argb8888";
    case output_format::DXT1:
        return "dxt1";
    case output_format::DXT3:
        return "dxt3";
    case output_format::DXT5:
        return "dxt5";
    case output_format::ASTC:
        return "astc";
    }
    return "unknown";
}

//=========================================================================

std::string build_profile::Fingerprint( void ) const
{
    std::ostringstream Result;
    Result << "bitmap-profile-v5;format=" << FormatName() << ";mips=" << static_cast<s32>( Mips )
           << ";mipCount=" << MipCount << ";alpha=" << static_cast<s32>( Alpha )
           << ";color=" << static_cast<s32>( Color ) << ";threshold=" << static_cast<s32>( AlphaThreshold );
    if( Format == output_format::ASTC )
        Result << ";astc=" << AstcBlockWidth << 'x' << AstcBlockHeight << ";quality=" << AstcQuality;
    return Result.str();
}

//=========================================================================

xbool ApplyBuildProfile( const image& Source, const build_profile& Profile, image& Result, std::string& Error )
{
    if( !Source.Validate( Error ) )
        return FALSE;
    if( Profile.Mips == mip_policy::Count && ( Profile.MipCount == 0 || Profile.MipCount > 33 ) )
    {
        Error = "Bitmap profile mipCount must be in range 1..33";
        return FALSE;
    }

    usize DesiredMips = 1;
    switch( Profile.Mips )
    {
    case mip_policy::None:
        DesiredMips = 1;
        break;
    case mip_policy::Preserve:
        DesiredMips = Source.MipCount();
        break;
    case mip_policy::Count:
        DesiredMips = Profile.MipCount;
        break;
    case mip_policy::FullChain:
    {
        DesiredMips = 1;
        u32 Width = Source.Width();
        u32 Height = Source.Height();
        while( Width > 1 || Height > 1 )
        {
            Width = x_max<u32>( 1, Width / 2 );
            Height = x_max<u32>( 1, Height / 2 );
            ++DesiredMips;
        }
        break;
    }
    }

    if( !BuildMipChain( Source, DesiredMips, Result, Error ) )
        return FALSE;
    Result.ColorSpace = Profile.Color;
    Result.AlphaMode = Profile.Alpha;
    if( Profile.Alpha == alpha_mode::Opaque )
    {
        for( mip_level& Mip : Result.Mips() )
            for( usize Offset = 3; Offset < Mip.Pixels.size(); Offset += 4 )
                Mip.Pixels[Offset] = 255;
    }
    return TRUE;
}

} // namespace resource_tool::bitmap
