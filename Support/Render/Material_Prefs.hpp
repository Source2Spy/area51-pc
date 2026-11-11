//=========================================================================
//
//  Material_Prefs.hpp
//
//=========================================================================

#ifndef MATERIAL_PREFS_HPP
#define MATERIAL_PREFS_HPP

//=========================================================================
//  ENUMS
//=========================================================================

enum material_type
{
    Material_Not_Used,

    Material_Diff,
    Material_Alpha,
    Material_Diff_PerPixelEnv,
    Material_Diff_PerPixelIllum,
    Material_Alpha_PerPolyEnv,
    Material_Alpha_PerPixelIllum,
    Material_Alpha_PerPolyIllum,
    Material_Distortion,
    Material_Distortion_PerPolyEnv,

    Material_NumTypes,
};

//-------------------------------------------------------------------------

// Material flags stored in geometry data. Geometry keeps aliases for format
// compatibility, while material owns their render semantics.
enum material_flags
{
    MATERIAL_FLAG_DOUBLE_SIDED       = 0x0001,
    MATERIAL_FLAG_HAS_ENV_MAP        = 0x0002,
    MATERIAL_FLAG_HAS_DETAIL_MAP     = 0x0004,
    MATERIAL_FLAG_ENV_WORLD_SPACE    = 0x0008,
    MATERIAL_FLAG_ENV_VIEW_SPACE     = 0x0010,
    MATERIAL_FLAG_ENV_CUBE_MAP       = 0x0020,
    MATERIAL_FLAG_FORCE_ZFILL        = 0x0040,
    MATERIAL_FLAG_ILLUM_USES_DIFFUSE = 0x0080,
    MATERIAL_FLAG_IS_PUNCH_THRU      = 0x0100,
    MATERIAL_FLAG_IS_ADDITIVE        = 0x0200,
    MATERIAL_FLAG_IS_SUBTRACTIVE     = 0x0400,
};

//=========================================================================
#endif  // MATERIAL_PREFS_HPP
//=========================================================================
