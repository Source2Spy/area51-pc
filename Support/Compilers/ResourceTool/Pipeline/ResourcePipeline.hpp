//=========================================================================
//
//  ResourcePipeline.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_PIPELINE_HPP
#define RESOURCE_TOOL_PIPELINE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "ResourceHandler.hpp"
#include <functional>

namespace resource_tool
{

//=========================================================================
//  TYPES
//=========================================================================

struct resource_pipeline_stage
{
    xstring Name;
    std::function<xbool( resource_context& )> Execute;
};

//-------------------------------------------------------------------------

class resource_pipeline
{
public:
    void AddStage( const char* pName, std::function<xbool( resource_context& )> Execute );
    xbool Execute( resource_context& Context ) const;

private:
    xarray<resource_pipeline_stage> m_Stages;
};

} // namespace resource_tool

//=========================================================================

#endif // RESOURCE_TOOL_PIPELINE_HPP
//=========================================================================
