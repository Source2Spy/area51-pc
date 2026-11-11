//==============================================================================
//
//  ClothInst.cpp
//
//==============================================================================

//==============================================================================
// INCLUDES
//==============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "ClothInst.hpp"
#include "Entropy.hpp"
#include "Render/Material.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

#ifdef X_EDITOR
extern xbool g_game_running;
#endif

//==============================================================================
// FUNCTIONS
//==============================================================================

cloth_inst::cloth_inst()
{
    m_RenderMask    = (u64)-1;
    m_MaterialIndex = -1;
    m_pRenderNormalsSource = NULL;
    m_RenderNormalsRevision = 0;
    m_isTessellationInitialized = FALSE;
    m_DamageUploadPending = FALSE;
    m_DamageUploadMinX = 0;
    m_DamageUploadMinY = 0;
    m_DamageUploadMaxX = -1;
    m_DamageUploadMaxY = -1;
}

//==============================================================================

cloth_inst::~cloth_inst()
{
    Kill();
}

//==============================================================================

void cloth_inst::OnEnumProp( prop_enum& List )
{
    // Rigid inst
    List.PropEnumHeader ( "RenderInst", "Render Instance", 0 );
    m_RigidInst.OnEnumProp ( List );
}

//=============================================================================

xbool cloth_inst::OnProperty( prop_query& I, cloth& Sim )
{
    // Rigid inst?
    if( I.IsSimilarPath( "RenderInst" ) )
    {
        if( m_RigidInst.OnProperty( I ) )
        {
            if( I.IsVar( "RenderInst\\File" ) )
            {
                // Setup cloth?
                if( I.IsRead() == FALSE )
                    Init( Sim );
            }

            return TRUE ;
        }
    }

    return FALSE;
}

//==============================================================================

void cloth_inst::Init( cloth& Sim )
{
    Kill();

    // Lookup geometry
    rigid_geom* pGeom = m_RigidInst.GetRigidGeom();
    if (!pGeom)
        return;

    // Let the simulation build its particles/connections/triangles from the
    // geometry's "cloth"/"flag" tagged submeshes, and hand back what render needs.
    cloth_init_result Result = Sim.Init( pGeom );
    m_RenderMask    = Result.RenderMask;
    m_MaterialIndex = Result.MaterialIndex;

    // Size the reused dynamic vertex buffer and build the (immutable) index
    // buffer from the simulation's now-populated triangle list.
    const xarray<cloth_particle>&  Particles = Sim.GetParticles();
    const xarray<cloth_triangle>&  Triangles = Sim.GetTriangles();
    ASSERT( Particles.GetCount() <= 65535 );
    m_RenderVertices.SetCapacity( Particles.GetCount() );
    m_RenderVertices.SetCount( Particles.GetCount() );
    m_RenderNormals.SetCapacity( Particles.GetCount() );
    m_RenderNormals.SetCount( Particles.GetCount() );

    m_RenderIndices.SetCapacity( Triangles.GetCount() * 3 );
    m_RenderIndices.SetCount( Triangles.GetCount() * 3 );
    for( s32 i = 0; i < Triangles.GetCount(); ++i )
    {
        const cloth_triangle& Triangle = Triangles[i];
        m_RenderIndices[i * 3 + 0] = (u16)Triangle.m_Particles[0];
        m_RenderIndices[i * 3 + 1] = (u16)Triangle.m_Particles[1];
        m_RenderIndices[i * 3 + 2] = (u16)Triangle.m_Particles[2];
    }

    texture* pTexture = NULL;
    geom const* pRenderGeom = m_RigidInst.GetGeom();
    if( pRenderGeom && ( m_MaterialIndex != -1 ) )
    {
        pTexture = render::GetVTexture( pRenderGeom, m_MaterialIndex, 0 );
    }

    if( pTexture && !InitDamageTexture( Sim, pTexture ) )
    {
        x_DebugMsg( "cloth_inst: Failed to initialize damage texture\n" );
    }
}

//==============================================================================

