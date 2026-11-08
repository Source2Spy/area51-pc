//=========================================================================
//
//  CollisionGeometryStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "CollisionGeometryStage.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::collision_geometry_stage::Execute( const geometry_asset& Model, const char* pOutputFile,
                                                                  const char* pBinaryFile, const char* pBinaryUriFile,
                                                                  std::string& Error ) const
{
    if( Model.Collision.Meshes.empty() )
    {
        Error = "Canonical geometry contains no collision geometry";
        return FALSE;
    }
    return geom_gltf_writer::WriteCollision( Model, pOutputFile, Error, pBinaryFile, pBinaryUriFile ) != FALSE;
}
