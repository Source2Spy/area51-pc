//=========================================================================
//
//  ResourceRegistry.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RESOURCE_REGISTRY_HPP
#define RESOURCE_TOOL_RESOURCE_REGISTRY_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "ResourceHandler.hpp"

#include <memory>
#include <vector>

namespace resource_tool
{

//=========================================================================
//  TYPES
//=========================================================================

class resource_registry
{
public:
    void Register( std::unique_ptr<resource_handler> Handler );
    resource_handler* Resolve( const resource_request& Request ) const;

private:
    std::vector<std::unique_ptr<resource_handler>> m_Handlers;
};

} // namespace resource_tool

#endif // RESOURCE_TOOL_RESOURCE_REGISTRY_HPP
//=========================================================================
