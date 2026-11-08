//=========================================================================
//
//  AnimationDecompileJob.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_ANIMATION_DECOMPILE_JOB_HPP
#define RESOURCE_TOOL_ANIMATION_DECOMPILE_JOB_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "ResourceHandler.hpp"

//=========================================================================
//  TYPES
//=========================================================================

class animation_decompile_job final
{
public:
    xbool Execute( resource_tool::resource_context& Context ) const;
};

#endif // RESOURCE_TOOL_ANIMATION_DECOMPILE_JOB_HPP
//=========================================================================
