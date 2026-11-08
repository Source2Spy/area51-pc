//=========================================================================
//
//  RawAnimReader.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RAW_ANIM_READER_HPP
#define RESOURCE_TOOL_RAW_ANIM_READER_HPP

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

class raw_anim_reader final
{
public:
    xbool Load( const char* pFileName, resource_tool::animation::asset& Asset, std::string& Error ) const;
};

} // namespace resource_tool::animation_product_format

#endif // RESOURCE_TOOL_RAW_ANIM_READER_HPP
//=========================================================================
