//=========================================================================
//
//  PrimitiveBatch.cpp
//
//=========================================================================

#include "PrimitiveBatch.hpp"

#include "Pipeline/RenderPipeline.hpp"
#include "Backend/FrameStorage.hpp"
#include "e_Engine.hpp"
#include "x_debug.hpp"

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
u32 PackPrimitiveColor( xcolor const& Color )
{
    return (static_cast<u32>(Color.A) << 24) | (static_cast<u32>(Color.R) << 16) |
           (static_cast<u32>(Color.G) << 8) | static_cast<u32>(Color.B);
}

//=========================================================================

static
xcolor ColorFromPackedRGBA( u32 color )
{
    return xcolor( static_cast<u8>( color & 0xff ), static_cast<u8>( ( color >> 8 ) & 0xff ),
                   static_cast<u8>( ( color >> 16 ) & 0xff ), static_cast<u8>( ( color >> 24 ) & 0xff ) );
}

//=========================================================================

static
xbool PrepareSpriteScratch( xarray<render::primitive_sprite_instance>& Sprites, s32 Count )
{
    s32 const Maximum = render::FrameStorage::MAX_STREAM_BYTES / sizeof(render::primitive_sprite_instance);
    if( Count <= 0 || Count > Maximum )
        return FALSE;
    Sprites.SetCount( 0 );
    if( Sprites.GetCapacity() < Count )
    {
        s32 Capacity = MAX(Sprites.GetCapacity(), 256);
        while( Capacity < Count )
            Capacity = Capacity > Maximum / 2 ? Maximum : Capacity * 2;
        Sprites.SetCapacity( Capacity );
    }
    return TRUE;
}

//=========================================================================

