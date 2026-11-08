//=========================================================================
//
//  Material.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Material.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum
{
    RENDER_FLAG_ALPHA                   = ( 1u << 0  ),
    RENDER_FLAG_DISTORTION              = ( 1u << 1  ),
    RENDER_FLAG_DISTORTION_PERPOLY_ENV  = ( 1u << 2  ),
    RENDER_FLAG_FORCE_Z_FILL            = ( 1u << 3  ),
    RENDER_FLAG_PUNCH_THRU              = ( 1u << 4  ),
    RENDER_FLAG_ENV_CUBEMAP             = ( 1u << 5  ),
    RENDER_FLAG_FORWARD                 = ( 1u << 6  ),
    RENDER_FLAG_DEPTH_SORTED            = ( 1u << 7  ),
    RENDER_FLAG_POST_EFFECT_BLEND       = ( 1u << 8  ),
    RENDER_FLAG_RECEIVES_PROJECTION     = ( 1u << 9  ),
    RENDER_FLAG_RECEIVES_SHADOW         = ( 1u << 10 ),
    RENDER_FLAG_USES_SHADOW_ALPHA       = ( 1u << 11 ),
    RENDER_FLAG_DEPTH_WRITE             = ( 1u << 12 ),
    RENDER_FLAG_CULL_BACK               = ( 1u << 13 ),
    RENDER_FLAG_CLAMP_SAMPLER           = ( 1u << 14 ),
};

//-------------------------------------------------------------------------

struct MaterialTypePolicy
{
    u32 RenderFlags;
    u8  RenderOrder;
    u8  SortPriority;
};

//=========================================================================
//  CONSTANTS
//=========================================================================

static MaterialTypePolicy const MaterialTypePolicies[Material_NumTypes] =
{
    { 0, MATERIAL_RENDER_ORDER_OPAQUE, 1 },
    { 0, MATERIAL_RENDER_ORDER_OPAQUE, 2 },
    { RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD | RENDER_FLAG_USES_SHADOW_ALPHA, MATERIAL_RENDER_ORDER_ALPHA, 5 },
    { 0, MATERIAL_RENDER_ORDER_OPAQUE, 2 },
    { 0, MATERIAL_RENDER_ORDER_GLOWING, 3 },
    { RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD, MATERIAL_RENDER_ORDER_ALPHA, 5 },
    { RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD, MATERIAL_RENDER_ORDER_ALPHA_GLOWING, 4 },
    { RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD, MATERIAL_RENDER_ORDER_ALPHA_GLOWING, 4 },
    { RENDER_FLAG_ALPHA | RENDER_FLAG_DISTORTION, MATERIAL_RENDER_ORDER_DISTORTION, 6 },
    { RENDER_FLAG_ALPHA | RENDER_FLAG_DISTORTION | RENDER_FLAG_DISTORTION_PERPOLY_ENV, MATERIAL_RENDER_ORDER_DISTORTION, 6 },
};

