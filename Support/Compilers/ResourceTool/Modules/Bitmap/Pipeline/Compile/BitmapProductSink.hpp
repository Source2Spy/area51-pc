//=========================================================================
//
//  BitmapProductSink.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_BITMAP_PRODUCT_SINK_HPP
#define RESOURCE_TOOL_BITMAP_PRODUCT_SINK_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "BitmapProduct.hpp"

#include <filesystem>
#include <set>
#include <string>

namespace resource_tool
{
class artifact_manifest;
class output_transaction;
} // namespace resource_tool

namespace resource_tool::bitmap
{

//=========================================================================
//  TYPES
//=========================================================================

class transactional_product_sink final : public product_sink
{
public:
    transactional_product_sink( resource_tool::output_transaction& Transaction,
                                resource_tool::artifact_manifest& Artifacts );

    xbool Write( const product_request& Request, const image& Source, std::string& Error ) override;

private:
    resource_tool::output_transaction& m_Transaction;
    resource_tool::artifact_manifest& m_Artifacts;
    std::set<std::filesystem::path> m_Paths;
};

//-------------------------------------------------------------------------

class filesystem_product_sink final : public product_sink
{
public:
    xbool Write( const product_request& Request, const image& Source, std::string& Error ) override;
};

} // namespace resource_tool::bitmap

#endif // RESOURCE_TOOL_BITMAP_PRODUCT_SINK_HPP
//=========================================================================
