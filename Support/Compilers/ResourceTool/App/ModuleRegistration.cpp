//=========================================================================
//
//  ModuleRegistration.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ModuleRegistration.hpp"

#include "ResourceRegistry.hpp"
#include "GeometryModule.hpp"
#include "BitmapModule.hpp"
#include "AnimationModule.hpp"

#include <memory>

namespace resource_tool
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void RegisterBuiltInModules( resource_registry& Registry )
{
    Registry.Register( std::make_unique<resource_tool::bitmap::module>() );
    Registry.Register( std::make_unique<resource_tool::geometry::module>() );
    Registry.Register( std::make_unique<resource_tool::animation::module>() );
}

} // namespace resource_tool
