//=========================================================================
//
//  LightSystem.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "LightSystem.hpp"
#ifndef X_RETAIL
#include "Pipeline/RenderPipeline.hpp"
#endif

namespace render
{

//=========================================================================
//  PIPELINE-OWNED SERVICE
//=========================================================================

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
f32 ComputeLightScore( xcolor const& Color, f32 Intensity )
{
    f32 const R = static_cast<f32>( Color.R ) * Intensity;
    f32 const G = static_cast<f32>( Color.G ) * Intensity;
    f32 const B = static_cast<f32>( Color.B ) * Intensity;

    return R * R + G * G + B * B;
}

//=========================================================================

static
void HashMemory( u32& Hash, void const* pData, s32 ByteCount )
{
    u8 const* pBytes = static_cast<u8 const*>( pData );
    for( s32 i = 0; i < ByteCount; ++i )
    {
        Hash ^= pBytes[i];
        Hash *= 16777619u;
    }
}

//=========================================================================

static
void ScaleLightColor( xcolor& Destination, xcolor const& Source, f32 Intensity )
{
    Destination.R = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.R ) * Intensity ) );
    Destination.G = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.G ) * Intensity ) );
    Destination.B = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.B ) * Intensity ) );
    Destination.A = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.A ) * Intensity ) );
}

//=========================================================================

static
f32 ComputeSpotConeAttenuation( vector3 const& LightPosition, vector3 const& LightDirection, f32 OuterConeCos,
                                f32 ConeCosRangeInv, vector3 const& TargetPosition )
{
    vector3 ToTarget = TargetPosition - LightPosition;
    if( !ToTarget.SafeNormalize() )
    {
        return 1.0f;
    }

    f32 const SpotCos = LightDirection.Dot( ToTarget );

    return MINMAX( 0.0f, ( SpotCos - OuterConeCos ) * ConeCosRangeInv, 1.0f );
}

//=========================================================================

static
xbool SphereIntersectsFiniteCone( vector3 const& SphereCenter, f32 SphereRadius, vector3 const& ConeApex,
                                  vector3 const& ConeAxis, f32 ConeOuterCos, f32 ConeOuterSin, f32 ConeOuterTan,
                                  f32 ConeLength )
{
    ASSERT( ConeOuterCos > 0.0f );

    vector3 const ToCenter = SphereCenter - ConeApex;
    f32 const     CenterDistSq = ToCenter.Dot( ToCenter );
    f32 const     AxialDistance = ConeAxis.Dot( ToCenter );
    f32 const     SphereRadiusSq = SphereRadius * SphereRadius;

    if( AxialDistance < -SphereRadius )
    {
        return FALSE;
    }

    if( AxialDistance > ( ConeLength + SphereRadius ) )
    {
        return FALSE;
    }

    if( AxialDistance <= 0.0f )
    {
        return ( CenterDistSq <= SphereRadiusSq );
    }

    f32 const RadialDistanceSq = MAX( CenterDistSq - ( AxialDistance * AxialDistance ), 0.0f );
    f32 const RadialDistance = x_sqrt( RadialDistanceSq );

    if( AxialDistance < ConeLength )
    {
        f32 const SideDistance = ( RadialDistance * ConeOuterCos ) - ( AxialDistance * ConeOuterSin );
        if( SideDistance <= SphereRadius )
        {
            return TRUE;
        }
    }

    f32 const ConeCapRadius = ConeLength * ConeOuterTan;
    f32 const CapAxialDelta = MAX( AxialDistance - ConeLength, 0.0f );
    f32 const CapRadialDelta = MAX( RadialDistance - ConeCapRadius, 0.0f );
    return ( ( CapAxialDelta * CapAxialDelta ) + ( CapRadialDelta * CapRadialDelta ) ) <= SphereRadiusSq;
}

//=========================================================================

static
xbool SpotLightAffectsSphere( vector3 const& LightPosition, vector3 const& LightDirection, f32 OuterConeCos,
                              f32 OuterConeSin, f32 OuterConeTan, f32 LightRadius, vector3 const& SphereCenter,
                              f32 SphereRadius )
{
    if( OuterConeCos <= 0.0f )
    {
        return TRUE;
    }

    return SphereIntersectsFiniteCone( SphereCenter, SphereRadius, LightPosition, LightDirection,
                                       OuterConeCos, OuterConeSin, OuterConeTan, LightRadius );
}

//=========================================================================

static
void SetupLightCone( f32 InnerAngle, f32 OuterAngle, f32& InnerConeCos, f32& OuterConeCos, f32& OuterConeSin,
                     f32& OuterConeTan, f32& ConeCosRangeInv )
{
    InnerConeCos = x_cos( DEG_TO_RAD( InnerAngle ) * 0.5f );
    OuterConeCos = x_cos( DEG_TO_RAD( OuterAngle ) * 0.5f );

    f32 const ConeCosRange = MAX( InnerConeCos - OuterConeCos, 0.0001f );
    ConeCosRangeInv = 1.0f / ConeCosRange;

    if( OuterConeCos > 0.0f )
    {
        OuterConeSin = x_sqrt( MAX( 1.0f - ( OuterConeCos * OuterConeCos ), 0.0f ) );
        OuterConeTan = OuterConeSin / OuterConeCos;
    }
    else
    {
        OuterConeSin = 0.0f;
        OuterConeTan = 0.0f;
    }
}

//=========================================================================

static
void SetupOmniLightCone( f32& InnerConeCos, f32& OuterConeCos, f32& OuterConeSin, f32& OuterConeTan,
                         f32& ConeCosRangeInv )
{
    InnerConeCos = 1.0f;
    OuterConeCos = 1.0f;
    OuterConeSin = 0.0f;
    OuterConeTan = 0.0f;
    ConeCosRangeInv = 0.0f;
}

//=========================================================================

