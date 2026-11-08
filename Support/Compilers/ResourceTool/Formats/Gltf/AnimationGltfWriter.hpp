//=========================================================================
//
//  AnimationGltfWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_GLTF_WRITER_HPP
#define RESOURCE_TOOL_ANIMATION_GLTF_WRITER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "Model/AnimationAsset.hpp"

#include <string>

namespace resource_tool::animation_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class animation_gltf_writer final
{
public:
    xbool Write( const resource_tool::animation::asset& Asset, const char* pOutputFile, const char* pBinaryFileName,
                 const char* pBinaryUriFileName, std::string& Error ) const;
};

} // namespace resource_tool::animation_product_format

#endif // RESOURCE_TOOL_ANIMATION_GLTF_WRITER_HPP
//=========================================================================
