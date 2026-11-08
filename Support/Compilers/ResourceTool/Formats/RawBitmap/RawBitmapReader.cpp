//=========================================================================
//
//  RawBitmapReader.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RawBitmapReader.hpp"

#include "PropertyTree.hpp"

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace resource_tool::bitmap_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

std::string Lower( std::string Value )
{
    for( char& Character : Value )
        Character = (char)x_tolower( Character );
    return Value;
}

//=========================================================================

xbool Integer( const char* pValue, s32& Result )
{
    char* pEnd = NULL;
    const long Parsed = std::strtol( pValue ? pValue : "", &pEnd, 10 );
    if( !pValue || pEnd == pValue || *pEnd != '\0' )
        return FALSE;
    Result = (s32)Parsed;
    return TRUE;
}

//=========================================================================

xbool Fail( std::string& Error, s32 Record, const char* pMessage )
{
    Error = xfs( "rawbitmap property record %d: %s", Record, pMessage ? pMessage : "" );
    return FALSE;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool raw_bitmap_reader::Load( const std::filesystem::path& FileName, raw_bitmap_descriptor& Descriptor,
                               std::string& Error ) const
{
    Descriptor = raw_bitmap_descriptor();
    resource_tool::property_tree::document Document;
    resource_tool::property_tree::reader Reader;
    if( !Reader.Load( FileName, Document, Error ) )
    {
        Error = "Unable to read rawbitmap: " + Error;
        return FALSE;
    }

    xbool HasVersion = FALSE;
    xbool HasType = FALSE;
    xbool HasSource = FALSE;
    const std::vector<resource_tool::property_tree::property_record>& Records = Document.Records();
    for( s32 Record = 0; Record < (s32)Records.size(); ++Record )
    {
        const resource_tool::property_tree::property_record& Property = Records[Record];
        const std::string& Name = Property.Name;
        const std::string& Type = Property.Type;
        const std::string& Value = Property.Value;

        const std::string& PropertyType = Type;
        const std::string& PropertyValue = Value;
        const std::string& PropertyName = Name;
        xbool Valid = TRUE;
        xbool Known = TRUE;

        if( PropertyName == "ResDesc\\Version" )
        {
            s32 Version = 0;
            Valid = PropertyType == "INT" && Integer( PropertyValue.c_str(), Version ) && Version == 1;
            HasVersion = Valid;
        }
        else if( PropertyName == "ResDesc\\Type" )
        {
            Valid = ( PropertyType == "ENUM" || PropertyType == "STRING" ) && Lower( PropertyValue ) == "bitmap";
            HasType = Valid;
        }
        else if( PropertyName == "ResDesc\\FileName" )
        {
            Valid = PropertyType == "FILENAME" || PropertyType == "STRING";
            if( Valid )
            {
                Descriptor.SourceFile = PropertyValue;
                HasSource = !Descriptor.SourceFile.empty();
            }
        }
        else if( PropertyName == "ResDesc\\Format" )
        {
            if( PropertyType != "ENUM" && PropertyType != "STRING" )
                Valid = FALSE;
            else
            {
                const std::string Format = Lower( PropertyValue );
                if( Format == "argb8888" || Format == "rgba8888" || Format == "uncompressed" )
                    Descriptor.Profile.Format = resource_tool::bitmap::output_format::ARGB8888;
                else if( Format == "dxt1" )
                    Descriptor.Profile.Format = resource_tool::bitmap::output_format::DXT1;
                else if( Format == "dxt3" )
                    Descriptor.Profile.Format = resource_tool::bitmap::output_format::DXT3;
                else if( Format == "dxt5" )
                    Descriptor.Profile.Format = resource_tool::bitmap::output_format::DXT5;
                else if( Format == "astc" )
                    Descriptor.Profile.Format = resource_tool::bitmap::output_format::ASTC;
                else
                    Valid = FALSE;
            }
        }
        else if( PropertyName == "ResDesc\\ASTC\\BlockWidth" ||
                 PropertyName == "ResDesc\\ASTC\\BlockHeight" || PropertyName == "ResDesc\\ASTC\\Quality" )
        {
            s32 Number = 0;
            const xbool bQuality = PropertyName == "ResDesc\\ASTC\\Quality";
            Valid = PropertyType == "INT" && Integer( PropertyValue.c_str(), Number ) &&
                    Number >= (bQuality ? 0 : 4) && Number <= (bQuality ? 100 : 12);
            if( Valid )
            {
                if( bQuality )
                    Descriptor.Profile.AstcQuality = (u32)Number;
                else if( PropertyName == "ResDesc\\ASTC\\BlockWidth" )
                    Descriptor.Profile.AstcBlockWidth = (u32)Number;
                else
                    Descriptor.Profile.AstcBlockHeight = (u32)Number;
            }
        }
        else if( PropertyName == "ResDesc\\Mips\\Policy" )
        {
            if( PropertyType != "ENUM" && PropertyType != "STRING" )
                Valid = FALSE;
            else
            {
                const std::string Policy = Lower( PropertyValue );
                if( Policy == "none" )
                    Descriptor.Profile.Mips = resource_tool::bitmap::mip_policy::None;
                else if( Policy == "full" || Policy == "fullchain" )
                    Descriptor.Profile.Mips = resource_tool::bitmap::mip_policy::FullChain;
                else if( Policy == "count" )
                    Descriptor.Profile.Mips = resource_tool::bitmap::mip_policy::Count;
                else if( Policy == "preserve" )
                    Descriptor.Profile.Mips = resource_tool::bitmap::mip_policy::Preserve;
                else
                    Valid = FALSE;
            }
        }
        else if( PropertyName == "ResDesc\\Mips\\Count" )
        {
            s32 MipCount = 0;
            Valid =
                PropertyType == "INT" && Integer( PropertyValue.c_str(), MipCount ) && MipCount > 0 && MipCount <= 33;
            if( Valid )
                Descriptor.Profile.MipCount = (usize)MipCount;
        }
        else if( PropertyName == "ResDesc\\Alpha\\Mode" )
        {
            if( PropertyType != "ENUM" && PropertyType != "STRING" )
                Valid = FALSE;
            else
            {
                const std::string Mode = Lower( PropertyValue );
                if( Mode == "opaque" )
                    Descriptor.Profile.Alpha = resource_tool::bitmap::alpha_mode::Opaque;
                else if( Mode == "straight" )
                    Descriptor.Profile.Alpha = resource_tool::bitmap::alpha_mode::Straight;
                else if( Mode == "premultiplied" )
                    Descriptor.Profile.Alpha = resource_tool::bitmap::alpha_mode::Premultiplied;
                else if( Mode == "mask" )
                    Descriptor.Profile.Alpha = resource_tool::bitmap::alpha_mode::Mask;
                else
                    Valid = FALSE;
            }
        }
        else if( PropertyName == "ResDesc\\Alpha\\Threshold" )
        {
            s32 Threshold = 0;
            Valid = PropertyType == "INT" && Integer( PropertyValue.c_str(), Threshold ) && Threshold >= 0 &&
                    Threshold <= 255;
            if( Valid )
                Descriptor.Profile.AlphaThreshold = (u8)Threshold;
        }
        else if( PropertyName == "ResDesc\\ColorSpace" )
        {
            if( PropertyType != "ENUM" && PropertyType != "STRING" )
                Valid = FALSE;
            else if( Lower( PropertyValue ) == "srgb" )
                Descriptor.Profile.Color = resource_tool::bitmap::color_space::SRGB;
            else if( Lower( PropertyValue ) == "linear" )
                Descriptor.Profile.Color = resource_tool::bitmap::color_space::Linear;
            else
                Valid = FALSE;
        }
        else
            Known = FALSE;

        if( Known && !Valid )
        {
            return Fail( Error, Record, "invalid property value" );
        }
    }

    if( !HasVersion || !HasType || !HasSource )
    {
        Error = "rawbitmap is missing ResDesc version, type or filename";
        return FALSE;
    }
    if( Descriptor.Profile.Mips == resource_tool::bitmap::mip_policy::Count &&
        ( Descriptor.Profile.MipCount == 0 || Descriptor.Profile.MipCount > 33 ) )
    {
        Error = "rawbitmap mip count is outside the range 1..33";
        return FALSE;
    }
    return TRUE;
}

} // namespace resource_tool::bitmap_format