static
void BuildLightCookieUV( vector3 const& Direction, vector3& CookieU, vector3& CookieV )
{
    vector3 Forward = Direction;
    if( !Forward.SafeNormalize() )
    {
        Forward.Set( 0.0f, 0.0f, 1.0f );
    }

    CookieU = vector3( 0.0f, 1.0f, 0.0f ).Cross( Forward );
    if( !CookieU.SafeNormalize() )
    {
        CookieU = vector3( 1.0f, 0.0f, 0.0f ).Cross( Forward );
        if( !CookieU.SafeNormalize() )
        {
            CookieU.Set( 1.0f, 0.0f, 0.0f );
        }
    }

    CookieV = Forward.Cross( CookieU );
    if( !CookieV.SafeNormalize() )
    {
        CookieV.Set( 0.0f, 1.0f, 0.0f );
    }
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

s32 LightSystem::SpadLightSortFn( void const* pA, void const* pB )
{
    LightSystem::spad_light const* pLightA = static_cast<LightSystem::spad_light const*>( pA );
    LightSystem::spad_light const* pLightB = static_cast<LightSystem::spad_light const*>( pB );

    if( pLightA->CharOnly > pLightB->CharOnly )
    {
        return 1;
    }
    if( pLightA->CharOnly < pLightB->CharOnly )
    {
        return -1;
    }
    if( pLightA->Score > pLightB->Score )
    {
        return -1;
    }
    if( pLightA->Score < pLightB->Score )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

xbool LightSystem::CalcDirLight( dir_light* pDestination, matrix4 const& LocalToWorld,
                               bbox const& LocalBBox, bbox const& WorldBBox,
                               vector3 const& WorldBBoxCenter, vector3 const& Position,
                               f32 Radius, f32 Intensity, xcolor const& Color, s32 Shape,
                               vector3 const& Direction, f32 OuterConeCos, f32 ConeCosRangeInv )
{
    if( WorldBBox.Intersect( Position, Radius ) )
    {
        // Move the light into local space so the bounding box remains accurate.
        vector3 Temp = Position - LocalToWorld.GetTranslation();
        vector3 LocalPosition( Temp.GetX() * LocalToWorld( 0, 0 ) +
                               Temp.GetY() * LocalToWorld( 0, 1 ) +
                               Temp.GetZ() * LocalToWorld( 0, 2 ),
                               Temp.GetX() * LocalToWorld( 1, 0 ) +
                               Temp.GetY() * LocalToWorld( 1, 1 ) +
                               Temp.GetZ() * LocalToWorld( 1, 2 ),
                               Temp.GetX() * LocalToWorld( 2, 0 ) +
                               Temp.GetY() * LocalToWorld( 2, 1 ) +
                               Temp.GetZ() * LocalToWorld( 2, 2 ) );

        f32 DistanceDelta;
        f32 DistanceSquared = 0.0f;
        // Find the squared distance from the light to the local bounding box.
        if( LocalPosition.GetX() > LocalBBox.Max.GetX() )
        {
            DistanceDelta = LocalPosition.GetX() - LocalBBox.Max.GetX();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetX() < LocalBBox.Min.GetX() )
        {
            DistanceDelta = LocalBBox.Min.GetX() - LocalPosition.GetX();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetY() > LocalBBox.Max.GetY() )
        {
            DistanceDelta = LocalPosition.GetY() - LocalBBox.Max.GetY();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetY() < LocalBBox.Min.GetY() )
        {
            DistanceDelta = LocalBBox.Min.GetY() - LocalPosition.GetY();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetZ() > LocalBBox.Max.GetZ() )
        {
            DistanceDelta = LocalPosition.GetZ() - LocalBBox.Max.GetZ();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetZ() < LocalBBox.Min.GetZ() )
        {
            DistanceDelta = LocalBBox.Min.GetZ() - LocalPosition.GetZ();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        f32 RadiusSquared = Radius * Radius;

        if( RadiusSquared == 0.0f )
        {
            RadiusSquared = 1.0f;
        }

        // Convert distance attenuation into light intensity.
        f32 LightIntensity = 1.0f - DistanceSquared / RadiusSquared;
        LightIntensity = MAX( LightIntensity, 0.0f );
        LightIntensity *= Intensity;

        if( LightIntensity > 0.0f )
        {
            if( Shape == LIGHT_SHAPE_SPOT )
            {
                LightIntensity *= ComputeSpotConeAttenuation( Position, Direction, OuterConeCos,
                                                               ConeCosRangeInv, WorldBBoxCenter );
            }

            if( LightIntensity <= 0.0f )
            {
                return FALSE;
            }

            vector3 LightDirection = WorldBBoxCenter - Position;
            // Point the derived directional light toward the object's center.
            if( !LightDirection.SafeNormalize() )
            {
                // The light may be placed exactly at the object's center.
                LightDirection.Set( 0.7071f, -0.7071f, 0.0f );
            }

            pDestination->Col.R = static_cast<u8>( MIN( LightIntensity * static_cast<f32>( Color.R ), 255.0f ) );
            pDestination->Col.G = static_cast<u8>( MIN( LightIntensity * static_cast<f32>( Color.G ), 255.0f ) );
            pDestination->Col.B = static_cast<u8>( MIN( LightIntensity * static_cast<f32>( Color.B ), 255.0f ) );
            pDestination->Col.A = 0;
            pDestination->Dir = LightDirection;

            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

LightSystem::LightSystem( void )
    : m_FirstLink( -1 )
    , m_nFadingLights( 0 )
    , m_nDynamicLights( 0 )
    , m_nCharLights( 0 )
    , m_LightCollectionPosition( 0.0f, 0.0f, 0.0f )
    , m_isInCollection( FALSE )
    , m_nSpadLights( 0 )
    , m_nNonCharLightsInSpad( 0 )
    , m_pSpadLights( nullptr )
    , m_pLightBvhNodes( nullptr )
    , m_pLightBvhLightIndices( nullptr )
    , m_nLightBvhNodes( 0 )
    , m_nCollectedLights( 0 )
    , m_CachedCollectionKey( 0 )
    , m_CachedSpadLights( 0 )
    , m_CachedNonCharLights( 0 )
    , m_CachedLightBvhNodes( 0 )
    , m_CachedCollectionValid( FALSE )
{
    x_memset( m_FadingLights, 0, sizeof( fading_light ) * MAX_FADING_LIGHTS );
    x_memset( &m_CollectionStats, 0, sizeof( m_CollectionStats ) );
    m_pSpadLights = m_SpadLights;
    m_pLightBvhNodes = m_LightBvhNodes;
    m_pLightBvhLightIndices = m_LightBvhLightIndices;
}

//=========================================================================

LightSystem::~LightSystem( void )
{
}

//=========================================================================

s32 LightSystem::AddLight( void )
{
    // Find a free slot and track the weakest light in case replacement is required.
    s32 LightIndex;
    f32 WeakestScore = F32_MAX;
    s32 LightToReplace = -1;

    for( LightIndex = 0; LightIndex < MAX_FADING_LIGHTS; LightIndex++ )
    {
        if( !m_FadingLights[LightIndex].Valid )
        {
            break;
        }

        f32 Score = ComputeLightScore( m_FadingLights[LightIndex].CurrentColor, m_FadingLights[LightIndex].Intensity );

        if( ( LightToReplace == -1 ) || ( Score < WeakestScore ) )
        {
            LightToReplace = LightIndex;
            WeakestScore = Score;
        }
    }

    if( LightIndex == MAX_FADING_LIGHTS )
    {
        // The replaced light is already linked, so no link repair is required.
        ASSERT( LightToReplace != -1 );
        LightIndex = LightToReplace;
    }
    else
    {
        // TODO: Consider cache-friendly insertion if this list becomes a hotspot.
        if( m_FirstLink != -1 )
        {
            m_FadingLights[m_FirstLink].PrevLink = LightIndex;
        }

        m_FadingLights[LightIndex].NextLink = m_FirstLink;
        m_FadingLights[LightIndex].PrevLink = -1;
        m_FirstLink = LightIndex;
        m_nFadingLights++;
    }

    ASSERT( ( LightIndex >= 0 ) && ( LightIndex < MAX_FADING_LIGHTS ) );
    return LightIndex;
}

//=========================================================================

void LightSystem::RemoveLight( s32 LightIndex )
{
    ASSERT( ( LightIndex >= 0 ) && ( LightIndex < MAX_FADING_LIGHTS ) );
    ASSERT( m_FadingLights[LightIndex].Valid );

    m_FadingLights[LightIndex].Valid = FALSE;

    // Repair the linked list.
    s32 Previous = m_FadingLights[LightIndex].PrevLink;
    s32 Next = m_FadingLights[LightIndex].NextLink;
    if( Previous != -1 )
    {
        m_FadingLights[Previous].NextLink = Next;
    }
    if( Next != -1 )
    {
        m_FadingLights[Next].PrevLink = Previous;
    }
    if( LightIndex == m_FirstLink )
    {
        ASSERT( Previous == -1 );
        m_FirstLink = Next;
    }

    m_nFadingLights--;
}

//=========================================================================

void LightSystem::AddFadingLight( vector3 const& Position, xcolor const& Color, f32 Radius, f32 Intensity, f32 FadeTime )
{
    if( FadeTime <= 0.0f )
    {
        return;
    }

    s32 LightIndex = AddLight();

    m_FadingLights[LightIndex].Pos = Position;
    m_FadingLights[LightIndex].Radius = Radius;
    m_FadingLights[LightIndex].StartColor = Color;
    m_FadingLights[LightIndex].CurrentColor = Color;
    m_FadingLights[LightIndex].FadeTime = FadeTime;
    m_FadingLights[LightIndex].ElapsedTime = 0.0f;
    m_FadingLights[LightIndex].Valid = TRUE;
    m_FadingLights[LightIndex].InterpolationT = 0.0f;
    m_FadingLights[LightIndex].Intensity = Intensity;
}

//=========================================================================

void LightSystem::OnUpdate( f32 DeltaTime )
{
    for( s32 LightIndex = 0; LightIndex < MAX_FADING_LIGHTS; LightIndex++ )
    {
        if( m_FadingLights[LightIndex].Valid )
        {
            m_FadingLights[LightIndex].ElapsedTime += DeltaTime;

            if( m_FadingLights[LightIndex].ElapsedTime >= m_FadingLights[LightIndex].FadeTime )
            {
                RemoveLight( LightIndex );
            }
            else
            {
                f32 T = m_FadingLights[LightIndex].ElapsedTime / m_FadingLights[LightIndex].FadeTime;
                f32 R = static_cast<f32>( m_FadingLights[LightIndex].StartColor.R );
                f32 G = static_cast<f32>( m_FadingLights[LightIndex].StartColor.G );
                f32 B = static_cast<f32>( m_FadingLights[LightIndex].StartColor.B );

                m_FadingLights[LightIndex].InterpolationT = T;
                m_FadingLights[LightIndex].CurrentColor.R = static_cast<u8>( R * ( 1.0f - T ) );
                m_FadingLights[LightIndex].CurrentColor.G = static_cast<u8>( G * ( 1.0f - T ) );
                m_FadingLights[LightIndex].CurrentColor.B = static_cast<u8>( B * ( 1.0f - T ) );
            }
        }
    }
}

//=========================================================================

s32 LightSystem::RegisterSpotLightCookie( texture::handle const& Cookie )
{
#ifndef X_RETAIL
    if( GetRenderPipeline().GetDebugOptions().DisableLightCookies )
    {
        return -1;
    }
#endif
    texture* pTexture = Cookie.GetPointer();
    if( pTexture == nullptr )
    {
        return -1;
    }

    for( s32 Index = 0; Index < m_LightCookieFaces.GetCount(); Index++ )
    {
        if( m_LightCookieFaces[Index].GetPointer() == pTexture )
        {
            return Index;
        }
    }

    m_LightCookieFaces.Append() = Cookie;
    return m_LightCookieFaces.GetCount() - 1;
}

//=========================================================================

void LightSystem::AddDynamicLight( vector3 const& Position, xcolor const& Color, f32 Radius,
                                 f32 Intensity, xbool CharOnly, s32 Shape, xbool CastShadows,
                                 f32 InnerRadius, vector3 const& Direction, f32 Falloff,
                                 f32 InnerAngle, f32 OuterAngle, s32 ShadowMapResolution, s32 ShadowPriority,
                                 texture::handle const& Cookie, xbool IsFlashlight )
{
#ifndef X_RETAIL
    debug_options const& Options = GetRenderPipeline().GetDebugOptions();
    if( Options.DisableLocalLights || (CharOnly ? Options.DisableCharLights : Options.DisableDynamicLights) )
        return;
#endif
    vector3 LightDirection = Direction;

    if( Shape != LIGHT_SHAPE_SPOT )
    {
        Shape = LIGHT_SHAPE_OMNI;
    }

    InnerRadius = MAX( 0.0f, MIN( InnerRadius, Radius ) );
    Falloff = MINMAX( 0.0f, Falloff, 1.0f );
    if( ( Radius > 0.0001f ) && ( InnerRadius > 0.0f ) )
    {
        Falloff = MINMAX( 0.0f, 1.0f - ( InnerRadius / Radius ), 1.0f );
    }
    InnerAngle = MAX( 0.0f, InnerAngle );
    OuterAngle = MAX( InnerAngle, OuterAngle );
    if( !LightDirection.SafeNormalize() )
    {
        LightDirection.Set( 0.0f, 0.0f, 1.0f );
    }

    if( CharOnly )
    {
        const f32 DistanceSquared = ( Position - m_LightCollectionPosition ).LengthSquared();
        const f32 Priority = 1.0f / ( 1.0f + DistanceSquared / MAX( Radius * Radius, F32_MIN ) );
        s32       LightIndex = m_nCharLights;

        if( m_nCharLights >= MAX_CHAR_LIGHTS )
        {
            LightIndex = 0;
            for( s32 Index = 1; Index < m_nCharLights; Index++ )
            {
                if( m_CharLightPriority[Index] < m_CharLightPriority[LightIndex] )
                {
                    LightIndex = Index;
                }
            }

            if( Priority <= m_CharLightPriority[LightIndex] )
            {
                return;
            }
        }
        else
        {
            m_nCharLights++;
        }

        m_CharLightPriority[LightIndex] = Priority;

        dynamic_light& Light = m_CharLights[LightIndex];
        x_memset( &Light, 0, sizeof( dynamic_light ) );
        Light.Pos = Position;
        Light.Radius = Radius;
        Light.Intensity = Intensity;
        Light.Color = Color;
        Light.Shape = LIGHT_SHAPE_OMNI;
        Light.InnerConeCos = 1.0f;
        Light.OuterConeCos = 1.0f;
        Light.OuterConeSin = 0.0f;
        Light.OuterConeTan = 0.0f;
        Light.ConeCosRangeInv = 0.0f;
        Light.CookieIndex = -1;
    }
    else
    {
        if( m_nDynamicLights >= MAX_DYNAMIC_LIGHTS )
        {
            return;
        }

        f32 InnerConeCos;
        f32 OuterConeCos;
        f32 OuterConeSin;
        f32 OuterConeTan;
        f32 ConeCosRangeInv;
        if( Shape == LIGHT_SHAPE_SPOT )
        {
            SetupLightCone( InnerAngle, OuterAngle, InnerConeCos, OuterConeCos, OuterConeSin, OuterConeTan,
                            ConeCosRangeInv );
        }
        else
        {
            SetupOmniLightCone( InnerConeCos, OuterConeCos, OuterConeSin, OuterConeTan, ConeCosRangeInv );
        }

        vector3   CookieU( 1.0f, 0.0f, 0.0f );
        vector3   CookieV( 0.0f, 1.0f, 0.0f );
        s32 const CookieIndex = ( Shape == LIGHT_SHAPE_SPOT ) ? RegisterSpotLightCookie( Cookie ) : -1;
        if( CookieIndex >= 0 )
        {
            BuildLightCookieUV( LightDirection, CookieU, CookieV );
        }

        x_memset( &m_DynamicLights[m_nDynamicLights], 0, sizeof( dynamic_light ) );
        m_DynamicLights[m_nDynamicLights].Pos = Position;
        m_DynamicLights[m_nDynamicLights].Radius = Radius;
        m_DynamicLights[m_nDynamicLights].Intensity = Intensity;
        m_DynamicLights[m_nDynamicLights].Color = Color;
        m_DynamicLights[m_nDynamicLights].Direction = LightDirection;
        m_DynamicLights[m_nDynamicLights].Falloff = Falloff;
        m_DynamicLights[m_nDynamicLights].InnerAngle = InnerAngle;
        m_DynamicLights[m_nDynamicLights].OuterAngle = OuterAngle;
        m_DynamicLights[m_nDynamicLights].InnerConeCos = InnerConeCos;
        m_DynamicLights[m_nDynamicLights].OuterConeCos = OuterConeCos;
        m_DynamicLights[m_nDynamicLights].OuterConeSin = OuterConeSin;
        m_DynamicLights[m_nDynamicLights].OuterConeTan = OuterConeTan;
        m_DynamicLights[m_nDynamicLights].ConeCosRangeInv = ConeCosRangeInv;
        m_DynamicLights[m_nDynamicLights].Shape = Shape;
        m_DynamicLights[m_nDynamicLights].ShadowMapResolution = ShadowMapResolution;
        m_DynamicLights[m_nDynamicLights].ShadowPriority = ShadowPriority;
        m_DynamicLights[m_nDynamicLights].CastShadows = CastShadows;
        m_DynamicLights[m_nDynamicLights].IsFlashlight = IsFlashlight;
        m_DynamicLights[m_nDynamicLights].CookieIndex = CookieIndex;
        m_DynamicLights[m_nDynamicLights].CookieU = CookieU;
        m_DynamicLights[m_nDynamicLights].CookieV = CookieV;
        m_nDynamicLights++;
    }
}

//=========================================================================

void LightSystem::GetDynamicLight( s32 Index, vector3& Position, f32& Radius, xcolor& Color,
                                 s32& Shape, xbool& CastShadows, f32& InnerRadius,
                                 vector3& Direction, f32& Falloff, f32& InnerAngle, f32& OuterAngle,
                                 s32& ShadowMapResolution, s32& ShadowPriority,
                                 xbool& IsFlashlight ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nDynamicLights ) );

    dynamic_light const& Light = m_DynamicLights[Index];
    Position = Light.Pos;
    Radius = Light.Radius;
    InnerRadius = MAX( 0.0f, Light.Radius * ( 1.0f - Light.Falloff ) );
    Direction = Light.Direction;
    Falloff = Light.Falloff;
    InnerAngle = Light.InnerAngle;
    OuterAngle = Light.OuterAngle;
    Shape = Light.Shape;
    ShadowMapResolution = Light.ShadowMapResolution;
    ShadowPriority = Light.ShadowPriority;
    CastShadows = Light.CastShadows;
    IsFlashlight = Light.IsFlashlight;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void LightSystem::BeginLightCollection( void )
{
    static xprofile_counter LightCollectionBuilds =
        x_GetProfiler().RegisterCounter( "LightCollectionBuilds", "RenderCounter" );
    static xprofile_counter LightCollectionCacheHits =
        x_GetProfiler().RegisterCounter( "LightCollectionCacheHits", "RenderCounter" );

    ASSERT( !m_isInCollection );

    x_memset( &m_CollectionStats, 0, sizeof( m_CollectionStats ) );

    u32 CollectionKey = 2166136261u;
#ifndef X_RETAIL
    debug_options const& Options = GetRenderPipeline().GetDebugOptions();
    xbool const DisableFading = Options.DisableLocalLights || Options.DisableFadingLights;
    HashMemory( CollectionKey, &DisableFading, sizeof( DisableFading ) );
#endif
    HashMemory( CollectionKey, &m_nFadingLights, sizeof( m_nFadingLights ) );
    HashMemory( CollectionKey, &m_nDynamicLights, sizeof( m_nDynamicLights ) );
    HashMemory( CollectionKey, &m_nCharLights, sizeof( m_nCharLights ) );
    HashMemory( CollectionKey, &m_FirstLink, sizeof( m_FirstLink ) );
    HashMemory( CollectionKey, m_FadingLights, sizeof( m_FadingLights ) );
    HashMemory( CollectionKey, m_DynamicLights, sizeof( dynamic_light ) * m_nDynamicLights );
    HashMemory( CollectionKey, m_CharLights, sizeof( dynamic_light ) * m_nCharLights );

    if ( m_CachedCollectionValid && ( CollectionKey == m_CachedCollectionKey ) )
    {
        LightCollectionCacheHits.Add();
        m_pSpadLights = m_SpadLights;
        m_pLightBvhNodes = m_LightBvhNodes;
        m_pLightBvhLightIndices = m_LightBvhLightIndices;
        m_nSpadLights = m_CachedSpadLights;
        m_nNonCharLightsInSpad = m_CachedNonCharLights;
        m_nLightBvhNodes = m_CachedLightBvhNodes;
        m_nCollectedLights = 0;
        m_isInCollection = TRUE;
        return;
    }

    LightCollectionBuilds.Add();

    // Reuse the pipeline-owned frame storage. The light counts are bounded by
    // MAX_SPAD_LIGHTS, so no dynamic allocation is required.
    s32 LightCount = m_nFadingLights + m_nDynamicLights + m_nCharLights;
    ASSERT( LightCount <= MAX_SPAD_LIGHTS );
    m_pSpadLights = m_SpadLights;
    m_pLightBvhNodes = m_LightBvhNodes;
    m_pLightBvhLightIndices = m_LightBvhLightIndices;

    LightCount = 0;

    // Add dynamic lights.
    s32 Index;
    for( Index = 0; Index < m_nDynamicLights; Index++ )
    {
        dynamic_light& Light = m_DynamicLights[Index];
        m_pSpadLights[LightCount].Pos = Light.Pos;
        m_pSpadLights[LightCount].Radius = Light.Radius;
        m_pSpadLights[LightCount].Intensity = Light.Intensity;
        m_pSpadLights[LightCount].Color = Light.Color;
        m_pSpadLights[LightCount].Falloff = Light.Falloff;
        m_pSpadLights[LightCount].Score = ComputeLightScore( Light.Color, Light.Intensity );
        m_pSpadLights[LightCount].CharOnly = FALSE;
        m_pSpadLights[LightCount].Shape = Light.Shape;
        m_pSpadLights[LightCount].Direction = Light.Direction;
        m_pSpadLights[LightCount].InnerConeCos = Light.InnerConeCos;
        m_pSpadLights[LightCount].OuterConeCos = Light.OuterConeCos;
        m_pSpadLights[LightCount].OuterConeSin = Light.OuterConeSin;
        m_pSpadLights[LightCount].OuterConeTan = Light.OuterConeTan;
        m_pSpadLights[LightCount].ConeCosRangeInv = Light.ConeCosRangeInv;
        m_pSpadLights[LightCount].CookieIndex = Light.CookieIndex;
        m_pSpadLights[LightCount].CookieU = Light.CookieU;
        m_pSpadLights[LightCount].CookieV = Light.CookieV;
        m_pSpadLights[LightCount].DynamicLightIndex = Index;
        LightCount++;
    }

    // Add fading lights.
    s32 CurrentLink = m_FirstLink;
#ifndef X_RETAIL
    if( DisableFading ) CurrentLink = -1;
#endif
    while( CurrentLink != -1 )
    {
        fading_light& Light = m_FadingLights[CurrentLink];
        m_pSpadLights[LightCount].Pos = Light.Pos;
        m_pSpadLights[LightCount].Radius = Light.Radius;
        m_pSpadLights[LightCount].Intensity = Light.Intensity;
        m_pSpadLights[LightCount].Color = Light.CurrentColor;
        m_pSpadLights[LightCount].Falloff = 1.0f;
        m_pSpadLights[LightCount].Score = ComputeLightScore( Light.CurrentColor, Light.Intensity );
        m_pSpadLights[LightCount].CharOnly = FALSE;
        m_pSpadLights[LightCount].Shape = LIGHT_SHAPE_OMNI;
        m_pSpadLights[LightCount].Direction.Set( 0.0f, 0.0f, 1.0f );
        m_pSpadLights[LightCount].InnerConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeSin = 0.0f;
        m_pSpadLights[LightCount].OuterConeTan = 0.0f;
        m_pSpadLights[LightCount].ConeCosRangeInv = 0.0f;
        m_pSpadLights[LightCount].CookieIndex = -1;
        m_pSpadLights[LightCount].CookieU.Set( 1.0f, 0.0f, 0.0f );
        m_pSpadLights[LightCount].CookieV.Set( 0.0f, 1.0f, 0.0f );
        m_pSpadLights[LightCount].DynamicLightIndex = -1;
        LightCount++;

        CurrentLink = Light.NextLink;
    }

    // Non-character geometry only sees lights collected up to this point.
    m_nNonCharLightsInSpad = LightCount;

    // Add character-only lights.
    for( Index = 0; Index < m_nCharLights; Index++ )
    {
        dynamic_light& Light = m_CharLights[Index];
        m_pSpadLights[LightCount].Pos = Light.Pos;
        m_pSpadLights[LightCount].Radius = Light.Radius;
        m_pSpadLights[LightCount].Intensity = Light.Intensity;
        m_pSpadLights[LightCount].Color = Light.Color;
        m_pSpadLights[LightCount].Falloff = 1.0f;
        m_pSpadLights[LightCount].Score = ComputeLightScore( Light.Color, Light.Intensity );
        m_pSpadLights[LightCount].CharOnly = TRUE;
        m_pSpadLights[LightCount].Shape = LIGHT_SHAPE_OMNI;
        m_pSpadLights[LightCount].Direction.Set( 0.0f, 0.0f, 1.0f );
        m_pSpadLights[LightCount].InnerConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeSin = 0.0f;
        m_pSpadLights[LightCount].OuterConeTan = 0.0f;
        m_pSpadLights[LightCount].ConeCosRangeInv = 0.0f;
        m_pSpadLights[LightCount].CookieIndex = -1;
        m_pSpadLights[LightCount].CookieU.Set( 1.0f, 0.0f, 0.0f );
        m_pSpadLights[LightCount].CookieV.Set( 0.0f, 1.0f, 0.0f );
        m_pSpadLights[LightCount].DynamicLightIndex = -1;
        LightCount++;
    }

    // Sort highest-scoring lights first.
    if( LightCount > 1 )
    {
        x_qsort( m_pSpadLights, LightCount, sizeof( spad_light ), SpadLightSortFn );
    }

    m_nSpadLights = LightCount;
    BuildLightBvh();
    m_CachedCollectionKey = CollectionKey;
    m_CachedSpadLights = m_nSpadLights;
    m_CachedNonCharLights = m_nNonCharLightsInSpad;
    m_CachedLightBvhNodes = m_nLightBvhNodes;
    m_CachedCollectionValid = TRUE;
    m_isInCollection = TRUE;
}

//=========================================================================

void LightSystem::EndLightCollection( void )
{
    ASSERT( m_isInCollection );

    m_nSpadLights = 0;
    m_nNonCharLightsInSpad = 0;
    m_nLightBvhNodes = 0;
    m_isInCollection = FALSE;
}

//=========================================================================

void LightSystem::ResetAfterException( void )
{
    m_pSpadLights = m_SpadLights;
    m_pLightBvhNodes = m_LightBvhNodes;
    m_pLightBvhLightIndices = m_LightBvhLightIndices;
    m_nLightBvhNodes = 0;
    m_isInCollection = FALSE;
}

//=========================================================================

s32 LightSystem::BuildLightBvhNode( s32 FirstLight, s32 LightCount )
{
    ASSERT( LightCount > 0 );
    ASSERT( m_nLightBvhNodes < MAX_LIGHT_BVH_NODES );

    light_bvh_node& Node = m_pLightBvhNodes[m_nLightBvhNodes++];
    Node.Bounds = bbox( m_pSpadLights[m_pLightBvhLightIndices[FirstLight]].Pos,
                        m_pSpadLights[m_pLightBvhLightIndices[FirstLight]].Radius );
    for( s32 LightOffset = 1; LightOffset < LightCount; LightOffset++ )
    {
        spad_light const& Light = m_pSpadLights[m_pLightBvhLightIndices[FirstLight + LightOffset]];
        Node.Bounds += bbox( Light.Pos, Light.Radius );
    }

    Node.LeftChild = -1;
    Node.RightChild = -1;
    Node.FirstLight = FirstLight;
    Node.LightCount = LightCount;
    if( LightCount <= 8 )
    {
        return m_nLightBvhNodes - 1;
    }

    vector3 const Extent = Node.Bounds.GetSize();
    s32 const Axis = ( Extent.GetX() >= Extent.GetY() && Extent.GetX() >= Extent.GetZ() )
                         ? 0
                         : ( Extent.GetY() >= Extent.GetZ() ) ? 1 : 2;

    // The light count is bounded, so insertion sort avoids comparator context storage.
    for( s32 LightOffset = FirstLight + 1; LightOffset < FirstLight + LightCount; LightOffset++ )
    {
        s32 const LightIndex = m_pLightBvhLightIndices[LightOffset];
        f32 const Coordinate = ( Axis == 0 ) ? m_pSpadLights[LightIndex].Pos.GetX()
                              : ( Axis == 1 ) ? m_pSpadLights[LightIndex].Pos.GetY()
                                            : m_pSpadLights[LightIndex].Pos.GetZ();
        s32 InsertAt = LightOffset;
        while( InsertAt > FirstLight )
        {
            spad_light const& Previous = m_pSpadLights[m_pLightBvhLightIndices[InsertAt - 1]];
            f32 const PreviousCoordinate = ( Axis == 0 ) ? Previous.Pos.GetX()
                                         : ( Axis == 1 ) ? Previous.Pos.GetY() : Previous.Pos.GetZ();
            if( PreviousCoordinate <= Coordinate )
            {
                break;
            }

            m_pLightBvhLightIndices[InsertAt] = m_pLightBvhLightIndices[InsertAt - 1];
            InsertAt--;
        }
        m_pLightBvhLightIndices[InsertAt] = LightIndex;
    }

    s32 const NodeIndex = m_nLightBvhNodes - 1;
    s32 const LeftLightCount = LightCount / 2;
    Node.LeftChild = BuildLightBvhNode( FirstLight, LeftLightCount );
    Node.RightChild = BuildLightBvhNode( FirstLight + LeftLightCount, LightCount - LeftLightCount );
    Node.FirstLight = 0;
    Node.LightCount = 0;
    return NodeIndex;
}

//=========================================================================

void LightSystem::BuildLightBvh( void )
{
    m_nLightBvhNodes = 0;
    m_pLightBvhNodes = m_LightBvhNodes;
    m_pLightBvhLightIndices = m_LightBvhLightIndices;
    if( m_nSpadLights <= 0 )
    {
        return;
    }

    for( s32 LightIndex = 0; LightIndex < m_nSpadLights; LightIndex++ )
    {
        m_pLightBvhLightIndices[LightIndex] = LightIndex;
    }

    BuildLightBvhNode( 0, m_nSpadLights );
}

//=========================================================================

void LightSystem::InsertCollectedSceneLight( s32 LightIndex, s32 MaxLightCount )
{
    f32 const NewScore = m_pSpadLights[LightIndex].Score;
    s32       InsertAt = m_nCollectedLights;

    if( m_nCollectedLights == MaxLightCount )
    {
        if( NewScore <= m_pSpadLights[m_CollectedLights[m_nCollectedLights - 1]].Score )
        {
            return;
        }

        InsertAt = m_nCollectedLights - 1;
    }
    else
    {
        m_nCollectedLights++;
    }

    while( InsertAt > 0 )
    {
        if( NewScore <= m_pSpadLights[m_CollectedLights[InsertAt - 1]].Score )
        {
            break;
        }

        m_CollectedLights[InsertAt] = m_CollectedLights[InsertAt - 1];
        InsertAt--;
    }

    m_CollectedLights[InsertAt] = LightIndex;
}

//=========================================================================

s32 LightSystem::CollectLights( bbox const& WorldBBox, s32 MaxLightCount )
{
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights || ( m_nSpadLights == 0 ) );

    m_nCollectedLights = 0;
    m_CollectionStats.SceneQueries++;
    MaxLightCount = MIN( MAX( 0, MaxLightCount ), MAX_COLLECTED_LIGHTS );
    if( ( MaxLightCount <= 0 ) || ( m_nNonCharLightsInSpad <= 0 ) || ( m_nLightBvhNodes <= 0 ) )
    {
        return m_nCollectedLights;
    }

    vector3 const WorldBBoxCenter = WorldBBox.GetCenter();
    f32 const     WorldBBoxRadius = WorldBBox.GetRadius();
    s32           NodeStack[MAX_LIGHT_BVH_NODES];
    s32           StackCount = 0;
    NodeStack[StackCount++] = 0;

    while( StackCount > 0 )
    {
        light_bvh_node const& Node = m_pLightBvhNodes[NodeStack[--StackCount]];
        if( !Node.Bounds.Intersect( WorldBBox ) )
        {
            continue;
        }

        if( Node.LightCount <= 0 )
        {
            ASSERT( ( Node.LeftChild >= 0 ) && ( Node.RightChild >= 0 ) );
            NodeStack[StackCount++] = Node.LeftChild;
            NodeStack[StackCount++] = Node.RightChild;
            continue;
        }

        for( s32 LightOffset = 0; LightOffset < Node.LightCount; LightOffset++ )
        {
            s32 const LightIndex = m_pLightBvhLightIndices[Node.FirstLight + LightOffset];
            spad_light const& Light = m_pSpadLights[LightIndex];
            if( Light.CharOnly )
            {
                continue;
            }

            m_CollectionStats.SceneCandidates++;
            xbool Intersects = WorldBBox.Intersect( Light.Pos, Light.Radius );
            if( Intersects && ( Light.Shape == LIGHT_SHAPE_SPOT ) )
            {
                Intersects = SpotLightAffectsSphere( Light.Pos, Light.Direction, Light.OuterConeCos,
                                                     Light.OuterConeSin, Light.OuterConeTan, Light.Radius,
                                                     WorldBBoxCenter, WorldBBoxRadius );
            }

            if( Intersects )
            {
                InsertCollectedSceneLight( LightIndex, MaxLightCount );
                m_CollectionStats.SceneHits++;
            }
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

void LightSystem::GetCollectedLight( s32 Index, vector3& Position, f32& Radius, xcolor& Color )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_CollectedLights[Index]];
    Position = Light.Pos;
    Radius = Light.Radius;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void LightSystem::GetCollectedLightInfo( s32 Index, vector3& Position, f32& Radius, xcolor& Color, f32& Falloff )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_CollectedLights[Index]];
    Position = Light.Pos;
    Radius = Light.Radius;
    Falloff = Light.Falloff;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void LightSystem::GetCollectedLightInfo( s32 Index, vector3& Position, f32& Radius, xcolor& Color,
                                       f32& Falloff, s32& Shape, vector3& Direction,
                                       f32& InnerConeCos, f32& OuterConeCos )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_CollectedLights[Index]];
    Position = Light.Pos;
    Radius = Light.Radius;
    Falloff = Light.Falloff;
    Shape = Light.Shape;
    Direction = Light.Direction;
    InnerConeCos = Light.InnerConeCos;
    OuterConeCos = Light.OuterConeCos;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void LightSystem::GetCollectedLightCookie( s32 Index, s32& CookieIndex, vector3& CookieU, vector3& CookieV )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_CollectedLights[Index]];
    CookieIndex = Light.CookieIndex;
    CookieU = Light.CookieU;
    CookieV = Light.CookieV;
}

//=========================================================================

s32 LightSystem::GetCollectedDynamicLightIndex( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    return m_pSpadLights[m_CollectedLights[Index]].DynamicLightIndex;
}

//=========================================================================

void LightSystem::GetLight( s32 Index, vector3& Position, f32& Radius, xcolor& Color ) const
{
    ASSERT( m_isInCollection );
    ASSERT( ( Index >= 0 ) && ( Index < m_nSpadLights ) );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[Index];
    Position = Light.Pos;
    Radius = Light.Radius;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void LightSystem::InsertCollectedCharLight( dir_light const& Light, s32 MaxLightCount )
{
    ASSERT( MaxLightCount <= MAX_COLLECTED_LIGHTS );

    if( MaxLightCount <= 0 )
    {
        return;
    }

    f32 const NewScore = ComputeLightScore( Light.Col, 1.0f );
    s32       InsertAt = m_nCollectedLights;

    if( m_nCollectedLights == MaxLightCount )
    {
        f32 const LowestScore = ComputeLightScore( m_CollectedCharLights[m_nCollectedLights - 1].Col, 1.0f );
        if( NewScore <= LowestScore )
        {
            return;
        }

        InsertAt = m_nCollectedLights - 1;
    }
    else
    {
        m_nCollectedLights++;
    }

    while( InsertAt > 0 )
    {
        f32 const PreviousScore = ComputeLightScore( m_CollectedCharLights[InsertAt - 1].Col, 1.0f );
        if( NewScore <= PreviousScore )
        {
            break;
        }

        m_CollectedCharLights[InsertAt] = m_CollectedCharLights[InsertAt - 1];
        InsertAt--;
    }

    m_CollectedCharLights[InsertAt] = Light;
}

//=========================================================================

s32 LightSystem::CollectCharLights( matrix4 const& LocalToWorld, bbox const& LocalBBox, s32 MaxLightCount )
{
    ASSERT( m_isInCollection );

    m_nCollectedLights = 0;
    m_CollectionStats.CharQueries++;
    MaxLightCount = MIN( MAX( 0, MaxLightCount ), MAX_COLLECTED_LIGHTS );
    if( MaxLightCount <= 0 )
    {
        return m_nCollectedLights;
    }

    bbox WorldBBox = LocalBBox;
    WorldBBox.Transform( LocalToWorld );
    vector3 const WorldBBoxCenter = LocalToWorld * LocalBBox.GetCenter();
    dir_light     Light;

    if( m_nLightBvhNodes <= 0 )
    {
        return m_nCollectedLights;
    }

    s32 NodeStack[MAX_LIGHT_BVH_NODES];
    s32 StackCount = 0;
    NodeStack[StackCount++] = 0;
    while( StackCount > 0 )
    {
        light_bvh_node const& Node = m_pLightBvhNodes[NodeStack[--StackCount]];
        if( !Node.Bounds.Intersect( WorldBBox ) )
        {
            continue;
        }

        if( Node.LightCount <= 0 )
        {
            NodeStack[StackCount++] = Node.LeftChild;
            NodeStack[StackCount++] = Node.RightChild;
            continue;
        }

        for( s32 LightOffset = 0; LightOffset < Node.LightCount; LightOffset++ )
        {
            spad_light const& Candidate = m_pSpadLights[m_pLightBvhLightIndices[Node.FirstLight + LightOffset]];
            m_CollectionStats.CharCandidates++;
            if( CalcDirLight( &Light, LocalToWorld, LocalBBox, WorldBBox, WorldBBoxCenter,
                              Candidate.Pos, Candidate.Radius, Candidate.Intensity, Candidate.Color,
                              Candidate.Shape, Candidate.Direction, Candidate.OuterConeCos,
                              Candidate.ConeCosRangeInv ) )
            {
                InsertCollectedCharLight( Light, MaxLightCount );
                m_CollectionStats.CharHits++;
            }
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

s32 LightSystem::CollectCharLightsOnly( matrix4 const& LocalToWorld, bbox const& LocalBBox, s32 MaxLightCount )
{
    ASSERT( m_isInCollection );

    m_nCollectedLights = 0;
    m_CollectionStats.CharQueries++;
    MaxLightCount = MIN( MAX( 0, MaxLightCount ), MAX_COLLECTED_LIGHTS );
    if( MaxLightCount <= 0 )
    {
        return m_nCollectedLights;
    }

    bbox WorldBBox = LocalBBox;
    WorldBBox.Transform( LocalToWorld );
    vector3 const WorldBBoxCenter = LocalToWorld * LocalBBox.GetCenter();
    dir_light     Light;

    if( m_nLightBvhNodes <= 0 )
    {
        return m_nCollectedLights;
    }

    s32 NodeStack[MAX_LIGHT_BVH_NODES];
    s32 StackCount = 0;
    NodeStack[StackCount++] = 0;
    while( StackCount > 0 )
    {
        light_bvh_node const& Node = m_pLightBvhNodes[NodeStack[--StackCount]];
        if( !Node.Bounds.Intersect( WorldBBox ) )
        {
            continue;
        }

        if( Node.LightCount <= 0 )
        {
            NodeStack[StackCount++] = Node.LeftChild;
            NodeStack[StackCount++] = Node.RightChild;
            continue;
        }

        for( s32 LightOffset = 0; LightOffset < Node.LightCount; LightOffset++ )
        {
            spad_light const& Candidate = m_pSpadLights[m_pLightBvhLightIndices[Node.FirstLight + LightOffset]];
            // Fading lights apply to characters; regular dynamic lights do not here.
            if( !Candidate.CharOnly && ( Candidate.DynamicLightIndex >= 0 ) )
            {
                continue;
            }

            m_CollectionStats.CharCandidates++;
            if( CalcDirLight( &Light, LocalToWorld, LocalBBox, WorldBBox, WorldBBoxCenter,
                              Candidate.Pos, Candidate.Radius, Candidate.Intensity, Candidate.Color,
                              Candidate.Shape, Candidate.Direction, Candidate.OuterConeCos,
                              Candidate.ConeCosRangeInv ) )
            {
                InsertCollectedCharLight( Light, MaxLightCount );
                m_CollectionStats.CharHits++;
            }
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

void LightSystem::GetCollectedCharLight( s32 Index, vector3& Direction, xcolor& Color )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    Direction = m_CollectedCharLights[Index].Dir;
    Color = m_CollectedCharLights[Index].Col;
}
} // namespace render
