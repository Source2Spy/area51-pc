//=========================================================================
//
//  RawBitmapReader.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_RAW_BITMAP_READER_HPP
#define RESOURCE_TOOL_RAW_BITMAP_READER_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "RawBitmapDescriptor.hpp"

#include <string>

namespace resource_tool::bitmap_format
{

//=========================================================================
//  TYPES
//=========================================================================

class raw_bitmap_reader final
{
public:
    xbool Load( const std::filesystem::path& FileName, raw_bitmap_descriptor& Descriptor, std::string& Error ) const;
};

} // namespace resource_tool::bitmap_format

#endif // RESOURCE_TOOL_RAW_BITMAP_READER_HPP
//=========================================================================
