//=========================================================================
//
//  RawBitmapWriter.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RAW_BITMAP_WRITER_HPP
#define RESOURCE_TOOL_RAW_BITMAP_WRITER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "RawBitmapDescriptor.hpp"

#include <filesystem>
#include <string>

namespace resource_tool::bitmap_format
{

//=========================================================================
//  TYPES
//=========================================================================

class raw_bitmap_writer final
{
public:
    xbool Write( const std::filesystem::path& FileName, const raw_bitmap_descriptor& Descriptor,
                 std::string& Error ) const;
};

} // namespace resource_tool::bitmap_format

#endif // RESOURCE_TOOL_RAW_BITMAP_WRITER_HPP
//=========================================================================
