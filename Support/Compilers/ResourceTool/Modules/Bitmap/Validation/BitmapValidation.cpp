//=========================================================================
//
//  BitmapValidation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapValidation.hpp"
#include "AstcCodec.hpp"

namespace resource_tool::bitmap
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

xbool Fail( std::string& Error, const char* pMessage )
{
    Error = pMessage;
    return FALSE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool ValidateImage( const image& Image, std::string& Error )
{
    return Image.Validate( Error );
}

//=========================================================================

xbool ValidateBuildProfile( const build_profile& Profile, std::string& Error )
{
    switch( Profile.Format )
    {
    case output_format::ARGB8888:
    case output_format::DXT1:
    case output_format::DXT3:
    case output_format::ASTC:
    case output_format::DXT5:
        break;
    default:
        return Fail( Error, "Bitmap profile has an unsupported output format" );
    }

    if( Profile.Format == output_format::ASTC &&
        ( !astc::FormatId( Profile.AstcBlockWidth, Profile.AstcBlockHeight ) || Profile.AstcQuality > 100 ) )
        return Fail( Error, "Invalid ASTC block size or quality (0..100)" );

    if( Profile.Mips == mip_policy::Count && ( Profile.MipCount == 0 || Profile.MipCount > 33 ) )
    {
        return Fail( Error, "Bitmap profile mip count must be in range 1..33" );
    }
    return TRUE;
}

} // namespace resource_tool::bitmap
