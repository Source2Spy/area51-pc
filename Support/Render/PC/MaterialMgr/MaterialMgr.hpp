//==============================================================================
//
//  MaterialMgr.hpp
//
//  Material Manager for PC platform
//
//==============================================================================

#ifndef MATERIAL_MANAGER_HPP
#define MATERIAL_MANAGER_HPP

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../../Texture.hpp"
#include "../../ProjTextureMgr.hpp"
#include "../../Material_Prefs.hpp"
#include "../../Material.hpp"
#include "../../Render.hpp"

#include "e_engine.hpp"

#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_rtarget.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define MAX_SKIN_BONES 96
#define MAX_GEOM_LIGHTS 4

//==============================================================================
//  MATERIAL FLAGS
//==============================================================================

enum material_flags
{
    MATERIAL_FLAG_ALPHA_TEST        = (1<<0),
    MATERIAL_FLAG_ADDITIVE          = (1<<1),
    MATERIAL_FLAG_SUBTRACTIVE       = (1<<2),
    MATERIAL_FLAG_VERTEX_COLOR      = (1<<3),
    MATERIAL_FLAG_TWO_SIDED         = (1<<4),
    MATERIAL_FLAG_ENVIRONMENT       = (1<<5),
    MATERIAL_FLAG_DISTORTION        = (1<<6),
    MATERIAL_FLAG_PERPIXEL_ILLUM    = (1<<7),
    MATERIAL_FLAG_PERPOLY_ILLUM     = (1<<8),
    MATERIAL_FLAG_PERPIXEL_ENV      = (1<<9),
    MATERIAL_FLAG_PERPOLY_ENV       = (1<<10),
    MATERIAL_FLAG_DETAIL            = (1<<11),
    MATERIAL_FLAG_ENV_CUBEMAP       = (1<<12),
    MATERIAL_FLAG_ENV_VIEWSPACE     = (1<<13),
    MATERIAL_FLAG_ENV_WORLDSPACE    = (1<<14),	
    MATERIAL_FLAG_ALPHA_BLEND       = (1<<15),	
    MATERIAL_FLAG_ILLUM_USE_DIFFUSE = (1<<16),	
};

//------------------------------------------------------------------------------

enum instance_flags
{
    INSTANCE_FLAG_PROJ_LIGHT        = (1<<0),
    INSTANCE_FLAG_PROJ_SHADOW       = (1<<1),
    INSTANCE_FLAG_DETAIL            = (1<<2),
    INSTANCE_FLAG_FILTERLIGHT       = (1<<3),
    INSTANCE_FLAG_CLIPPED           = (1<<4),
    INSTANCE_FLAG_SHADOW_PASS       = (1<<5),
    INSTANCE_FLAG_GLOWING           = (1<<6),
    INSTANCE_FLAG_FADING_ALPHA      = (1<<7),	
};

//==============================================================================
//  CONSTANT BUFFER STRUCTURES
//==============================================================================

struct d3d_lighting
{
    s32     LightCount;
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 AmbCol;
};

//------------------------------------------------------------------------------

struct cb_geom_frame
{
    matrix4 View;              // World to view matrix
    matrix4 Projection;        // View to clip matrix

    vector4 MaterialParams;    // x = flags, y = alpha ref, z = nearZ, w = farZ
    vector4 UVAnim;            // xy = uv animation offsets, z = detail scale
    vector4 CameraPosition;    // xyz = camera position, w = 1
    vector4 EnvParams;         // x = fixed alpha, y = cubemap intensity, zw = unused
};

//------------------------------------------------------------------------------

struct cb_geom_object
{
    matrix4 World;             // Local to world matrix
};

//------------------------------------------------------------------------------

struct cb_proj_textures
{
    matrix4 ProjLightMatrix [proj_texture_mgr::MAX_PROJ_LIGHTS];
    matrix4 ProjShadowMatrix[proj_texture_mgr::MAX_PROJ_SHADOWS];
    u32     ProjLightCount;
    u32     ProjShadowCount;
    f32     EdgeSize;
    f32     Padding[3];
};

//------------------------------------------------------------------------------

struct cb_lighting
{
    vector4 LightVec[MAX_GEOM_LIGHTS];
    vector4 LightCol[MAX_GEOM_LIGHTS];
    vector4 LightAmbCol;
    u32     LightCount;
    f32     Padding[3];
};

//------------------------------------------------------------------------------

struct cb_skin_bone
{
    matrix4 L2W;                               // Local to world matrix
};

//==============================================================================
//  HELPER FUNCS
//==============================================================================

inline f32 ComputeCubeMapIntensity( const material* pMaterial )
{
    const f32 kDefaultCubeMapIntensity = 0.35f;

    if( !pMaterial )
        return 1.0f;

    if( !(pMaterial->m_Flags & geom::material::FLAG_ENV_CUBE_MAP) )
        return 1.0f;

    const s8 materialType = pMaterial->m_Type;
    if( (materialType == Material_Alpha_PerPolyEnv) ||
        (materialType == Material_Distortion_PerPolyEnv) )
    {
        return 1.0f;
    }

    f32 cubeIntensity = pMaterial->m_FixedAlpha;

    if( cubeIntensity <= 0.0f )
        cubeIntensity = kDefaultCubeMapIntensity;

    if( cubeIntensity < 0.0f )
        cubeIntensity = 0.0f;
    if( cubeIntensity > 1.0f )
        cubeIntensity = 1.0f;

    return cubeIntensity;
}

//==============================================================================
//  MATERIAL TYPES
//==============================================================================

