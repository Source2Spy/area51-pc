#include "BitmapFormat.hpp"
#include "AstcCodec.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main( int argc, char** argv )
{
    if( argc != 3 )
        return 1;

    x_Init( argc, argv );
    s32 Result = 0;
    {
        std::ifstream Jobs( argv[1] );
        std::ofstream Report( argv[2] );
        std::string Line;
        while( std::getline( Jobs, Line ) )
        {
            std::istringstream Stream( Line );
            std::vector<std::string> Fields;
            std::string Field;
            while( std::getline( Stream, Field, '\t' ) )
                Fields.push_back( Field );
            if( Fields.size() != 5 && Fields.size() != 6 )
            {
                Result = 2;
                break;
            }

            using namespace resource_tool::bitmap;
            image Source;
            std::string Error;
            const std::filesystem::path Input = Fields[0];
            if( !format_io::LoadSource( Input, Source, Error ) )
            {
                std::cerr << Input << ": " << Error << '\n';
                Result = 3;
                break;
            }
            for( s32 Mip = 1; Mip < std::stoi( Fields[3] ); Mip++ )
            {
                image Extra;
                const auto Path = Input.parent_path() / (Input.stem().string() + ".mips") /
                                  ("mip_" + std::to_string( Mip ) + ".tga");
                if( !format_io::LoadSource( Path, Extra, Error ) )
                {
                    std::cerr << Path << ": " << Error << '\n';
                    Result = 4;
                    break;
                }
                Source.Mips().push_back( Extra.Mip( 0 ) );
            }
            if( Result )
                break;

            for( s32 Platform = 0; Platform < 2; Platform++ )
            {
                build_profile Profile;
                Profile.Mips = Fields[4] == "none" ? mip_policy::None :
                               Fields[4] == "preserve" ? mip_policy::Preserve : mip_policy::FullChain;
                if( Fields.size() == 6 )
                    Profile.AstcBlockWidth = Profile.AstcBlockHeight = std::stoi( Fields[5] );
                Profile.Color = color_space::Linear;
                Profile.Alpha = Source.HasAlpha() ? alpha_mode::Straight : alpha_mode::Opaque;
                Profile.Format = Platform ? output_format::ASTC :
                                 Source.HasAlpha() ? output_format::DXT5 : output_format::DXT1;
                image Processed;
                if( !ApplyBuildProfile( Source, Profile, Processed, Error ) )
                {
                    std::cerr << Input << ": " << Error << '\n';
                    Result = 5;
                    break;
                }
                for( usize Mip = 0; Mip < Source.MipCount(); Mip++ )
                {
                    if( Processed.Mip( Mip ).Pixels != Source.Mip( Mip ).Pixels )
                    {
                        std::cerr << Input << ": original mip pixels changed before compression\n";
                        Result = 7;
                        break;
                    }
                }
                if( Result )
                    break;
                const std::filesystem::path Output = Fields[Platform + 1];
                if( std::filesystem::exists( Output ) && Fields[4] != "regenerate" )
                {
                    image Existing;
                    format_io::xbmp_metadata Metadata;
                    xbitmap Runtime;
                    const s32 Expected = Platform ? astc::FormatId( Profile.AstcBlockWidth, Profile.AstcBlockHeight ) :
                                         Source.HasAlpha() ? 77 : 73;
                    if( format_io::LoadXbmp( Output, Existing, Metadata, Error ) &&
                        Runtime.Load( Output.string().c_str() ) && Metadata.Format == Expected &&
                        Existing.Width() == Source.Width() && Existing.Height() == Source.Height() &&
                        Existing.MipCount() == Processed.MipCount() && !Runtime.IsSRGB() )
                    {
                        Report << Output.generic_string() << '\t' << Metadata.Format << '\t'
                               << Source.Width() << '\t' << Source.Height() << '\t' << Processed.MipCount() << '\n';
                        Report.flush();
                        continue;
                    }
                }
                std::filesystem::create_directories( Output.parent_path() );
                if( !format_io::SaveXbmp( Output, Processed, Profile, Error ) )
                {
                    std::cerr << Output << ": " << Error << '\n';
                    Result = 5;
                    break;
                }
                image Decoded;
                format_io::xbmp_metadata Metadata;
                xbitmap Runtime;
                if( !format_io::LoadXbmp( Output, Decoded, Metadata, Error ) ||
                    !Runtime.Load( Output.string().c_str() ) ||
                    Decoded.Width() != Source.Width() || Decoded.Height() != Source.Height() ||
                    Decoded.MipCount() != Processed.MipCount() || Runtime.IsSRGB() )
                {
                    std::cerr << Output << ": product verification failed: " << Error << '\n';
                    Result = 6;
                    break;
                }
                Report << Output.generic_string() << '\t' << Metadata.Format << '\t'
                       << Source.Width() << '\t' << Source.Height() << '\t' << Processed.MipCount() << '\n';
                Report.flush();
            }
            if( Result )
                break;
        }
    }
    x_Kill();
    return Result;
}
