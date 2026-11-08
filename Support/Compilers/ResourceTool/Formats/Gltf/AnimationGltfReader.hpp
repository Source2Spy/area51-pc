//=========================================================================
//
//  AnimationGltfReader.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_GLTF_READER_HPP
#define RESOURCE_TOOL_ANIMATION_GLTF_READER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include <string>

namespace resource_tool::animation
{
struct asset;
}

namespace resource_tool::animation_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class animation_gltf_reader final
{
public:
    xbool Load( const char* pFileName, resource_tool::animation::asset& Asset, std::string& Error ) const;
};

} // namespace resource_tool::animation_product_format

#endif // RESOURCE_TOOL_ANIMATION_GLTF_READER_HPP
//=========================================================================
