//==============================================================================
//
//  ProjTextureMgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ProjTextureMgr.hpp"

//=========================================================================
// GLOBAL INSTANCE
//=========================================================================

proj_texture_mgr    g_ProjTextureMgr;

//=========================================================================
// FUNCTIONS
//=========================================================================

proj_texture_mgr::proj_texture_mgr( void ) :
    m_NLightProjections     ( 0 ),
    m_NShadowProjections    ( 0 ),
    m_NCollectedLights      ( 0 ),
    m_NCollectedShadows     ( 0 ),
    m_CurrCollectedLight    ( 0 ),
    m_CurrCollectedShadow   ( 0 )
{
}

//=========================================================================

proj_texture_mgr::~proj_texture_mgr( void )
{
    ClearProjTextures(); 	
}

//=========================================================================

xbool proj_texture_mgr::CanReceiveProjTexture( material_type Type,
                                               u16           MaterialFlags ) const
{
    if( !IsAlphaMaterial( Type ) )
        return TRUE;

    if( !(MaterialFlags & geom::material::FLAG_FORCE_ZFILL) )
        return FALSE;

    if( MaterialFlags & geom::material::FLAG_IS_SUBTRACTIVE )
        return FALSE;

    return TRUE;
}

//=========================================================================

xbool proj_texture_mgr::CanReceiveProjTexture( const geom::material& Mat ) const
{
    return CanReceiveProjTexture( (material_type)Mat.Type, Mat.Flags );
}

//=========================================================================

xbool proj_texture_mgr::CanReceiveProjTexture( const material& Mat ) const
{
    return CanReceiveProjTexture( (material_type)Mat.m_Type, Mat.m_Flags );
}

//=========================================================================

xbool proj_texture_mgr::ProjectionIntersectsBBox( const projection& Proj, const bbox& B )
{
    if( Proj.ProjView.BBoxInView( B ) == view::VISIBLE_NONE )
        return FALSE;

    f32 MinZ, MaxZ;
    Proj.ProjView.GetMinMaxZ( B, MinZ, MaxZ );

    f32 ZNear, ZFar;
    Proj.ProjView.GetZLimits( ZNear, ZFar );
    if( MaxZ < ZNear )
        return FALSE;

    vector3 Delta = B.GetCenter() - Proj.ProjView.GetPosition();
    if( Proj.ProjView.GetViewZ().Dot( Delta ) <= 0.0f )
        return FALSE;

    return TRUE;
}

//=========================================================================

void proj_texture_mgr::AddProjLight( const matrix4&  L2W,
                                     radian          FOV,
                                     f32             Length,
                                     texture::handle Texture )
{
    if( Texture.GetPointer() == NULL )
        return;

    ASSERT( m_NLightProjections < MAX_PROJ_LIGHTS );
    SetupProjection( m_LightProjections[m_NLightProjections], L2W, FOV, Length, Texture );
    m_NLightProjections++;
}

//=========================================================================

void proj_texture_mgr::AddProjShadow( const matrix4&  L2W,
                                      radian          FOV,
                                      f32             Length,
                                      texture::handle Texture )
{
    if( Texture.GetPointer() == NULL )
        return;

    ASSERT( m_NShadowProjections < MAX_PROJ_SHADOWS );
    SetupProjection( m_ShadowProjections[m_NShadowProjections], L2W, FOV, Length, Texture );
    m_NShadowProjections++;
}

//=========================================================================

s32 proj_texture_mgr::CollectLights( const matrix4& L2W, const bbox& B, s32 MaxLightCount )
{
    bbox WorldBBox = B;
    WorldBBox.Transform( L2W );

    m_NCollectedLights   = 0;
    m_CurrCollectedLight = 0;

    if( MaxLightCount > MAX_PROJ_LIGHTS )
    {
        x_DebugMsg( "ProjTextureMgr: MaxLightCount (%d) exceeds MAX_PROJ_LIGHTS (%d)\n", MaxLightCount, MAX_PROJ_LIGHTS );
        MaxLightCount = MAX_PROJ_LIGHTS;
    }

    for( s32 i = 0; (i < m_NLightProjections) && (m_NCollectedLights < MaxLightCount); i++ )
    {
        if( ProjectionIntersectsBBox( m_LightProjections[i], WorldBBox ) )
        {
            m_CollectedLights[m_NCollectedLights++] = i;
        }
    }

    if( (m_NCollectedLights == MaxLightCount) && (m_NCollectedLights < m_NLightProjections) )
    {
        x_DebugMsg( "ProjTextureMgr: more lights intersect than allowed\n" );
    }

    return m_NCollectedLights;
}

