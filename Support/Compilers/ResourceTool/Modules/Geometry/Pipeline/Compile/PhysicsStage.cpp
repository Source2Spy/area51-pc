//=========================================================================
//
//  PhysicsStage.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PhysicsStage.hpp"

#include "Diagnostics.hpp"
#include "GeometryNames.hpp"

//=========================================================================
//  FILE-LOCAL HELPERS
//=========================================================================

namespace
{

template <typename Item> s32 FindNamed( const std::vector<Item>& Items, const std::string& Name )
{
    s32 Found = -1;
    for( s32 i = 0; i < (s32)Items.size(); ++i )
    {
        if( !geometry_names_match( Items[i].Name.c_str(), Name.c_str() ) )
            continue;
        if( Found >= 0 )
            return -2;
        Found = i;
    }
    return Found;
}

} // namespace

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool resource_tool::geometry::physics_stage::Execute( const geometry_asset& Asset, diagnostic_log& Diagnostics,
                                                       const char* pSourceFile ) const
{
    xbool bValid = TRUE;
    for( const geometry_rigid_body& Body : Asset.Physics.RigidBodies )
    {
        if( Body.Name.empty() )
        {
            Diagnostics.Error( "rigid body has no semantic name", "GeometryPhysics", "rigid-body", pSourceFile );
            bValid = FALSE;
        }
        if( !Body.BoneName.empty() && FindNamed( Asset.Skeleton.Bones, Body.BoneName ) != Body.iBone )
        {
            Diagnostics.Error( xfs( "rigid body [%s] has a bone identity/index mismatch for [%s]", Body.Name.c_str(),
                                    Body.BoneName.c_str() ),
                               "GeometryPhysics", Body.Name.c_str(), pSourceFile );
            bValid = FALSE;
        }
        if( !Body.ParentName.empty() && FindNamed( Asset.Physics.RigidBodies, Body.ParentName ) != Body.iParent )
        {
            Diagnostics.Error( xfs( "rigid body [%s] has a parent identity/index mismatch for [%s]", Body.Name.c_str(),
                                    Body.ParentName.c_str() ),
                               "GeometryPhysics", Body.Name.c_str(), pSourceFile );
            bValid = FALSE;
        }
    }
    return bValid;
}
