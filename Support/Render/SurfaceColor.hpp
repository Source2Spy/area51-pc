//=========================================================================
//
//  SurfaceColor.hpp
//
//  Shared rigid/play-surface vertex-color sampling.
//
//=========================================================================

#ifndef SURFACE_COLOR_HPP
#define SURFACE_COLOR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Objects/object.hpp"

//=========================================================================

namespace render
{
xbool SampleSurfaceColor ( object::detail_tri const& triangle, vector3 const& position, xcolor& color );
}

#endif // SURFACE_COLOR_HPP
//=========================================================================
