//=========================================================================
//
//  LightSystem.hpp
//
//=========================================================================

#ifndef LIGHT_SYSTEM_HPP
#define LIGHT_SYSTEM_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_color.hpp"
#include "x_math.hpp"
#include "x_array.hpp"
#include "Texture.hpp"

namespace render
{

//=========================================================================
//  TYPES
//=========================================================================

class LightSystem
{
public:

    enum
    {
        MAX_CHAR_LIGHTS      = 128,
        MAX_DYNAMIC_LIGHTS   = 64,
        MAX_FADING_LIGHTS    = 64,
        MAX_SHADOW_LIGHTS    = MAX_DYNAMIC_LIGHTS + MAX_FADING_LIGHTS,
        MAX_COLLECTED_LIGHTS = 16,
        MAX_SPAD_LIGHTS      = MAX_FADING_LIGHTS + MAX_DYNAMIC_LIGHTS + MAX_CHAR_LIGHTS,
        MAX_LIGHT_BVH_NODES  = ( MAX_SPAD_LIGHTS * 2 ) - 1,
    };

    //-------------------------------------------------------------------------

    enum light_shape
    {
        LIGHT_SHAPE_OMNI = 0,
        LIGHT_SHAPE_SPOT,
    };

    //-------------------------------------------------------------------------

    struct collection_stats
    {
        u32 SceneQueries;
        u32 SceneCandidates;
        u32 SceneHits;
        u32 CharQueries;
        u32 CharCandidates;
        u32 CharHits;
    };

protected:

    struct fading_light
    {
        vector3 Pos;
        f32     Radius;
        xcolor  StartColor;
        xcolor  CurrentColor;
        f32     FadeTime;
        f32     ElapsedTime;
        xbool   Valid;
        f32     InterpolationT;
        f32     Intensity;
        s32     PrevLink;
        s32     NextLink;
        xbool   CastShadows;
    };

    //-------------------------------------------------------------------------

    struct dynamic_light
    {
        vector3 Pos;
        f32     Radius;
        xcolor  Color;
        f32     Intensity;
        vector3 Direction;
        f32     Falloff;
        f32     InnerAngle;
        f32     OuterAngle;
        f32     InnerConeCos;
        f32     OuterConeCos;
        f32     OuterConeSin;
        f32     OuterConeTan;
        f32     ConeCosRangeInv;
        s32     Shape;
        s32     ShadowMapResolution;
        s32     ShadowPriority;
        xbool   CastShadows;
        xbool   IsFlashlight;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;
    };

    //-------------------------------------------------------------------------

    struct dir_light
    {
        vector3 Dir;
        xcolor  Col;
    };

    //-------------------------------------------------------------------------

    struct spad_light
    {
        // Position and radius must remain the first fields.
        // The structure itself must remain 16-byte aligned.
        vector3 Pos;
        f32     Radius;
        xcolor  Color;
        f32     Intensity;
        f32     Falloff;
        f32     Score;
        xbool   CharOnly;
        s32     Shape;
        vector3 Direction;
        f32     InnerConeCos;
        f32     OuterConeCos;
        f32     OuterConeSin;
        f32     OuterConeTan;
        f32     ConeCosRangeInv;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;
        s32     DynamicLightIndex;
    };

    //-------------------------------------------------------------------------

    struct light_bvh_node
    {
        bbox Bounds;
        s32  LeftChild;
        s32  RightChild;
        s32  FirstLight;
        s32  LightCount;
    };

    //=========================================================================

