//=========================================================================
//
//  GltfReader.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GLTF_READER_HPP
#define RESOURCE_TOOL_GLTF_READER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "GeometrySource.hpp"

#include <string>

namespace gltf_reader
{

//=========================================================================
//  FUNCTIONS
//=========================================================================

xbool LoadMesh( const char* pFileName, geometry_asset& Asset, geometry_source::asset_role Role, f32 UnitScale,
                std::string& Error );

} // namespace gltf_reader

#endif // RESOURCE_TOOL_GLTF_READER_HPP
//=========================================================================
