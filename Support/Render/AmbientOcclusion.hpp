//=========================================================================
//
//  AmbientOcclusion.hpp
//
//=========================================================================

#ifndef AMBIENT_OCCLUSION_HPP
#define AMBIENT_OCCLUSION_HPP

#include "x_types.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

enum class AmbientOcclusionQuality : s32
{
    Disabled,
    Low,
    Medium,
    High,
    Ultra
};

constexpr AmbientOcclusionQuality AMBIENT_OCCLUSION_DEFAULT = AmbientOcclusionQuality::High;

} // namespace render

#endif // AMBIENT_OCCLUSION_HPP
