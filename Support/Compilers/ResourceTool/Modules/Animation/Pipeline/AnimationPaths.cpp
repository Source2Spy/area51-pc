//=========================================================================
//
//  AnimationPaths.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "AnimationPaths.hpp"

namespace resource_tool::animation
{

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

std::filesystem::path MakeOutputPath( const std::filesystem::path& Input, const char* pExtension )
{
    std::filesystem::path Result = Input;
    Result.replace_extension( pExtension );
    return Result.lexically_normal();
}

} // namespace resource_tool::animation