    static s32   SpadLightSortFn           ( void const* pA, void const* pB );
    static xbool CalcDirLight              ( dir_light* pDestination, matrix4 const& LocalToWorld,
                                             bbox const& LocalBBox, bbox const& WorldBBox,
                                             vector3 const& WorldBBoxCenter, vector3 const& Position,
                                             f32 Radius, f32 Intensity, xcolor const& Color,
                                             s32 Shape = LIGHT_SHAPE_OMNI,
                                             vector3 const& Direction = vector3( 0.0f, 0.0f, 1.0f ),
                                             f32 OuterConeCos = 1.0f, f32 ConeCosRangeInv = 0.0f );
    s32          AddLight                  ( void );
    void         RemoveLight               ( s32 LightIndex );
    s32          RegisterSpotLightCookie   ( texture::handle const& Cookie );
    void         InsertCollectedSceneLight ( s32 LightIndex, s32 MaxLightCount );
    void         InsertCollectedCharLight  ( dir_light const& Light, s32 MaxLightCount );
    s32          BuildLightBvhNode         ( s32 FirstLight, s32 LightCount );
    void         BuildLightBvh             ( void );

public:

    LightSystem( void );
    virtual ~LightSystem( void );

    // Add lights once per frame. Fading lights persist, cast shadows by default, and expire automatically.
    void ClearLights     ( vector3 const& ViewPosition );
    void AddFadingLight  ( vector3 const& Position, xcolor const& Color, f32 Radius,
                           f32 Intensity, f32 FadeTime, xbool CastShadows = TRUE );
    void AddDynamicLight ( vector3 const& Position, xcolor const& Color, f32 Radius,
                           f32 Intensity, xbool CharOnly, s32 Shape = LIGHT_SHAPE_OMNI,
                           xbool CastShadows = TRUE, f32 InnerRadius = 0.0f,
                           vector3 const& Direction = vector3( 0.0f, 0.0f, 1.0f ),
                           f32 Falloff = 1.0f, f32 InnerAngle = 30.0f,
                           f32 OuterAngle = 45.0f, s32 ShadowMapResolution = 512,
                           s32 ShadowPriority = 1,
                           texture::handle const& Cookie = texture::handle(),
                           xbool IsFlashlight = FALSE );
    void OnUpdate          ( f32 DeltaTime );
    s32  GetNDynamicLights ( void ) const;
    s32  GetNCharLights    ( void ) const { return m_nCharLights; }
    s32  GetNFadingLights  ( void ) const { return m_nFadingLights; }
    void GetDynamicLight   ( s32 Index, vector3& Position, f32& Radius, xcolor& Color,
                             s32& Shape, xbool& CastShadows, f32& InnerRadius,
                             vector3& Direction, f32& Falloff, f32& InnerAngle,
                             f32& OuterAngle, s32& ShadowMapResolution,
                             s32& ShadowPriority, xbool& IsFlashlight ) const;

    // Sparse shadow IDs: dynamic slots followed by fading slots. Inactive slots return FALSE.
    xbool GetShadowLight   ( s32 Index, vector3& Position, f32& Radius, xcolor& Color,
                             s32& Shape, xbool& CastShadows, f32& InnerRadius,
                             vector3& Direction, f32& Falloff, f32& InnerAngle,
                             f32& OuterAngle, s32& ShadowMapResolution,
                             s32& ShadowPriority, xbool& IsFlashlight ) const;

    // Build frame-local collection data once before querying object lights.
    void                    BeginLightCollection ( void );
    void                    EndLightCollection   ( void );
    void                    ResetAfterException  ( void );
    collection_stats const& GetCollectionStats   ( void ) const;

