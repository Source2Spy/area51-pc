//=========================================================================
//
//  GeometryTextureMap.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_TEXTURE_MAP_HPP
#define RESOURCE_TOOL_GEOMETRY_TEXTURE_MAP_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace resource_tool::geometry
{

//=========================================================================
//  TYPES
//=========================================================================

struct texture_export_entry
{
    std::string SourcePath;
    std::string GltfUri;
    std::string RawGeomPath;
};

//-------------------------------------------------------------------------

class texture_export_map final
{
public:
    void Add( const std::string& Reference, const std::filesystem::path& ResolvedSource,
              const std::filesystem::path& GltfFile, const std::filesystem::path& RawGeomFile,
              const std::filesystem::path& ExportedFile );

    const texture_export_entry* Find( const std::string& Reference ) const;

private:
    static std::string Key( const std::string& Value );

    std::vector<texture_export_entry> m_Entries;
    std::unordered_map<std::string, usize> m_References;
};

} // namespace resource_tool::geometry

#endif // RESOURCE_TOOL_GEOMETRY_TEXTURE_MAP_HPP
//=========================================================================
