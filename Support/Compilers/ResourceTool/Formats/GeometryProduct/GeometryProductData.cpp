//=========================================================================
//
//  GeometryProductData.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeometryProductData.hpp"

namespace resource_tool::geometry_product_format
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

const char* geometry_product_data::GetMeshName( s32 iMesh ) const
{
    return iMesh >= 0 && iMesh < (s32)Meshes.size() ? Meshes[iMesh].Name.c_str() : "";
}

//=========================================================================

const char* geometry_product_data::GetVMeshName( s32 iVMesh ) const
{
    return iVMesh >= 0 && iVMesh < (s32)VirtualMeshes.size() ? VirtualMeshes[iVMesh].Name.c_str() : "";
}

//=========================================================================

const char* geometry_product_data::GetVTextureName( s32 iVTexture ) const
{
    return iVTexture >= 0 && iVTexture < (s32)VirtualTextures.size() ? VirtualTextures[iVTexture].Name.c_str() : "";
}

//=========================================================================

const char* geometry_product_data::GetTextureDesc( s32 iTexture ) const
{
    return iTexture >= 0 && iTexture < (s32)Textures.size() ? Textures[iTexture].Desc.c_str() : "";
}

//=========================================================================

const char* geometry_product_data::GetTextureName( s32 iTexture ) const
{
    return iTexture >= 0 && iTexture < (s32)Textures.size() ? Textures[iTexture].FileName.c_str() : "";
}

//=========================================================================

const char* geometry_product_data::GetRigidBodyName( s32 iRigidBody ) const
{
    return iRigidBody >= 0 && iRigidBody < (s32)RigidBodies.size() ? RigidBodies[iRigidBody].Name.c_str() : "";
}

} // namespace resource_tool::geometry_product_format