enum texture_slot
{
    TEXTURE_SLOT_DIFFUSE     = 0,
    TEXTURE_SLOT_DETAIL      = 1,
    TEXTURE_SLOT_ENVIRONMENT = 2,
    TEXTURE_SLOT_ENVIRONMENT_CUBE = TEXTURE_SLOT_ENVIRONMENT + 1,
};

//==============================================================================
//  MATERIAL MANAGER CLASS
//==============================================================================

class material_mgr
{
public:

    void        Init                ( void );
    void        Kill                ( void );

    // Rigid material management
    void        SetRigidMaterial    ( const matrix4*      pL2W,
                                      const bbox*         pBBox,
                                      const d3d_lighting* pLighting,
                                      const material*     pMaterial,
                                      u32                 RenderFlags,
                                      u8                  UOffset,
                                      u8                  VOffset,
                                      u8                  Alpha = 255,
									  u8                  OverrideMat = FALSE );

    // Skin material management
    void        SetSkinMaterial     ( const matrix4*      pL2W,
                                      const bbox*         pBBox,
                                      const d3d_lighting* pLighting,
                                      const material*     pMaterial,
                                      u32                 RenderFlags,
                                      u8                  UOffset,
                                      u8                  VOffset,
                                      u8                  Alpha = 255,
									  u8                  OverrideMat = FALSE );

    void        ResetProjTextures   ( void );

    // General material functions
    void        SetBitmap           ( const xbitmap* pBitmap,
                                      texture_slot   slot );
    void        SetEnvironmentCubemap( const cubemap* pCubemap );
    void        InvalidateCache     ( void );

    // Resource access for vertex managers
    ID3D11Buffer* GetSkinBoneBuffer ( void );

protected:

    struct material_constants
    {
        u32 Flags;
        f32 AlphaRef;
    };

    void        ApplyRenderStates ( const material* pMaterial,
                                    u32             RenderFlags );

    // Shader initialization
    xbool       InitRigidShaders    ( void );
    void        KillRigidShaders    ( void );
    xbool       InitSkinShaders     ( void );
    void        KillSkinShaders     ( void );
    xbool       InitProjTextures    ( void );
    void        KillProjTextures    ( void );

    // Internal helpers
    xbool               UpdateRigidConstants   ( const matrix4*      pL2W,
                                                 const material*     pMaterial,
                                                 u32                 RenderFlags,
                                                 const d3d_lighting* pLighting,
                                                 u8                  UOffset,
                                                 u8                  VOffset,
                                                 u8                  Alpha = 255,
									             u8                  OverrideMat = FALSE );
    xbool               UpdateSkinConstants    ( const d3d_lighting* pLighting,
                                                 const material*     pMaterial,
                                                 u32                 RenderFlags,
                                                 u8                  UOffset,
                                                 u8                  VOffset,
                                                 u8                  Alpha = 255,
									             u8                  OverrideMat = FALSE );
    xbool               UpdateProjTextures     ( const matrix4& L2W,
                                                 const bbox&    B,
                                                 u32            Slot,
                                                 u32            RenderFlags );
    material_constants  BuildMaterialFlags     ( const material* pMaterial,
                                                 u32             RenderFlags,
                                                 xbool           IncludeVertexColor ) const;
    u32                 BuildInstanceFlags     ( u32 RenderFlags ) const;												 
    cb_lighting         BuildLightingConstants ( const d3d_lighting* pLighting ) const;

protected:

    // Initialization tracking
    xbool       m_bInitialized;

    // Rigid geometry resources
    ID3D11VertexShader*     m_pRigidVertexShader;
    ID3D11PixelShader*      m_pRigidPixelShader;
    ID3D11InputLayout*      m_pRigidInputLayout;
    ID3D11Buffer*           m_pRigidFrameBuffer;
    ID3D11Buffer*           m_pRigidObjectBuffer;
    ID3D11Buffer*           m_pRigidLightBuffer;

    // Proj textures resources
    ID3D11Buffer*           m_pProjTextureBuffer;
    ID3D11SamplerState*     m_pProjSampler;

    // Skin geometry resources
    ID3D11VertexShader*     m_pSkinVertexShader;
    ID3D11PixelShader*      m_pSkinPixelShader;
    ID3D11InputLayout*      m_pSkinInputLayout;
    ID3D11Buffer*           m_pSkinFrameBuffer;
    ID3D11Buffer*           m_pSkinBoneBuffer;
    ID3D11Buffer*           m_pSkinLightBuffer;

    // Current state tracking for caching
    const xbitmap*          m_pCurrentTexture;
    const xbitmap*          m_pCurrentDetailTexture;
    const xbitmap*          m_pCurrentEnvironmentTexture;
    const cubemap*          m_pCurrentEnvCubemap;
    xbool                   m_bZTestEnabled;

    // Cached constant buffer data to avoid redundant updates
    cb_geom_frame           m_CachedRigidFrame;
    cb_geom_object          m_CachedRigidObject;
    cb_lighting             m_CachedRigidLighting;
    cb_geom_frame           m_CachedSkinFrame;
    cb_lighting             m_CachedSkinLighting;
    u32                     m_LastProjLightCount;
    u32                     m_LastProjShadowCount;
    xbool                   m_bRigidFrameDirty;
    xbool                   m_bRigidObjectDirty;
    xbool                   m_bRigidLightingDirty;
    xbool                   m_bSkinFrameDirty;
    xbool                   m_bSkinLightingDirty;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern material_mgr g_MaterialMgr;

//==============================================================================
//  END
//==============================================================================
#endif