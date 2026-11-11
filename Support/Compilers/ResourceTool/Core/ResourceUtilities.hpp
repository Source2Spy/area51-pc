//=========================================================================
//
//  ResourceUtilities.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RESOURCE_UTILITIES_HPP
#define RESOURCE_TOOL_RESOURCE_UTILITIES_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <filesystem>
#include <string>

namespace resource_tool
{

class resource_context;

//=========================================================================
//  FUNCTIONS
//=========================================================================

xstring LowerExtension( const std::filesystem::path& Path );

xbool ReportError( resource_context& Context, const xstring& Message, const char* pStage );
xbool ReportError( resource_context& Context, const std::string& Message, const char* pStage );

} // namespace resource_tool

//=========================================================================

#endif // RESOURCE_TOOL_RESOURCE_UTILITIES_HPP
//=========================================================================