void cloth_inst::Kill( void )
{
    m_Tessellation.Kill();
    m_isTessellationInitialized = FALSE;
    vram_DestroyTexture( m_DamageTexture );
    m_DamageUpload.SetCount( 0 );
    m_RenderVertices.SetCount( 0 );
    m_RenderNormals.SetCount( 0 );
    m_RenderIndices.SetCount( 0 );
    m_pRenderNormalsSource = NULL;
    m_RenderNormalsRevision = 0;
    m_RenderMask    = (u64)-1;
    m_MaterialIndex = -1;
    m_DamageUploadPending = FALSE;
    m_DamageUploadMinX = 0;
    m_DamageUploadMinY = 0;
    m_DamageUploadMaxX = -1;
    m_DamageUploadMaxY = -1;
}

//==============================================================================

xbool cloth_inst::InitDamageTexture( cloth& Sim, texture const* pTexture )
{
    if( !pTexture )
    {
        return FALSE;
    }

    s32 const TextureWidth = pTexture->m_bitmap.GetWidth();
    s32 const TextureHeight = pTexture->m_bitmap.GetHeight();
    if( ( TextureWidth <= 0 ) || ( TextureHeight <= 0 ) )
    {
        return FALSE;
    }

    // Small diffuse maps still need room for narrow tears and loose yarns.
    // Preserve the texture aspect ratio and keep the mask within 512 texels.
    f32 const Scale = MIN( 512.0f / MAX( TextureWidth, TextureHeight ),
                          MAX( 1.0f, 256.0f / MIN( TextureWidth, TextureHeight ) ) );
    s32 const Width = MINMAX( 1, (s32)( TextureWidth * Scale ), 512 );
    s32 const Height = MINMAX( 1, (s32)( TextureHeight * Scale ), 512 );

    Sim.InitDamageMap( Width, Height );
    m_DamageUpload.SetCapacity( Width * Height );

    vram_texture_desc Desc;
    Desc.Width      = Width;
    Desc.Height     = Height;
    Desc.Format     = VRAM_TEXTURE_FORMAT_R8;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.pDebugName = "ClothDamage";
    if( !vram_CreateTexture( m_DamageTexture, Desc ) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool cloth_inst::PrepareDamageUpload( cloth& Sim )
{
    s32 MinX, MinY, MaxX, MaxY;
    if( !Sim.GetDamageDirty( MinX, MinY, MaxX, MaxY ) )
    {
        return TRUE;
    }

    if( !m_DamageTexture )
    {
        return FALSE;
    }

    if( m_DamageUploadPending )
    {
        MinX = MIN( MinX, m_DamageUploadMinX );
        MinY = MIN( MinY, m_DamageUploadMinY );
        MaxX = MAX( MaxX, m_DamageUploadMaxX );
        MaxY = MAX( MaxY, m_DamageUploadMaxY );
    }

    m_DamageUploadMinX = MinX;
    m_DamageUploadMinY = MinY;
    m_DamageUploadMaxX = MaxX;
    m_DamageUploadMaxY = MaxY;

    s32 const Width = m_DamageUploadMaxX - m_DamageUploadMinX + 1;
    s32 const Height = m_DamageUploadMaxY - m_DamageUploadMinY + 1;
    s32 const UploadSize = Width * Height;
    xarray<u8> const& DamageMap = Sim.GetDamageMap();
    m_DamageUpload.SetCount( UploadSize );
    for( s32 Y = 0; Y < Height; ++Y )
    {
        u8 const* pSource = &DamageMap[( ( m_DamageUploadMinY + Y ) * Sim.GetDamageWidth() ) + m_DamageUploadMinX];
        x_memcpy( &m_DamageUpload[Y * Width], pSource, Width );
    }

    Sim.ClearDamageDirty();
    m_DamageUploadPending = TRUE;
    return TRUE;
}

//==============================================================================
// Render functions
//==============================================================================

void cloth_inst::ComputeRenderNormals( cloth const& Sim )
{
    if( ( m_pRenderNormalsSource == &Sim ) &&
        ( m_RenderNormalsRevision == Sim.GetRenderRevision() ) )
    {
        return;
    }

    const xarray<cloth_particle>& Particles = Sim.GetParticles();
    const xarray<cloth_triangle>& Triangles = Sim.GetTriangles();
    ASSERT( m_RenderNormals.GetCount() == Particles.GetCount() );

    for( s32 i = 0; i < Particles.GetCount(); ++i )
        m_RenderNormals[i].Zero();

    for( s32 i = 0; i < Triangles.GetCount(); ++i )
    {
        const cloth_triangle& Triangle = Triangles[i];
        const vector3& P0 = Particles[(s32)Triangle.m_Particles[0]].m_Pos;
        const vector3& P1 = Particles[(s32)Triangle.m_Particles[1]].m_Pos;
        const vector3& P2 = Particles[(s32)Triangle.m_Particles[2]].m_Pos;
        const vector3 Normal = ( P1 - P0 ).Cross( P2 - P0 );

        m_RenderNormals[(s32)Triangle.m_Particles[0]] += Normal;
        m_RenderNormals[(s32)Triangle.m_Particles[1]] += Normal;
        m_RenderNormals[(s32)Triangle.m_Particles[2]] += Normal;
    }

    for( s32 i = 0; i < Particles.GetCount(); ++i )
        m_RenderNormals[i].Normalize();

    m_pRenderNormalsSource = &Sim;
    m_RenderNormalsRevision = Sim.GetRenderRevision();
}

//==============================================================================

void cloth_inst::BuildRenderVertices( cloth const& Sim )
{
    xarray<cloth_particle> const& Particles = Sim.GetParticles();
    ASSERT( m_RenderVertices.GetCount() == Particles.GetCount() );

    ComputeRenderNormals( Sim );

    for( s32 i = 0; i < Particles.GetCount(); ++i )
    {
        cloth_particle const& Particle = Particles[i];
        dynamic_geom_vertex& Vertex = m_RenderVertices[i];
        Vertex.Position = Particle.m_Pos;
        Vertex.Normal   = m_RenderNormals[i];
        Vertex.UV       = Particle.m_UV;
        Vertex.Color    = XCOLOR_BLACK;
    }
}

//==============================================================================

xbool cloth_inst::PrepareTessellation( cloth const& Sim )
{
    BuildRenderVertices( Sim );

    if( !render::GetRenderPipeline().IsTessellationEnabled() )
    {
        return TRUE;
    }

    if( !m_isTessellationInitialized )
    {
        if( !m_Tessellation.Init( m_RenderVertices.GetCount(), m_RenderIndices.GetPtr(), m_RenderIndices.GetCount() ) )
        {
            RENDER_THROW( "cloth_inst", "failed to initialize compute tessellation" );
            return FALSE;
        }
        m_isTessellationInitialized = TRUE;
    }

    return TRUE;
}

//==============================================================================

void cloth_inst::RenderClothGeometry( cloth& Sim, xcolor Ambient, s32 VTexture /*= 0*/, u32 Flags /*= 0*/ )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth_inst::RenderClothGeometry" );

    // Grab the texture that is currently being used by the cloth. Note
    // that we go through the render system to grab the texture out. Since
    // the geometry has been registered, the render system will have
    // registered the texture and have a nice way to access it rather than
    // using the resource system.
    texture* pTexture = NULL;
    const geom* pGeom = m_RigidInst.GetGeom();
    if( pGeom && (m_MaterialIndex != -1) )
    {
        pTexture = render::GetVTexture( pGeom, m_MaterialIndex, VTexture );
    }

    // Render dynamic cloth?
    if( pTexture && PrepareDamageUpload( Sim ) )
    {
        if( !PrepareTessellation( Sim ) )
        {
            return;
        }

        geom::material const& GeomMaterial = pGeom->m_pMaterial[m_MaterialIndex];
        render::MaterialDesc const& Material = pGeom->GetPreparedMaterial( GeomMaterial.iVirtualMat ).GetDesc();

        dynamic_geom_draw Draw;
        Draw.pGpuMesh       = render::GetRenderPipeline().IsTessellationEnabled() ? m_Tessellation.GetMesh() : NULL;
        Draw.pClothTessellation = Draw.pGpuMesh ? &m_Tessellation : NULL;
        Draw.ClothRevision = Sim.GetRenderRevision();
        Draw.pVertices      = m_RenderVertices.GetPtr();
        Draw.pIndices       = m_RenderIndices.GetPtr();
        Draw.pDiffuseTexture = pTexture;
        Draw.CoverageUsesTextureAlpha = Material.AlphaBlend || Material.AlphaTest;
        Draw.pDamageMask    = vram_GetShaderResource( m_DamageTexture );
        Draw.pDamageTexture = &m_DamageTexture;
        Draw.pDamageUploadPending = &m_DamageUploadPending;
        if( m_DamageUploadPending )
        {
            Draw.pDamageUpload = m_DamageUpload.GetPtr();
            Draw.DamageUploadX = m_DamageUploadMinX;
            Draw.DamageUploadY = m_DamageUploadMinY;
            Draw.DamageUploadWidth = m_DamageUploadMaxX - m_DamageUploadMinX + 1;
            Draw.DamageUploadHeight = m_DamageUploadMaxY - m_DamageUploadMinY + 1;
        }
        Draw.VertexCount    = m_RenderVertices.GetCount();
        Draw.IndexCount     = m_RenderIndices.GetCount();
        Draw.Bounds         = Sim.GetWorldBBox();
        Draw.Ambient        = Ambient;
        Draw.RenderFlags    = Flags;
        render::GetRenderPipeline().GetSubmission().AddDynamicGeom( Draw );
    }
}

//==============================================================================

void cloth_inst::RenderShadowCast( cloth& Sim, s32 VTexture, u64 ProjMask )
{
    if( ( ProjMask == 0 ) || !m_RigidInst.GetGeom() )
        return;

    u64 const RigidMask = m_RigidInst.GetLODMask( Sim.GetL2W() ) & m_RenderMask;
    if( RigidMask )
        render::GetRenderPipeline().GetShadowSubmission().AddRigidCaster( m_RigidInst.GetInst(), &Sim.GetL2W(), RigidMask, ProjMask );

    texture* pTexture = NULL;
    geom const* pGeom = m_RigidInst.GetGeom();
    if( pGeom && ( m_MaterialIndex != -1 ) )
        pTexture = render::GetVTexture( pGeom, m_MaterialIndex, VTexture );

    if( !pTexture || !pTexture->GetShaderResource() || !PrepareDamageUpload( Sim ) )
        return;

    if( !PrepareTessellation( Sim ) )
    {
        return;
    }

    geom::material const& GeomMaterial = pGeom->m_pMaterial[m_MaterialIndex];
    render::MaterialDesc const& Material = pGeom->GetPreparedMaterial( GeomMaterial.iVirtualMat ).GetDesc();

    dynamic_geom_shadow_draw Draw;
    Draw.pGpuMesh = render::GetRenderPipeline().IsTessellationEnabled() ? m_Tessellation.GetMesh() : NULL;
    Draw.pClothTessellation = Draw.pGpuMesh ? &m_Tessellation : NULL;
    Draw.ClothRevision = Sim.GetRenderRevision();
    Draw.Bounds = Sim.GetWorldBBox();
    Draw.pVertices = m_RenderVertices.GetPtr();
    Draw.pIndices = m_RenderIndices.GetPtr();
    Draw.pDiffuse = pTexture->GetShaderResource();
    Draw.CoverageUsesTextureAlpha = Material.AlphaBlend || Material.AlphaTest;
    Draw.pDamageMask = vram_GetShaderResource( m_DamageTexture );
    Draw.pDamageTexture = &m_DamageTexture;
    Draw.pDamageUploadPending = &m_DamageUploadPending;
    if( m_DamageUploadPending )
    {
        Draw.pDamageUpload = m_DamageUpload.GetPtr();
        Draw.DamageUploadX = m_DamageUploadMinX;
        Draw.DamageUploadY = m_DamageUploadMinY;
        Draw.DamageUploadWidth = m_DamageUploadMaxX - m_DamageUploadMinX + 1;
        Draw.DamageUploadHeight = m_DamageUploadMaxY - m_DamageUploadMinY + 1;
    }
    Draw.VertexCount = m_RenderVertices.GetCount();
    Draw.IndexCount = m_RenderIndices.GetCount();
    Draw.ShadowSourceMask = ProjMask;
    render::GetRenderPipeline().GetShadowSubmission().AddDynamicCaster( Draw );
}

//==============================================================================

void cloth_inst::RenderRigidGeometry( const cloth& Sim, xcolor Ambient, u32 Flags )
{
    // Render rigid instance
    m_RigidInst.Render( &Sim.GetL2W(), Flags, m_RenderMask, Ambient );
}

//==============================================================================

#if !defined( CONFIG_RETAIL )

void cloth_inst::RenderSkeleton( const cloth& Sim )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "cloth_inst::RenderSkeleton");

    ComputeRenderNormals( Sim );

    s32 i;

    // Render world wind direction arrow
    f32     Radius = Sim.GetWorldBBox().GetRadius();
    vector3 Dir    = Radius * 0.5f * Sim.GetL2W().RotateVector( Sim.GetWindDir() );
    vector3 Center = ( Sim.GetL2W() * Sim.GetLocalBBox().GetCenter() ) - ( Dir * 2.0f );
    vector3 Start  = Center - Dir;
    vector3 End    = Center + Dir;
    xcolor  Color  = Sim.IsWindActive() ? XCOLOR_GREEN : XCOLOR_RED;

