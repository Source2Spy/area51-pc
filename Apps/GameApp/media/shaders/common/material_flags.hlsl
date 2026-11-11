//==============================================================================
//
//  material_flags.hlsl
//
//  Shared shader flag definitions.
//
//==============================================================================
//  DEFINES
//==============================================================================

#define MATERIAL_FLAG_ALPHA_TEST             ( 1u << 0 )
#define MATERIAL_FLAG_ADDITIVE               ( 1u << 1 )
#define MATERIAL_FLAG_SUBTRACTIVE            ( 1u << 2 )
#define MATERIAL_FLAG_VERTEX_COLOR           ( 1u << 3 )
#define MATERIAL_FLAG_ENVIRONMENT            ( 1u << 5 )
#define MATERIAL_FLAG_DISTORTION             ( 1u << 6 )
#define MATERIAL_FLAG_DISTORTION_PERPOLY_ENV ( 1u << 7 )
#define MATERIAL_FLAG_DIFF_PERPIXEL_ILLUM    ( 1u << 8 )
#define MATERIAL_FLAG_ALPHA_PERPIXEL_ILLUM   ( 1u << 9 )
#define MATERIAL_FLAG_ALPHA_PERPOLY_ILLUM    ( 1u << 10 )
#define MATERIAL_FLAG_DIFF_PERPIXEL_ENV      ( 1u << 11 )
#define MATERIAL_FLAG_ALPHA_PERPOLY_ENV      ( 1u << 12 )
#define MATERIAL_FLAG_DETAIL                 ( 1u << 13 )
#define MATERIAL_FLAG_ENV_CUBEMAP            ( 1u << 14 )
#define MATERIAL_FLAG_ENV_VIEWSPACE          ( 1u << 15 )
#define MATERIAL_FLAG_ENV_WORLDSPACE         ( 1u << 16 )

//------------------------------------------------------------------------------

#define GEOM_INST_FLAG_GLOWING                    ( 1u << 20 )
#define GEOM_INST_FLAG_RECEIVE_LOCAL_SHADOW       ( 1u << 21 )
#define GEOM_INST_FLAG_FILTER_LIGHT               ( 1u << 22 )
#define GEOM_INST_FLAG_PROJ_LIGHT                 ( 1u << 23 )
#define GEOM_INST_FLAG_PROJ_SHADOW                ( 1u << 27 )

//------------------------------------------------------------------------------

#define MAX_PROJ_LIGHTS  4
#define MAX_PROJ_SHADOWS 8
#define MAX_SKIN_BONES 96
#define MAX_GEOM_LIGHTS 4