static_assert( ( sizeof( MaterialTypePolicies ) / sizeof( MaterialTypePolicies[0] ) ) == Material_NumTypes,
               "Every material type must have a render policy" );

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
xbool HasRenderFlag( u32 Flags, u32 Flag )
{
    return ( ( Flags & Flag ) != 0 );
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

material::uvanim::uvanim( void )
    : CurrentFrame( 0.0f )
    , iKey( 0 )
    , iFrame( 0 )
    , Dir( 1 )
    , Type( 0 )
    , nFrames( 0 )
    , FPS( 0 )
    , StartFrame( 0 )
{
}

//=========================================================================

material::material( void )
    : m_Type( Material_Not_Used )
    , m_DetailScale( 1.0f )
    , m_FixedAlpha( 0.0f )
    , m_Flags( 0 )
    , m_DiffuseMap()
    , m_EnvironmentMap()
    , m_DetailMap()
    , m_UVAnim()
    , m_RefCount( 0 )
    , m_RenderData()
{
    Finalize();
}

//=========================================================================

material::~material( void )
{
}

//=========================================================================

void material::Finalize( void )
{
    m_Desc = render::MaterialDesc();
    if( m_Type < Material_Not_Used || m_Type >= Material_NumTypes )
    {
        x_DebugMsg( "Material: invalid surface type %d\n", m_Type );
        ASSERTS( FALSE, "Invalid material type" );
        return;
    }

    s32 const TypeIndex = static_cast<s32>( m_Type );

    MaterialTypePolicy const& Policy = MaterialTypePolicies[TypeIndex];

    m_RenderData.Flags            = Policy.RenderFlags;
    m_RenderData.BlendMode        = MATERIAL_BLEND_OPAQUE;
    m_RenderData.RenderOrder      = Policy.RenderOrder;
    m_RenderData.SortPriority     = Policy.SortPriority;
    m_RenderData.AlphaRef         = 0.0f;
    m_RenderData.DetailScale      = ( m_DetailScale > 0.0f ) ? m_DetailScale : 1.0f;
    m_RenderData.FixedAlpha       = m_FixedAlpha;
    m_RenderData.CubeMapIntensity = 1.0f;

    xbool const IsAlpha      = HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_ALPHA );
    xbool const IsDistortion = HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_DISTORTION );
    xbool const ForceZFill   = ( ( m_Flags & MATERIAL_FLAG_FORCE_ZFILL ) != 0 );
    xbool const PunchThru    = ( ( m_Flags & MATERIAL_FLAG_IS_PUNCH_THRU ) != 0 );
    xbool const DoubleSided  = ( ( m_Flags & MATERIAL_FLAG_DOUBLE_SIDED ) != 0 );
    xbool const Additive     = ( ( m_Flags & MATERIAL_FLAG_IS_ADDITIVE ) != 0 );
    xbool const Subtractive  = ( ( m_Flags & MATERIAL_FLAG_IS_SUBTRACTIVE ) != 0 );
    xbool const IsPostEffect = Additive || Subtractive;

    if( ForceZFill )
    {
        m_RenderData.Flags |= RENDER_FLAG_FORCE_Z_FILL;
    }

    if( PunchThru )
    {
        m_RenderData.Flags |= RENDER_FLAG_PUNCH_THRU | RENDER_FLAG_USES_SHADOW_ALPHA;
    }

    if( IsPostEffect )
    {
        m_RenderData.Flags |= RENDER_FLAG_POST_EFFECT_BLEND;
    }

    xbool const EnvironmentRecipe = m_Type == Material_Diff_PerPixelEnv ||
                                    m_Type == Material_Alpha_PerPolyEnv ||
                                    m_Type == Material_Distortion_PerPolyEnv;
    xbool const DeclaresEnvironment = ( m_Flags & ( MATERIAL_FLAG_HAS_ENV_MAP | MATERIAL_FLAG_ENV_CUBE_MAP ) ) != 0;
    if( EnvironmentRecipe && !DeclaresEnvironment )
    {
        x_DebugMsg( "Material: environment recipe %d has no declared environment resource\n", m_Type );
        ASSERTS( FALSE, "Environment recipe requires a declared texture or cubemap" );
        return;
    }
    if( Additive && Subtractive )
    {
        x_DebugMsg( "Material: additive and subtractive flags cannot be combined\n" );
        ASSERTS( FALSE, "Conflicting material blend modes" );
        return;
    }
    if( EnvironmentRecipe && ( m_Flags & MATERIAL_FLAG_ENV_VIEW_SPACE ) && ( m_Flags & MATERIAL_FLAG_ENV_WORLD_SPACE ) )
    {
        x_DebugMsg( "Material: environment cannot use both view and world coordinates\n" );
        ASSERTS( FALSE, "Conflicting environment coordinate modes" );
        return;
    }

    if( EnvironmentRecipe && ( m_Flags & MATERIAL_FLAG_ENV_CUBE_MAP ) )
    {
        m_RenderData.Flags |= RENDER_FLAG_ENV_CUBEMAP;
    }

    m_Desc = render::ResolveMaterialDescription( static_cast<material_type>(m_Type), m_Flags );
    if( m_Desc.AlphaTest )
        m_RenderData.AlphaRef = PunchThru ? 0.5f : ( 4.0f / 255.0f );

    if( IsDistortion )
    {
        m_RenderData.BlendMode = MATERIAL_BLEND_DISTORTION;
        m_RenderData.Flags |= RENDER_FLAG_CLAMP_SAMPLER;
    }
    else if( HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_FORWARD ) )
    {
        if( Additive )
        {
            m_RenderData.BlendMode = MATERIAL_BLEND_ADDITIVE;
        }
        else if( Subtractive )
        {
            m_RenderData.BlendMode = MATERIAL_BLEND_SUBTRACTIVE;
        }
        else
        {
            m_RenderData.BlendMode = MATERIAL_BLEND_ALPHA;
        }
    }

    if( HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_FORWARD ) && !IsPostEffect )
    {
        m_RenderData.Flags |= RENDER_FLAG_DEPTH_SORTED;
    }

    if( !IsAlpha || ( ForceZFill && !Subtractive ) )
    {
        m_RenderData.Flags |= RENDER_FLAG_RECEIVES_PROJECTION;
    }

    if( ( !IsAlpha || ForceZFill ) && !PunchThru )
    {
        m_RenderData.Flags |= RENDER_FLAG_RECEIVES_SHADOW;
    }

    xbool const EnablesBlend = ( m_RenderData.BlendMode == MATERIAL_BLEND_ALPHA ) ||
                               ( m_RenderData.BlendMode == MATERIAL_BLEND_ADDITIVE ) ||
                               ( m_RenderData.BlendMode == MATERIAL_BLEND_SUBTRACTIVE );

    if( !( ( IsAlpha || EnablesBlend ) && !ForceZFill && !IsDistortion ) )
    {
        m_RenderData.Flags |= RENDER_FLAG_DEPTH_WRITE;
    }

    if( !DoubleSided && !( IsAlpha && !IsDistortion ) )
    {
        m_RenderData.Flags |= RENDER_FLAG_CULL_BACK;
    }

    if( HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_ENV_CUBEMAP ) )
    {
        if( HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_DISTORTION_PERPOLY_ENV ) ||
            ( m_Type == Material_Alpha_PerPolyEnv ) )
        {
            m_RenderData.CubeMapIntensity = 1.0f;
        }
        else
        {
            f32 const DefaultCubeMapIntensity = 0.35f;
            f32       CubeMapIntensity        = m_FixedAlpha;

            if( CubeMapIntensity <= 0.0f )
            {
                CubeMapIntensity = DefaultCubeMapIntensity;
            }

            if( CubeMapIntensity < 0.0f )
            {
                CubeMapIntensity = 0.0f;
            }

            if( CubeMapIntensity > 1.0f )
            {
                CubeMapIntensity = 1.0f;
            }

            m_RenderData.CubeMapIntensity = CubeMapIntensity;
        }
    }


}