#ifdef X_EDITOR
    // Render white if not running the game
    if( g_game_running == FALSE )
        Color = XCOLOR_WHITE;
#endif

    render::debug::Arrow( Start, End, Color );

#ifdef X_EDITOR
    if( g_game_running )
        render::debug::Label( Center, Color, Sim.IsWindActive() ? "\n\nWindOn" : "\n\nWindOff" );
    else
        render::debug::Label( Center, Color, "\n\nWind" );
#else
    render::debug::Label( Center, Color, Sim.IsWindActive() ? "\n\nWindOn" : "\n\nWindOff" );
#endif

    // Render all connections
    const xarray<cloth_particle>&    Particles   = Sim.GetParticles();
    const xarray<cloth_connection>&  Connections = Sim.GetConnections();
    for (i = 0; i < Connections.GetCount(); i++)
    {
        const cloth_connection& Connection = Connections[i];

        render::debug::Line( Particles[(s32)Connection.m_ParticleA].m_Pos,
                   Particles[(s32)Connection.m_ParticleB].m_Pos, XCOLOR_BLUE );
    }

    // Render all particles
    for (i = 0; i < Particles.GetCount(); i++)
    {
        // Lookup particle info
        const cloth_particle& Particle = Particles[i];
        f32    Mass = Particle.GetMass();
        xcolor Color = (Mass == 0.0f) ? XCOLOR_RED : XCOLOR_GREEN;
        s32    Size  = (Mass == 0.0f) ? 5 : 2 ;

        // Draw particle and normal
        render::debug::Point(Particle.m_Pos, Color, Size );
        render::debug::Arrow( Particle.m_Pos, Particle.m_Pos + (m_RenderNormals[i] * 15.0f), Color );

        // Show mass
        render::debug::Label( Particle.m_Pos, Color, "\n\n\n\nP[%d]\nMass(%.2f)", i, Mass );
    }

    // Render world collision bbox
    render::debug::Box( Sim.GetWorldCollBBox(), XCOLOR_YELLOW );
}

#endif  //#if !defined( CONFIG_RETAIL )

//==============================================================================
