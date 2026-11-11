//=========================================================================
//
//  AnimationProductWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_PRODUCT_WRITER_HPP
#define RESOURCE_TOOL_ANIMATION_PRODUCT_WRITER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include <string>

class anim_group;

namespace resource_tool::animation
{
struct asset;
}

namespace resource_tool::animation_product_format
{

//=========================================================================
//  TYPES
//=========================================================================

class animation_product_writer final
{
public:
    xbool Write( const resource_tool::animation::asset& Asset, const char* pOutputFile, std::string& Error ) const;

private:
    xbool BuildGroup( const resource_tool::animation::asset& Asset, ::anim_group& Group, std::string& Error ) const;
    xbool WriteDiskFile( const ::anim_group& Group, const char* pOutputFile, std::string& Error ) const;
};

} // namespace resource_tool::animation_product_format

#endif // RESOURCE_TOOL_ANIMATION_PRODUCT_WRITER_HPP
//=========================================================================