    // Get scene lights that intersect an object.
    s32                    CollectLights                  ( bbox const& WorldBBox,
                                                            s32 MaxLightCount = 3 );
    void                   GetCollectedLight              ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color );
    void                   GetCollectedLightInfo          ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color,
                                                            f32& Falloff );
    void                   GetCollectedLightInfo          ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color,
                                                            f32& Falloff, s32& Shape,
                                                            vector3& Direction,
                                                            f32& InnerConeCos,
                                                            f32& OuterConeCos );
    void                   GetCollectedLightCookie        ( s32 Index, s32& CookieIndex,
                                                            vector3& CookieU, vector3& CookieV );
    s32                    GetCollectedDynamicLightIndex  ( s32 Index ) const;
    s32                    GetNNonCharLights              ( void ) const;
    void                   GetLight                       ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color ) const;
    s32                    GetLightCookieCount            ( void ) const;
    texture::handle const& GetLightCookieHandle           ( s32 Index ) const;

    // Get directional character lights derived from scene lights.
    s32  CollectCharLights     ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                 s32 MaxLightCount = 3 );
    s32  CollectCharLightsOnly ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                 s32 MaxLightCount = 3 );
    void GetCollectedCharLight ( s32 Index, vector3& Direction, xcolor& Color );

protected:

    // Fading lights.
    s32          m_FirstLink;
    s32          m_nFadingLights;
    fading_light m_FadingLights[MAX_FADING_LIGHTS];

    // Dynamic lights.
    s32                     m_nDynamicLights;
    dynamic_light           m_DynamicLights[MAX_DYNAMIC_LIGHTS];
    s32                     m_nCharLights;
    dynamic_light           m_CharLights[MAX_CHAR_LIGHTS];
    f32                     m_CharLightPriority[MAX_CHAR_LIGHTS];
    vector3                 m_LightCollectionPosition;
    xarray<texture::handle> m_LightCookieFaces;

    // Frame-local collection data.
    xbool           m_isInCollection;
    s32             m_nSpadLights;
    s32             m_nNonCharLightsInSpad;
    spad_light      m_SpadLights[MAX_SPAD_LIGHTS];
    light_bvh_node  m_LightBvhNodes[MAX_LIGHT_BVH_NODES];
    s32             m_LightBvhLightIndices[MAX_SPAD_LIGHTS];
    spad_light*     m_pSpadLights;
    light_bvh_node* m_pLightBvhNodes;
    s32*            m_pLightBvhLightIndices;
    s32             m_nLightBvhNodes;

    s32              m_nCollectedLights;
    s32              m_CollectedLights[MAX_COLLECTED_LIGHTS];
    dir_light        m_CollectedCharLights[MAX_COLLECTED_LIGHTS];
    collection_stats m_CollectionStats;

    // Collection data is immutable while a render scope is executing. Keep
    // the spatial set cached for matching main/PIP scopes.
    u32             m_CachedCollectionKey;
    s32             m_CachedSpadLights;
    s32             m_CachedNonCharLights;
    s32             m_CachedLightBvhNodes;
    xbool           m_CachedCollectionValid;
};

//=========================================================================
//  PIPELINE-OWNED SERVICE
//=========================================================================

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline void LightSystem::ClearLights( vector3 const& ViewPosition )
{
    for( s32 Index = 0; Index < m_LightCookieFaces.GetCount(); Index++ )
    {
        m_LightCookieFaces[Index].Destroy();
    }

    m_nDynamicLights = 0;
    m_nCharLights = 0;
    m_LightCollectionPosition = ViewPosition;
    m_LightCookieFaces.SetCount( 0 );
}

//=========================================================================

inline s32 LightSystem::GetNDynamicLights( void ) const
{
    return m_nDynamicLights;
}

//=========================================================================

inline LightSystem::collection_stats const& LightSystem::GetCollectionStats( void ) const
{
    return m_CollectionStats;
}

//=========================================================================

inline s32 LightSystem::GetNNonCharLights( void ) const
{
    ASSERT( m_isInCollection );

    return m_nNonCharLightsInSpad;
}

//=========================================================================

inline s32 LightSystem::GetLightCookieCount( void ) const
{
    return m_LightCookieFaces.GetCount();
}

//=========================================================================

inline texture::handle const& LightSystem::GetLightCookieHandle( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_LightCookieFaces.GetCount() ) );
    return m_LightCookieFaces[Index];
}

//=========================================================================
} // namespace render

#endif // LIGHT_SYSTEM_HPP
//=========================================================================
