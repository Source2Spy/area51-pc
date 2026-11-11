//=========================================================================
//
//  BitmapProduct.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_PRODUCT_HPP
#define RESOURCE_TOOL_BITMAP_PRODUCT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "Model/BitmapBuildProfile.hpp"
#include "Model/BitmapImage.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace resource_tool
{
class artifact_manifest;
class output_transaction;
} // namespace resource_tool

namespace resource_tool::bitmap
{

//=========================================================================
//  TYPES
//=========================================================================

struct product_request
{
    std::filesystem::path OutputPath;
    build_profile Profile;
    std::string Role = "bitmap";
};

//-------------------------------------------------------------------------

class product_sink
{
public:
    virtual ~product_sink( void ) = default;

    virtual xbool Write( const product_request& Request, const image& Source, std::string& Error ) = 0;
};

//=========================================================================
//  FACTORIES
//=========================================================================

std::unique_ptr<product_sink> CreateTransactionalProductSink( resource_tool::output_transaction& Transaction,
                                                              resource_tool::artifact_manifest& Artifacts );

std::unique_ptr<product_sink> CreateFilesystemProductSink( void );

xbool DecompileProduct( const std::filesystem::path& SourcePath, const std::filesystem::path& ImageOutputPath,
                        std::string& Error );

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_PRODUCT_HPP
//=========================================================================
