//=========================================================================
//
//  RigidColor.hpp
//
//=========================================================================

#ifndef RIGID_COLOR_HPP
#define RIGID_COLOR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

//=========================================================================
//  RIGID COLOR DATA
//=========================================================================

struct RigidColorData
{
    xarray<u32> Colors;
};

//=========================================================================
//  RIGID COLOR FILE
//=========================================================================

namespace rigid_color_file
{
xbool Load( X_FILE* pFile, RigidColorData& data, xstring& error );
xbool Save( X_FILE* pFile, RigidColorData const& data, xstring& error );
xbool Save( char const* pFileName, RigidColorData const& data, xstring& error );
} // namespace rigid_color_file

//=========================================================================

#endif // RIGID_COLOR_HPP
//=========================================================================
