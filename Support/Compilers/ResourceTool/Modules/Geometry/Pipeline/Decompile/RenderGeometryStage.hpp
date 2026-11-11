//=========================================================================
//
//  RenderGeometryStage.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_RENDER_GEOMETRY_STAGE_HPP
#define RESOURCE_TOOL_GEOMETRY_RENDER_GEOMETRY_STAGE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "GltfWriter.hpp"
#include "GeometryDescriptor.hpp"
#include "Model/AnimationAsset.hpp"
#include "Model/GeometryAsset.hpp"

namespace resource_tool
{
namespace geometry
{

//=========================================================================
//  TYPES
//=========================================================================

class render_geometry_stage final
{
public:
    xbool Execute( geometry_descriptor::geometry_type Type, const char* pOutputFile, const char* pBinaryFile,
                   const char* pBinaryUriFile, const geometry_asset& Model, std::string& Error,
                   const texture_export_map* pTextureMap = NULL,
                   const resource_tool::animation::asset* pAnimation = NULL ) const;
};

} // namespace geometry
} // namespace resource_tool

#endif // RESOURCE_TOOL_GEOMETRY_RENDER_GEOMETRY_STAGE_HPP
//=========================================================================
