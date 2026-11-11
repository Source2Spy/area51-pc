//=========================================================================
//
//  GeometryNames.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_NAMES_HPP
#define RESOURCE_TOOL_GEOMETRY_NAMES_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <string>

//=========================================================================
//  FUNCTIONS
//=========================================================================

inline xbool geometry_names_match( const char* pExpected, const char* pActual )
{
    if( !pExpected || !pActual )
        return FALSE;

    std::string Expected = pExpected;
    std::string Actual = pActual;
    auto StripBlenderSuffix = []( std::string& Name )
    {
        if( Name.size() < 4 || Name[Name.size() - 4] != '.' )
            return;
        for( usize i = Name.size() - 3; i < Name.size(); ++i )
        {
            if( Name[i] < '0' || Name[i] > '9' )
                return;
        }
        Name.resize( Name.size() - 4 );
    };

    StripBlenderSuffix( Expected );
    StripBlenderSuffix( Actual );
    return !x_stricmp( Expected.c_str(), Actual.c_str() );
}

#endif // RESOURCE_TOOL_GEOMETRY_NAMES_HPP
//=========================================================================
