//=========================================================================
//
//  BitmapDecompileJob.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "BitmapDecompileJob.hpp"

#include "BitmapFormat.hpp"
#include "AstcCodec.hpp"
#include "RawBitmapWriter.hpp"
#include "BitmapValidation.hpp"

namespace resource_tool::bitmap
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool bitmap_decompile_job::Execute( const std::filesystem::path& SourcePath,
                                     const std::filesystem::path& ImageOutputPath, std::string& Error ) const
{
    image Image;
    format_io::xbmp_metadata Metadata;
    if( !format_io::LoadXbmp( SourcePath, Image, Metadata, Error ) )
        return FALSE;
    if( !ValidateImage( Image, Error ) )
        return FALSE;
    return format_io::SaveTga( ImageOutputPath, Image, Error );
}

//=========================================================================

xbool bitmap_decompile_job::Execute( const std::filesystem::path& SourcePath,
                                     const std::filesystem::path& ImageOutputPath,
                                     const std::filesystem::path& RawBitmapOutputPath, std::string& Error ) const
{
    image Image;
    format_io::xbmp_metadata Metadata;
    if( !format_io::LoadXbmp( SourcePath, Image, Metadata, Error ) )
        return FALSE;
    if( !ValidateImage( Image, Error ) )
        return FALSE;
    if( !format_io::SaveTga( ImageOutputPath, Image, Error ) )
        return FALSE;

    bitmap_format::raw_bitmap_descriptor Descriptor;
    std::filesystem::path SourceFile = SourcePath;
    SourceFile.replace_extension( ".tga" );
    Descriptor.SourceFile = SourceFile.filename();
    Descriptor.Profile.Mips = Metadata.MipCount > 0 ? mip_policy::Count : mip_policy::None;
    Descriptor.Profile.MipCount = (usize)Metadata.MipCount + 1;
    Descriptor.Profile.Alpha = Metadata.Format == 73 ? alpha_mode::Mask : Image.AlphaMode;
    Descriptor.Profile.AlphaThreshold = 128;
    Descriptor.Profile.Color = color_space::SRGB;
    switch( Metadata.Format )
    {
    case 3:
        Descriptor.Profile.Format = output_format::ARGB8888;
        break;
    case 73:
        Descriptor.Profile.Format = output_format::DXT1;
        break;
    case 75:
        Descriptor.Profile.Format = output_format::DXT3;
        break;
    case 77:
        Descriptor.Profile.Format = output_format::DXT5;
        break;
    default:
        if( !astc::BlockSize( Metadata.Format, Descriptor.Profile.AstcBlockWidth, Descriptor.Profile.AstcBlockHeight ) )
        {
            Error = xfs( "Cannot represent XBMP format %u in rawbitmap", Metadata.Format );
            return FALSE;
        }
        Descriptor.Profile.Format = output_format::ASTC;
        Descriptor.Profile.Color = Image.ColorSpace;
        break;
    }

    bitmap_format::raw_bitmap_writer Writer;
    return Writer.Write( RawBitmapOutputPath, Descriptor, Error );
}

//=========================================================================

xbool DecompileProduct( const std::filesystem::path& SourcePath, const std::filesystem::path& ImageOutputPath,
                        std::string& Error )
{
    bitmap_decompile_job Job;
    return Job.Execute( SourcePath, ImageOutputPath, Error );
}

} // namespace resource_tool::bitmap
