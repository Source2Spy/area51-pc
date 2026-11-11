//=========================================================================
//
//  GeometryProductSnapshot.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_PRODUCT_SNAPSHOT_HPP
#define RESOURCE_TOOL_GEOMETRY_PRODUCT_SNAPSHOT_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"
#include "GeometryProductData.hpp"
#include "GeometryProductEncodingTypes.hpp"

#include <memory>

namespace resource_tool::geometry_product_format
{

class geometry_product_reconstruction;

//=========================================================================
//  TYPES
//=========================================================================

class geometry_product_snapshot final
{
public:
    geometry_product_snapshot( void );
    ~geometry_product_snapshot( void );

    geometry_product_snapshot( const geometry_product_snapshot& ) = delete;
    geometry_product_snapshot& operator=( const geometry_product_snapshot& ) = delete;

    void Reset( void );
    xbool Load( const char* pFileName, geometry_product_kind Kind, std::string& Error );

    s32 TextureCount( void ) const;
    const char* TextureName( s32 iTexture ) const;

private:
    friend class geometry_product_reconstruction;

    geometry_product_kind Kind( void ) const;
    const geometry_product_data& Geometry( void ) const;
    const geometry_product_rigid_data& Rigid( void ) const;
    const geometry_product_skin_data& Skin( void ) const;

    geometry_product_kind m_Kind;
    std::unique_ptr<geometry_product_rigid_data> m_pRigid;
    std::unique_ptr<geometry_product_skin_data> m_pSkin;
};

} // namespace resource_tool::geometry_product_format

#endif // RESOURCE_TOOL_GEOMETRY_PRODUCT_SNAPSHOT_HPP
//=========================================================================