//=========================================================================

void proj_texture_mgr::GetCollectedLight( matrix4& LightMatrix, xbitmap*& pBitmap )
{
    ASSERT( m_CurrCollectedLight < m_NCollectedLights );

    const projection& Proj = m_LightProjections[m_CollectedLights[m_CurrCollectedLight++]];

    LightMatrix = Proj.ProjMatrix;

    texture* pTex = Proj.ProjTexture.GetPointer();
    pBitmap = pTex ? &pTex->m_Bitmap : NULL;
}

//=========================================================================

s32 proj_texture_mgr::CollectShadows( const matrix4& L2W, const bbox& B, s32 MaxShadowCount )
{
    bbox WorldBBox = B;
    WorldBBox.Transform( L2W );

    m_NCollectedShadows    = 0;
    m_CurrCollectedShadow  = 0;

    if( MaxShadowCount > MAX_PROJ_SHADOWS )
    {
        x_DebugMsg( "ProjTextureMgr: MaxShadowCount (%d) exceeds MAX_PROJ_SHADOWS (%d)\n", MaxShadowCount, MAX_PROJ_SHADOWS );
        MaxShadowCount = MAX_PROJ_SHADOWS;
    }

    for( s32 i = 0; (i < m_NShadowProjections) && (m_NCollectedShadows < MaxShadowCount); i++ )
    {
        if( ProjectionIntersectsBBox( m_ShadowProjections[i], WorldBBox ) )
        {
            m_CollectedShadows[m_NCollectedShadows++] = i;
        }
    }

    if( (m_NCollectedShadows == MaxShadowCount) && (m_NCollectedShadows < m_NShadowProjections) )
    {
        x_DebugMsg( "ProjTextureMgr: more shadows intersect than allowed\n" );
    }

    return m_NCollectedShadows;
}

//=========================================================================

void proj_texture_mgr::GetCollectedShadow( matrix4& ShadMatrix, xbitmap*& pBitmap )
{
    ASSERT( m_CurrCollectedShadow < m_NCollectedShadows );

    const projection& Proj = m_ShadowProjections[m_CollectedShadows[m_CurrCollectedShadow++]];

    ShadMatrix = Proj.ProjMatrix;

    texture* pTex = Proj.ProjTexture.GetPointer();
    pBitmap = pTex ? &pTex->m_Bitmap : NULL;
}

//=========================================================================

void proj_texture_mgr::SetupProjection ( projection&     Dest,
                                         const matrix4&  L2W,
                                         radian          FOV,
                                         f32             Length,
                                         texture::handle Texture )
{
    // set up the bitmap
    Dest.ProjTexture = Texture;

    // set up the view
    texture* pProjTexture = Texture.GetPointer();
    ASSERT( pProjTexture );
    xbitmap& ProjBMP = pProjTexture->m_Bitmap;
    Dest.ProjView.SetXFOV( FOV );
    Dest.ProjView.SetZLimits( 1.0f, Length );
    Dest.ProjView.SetViewport( 0, 0, ProjBMP.GetWidth(), ProjBMP.GetHeight() );
    Dest.ProjView.SetV2W( L2W );

    // set up the projection matrix
    Dest.ProjMatrix.Identity();
    Dest.ProjMatrix.Scale( vector3( 0.5f, -0.5f, 1.0f ) );
    Dest.ProjMatrix.Translate( vector3( 0.5f, 0.5f, 0.0f ) );
    Dest.ProjMatrix *= Dest.ProjView.GetW2C();
}

//=========================================================================
