//=========================================================================
//
//  GltfWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GLTF_WRITER_HPP
#define RESOURCE_TOOL_GLTF_WRITER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "x_math.hpp"
#include "Model/GeometryAsset.hpp"
#include "Model/GeometryTextureMap.hpp"

#include <string>

namespace resource_tool::animation
{
struct asset;
}

namespace geom_gltf_writer
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool WriteRigid( const geometry_asset& Model, const char* pOutputFile, std::string& Error,
                  const char* pBinaryFileName = NULL, const char* pBinaryUriFileName = NULL,
                  const resource_tool::geometry::texture_export_map* pTextureMap = NULL );
xbool WriteSkin( const geometry_asset& Model, const char* pOutputFile, std::string& Error,
                 const char* pBinaryFileName = NULL, const char* pBinaryUriFileName = NULL,
                 const resource_tool::geometry::texture_export_map* pTextureMap = NULL,
                 const resource_tool::animation::asset* pAnimation = NULL );
xbool WriteCollision( const geometry_asset& Model, const char* pOutputFile, std::string& Error,
                      const char* pBinaryFileName = NULL, const char* pBinaryUriFileName = NULL );

} // namespace geom_gltf_writer

#endif // RESOURCE_TOOL_GLTF_WRITER_HPP
//=========================================================================
