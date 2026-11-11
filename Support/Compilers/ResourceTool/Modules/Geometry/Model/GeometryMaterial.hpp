//=========================================================================
//
//  GeometryMaterial.hpp
//
//=========================================================================

#ifndef RESOURCE_TOOL_GEOMETRY_MATERIAL_HPP
#define RESOURCE_TOOL_GEOMETRY_MATERIAL_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_files.hpp"

#include <array>
#include <string>
#include <vector>

//=========================================================================
//  TYPES
//=========================================================================

enum
{
    GEOMETRY_MATERIAL_MAX_MAPS = 16,
    GEOMETRY_MATERIAL_MAX_CONSTANTS = 8,
    GEOMETRY_PARAM_PKG_MAX_ITEMS = 4,
};

//-------------------------------------------------------------------------

enum geometry_blend_type
{
    GEOMETRY_BLEND_OVERWRITE,
    GEOMETRY_BLEND_ADD,
    GEOMETRY_BLEND_SUB,
    GEOMETRY_BLEND_MUL,
};

//-------------------------------------------------------------------------

enum geometry_lighting_type
{
    GEOMETRY_LIGHTING_STATIC,
    GEOMETRY_LIGHTING_DYNAMIC,
    GEOMETRY_LIGHTING_STATIC_AND_DYNAMIC,
    GEOMETRY_LIGHTING_SELF_ILLUM,
};

//-------------------------------------------------------------------------

enum geometry_tint_type
{
    GEOMETRY_TINT_NONE,
    GEOMETRY_TINT_FIRST_DIFFUSE,
    GEOMETRY_TINT_FINAL_DIFFUSE_OUTPUT,
    GEOMETRY_TINT_FINAL_OUTPUT,
    GEOMETRY_TINT_CUSTOM1,
    GEOMETRY_TINT_CUSTOM2,
};

//-------------------------------------------------------------------------

enum geometry_address_type
{
    GEOMETRY_ADDRESS_WRAP,
    GEOMETRY_ADDRESS_CLAMP,
    GEOMETRY_ADDRESS_MIRROR,
};

//-------------------------------------------------------------------------

enum geometry_filter_type
{
    GEOMETRY_FILTER_BILINEAR,
    GEOMETRY_FILTER_TRILINEAR,
    GEOMETRY_FILTER_POINT,
    GEOMETRY_FILTER_ANISOTROPIC,
};

//-------------------------------------------------------------------------

enum geometry_animation_type
{
    GEOMETRY_ANIM_CLAMP,
    GEOMETRY_ANIM_CYCLE,
    GEOMETRY_ANIM_PING_PONG,
    GEOMETRY_ANIM_LINEAR,
    GEOMETRY_ANIM_RELATIVE_REPEAT,
};

//-------------------------------------------------------------------------

enum geometry_rgba_source_type
{
    GEOMETRY_RGBA_RGB,
    GEOMETRY_RGBA_R,
    GEOMETRY_RGBA_G,
    GEOMETRY_RGBA_B,
    GEOMETRY_RGBA_A,
};

//-------------------------------------------------------------------------

struct geometry_texture
{
    s32 Slot = 0;
    std::string FileName;
    std::string Name;
};

//-------------------------------------------------------------------------

struct geometry_param_pkg
{
    s32 nKeys = 0;
    s32 nParamsPerKey = 0;
    s32 iFirstKey = 0;
    s32 FPS = 0;
    geometry_animation_type ModeType = GEOMETRY_ANIM_CYCLE;
    f32 Current[GEOMETRY_PARAM_PKG_MAX_ITEMS] = { 0, 0, 0, 0 };
};

//-------------------------------------------------------------------------

struct geometry_map
{
    geometry_rgba_source_type RGBASource = GEOMETRY_RGBA_RGB;
    s32 iUVChannel = 0;
    geometry_address_type UAddress = GEOMETRY_ADDRESS_WRAP;
    geometry_address_type VAddress = GEOMETRY_ADDRESS_WRAP;
    geometry_filter_type FilterType = GEOMETRY_FILTER_BILINEAR;
    geometry_param_pkg UVTranslation;
    geometry_param_pkg UVRotation;
    geometry_param_pkg UVScale;
    s32 iTexture = -1;
    s32 nTextures = 0;
    s32 TextureFPS = 0;
};

//-------------------------------------------------------------------------

enum geometry_texture_slot
{
    GEOMETRY_TEXTURE_DIFFUSE = 0,
    GEOMETRY_TEXTURE_ENVIRONMENT,
    GEOMETRY_TEXTURE_DETAIL,
    GEOMETRY_TEXTURE_PUNCH_THROUGH,
};

//-------------------------------------------------------------------------

struct geometry_uv_key
{
    u8 U = 0;
    u8 V = 0;
};

//-------------------------------------------------------------------------

struct geometry_uv_animation
{
    s32 Type = 0;
    s32 StartFrame = 0;
    s32 FPS = 30;
    std::vector<geometry_uv_key> Keys;
};

//-------------------------------------------------------------------------

struct geometry_virtual_texture
{
    std::string Name;
    xbool OverrideDefault = FALSE;
    std::string OverrideFileName;
    std::vector<geometry_texture> Textures;
};

//-------------------------------------------------------------------------

struct geometry_material
{
    std::string Name;
    s32 Type = 0;
    s32 Sort = 0;
    geometry_lighting_type LightingType = GEOMETRY_LIGHTING_STATIC_AND_DYNAMIC;
    geometry_blend_type BlendType = GEOMETRY_BLEND_OVERWRITE;
    geometry_tint_type TintType = GEOMETRY_TINT_FINAL_OUTPUT;
    xbool bTwoSided = FALSE;
    xbool bPunchthrough = FALSE;
    xbool bRandomizeAnim = FALSE;
    xbool bHasVariableVertAlpha = FALSE;
    xbool bExposeName = FALSE;
    geometry_param_pkg TintColor;
    geometry_param_pkg TintAlpha;
    std::array<geometry_param_pkg, GEOMETRY_MATERIAL_MAX_CONSTANTS> Constants;
    f32 DetailScale = 1.0f;
    f32 FixedAlpha = 1.0f;
    s32 ProductFlags = 0;
    std::vector<geometry_texture> Textures;
    geometry_uv_animation UVAnimation;
    s32 nMaps = 0;
    std::array<geometry_map, GEOMETRY_MATERIAL_MAX_MAPS> Map;
    s32 iFirstKey = 0;
    s32 nKeys = 0;
};

//-------------------------------------------------------------------------

struct geometry_material_model
{
    std::vector<geometry_texture> Textures;
    std::vector<geometry_material> Items;
    std::vector<geometry_virtual_texture> VirtualTextures;
    xbool PreserveMaterialEntries = FALSE;

    void Clear( void )
    {
        Textures.clear();
        Items.clear();
        VirtualTextures.clear();
        PreserveMaterialEntries = FALSE;
    }
};

#endif // RESOURCE_TOOL_GEOMETRY_MATERIAL_HPP
//=========================================================================