//=========================================================================

xbool material::operator==( material const& Rhs ) const
{
    if( m_DiffuseMap.GetIndex() != Rhs.m_DiffuseMap.GetIndex() )
    {
        return FALSE;
    }

    if( m_EnvironmentMap.GetIndex() != Rhs.m_EnvironmentMap.GetIndex() )
    {
        return FALSE;
    }

    if( m_DetailMap.GetIndex() != Rhs.m_DetailMap.GetIndex() )
    {
        return FALSE;
    }

    if( m_Type != Rhs.m_Type )
    {
        return FALSE;
    }

    if( m_DetailScale != Rhs.m_DetailScale )
    {
        return FALSE;
    }

    if( m_FixedAlpha != Rhs.m_FixedAlpha )
    {
        return FALSE;
    }

    if( m_Flags != Rhs.m_Flags )
    {
        return FALSE;
    }

    // Mutable animation state is not part of material identity.
    if( m_UVAnim.iKey != Rhs.m_UVAnim.iKey )
    {
        return FALSE;
    }

    if( m_UVAnim.Type != Rhs.m_UVAnim.Type )
    {
        return FALSE;
    }

    if( m_UVAnim.nFrames != Rhs.m_UVAnim.nFrames )
    {
        return FALSE;
    }

    if( m_UVAnim.FPS != Rhs.m_UVAnim.FPS )
    {
        return FALSE;
    }

    if( m_UVAnim.StartFrame != Rhs.m_UVAnim.StartFrame )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool material::IsAlpha( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_ALPHA );
}

//=========================================================================

xbool material::IsDistortion( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_DISTORTION );
}

//=========================================================================

xbool material::IsDistortionPerPolyEnv( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_DISTORTION_PERPOLY_ENV );
}

//=========================================================================

xbool material::ForcesZFill( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_FORCE_Z_FILL );
}

//=========================================================================

xbool material::IsPunchThrough( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_PUNCH_THRU );
}

//=========================================================================

xbool material::UsesCubeMap( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_ENV_CUBEMAP );
}

//=========================================================================

xbool material::RequiresForwardPass( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_FORWARD );
}

//=========================================================================

xbool material::RequiresDepthSort( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_DEPTH_SORTED );
}

//=========================================================================

xbool material::IsPostEffectBlend( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_POST_EFFECT_BLEND );
}

//=========================================================================

xbool material::ReceivesProjection( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_RECEIVES_PROJECTION );
}

//=========================================================================

xbool material::ReceivesShadow( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_RECEIVES_SHADOW );
}

//=========================================================================

xbool material::UsesShadowAlpha( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_USES_SHADOW_ALPHA );
}

//=========================================================================

xbool material::WritesDepth( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_DEPTH_WRITE );
}

//=========================================================================

xbool material::CullsBackFaces( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_CULL_BACK );
}

//=========================================================================

xbool material::ClampsSampler( void ) const
{
    return HasRenderFlag( m_RenderData.Flags, RENDER_FLAG_CLAMP_SAMPLER );
}
