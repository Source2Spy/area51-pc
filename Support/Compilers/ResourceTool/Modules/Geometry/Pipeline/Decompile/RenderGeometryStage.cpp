//=========================================================================
//
//  RenderGeometryStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RenderGeometryStage.hpp"

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::render_geometry_stage::Execute( geometry_descriptor::geometry_type Type,
                                                               const char* pOutputFile, const char* pBinaryFile,
                                                               const char* pBinaryUriFile, const geometry_asset& Model,
                                                               std::string& Error,
                                                               const texture_export_map* pTextureMap,
                                                               const resource_tool::animation::asset* pAnimation ) const
{
    if( Type == geometry_descriptor::GEOMETRY_TYPE_RIGID )
    {
        return geom_gltf_writer::WriteRigid( Model, pOutputFile, Error, pBinaryFile, pBinaryUriFile, pTextureMap ) !=
               FALSE;
    }
    if( Type == geometry_descriptor::GEOMETRY_TYPE_SKIN )
    {
        return geom_gltf_writer::WriteSkin( Model, pOutputFile, Error, pBinaryFile, pBinaryUriFile, pTextureMap,
                                            pAnimation ) != FALSE;
    }
    Error = "Geometry type must be Rigid or Skin for render export";
    return FALSE;
}
