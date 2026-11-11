//=========================================================================
//
//  PropertyTree.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_PROPERTY_TREE_HPP
#define RESOURCE_TOOL_PROPERTY_TREE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include <filesystem>
#include <string>
#include <vector>

//=========================================================================
//  TYPES
//=========================================================================

namespace resource_tool::property_tree
{

struct property_record
{
    std::string Name;
    std::string Type;
    std::string Value;
};

//-------------------------------------------------------------------------

class document final
{
public:
    void Clear( void );
    void Add( const std::string& Name, const std::string& Type, const std::string& Value );

    const std::vector<property_record>& Records( void ) const;

private:
    std::vector<property_record> m_Records;
};

//-------------------------------------------------------------------------

class reader final
{
public:
    xbool Load( const std::filesystem::path& FileName, document& Document, std::string& Error ) const;
};

//-------------------------------------------------------------------------

class writer final
{
public:
    xbool Write( const std::filesystem::path& FileName, const document& Document, std::string& Error ) const;
};

} // namespace resource_tool::property_tree

#endif // RESOURCE_TOOL_PROPERTY_TREE_HPP
//=========================================================================
