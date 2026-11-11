//=========================================================================
//
//  ResourceRegistry.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ResourceRegistry.hpp"

#include <utility>

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void resource_registry::Register( std::unique_ptr<resource_handler> Handler )
{
    if( Handler )
        m_Handlers.push_back( std::move( Handler ) );
}

//=========================================================================

resource_handler* resource_registry::Resolve( const resource_request& Request ) const
{
    for( const std::unique_ptr<resource_handler>& Handler : m_Handlers )
    {
        if( Handler && Handler->CanHandle( Request ) )
            return Handler.get();
    }
    return NULL;
}

} // namespace resource_tool
