//=========================================================================
//
//  DependencyGraph.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "DependencyGraph.hpp"

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void dependency_graph::Add( dependency_kind Kind, const std::filesystem::path& Path )
{
    const std::filesystem::path Normalized = Path.lexically_normal();
    for( s32 iEntry = 0; iEntry < m_Entries.GetCount(); ++iEntry )
    {
        const dependency& Entry = m_Entries[iEntry];
        if( Entry.Kind == Kind && Entry.Path == Normalized )
            return;
    }

    dependency Entry;
    Entry.Kind = Kind;
    Entry.Path = Normalized;
    m_Entries.Append( Entry );
}

//=========================================================================

const xarray<dependency>& dependency_graph::Entries( void ) const
{
    return m_Entries;
}

} // namespace resource_tool
