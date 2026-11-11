//=========================================================================
//
//  GeometryTextureExport.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_TEXTURE_EXPORT_HPP
#define RESOURCE_TOOL_GEOMETRY_TEXTURE_EXPORT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "ArtifactManifest.hpp"
#include "Diagnostics.hpp"
#include "GeometryDescriptor.hpp"
#include "Model/GeometryAsset.hpp"
#include "Model/GeometryTextureMap.hpp"
#include "OutputTransaction.hpp"

#include <filesystem>
#include <string>

namespace resource_tool::geometry
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_texture_export_stage final
{
public:
    xbool Execute( const geometry_descriptor& Settings, const geometry_asset& Asset,
                   const std::filesystem::path& GltfFile, const std::filesystem::path& RawGeomFile,
                   output_transaction& Transaction, artifact_manifest& Artifacts, diagnostic_log& Diagnostics,
                   texture_export_map& Result, std::string& Error ) const;
};

} // namespace resource_tool::geometry

#endif // RESOURCE_TOOL_GEOMETRY_TEXTURE_EXPORT_HPP
//=========================================================================
