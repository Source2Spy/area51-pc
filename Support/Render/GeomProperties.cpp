//=========================================================================
//
//  GeomProperties.cpp
//
//  Serialized geometry property lookup shared with resource tools.
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "geom.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

geom::property_section const* geom::FindPropertySection( char const* pSection ) const
{
    ASSERT( pSection );

    for( s32 iSection = 0; iSection < m_nPropertySections; ++iSection )
    {
        const property_section& Section = m_pPropertySections[iSection];
        if( x_strcmp( m_pStringData + Section.NameOffset, pSection ) == 0 )
        {
            return &Section;
        }
    }

    return NULL;
}

//=========================================================================

geom::property const* geom::FindProperty( property_section const* pSection, char const* pName,
                                          geom::property::type Type ) const
{
    ASSERT( pSection );
    ASSERT( pSection >= m_pPropertySections );
    ASSERT( pSection < m_pPropertySections + m_nPropertySections );

    for( s32 i = 0; i < pSection->nProperties; ++i )
    {
        const property& Property = m_pProperties[pSection->iProperty + i];
        if( Property.Type == Type && x_strcmp( m_pStringData + Property.NameOffset, pName ) == 0 )
        {
            return &Property;
        }
    }

    return NULL;
}

//=========================================================================
