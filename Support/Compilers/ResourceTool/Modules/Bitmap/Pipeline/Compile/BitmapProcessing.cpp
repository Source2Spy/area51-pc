//=========================================================================
//
//  BitmapProcessing.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapProcessing.hpp"

#include <algorithm>
#include <utility>

namespace resource_tool::bitmap
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool ApplyDetailMap( image& Source, std::string& Error )
{
    if( !Source.Validate( Error ) )
        return FALSE;

    mip_level& Base = Source.Mip( 0 );
    for( u32 Y = 0; Y < Base.Height; ++Y )
    {
        for( u32 X = 0; X < Base.Width; ++X )
        {
            const usize Offset = ( static_cast<usize>( Y ) * Base.Width + X ) * 4;
            const u8 Alpha = x_max<u8>( Base.Pixels[Offset + 0], 2 );
            Base.Pixels[Offset + 0] = Alpha;
            Base.Pixels[Offset + 1] = Alpha;
            Base.Pixels[Offset + 2] = Alpha;
            Base.Pixels[Offset + 3] = Alpha;
        }
    }

    image Mipped;
    if( !BuildMipChain( Source, 3, Mipped, Error ) )
        return FALSE;
    for( usize MipIndex = 1; MipIndex < Mipped.MipCount(); ++MipIndex )
    {
        mip_level& Mip = Mipped.Mip( MipIndex );
        const f32 T = static_cast<f32>( MipIndex ) / 2.0f;
        for( usize Offset = 0; Offset < Mip.Pixels.size(); Offset += 4 )
        {
            const u8 OriginalAlpha = Mip.Pixels[Offset + 3];
            const u8 Alpha = static_cast<u8>( static_cast<f32>( OriginalAlpha ) * ( 1.0f - T ) + 128.0f * T );
            Mip.Pixels[Offset + 0] = Alpha;
            Mip.Pixels[Offset + 1] = Alpha;
            Mip.Pixels[Offset + 2] = Alpha;
            Mip.Pixels[Offset + 3] = Alpha;
        }
    }
    Source = std::move( Mipped );
    return TRUE;
}

//=========================================================================

xbool ApplyPunchThrough( image& Diffuse, const image& PunchThrough, std::string& Error )
{
    if( !Diffuse.Validate( Error ) || !PunchThrough.Validate( Error ) )
        return FALSE;
    if( Diffuse.Width() != PunchThrough.Width() || Diffuse.Height() != PunchThrough.Height() )
    {
        Error = "Diffuse and punch-through textures have different dimensions";
        return FALSE;
    }

    mip_level& Base = Diffuse.Mip( 0 );
    const mip_level& Punch = PunchThrough.Mip( 0 );
    for( u32 Y = 0; Y < Base.Height; ++Y )
    {
        for( u32 X = 0; X < Base.Width; ++X )
        {
            const usize Offset = ( static_cast<usize>( Y ) * Base.Width + X ) * 4;
            const usize PunchOffset = ( static_cast<usize>( Y ) * Punch.Width + X ) * 4;
            Base.Pixels[Offset + 3] = Punch.Pixels[PunchOffset + 0] > 0 ? x_max<u8>( Base.Pixels[Offset + 3], 2 ) : 0;
        }
    }
    return TRUE;
}

} // namespace resource_tool::bitmap
