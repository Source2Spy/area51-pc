//=========================================================================
//
//  GeometryCompileJob.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_COMPILE_JOB_HPP
#define RESOURCE_TOOL_GEOMETRY_COMPILE_JOB_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include "GeometryDescriptor.hpp"

#include <filesystem>

namespace resource_tool
{
class diagnostic_log;
namespace bitmap
{
class product_sink;
}

namespace geometry
{

//=========================================================================
//  TYPES
//=========================================================================

class geometry_compile_job final
{
public:
    xbool Execute( const geometry_descriptor& Settings, const std::filesystem::path& FinalOutput, xbool bVerbose,
                   diagnostic_log& Diagnostics, bitmap::product_sink* pBitmapSink = NULL );
};

} // namespace geometry
} // namespace resource_tool

#endif // RESOURCE_TOOL_GEOMETRY_COMPILE_JOB_HPP
//=========================================================================
