//==========================================================================
//
//  RenderScale.hpp
//
//==========================================================================

#ifndef RENDER_SCALE_HPP
#define RENDER_SCALE_HPP

//==========================================================================
//  INCLUDES
//==========================================================================

#include "x_types.hpp"

//==========================================================================
//  CONSTANTS
//==========================================================================

constexpr s32 RENDER_SCALE_MIN_PERCENT     = 25;
constexpr s32 RENDER_SCALE_MAX_PERCENT     = 200;
constexpr s32 RENDER_SCALE_DEFAULT_PERCENT = 100;
constexpr f32 RENDER_SCALE_MIN             = static_cast<f32>( RENDER_SCALE_MIN_PERCENT ) / 100.0f;
constexpr f32 RENDER_SCALE_MAX             = static_cast<f32>( RENDER_SCALE_MAX_PERCENT ) / 100.0f;
constexpr f32 RENDER_SCALE_DEFAULT         = static_cast<f32>( RENDER_SCALE_DEFAULT_PERCENT ) / 100.0f;

#endif // RENDER_SCALE_HPP
