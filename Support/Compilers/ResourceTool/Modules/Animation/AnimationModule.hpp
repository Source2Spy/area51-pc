//=========================================================================
//
//  AnimationModule.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_MODULE_HPP
#define RESOURCE_TOOL_ANIMATION_MODULE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "ResourceHandler.hpp"

namespace resource_tool::animation
{

//=========================================================================
//  TYPES
//=========================================================================

class module final : public resource_tool::resource_handler
{
public:
    xbool CanHandle( const resource_tool::resource_request& Request ) const override;
    xbool DiscoverDependencies( resource_tool::resource_context& Context ) override;
    xbool Execute( resource_tool::resource_context& Context ) override;

private:
    xbool Compile( resource_tool::resource_context& Context );
    xbool Decompile( resource_tool::resource_context& Context );
};

} // namespace resource_tool::animation

#endif // RESOURCE_TOOL_ANIMATION_MODULE_HPP
//=========================================================================
