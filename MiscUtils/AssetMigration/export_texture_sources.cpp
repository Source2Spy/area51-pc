#include "BitmapFormat.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main( int argc, char** argv )
{
    if( argc != 3 )
        return 1;

    x_Init( argc, argv );
    s32 Result = 0;
    {
        std::ifstream Jobs( argv[1] );
        std::ofstream Report( argv[2] );
        if( !Jobs || !Report )
            return 2;

        std::string Line;
        s32 Count = 0;
        while( std::getline( Jobs, Line ) )
        {
            const auto Tab = Line.find( '\t' );
            if( Tab == std::string::npos )
            {
                Result = 3;
                break;
            }

            const std::filesystem::path Input = Line.substr( 0, Tab );
            const std::filesystem::path Output = Line.substr( Tab+1 );
            resource_tool::bitmap::image Image;
            resource_tool::bitmap::format_io::xbmp_metadata Metadata;
            std::string Error;

            if( !resource_tool::bitmap::format_io::LoadXbmp( Input, Image, Metadata, Error ) )
            {
                std::cerr << Input << ": " << Error << '\n';
                Result = 4;
                break;
            }

            std::filesystem::create_directories( Output.parent_path() );
            if( !resource_tool::bitmap::format_io::SaveTga( Output, Image, Error ) )
            {
                std::cerr << Output << ": " << Error << '\n';
                Result = 5;
                break;
            }

            for( usize Mip = 1; Mip < Image.MipCount(); Mip++ )
            {
                resource_tool::bitmap::image MipImage;
                if( !MipImage.Create( Image.Width( Mip ), Image.Height( Mip ), Error ) )
                {
                    Result = 6;
                    break;
                }
                MipImage.Mip( 0 ).Pixels = Image.Mip( Mip ).Pixels;
                const auto Folder = Output.parent_path() / (Output.stem().string() + ".mips");
                std::filesystem::create_directories( Folder );
                const auto Path = Folder / ("mip_" + std::to_string( Mip ) + ".tga");
                if( !resource_tool::bitmap::format_io::SaveTga( Path, MipImage, Error ) )
                {
                    std::cerr << Path << ": " << Error << '\n';
                    Result = 7;
                    break;
                }
            }
            if( Result )
                break;

            Report << Output.generic_string() << '\t' << Metadata.Width << '\t' << Metadata.Height
                   << '\t' << Metadata.Format << '\t' << Metadata.Flags << '\t' << Image.MipCount()
                   << '\t' << (Image.HasAlpha() ? 1 : 0) << '\n';
            if( ++Count % 250 == 0 )
                std::cout << Count << " textures exported" << std::endl;
        }
    }
    x_Kill();
    return Result;
}
