//=========================================================================
//
//  RawBitmapWriter.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RawBitmapWriter.hpp"

#include "PropertyTree.hpp"

#include <cstdint>
#include <string>

namespace resource_tool::bitmap_format
{

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

class property_builder final
{
public:
    void Add( const char* pName, const char* pType, const std::string& Value )
    {
        m_Document.Add( pName, pType, Value );
    }

    void AddString( const char* pName, const std::string& Value )
    {
        Add( pName, "STRING", Value );
    }

    void AddFile( const char* pName, const std::string& Value )
    {
        Add( pName, "FILENAME", Value );
    }

    void AddEnum( const char* pName, const std::string& Value )
    {
        Add( pName, "ENUM", Value );
    }

    void AddInt( const char* pName, s32 Value )
    {
        char Buffer[32];
        x_sprintf( Buffer, "%d", Value );
        Add( pName, "INT", Buffer );
    }

    const resource_tool::property_tree::document& Properties( void ) const
    {
        return m_Document;
    }

private:
    resource_tool::property_tree::document m_Document;
};

//=========================================================================

const char* FormatName( resource_tool::bitmap::output_format Format )
{
    switch( Format )
    {
    case resource_tool::bitmap::output_format::ARGB8888:
        return "ARGB8888";
    case resource_tool::bitmap::output_format::DXT1:
        return "DXT1";
    case resource_tool::bitmap::output_format::DXT3:
        return "DXT3";
    case resource_tool::bitmap::output_format::DXT5:
        return "DXT5";
    case resource_tool::bitmap::output_format::ASTC:
        return "ASTC";
    }
    return "";
}

//=========================================================================

const char* MipPolicyName( resource_tool::bitmap::mip_policy Policy )
{
    switch( Policy )
    {
    case resource_tool::bitmap::mip_policy::None:
        return "None";
    case resource_tool::bitmap::mip_policy::FullChain:
        return "FullChain";
    case resource_tool::bitmap::mip_policy::Count:
        return "Count";
    case resource_tool::bitmap::mip_policy::Preserve:
        return "Preserve";
    }
    return "";
}

//=========================================================================

const char* AlphaModeName( resource_tool::bitmap::alpha_mode Mode )
{
    switch( Mode )
    {
    case resource_tool::bitmap::alpha_mode::Opaque:
        return "Opaque";
    case resource_tool::bitmap::alpha_mode::Straight:
        return "Straight";
    case resource_tool::bitmap::alpha_mode::Premultiplied:
        return "Premultiplied";
    case resource_tool::bitmap::alpha_mode::Mask:
        return "Mask";
    }
    return "";
}

//=========================================================================

const char* ColorSpaceName( resource_tool::bitmap::color_space Color )
{
    return Color == resource_tool::bitmap::color_space::Linear ? "Linear" : "sRGB";
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool raw_bitmap_writer::Write( const std::filesystem::path& FileName, const raw_bitmap_descriptor& Descriptor,
                                std::string& Error ) const
{
    if( FileName.empty() || Descriptor.SourceFile.empty() )
    {
        Error = "Raw bitmap descriptor requires an output and source filename";
        return FALSE;
    }

    property_builder Properties;
    Properties.AddInt( "ResDesc\\Version", 1 );
    Properties.AddEnum( "ResDesc\\Type", "Bitmap" );
    Properties.AddFile( "ResDesc\\FileName", Descriptor.SourceFile.generic_string() );
    Properties.AddEnum( "ResDesc\\Format", FormatName( Descriptor.Profile.Format ) );
    if( Descriptor.Profile.Format == resource_tool::bitmap::output_format::ASTC )
    {
        Properties.AddInt( "ResDesc\\ASTC\\BlockWidth", (s32)Descriptor.Profile.AstcBlockWidth );
        Properties.AddInt( "ResDesc\\ASTC\\BlockHeight", (s32)Descriptor.Profile.AstcBlockHeight );
        Properties.AddInt( "ResDesc\\ASTC\\Quality", (s32)Descriptor.Profile.AstcQuality );
    }
    Properties.AddEnum( "ResDesc\\Mips\\Policy", MipPolicyName( Descriptor.Profile.Mips ) );
    Properties.AddInt( "ResDesc\\Mips\\Count", (s32)Descriptor.Profile.MipCount );
    Properties.AddEnum( "ResDesc\\Alpha\\Mode", AlphaModeName( Descriptor.Profile.Alpha ) );
    Properties.AddInt( "ResDesc\\Alpha\\Threshold", Descriptor.Profile.AlphaThreshold );
    Properties.AddEnum( "ResDesc\\ColorSpace", ColorSpaceName( Descriptor.Profile.Color ) );

    resource_tool::property_tree::writer Writer;
    return Writer.Write( FileName, Properties.Properties(), Error );
}

} // namespace resource_tool::bitmap_format
