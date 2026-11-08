//=========================================================================
//
//  DependencyGraph.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_DEPENDENCY_GRAPH_HPP
#define RESOURCE_TOOL_DEPENDENCY_GRAPH_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include <filesystem>

namespace resource_tool
{

//=========================================================================
//  TYPES
//=========================================================================

enum class dependency_kind
{
    Source,
    Pipeline,
    Product,
};

//-------------------------------------------------------------------------

struct dependency
{
    dependency_kind Kind = dependency_kind::Source;
    std::filesystem::path Path;
};

//-------------------------------------------------------------------------

class dependency_graph
{
public:
    void Add( dependency_kind Kind, const std::filesystem::path& Path );
    const xarray<dependency>& Entries( void ) const;

private:
    xarray<dependency> m_Entries;
};

} // namespace resource_tool

//=========================================================================

#endif // RESOURCE_TOOL_DEPENDENCY_GRAPH_HPP
//=========================================================================
