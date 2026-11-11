//=========================================================================
//
//  RawAnimWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RAW_ANIM_WRITER_HPP
#define RESOURCE_TOOL_RAW_ANIM_WRITER_HPP

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

class raw_anim_writer final
{
public:
    xbool Write( const resource_tool::animation::asset& Asset, const char* pOutputFile, std::string& Error ) const;
};

} // namespace resource_tool::animation_product_format

#endif // RESOURCE_TOOL_RAW_ANIM_WRITER_HPP
//=========================================================================