static
xbool AppendQuad( render::primitive_vertex* pVertices,
                  s32                       MaxVertices,
                  s32&                      VertexCount,
                  u16*                      pIndices,
                  s32                       MaxIndices,
                  s32&                      IndexCount,
                  vector3 const*            pPositions,
                  vector2 const*            pUVs,
                  xcolor const&             color )
{
    if( !pVertices || !pIndices || !pPositions || !pUVs ||
        ((VertexCount + 4) > MaxVertices) || ((IndexCount + 6) > MaxIndices) )
    {
        return FALSE;
    }

    const u16 BaseVertex = (u16)VertexCount;
    for( s32 i = 0; i < 4; ++i )
        pVertices[VertexCount++] = render::primitive_vertex( pPositions[i], pUVs[i], color );

    const u16 QuadIndices[6] = { 0, 1, 2, 2, 3, 0 };
    for( s32 i = 0; i < 6; ++i )
        pIndices[IndexCount++] = (u16)(BaseVertex + QuadIndices[i]);

    return TRUE;
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

render::primitive_vertex::primitive_vertex( void )
    : Position( 0.0f, 0.0f, 0.0f ), Color( 0xffffffffu ), UV( 0.0f, 0.0f )
{
}

//=========================================================================

render::primitive_vertex::primitive_vertex( vector3 const& position, vector2 const& uV, xcolor const& color )
    : Position( position ), Color( PackPrimitiveColor( color ) ),
      UV( uV )
{
}

//=========================================================================

render::primitive_draw_desc::primitive_draw_desc( void )
    : pTexture( NULL ), Topology( PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ), Blend( PRIMITIVE_BLEND_OPAQUE ),
      Depth( PRIMITIVE_DEPTH_READ_WRITE ), Raster( PRIMITIVE_RASTER_SOLID ),
      Sampler( PRIMITIVE_SAMPLER_LINEAR_WRAP ), Output( PRIMITIVE_OUTPUT_COLOR ), DistortionScale( 12.0f )
{
}

//=========================================================================

render::primitive_draw_desc::primitive_draw_desc( texture const* pDrawTexture, primitive_topology topology,
                                                  primitive_blend_mode blend, primitive_depth_mode depth,
                                                  primitive_raster_mode raster, primitive_sampler_mode sampler,
                                                  primitive_output_mode OutputMode,
                                                  f32 DistortionScalePixels )
    : pTexture( pDrawTexture ), Topology( topology ), Blend( blend ), Depth( depth ), Raster( raster ),
      Sampler( sampler ), Output( OutputMode ), DistortionScale( DistortionScalePixels )
{
    ASSERT( ( Topology >= PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) && ( Topology < PRIMITIVE_TOPOLOGY_COUNT ) );
    ASSERT( ( Blend >= PRIMITIVE_BLEND_OPAQUE ) && ( Blend < PRIMITIVE_BLEND_COUNT ) );
    ASSERT( ( Depth >= PRIMITIVE_DEPTH_DISABLED ) && ( Depth < PRIMITIVE_DEPTH_COUNT ) );
    ASSERT( ( Raster >= PRIMITIVE_RASTER_SOLID ) && ( Raster < PRIMITIVE_RASTER_COUNT ) );
    ASSERT( ( Sampler >= PRIMITIVE_SAMPLER_LINEAR_WRAP ) && ( Sampler < PRIMITIVE_SAMPLER_COUNT ) );
    ASSERT( ( Output >= PRIMITIVE_OUTPUT_COLOR ) && ( Output < PRIMITIVE_OUTPUT_COUNT ) );
}

//=========================================================================

render::PrimitiveBatch::PrimitiveBatch( primitive_draw_desc const& desc )
    : m_Desc( desc ), m_Vertices(), m_Indices()
{
}

//=========================================================================

void render::PrimitiveBatch::Reserve( s32 nVertices, s32 nIndices )
{
    if ( ( nVertices > m_Vertices.GetCapacity() ) && ( nVertices <= MAX_PRIMITIVE_VERTICES ) )
    {
        m_Vertices.SetCapacity( nVertices );
    }

    if ( nIndices > m_Indices.GetCapacity() )
    {
        m_Indices.SetCapacity( nIndices );
    }
}

//=========================================================================

void render::PrimitiveBatch::Clear( void )
{
    m_Vertices.SetCount( 0 );
    m_Indices.SetCount( 0 );
}

//=========================================================================

xbool render::PrimitiveBatch::CanAppend( s32 nVertices, s32 nIndices ) const
{
    return ( nVertices > 0 ) && ( nIndices > 0 ) &&
           ( nVertices <= ( MAX_PRIMITIVE_VERTICES - m_Vertices.GetCount() ) ) &&
           ( nIndices <= ( 0x7fffffff - m_Indices.GetCount() ) );
}

//=========================================================================

xbool render::PrimitiveBatch::AppendIndexed( primitive_vertex const* pVertices, s32 nVertices, u16 const* pIndices,
                                             s32 nIndices )
{
    if ( !pVertices || !pIndices || !CanAppend( nVertices, nIndices ) )
    {
        return FALSE;
    }

    for ( s32 i = 0; i < nIndices; ++i )
    {
        if ( pIndices[i] >= nVertices )
        {
            return FALSE;
        }
    }

    u16 const BaseVertex = static_cast<u16>( m_Vertices.GetCount() );
    for ( s32 i = 0; i < nVertices; ++i )
    {
        m_Vertices.Append() = pVertices[i];
    }

    for ( s32 i = 0; i < nIndices; ++i )
    {
        m_Indices.Append() = static_cast<u16>( BaseVertex + pIndices[i] );
    }

    return TRUE;
}

//=========================================================================

xbool render::PrimitiveBatch::AddTriangle( primitive_vertex const& vertex0, primitive_vertex const& vertex1,
                                           primitive_vertex const& vertex2 )
{
    if ( m_Desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST )
    {
        return FALSE;
    }

    primitive_vertex const vertices[3] = { vertex0, vertex1, vertex2 };
    u16 const              indices[3] = { 0, 1, 2 };
    return AppendIndexed( vertices, 3, indices, 3 );
}

//=========================================================================

xbool render::PrimitiveBatch::AddLine( primitive_vertex const& vertex0, primitive_vertex const& vertex1 )
{
    if ( m_Desc.Topology != PRIMITIVE_TOPOLOGY_LINE_LIST )
    {
        return FALSE;
    }

    primitive_vertex const vertices[2] = { vertex0, vertex1 };
    u16 const              indices[2] = { 0, 1 };
    return AppendIndexed( vertices, 2, indices, 2 );
}

//=========================================================================

xbool render::PrimitiveBatch::AddLine( vector3 const& position0, vector3 const& position1, xcolor const& color0,
                                       xcolor const& color1 )
{
    return AddLine( primitive_vertex( position0, vector2( 0.0f, 0.0f ), color0 ),
                    primitive_vertex( position1, vector2( 0.0f, 0.0f ), color1 ) );
}

//=========================================================================

xbool render::PrimitiveBatch::AddQuad( vector3 const* pPositions, vector2 const* pUVs, xcolor const* pColors )
{
    if ( ( m_Desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) || !pPositions || !pUVs || !pColors )
    {
        return FALSE;
    }

    primitive_vertex const vertices[4] = {
        primitive_vertex( pPositions[0], pUVs[0], pColors[0] ),
        primitive_vertex( pPositions[1], pUVs[1], pColors[1] ),
        primitive_vertex( pPositions[2], pUVs[2], pColors[2] ),
        primitive_vertex( pPositions[3], pUVs[3], pColors[3] ) };
    u16 const indices[6] = { 0, 1, 2, 2, 3, 0 };
    return AppendIndexed( vertices, 4, indices, 6 );
}

//=========================================================================

xbool render::PrimitiveBatch::AddTriangleStripQuad( vector3 const* pPositions, vector2 const* pUVs,
                                                    xcolor const* pColors )
{
    if ( ( m_Desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) || !pPositions || !pUVs || !pColors )
    {
        return FALSE;
    }

    primitive_vertex const vertices[4] = {
        primitive_vertex( pPositions[0], pUVs[0], pColors[0] ),
        primitive_vertex( pPositions[1], pUVs[1], pColors[1] ),
        primitive_vertex( pPositions[2], pUVs[2], pColors[2] ),
        primitive_vertex( pPositions[3], pUVs[3], pColors[3] ) };
    u16 const indices[6] = { 0, 1, 2, 2, 1, 3 };
    return AppendIndexed( vertices, 4, indices, 6 );
}

//=========================================================================

xbool render::PrimitiveBatch::AddViewOrientedQuad( vector3 const& position0, vector3 const& position1,
                                                   vector3 const& ViewPosition, vector2 const& uV0, vector2 const& uV1,
                                                   xcolor const& color0, xcolor const& color1, f32 radius0,
                                                   f32 radius1 )
{
    vector3 direction = position1 - position0;
    if ( !direction.SafeNormalize() )
    {
        return FALSE;
    }

    vector3 cross = direction.Cross( ViewPosition - position0 );
    if ( !cross.SafeNormalize() )
    {
        return FALSE;
    }

    vector3 const cross0 = cross * radius0;
    vector3 const cross1 = cross * radius1;
    vector3 const positions[4] = { position1 + cross1, position1 - cross1, position0 - cross0, position0 + cross0 };
    vector2 const uVs[4] = { vector2( uV1.X, uV1.Y ), vector2( uV1.X, uV0.Y ), vector2( uV0.X, uV0.Y ),
                             vector2( uV0.X, uV1.Y ) };
    xcolor const colors[4] = { color1, color1, color0, color0 };
    return AddQuad( positions, uVs, colors );
}

//=========================================================================

xbool render::PrimitiveBatch::AddViewOrientedStrand( vector3 const* pPositions, s32 nPositions,
                                                     vector3 const& ViewPosition, vector2 const& uV0,
                                                     vector2 const& uV1, xcolor const& color0, xcolor const& color1,
                                                     f32 radius )
{
    if ( !pPositions || ( nPositions < 2 ) )
    {
        return FALSE;
    }

    for ( s32 i = 1; i < nPositions; ++i )
    {
        f32 const t0 = static_cast<f32>( i - 1 ) / static_cast<f32>( nPositions - 1 );
        f32 const t1 = static_cast<f32>( i ) / static_cast<f32>( nPositions - 1 );
        vector2 const StrandUV0( uV0.X + ( uV1.X - uV0.X ) * t0, uV0.Y );
        vector2 const StrandUV1( uV0.X + ( uV1.X - uV0.X ) * t1, uV1.Y );

        if ( !AddViewOrientedQuad( pPositions[i - 1], pPositions[i], ViewPosition, StrandUV0, StrandUV1, color0,
                                   color1, radius, radius ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

xbool render::PrimitiveBatch::AddPlaneAlignedQuad( vector3 const& center, vector3 const& normal, f32 radius,
                                                   xcolor const& color )
{
    vector3 UnitNormal = normal;
    if ( !UnitNormal.SafeNormalize() || ( radius <= 0.0f ) )
    {
        return FALSE;
    }

    vector3 AxisA;
    vector3 AxisB;
    if ( x_abs( UnitNormal.GetY() ) < 0.001f )
    {
        AxisA = UnitNormal.Cross( vector3( 0.0f, 1.0f, 0.0f ) );
        AxisB = UnitNormal.Cross( AxisA );
        if ( !AxisA.SafeNormalize() || !AxisB.SafeNormalize() )
        {
            return FALSE;
        }
    }
    else
    {
        plane OrientationPlane( UnitNormal, 0.0f );
        OrientationPlane.GetOrthoVectors( AxisA, AxisB );
    }

    AxisA *= radius;
    AxisB *= radius;
    vector3 const positions[4] = { center + AxisA + AxisB, center + AxisA - AxisB, center - AxisA - AxisB,
                                   center - AxisA + AxisB };
    vector2 const uVs[4] = { vector2( 0.0f, 0.0f ), vector2( 1.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                             vector2( 0.0f, 1.0f ) };
    xcolor const colors[4] = { color, color, color, color };
    return AddQuad( positions, uVs, colors );
}

//=========================================================================

xbool render::PrimitiveBatch::Submit( matrix4 const& LocalToWorld ) const
{
    if ( ( m_Vertices.GetCount() == 0 ) || ( m_Indices.GetCount() == 0 ) )
    {
        return TRUE;
    }

    return SubmitPrimitives( m_Desc, LocalToWorld, m_Vertices.GetPtr(), m_Vertices.GetCount(), m_Indices.GetPtr(),
                             m_Indices.GetCount() );
}

//=========================================================================

xbool render::PrimitiveBatch::TrySubmit( matrix4 const& LocalToWorld ) const
{
    if( m_Vertices.GetCount() == 0 || m_Indices.GetCount() == 0 )
    {
        return TRUE;
    }

    const view* pView = eng_GetView();
    PrimitiveEncoder& Encoder = GetRenderPipeline().GetPrimitiveEncoder();
    if( !pView || !Encoder.IsSubmissionActive() )
    {
        return FALSE;
    }

    return Encoder.SubmitPrimitives( m_Desc, LocalToWorld, *pView, m_Vertices.GetPtr(), m_Vertices.GetCount(),
                                     m_Indices.GetPtr(), m_Indices.GetCount() );
}

//=========================================================================

render::primitive_draw_desc const& render::PrimitiveBatch::GetDesc( void ) const
{
    return m_Desc;
}

//=========================================================================

s32 render::PrimitiveBatch::GetVertexCount( void ) const
{
    return m_Vertices.GetCount();
}

//=========================================================================

s32 render::PrimitiveBatch::GetIndexCount( void ) const
{
    return m_Indices.GetCount();
}

//=========================================================================

xbool render::SubmitPrimitiveSprite( primitive_draw_desc const& desc, vector3 const& position, vector2 const& size,
                                     vector2 const& uV0, vector2 const& uV1, xcolor const& color, radian rotation )
{
    view const* pView = eng_GetView();
    if ( !pView || !position.IsValid() || !size.IsValid() || !uV0.IsValid() || !uV1.IsValid() ||
         !x_isvalid( rotation ) )
    {
        return FALSE;
    }

    // FX scale controllers legitimately cross zero and use negative values to
    // mirror sprites. A zero-sized sprite is a successful, empty submission.
    if ( ( size.X == 0.0f ) || ( size.Y == 0.0f ) )
    {
        return TRUE;
    }

    vector3 center = pView->GetW2V() * position;
    f32 sine;
    f32 cosine;
    x_sincos( -rotation, sine, cosine );

    f32 const     HalfWidth = size.X * 0.5f;
    f32 const     HalfHeight = size.Y * 0.5f;
    primitive_sprite_instance Sprite;
    Sprite.Position = center;
    Sprite.Color = PackPrimitiveColor( color );
    Sprite.Right.Set( -cosine * HalfWidth, -sine * HalfWidth, 0.0f );
    Sprite.Up.Set( sine * HalfHeight, -cosine * HalfHeight, 0.0f );
    Sprite.UVRect[0] = uV0.X;
    Sprite.UVRect[1] = uV0.Y;
    Sprite.UVRect[2] = uV1.X;
    Sprite.UVRect[3] = uV1.Y;
    return GetRenderPipeline().GetBackend().GetPrimitiveEncoder().SubmitSprites( desc, *pView, &Sprite, 1 );
}

//=========================================================================

xbool render::SubmitPrimitiveBillboards( primitive_draw_desc const& desc, s32 nSprites, f32 UniformScale,
                                         matrix4 const* pLocalToWorld, vector4 const* pPositions,
                                         vector2 const* pRotScales, u32 const* pColors,
                                         s32 const* pSpriteIndices )
{
    if ( ( nSprites <= 0 ) || !x_isvalid( UniformScale ) || !pPositions || !pRotScales || !pColors ||
         ( pLocalToWorld && !pLocalToWorld->IsValid() ) )
    {
        return FALSE;
    }

    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    matrix4 LocalToView = pView->GetW2V();
    if ( pLocalToWorld )
    {
        LocalToView = LocalToView * ( *pLocalToWorld );
    }

    if( desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST )
        return FALSE;

    xarray<primitive_sprite_instance>& Sprites = GetRenderPipeline().GetPrimitiveStorage().GetSpriteScratch();
    if( !PrepareSpriteScratch( Sprites, nSprites ) )
        return FALSE;
    for ( s32 i = 0; i < nSprites; ++i )
    {
        s32 const SpriteIndex = pSpriteIndices ? pSpriteIndices[i] : i;
        if ( ( pPositions[SpriteIndex].GetIW() & 0x8000 ) != 0 )
            continue;
        primitive_sprite_instance& Sprite = Sprites.Append();
        Sprite.Position = LocalToView * vector3( pPositions[SpriteIndex].GetX(),
                                                pPositions[SpriteIndex].GetY(), pPositions[SpriteIndex].GetZ() );
        f32 Sine, Cosine;
        x_sincos( -pRotScales[SpriteIndex].X, Sine, Cosine );
        f32 const Scale = pRotScales[SpriteIndex].Y * UniformScale;
        Sprite.Right.Set( -Cosine * Scale, -Sine * Scale, 0.0f );
        Sprite.Up.Set( Sine * Scale, -Cosine * Scale, 0.0f );
        Sprite.Color = PackPrimitiveColor( ColorFromPackedRGBA(pColors[SpriteIndex]) );
        Sprite.UVRect[0] = Sprite.UVRect[1] = 0.0f;
        Sprite.UVRect[2] = Sprite.UVRect[3] = 1.0f;
    }
    return Sprites.GetCount() == 0 ||
        GetRenderPipeline().GetBackend().GetPrimitiveEncoder().SubmitSprites( desc, *pView, Sprites.GetPtr(), Sprites.GetCount() );
}

//=========================================================================

xbool render::SubmitPrimitiveVelocityBillboards( primitive_draw_desc const& desc, s32 nSprites, f32 UniformScale,
                                                 matrix4 const* pLocalToWorld, matrix4 const* pVelocityMatrix,
                                                 vector4 const* pPositions,
                                                 vector4 const* pVelocities, u32 const* pColors,
                                                 s32 const* pSpriteIndices )
{
    if ( ( nSprites <= 0 ) || !x_isvalid( UniformScale ) || !pPositions || !pVelocities || !pColors ||
         ( pLocalToWorld && !pLocalToWorld->IsValid() ) ||
         ( pVelocityMatrix && !pVelocityMatrix->IsValid() ) )
    {
        return FALSE;
    }

    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    matrix4 LocalToWorld;
    if ( pLocalToWorld )
    {
        LocalToWorld = *pLocalToWorld;
    }
    else
    {
        LocalToWorld.Identity();
    }

    matrix4 VelocityToWorld = LocalToWorld;
    VelocityToWorld.ClearTranslation();
    if ( pVelocityMatrix )
    {
        VelocityToWorld = VelocityToWorld * ( *pVelocityMatrix );
    }
    vector3 const ViewDirection = pView->GetViewZ();
    matrix4 WorldToView = pView->GetW2V();
    matrix4 DirectionToView = WorldToView;
    DirectionToView.ClearTranslation();
    if( desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
        static_cast<u64>( nSprites ) * sizeof( primitive_sprite_instance ) > render::FrameStorage::MAX_STREAM_BYTES )
        return FALSE;

    xarray<primitive_sprite_instance>& Sprites = GetRenderPipeline().GetPrimitiveStorage().GetSpriteScratch();
    if( !PrepareSpriteScratch( Sprites, nSprites ) )
        return FALSE;
    for ( s32 i = 0; i < nSprites; ++i )
    {
        s32 const SpriteIndex = pSpriteIndices ? pSpriteIndices[i] : i;
        if ( ( pPositions[SpriteIndex].GetIW() & 0x8000 ) != 0 )
            continue;
        vector3 Right( pVelocities[SpriteIndex].GetX(), pVelocities[SpriteIndex].GetY(), pVelocities[SpriteIndex].GetZ() );
        Right = VelocityToWorld * Right;
        if( !Right.SafeNormalize() )
            continue;
        vector3 Up = ViewDirection.Cross( Right );
        f32 const Scale = pVelocities[SpriteIndex].GetW() * UniformScale;
        primitive_sprite_instance& Sprite = Sprites.Append();
        Sprite.Position = WorldToView * ( LocalToWorld * vector3( pPositions[SpriteIndex].GetX(),
            pPositions[SpriteIndex].GetY(), pPositions[SpriteIndex].GetZ() ) );
        Sprite.Right = DirectionToView * ( Right * Scale );
        Sprite.Up = DirectionToView * ( Up * Scale );
        Sprite.Color = PackPrimitiveColor( ColorFromPackedRGBA(pColors[SpriteIndex]) );
        Sprite.UVRect[0] = Sprite.UVRect[1] = 0.0f;
        Sprite.UVRect[2] = Sprite.UVRect[3] = 1.0f;
    }
    return Sprites.GetCount() == 0 ||
        GetRenderPipeline().GetBackend().GetPrimitiveEncoder().SubmitSprites( desc, *pView, Sprites.GetPtr(), Sprites.GetCount() );
}
