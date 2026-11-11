//=========================================================================
//
//  PrimitiveDebug.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "PrimitiveDebug.hpp"

#include "UI/ui_renderer.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_font.hpp"

#include "e_Engine.hpp"
#include "x_debug.hpp"

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static
render::primitive_draw_desc LineDesc( render::primitive_depth_mode depth, render::primitive_raster_mode raster = render::PRIMITIVE_RASTER_SOLID_NO_CULL )
{
    return render::primitive_draw_desc( NULL, render::PRIMITIVE_TOPOLOGY_LINE_LIST, render::PRIMITIVE_BLEND_ALPHA,
                                        depth, raster, render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);
}

//=========================================================================

static
render::primitive_draw_desc TriangleDesc( render::primitive_depth_mode depth, render::primitive_raster_mode raster )
{
    return render::primitive_draw_desc( NULL, render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                        render::PRIMITIVE_BLEND_ALPHA, depth, raster,
                                        render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);
}

//=========================================================================

static
matrix4 IdentityMatrix( void )
{
    matrix4 result;
    result.Identity();
    return result;
}

//=========================================================================

static
xbool AddBoxLines( render::PrimitiveBatch& batch, bbox const& bounds, xcolor color )
{
    vector3 const positions[8] = {
        vector3( bounds.Min.GetX(), bounds.Min.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Min.GetX(), bounds.Min.GetY(), bounds.Max.GetZ() ),
        vector3( bounds.Min.GetX(), bounds.Max.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Min.GetX(), bounds.Max.GetY(), bounds.Max.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Min.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Min.GetY(), bounds.Max.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Max.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Max.GetY(), bounds.Max.GetZ() ) };
    u16 const pairs[24] = { 1, 5, 5, 7, 7, 3, 3, 1, 0, 4, 4, 6, 6, 2, 2, 0, 3, 2, 7, 6, 5, 4, 1, 0 };

    for ( s32 i = 0; i < ARRAYSIZE( pairs ); i += 2 )
    {
        if ( !batch.AddLine( positions[pairs[i]], positions[pairs[i + 1]], color, color ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=========================================================================

static
xbool SubmitVolumeGeometry( vector3 const& position0, vector3 const& position1, f32 width, f32 height,
                            xcolor color, render::primitive_depth_mode depth, matrix4 const& LocalToWorld )
{
    vector3 corners[8];
    vector3 slope = position0 - position1;
    slope.GetY() = 0.0f;

    corners[0] = slope;
    corners[0].RotateY( PI * 0.5f );
    corners[0].NormalizeAndScale( width );
    corners[0] += position0;
    corners[0].GetY() += height;

    corners[1] = slope;
    corners[1].RotateY( PI * 1.5f );
    corners[1].NormalizeAndScale( width );
    corners[1] += position0;
    corners[1].GetY() += height;
    corners[2] = corners[0];
    corners[2].GetY() -= height * 2.0f;
    corners[3] = corners[1];
    corners[3].GetY() -= height * 2.0f;

    vector3 const offset = position1 - position0;
    for ( s32 i = 0; i < 4; ++i )
    {
        corners[i + 4] = corners[i] + offset;
    }

    u16 const indices[36] = { 0, 1, 3, 3, 2, 0, 6, 7, 5, 5, 4, 6, 4, 5, 1, 1, 0, 4,
                              6, 2, 3, 3, 7, 6, 6, 4, 0, 0, 2, 6, 3, 1, 5, 5, 7, 3 };
    render::primitive_vertex vertices[8];
    for ( s32 i = 0; i < ARRAYSIZE( vertices ); ++i )
    {
        vertices[i] = render::primitive_vertex( corners[i], vector2( 0.0f, 0.0f ), color );
    }

    return render::SubmitPrimitives( TriangleDesc( depth, render::PRIMITIVE_RASTER_SOLID_NO_CULL ),
                                     LocalToWorld, vertices, ARRAYSIZE( vertices ), indices, ARRAYSIZE( indices ) );
}

//=========================================================================

static
xbool GetProjectedPoint( vector3 const& position, vector3& ScreenPosition, irect& ScreenBounds )
{
    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    ScreenPosition = pView->PointToScreen( position );
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
    pView->GetViewport( x0, y0, x1, y1 );

    s32 width;
    s32 height;
    eng_GetRes( width, height );
    ScreenBounds.Set( 0, 0, width, height );
    return TRUE;
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool render::debug::Line( vector3 const& position0, vector3 const& position1, xcolor color,
                           primitive_depth_mode depth )
{
    primitive_vertex const vertices[2] = {
        primitive_vertex( position0, vector2( 0.0f, 0.0f ), color ),
        primitive_vertex( position1, vector2( 0.0f, 0.0f ), color ) };
    u16 const indices[2] = { 0, 1 };
    matrix4 const identity = IdentityMatrix();
    return SubmitPrimitives( LineDesc( depth ), identity, vertices, ARRAYSIZE( vertices ), indices,
                             ARRAYSIZE( indices ) );
}

//=========================================================================

xbool render::debug::Box( bbox const& bounds, xcolor color, primitive_depth_mode depth,
                          primitive_raster_mode raster )
{
    PrimitiveBatch batch( LineDesc( depth, raster ) );
    batch.Reserve( 24, 24 );
    matrix4 const identity = IdentityMatrix();
    return AddBoxLines( batch, bounds, color ) && batch.Submit( identity );
}

//=========================================================================

xbool render::debug::Box( bbox const& bounds, matrix4 const& LocalToWorld, xcolor color,
                          primitive_depth_mode depth, primitive_raster_mode raster )
{
    PrimitiveBatch batch( LineDesc( depth, raster ) );
    batch.Reserve( 24, 24 );
    return AddBoxLines( batch, bounds, color ) && batch.Submit( LocalToWorld );
}

//=========================================================================

xbool render::debug::SolidBox( bbox const& bounds, xcolor color, primitive_depth_mode depth,
                               primitive_raster_mode raster )
{
    vector3 const positions[8] = {
        vector3( bounds.Min.GetX(), bounds.Min.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Min.GetX(), bounds.Min.GetY(), bounds.Max.GetZ() ),
        vector3( bounds.Min.GetX(), bounds.Max.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Min.GetX(), bounds.Max.GetY(), bounds.Max.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Min.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Min.GetY(), bounds.Max.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Max.GetY(), bounds.Min.GetZ() ),
        vector3( bounds.Max.GetX(), bounds.Max.GetY(), bounds.Max.GetZ() ) };
    primitive_vertex vertices[8];
    for ( s32 i = 0; i < ARRAYSIZE( vertices ); ++i )
    {
        vertices[i] = primitive_vertex( positions[i], vector2( 0.0f, 0.0f ), color );
    }

    u16 const indices[36] = { 0, 2, 3, 3, 1, 0, 4, 5, 7, 7, 6, 4, 0, 1, 5, 5, 4, 0,
                              2, 6, 7, 7, 3, 2, 0, 4, 6, 6, 2, 0, 1, 3, 7, 7, 5, 1 };
    matrix4 const identity = IdentityMatrix();
    return SubmitPrimitives( TriangleDesc( depth, raster ), identity, vertices, ARRAYSIZE( vertices ), indices,
                             ARRAYSIZE( indices ) );
}

//=========================================================================

xbool render::debug::Sphere( vector3 const& position, f32 radius, xcolor color, primitive_depth_mode depth )
{
    f32 constexpr a = 0.8506f;
    f32 constexpr b = 0.5257f;
    vector3 const UnitPositions[12] = {
        vector3( +a, 0, +b ), vector3( +a, 0, -b ), vector3( -a, 0, +b ), vector3( -a, 0, -b ),
        vector3( +b, +a, 0 ), vector3( -b, +a, 0 ), vector3( +b, -a, 0 ), vector3( -b, -a, 0 ),
        vector3( 0, +b, +a ), vector3( 0, -b, +a ), vector3( 0, +b, -a ), vector3( 0, -b, -a ) };
    u16 const pairs[60] = { 0, 1, 1, 4, 0, 4, 0, 6, 1, 6, 2, 3, 2, 5, 3, 5, 3, 7, 2, 7,
                            4, 5, 5, 8, 4, 8, 4, 10, 5, 10, 6, 7, 6, 9, 7, 9, 7, 11, 6, 11,
                            0, 8, 0, 9, 8, 9, 2, 8, 2, 9, 1, 10, 10, 11, 1, 11, 3, 10, 3, 11 };

    PrimitiveBatch batch( LineDesc( depth ) );
    batch.Reserve( ARRAYSIZE( pairs ), ARRAYSIZE( pairs ) );
    for ( s32 i = 0; i < ARRAYSIZE( pairs ); i += 2 )
    {
        vector3 const position0 = position + UnitPositions[pairs[i]] * radius;
        vector3 const position1 = position + UnitPositions[pairs[i + 1]] * radius;
        if ( !batch.AddLine( position0, position1, color, color ) )
        {
            return FALSE;
        }
    }

    matrix4 const identity = IdentityMatrix();
    return batch.Submit( identity );
}

//=========================================================================

xbool render::debug::Arc( vector3 const& center, f32 radius, radian direction, radian FieldOfView,
                          xcolor color, f32 SegmentDensity, primitive_depth_mode depth )
{
    if ( ( radius <= 0.0f ) || ( FieldOfView <= 0.0f ) || ( SegmentDensity <= 0.0f ) )
    {
        return FALSE;
    }

    s32 const SegmentCount = MAX( 1, 1 + static_cast<s32>( radius * FieldOfView * SegmentDensity ) );
    radian const StartAngle = direction - FieldOfView * 0.5f;
    radian const AngleStep = FieldOfView / SegmentCount;

    PrimitiveBatch batch( LineDesc( depth ) );
    batch.Reserve( ( SegmentCount + 2 ) * 2, ( SegmentCount + 2 ) * 2 );

    vector3 previous = center;
    for ( s32 i = 0; i <= SegmentCount; ++i )
    {
        f32 sine;
        f32 cosine;
        x_sincos( StartAngle + AngleStep * i, sine, cosine );
        vector3 const position( center.GetX() + radius * sine, center.GetY(), center.GetZ() + radius * cosine );
        if ( !batch.AddLine( previous, position, color, color ) )
        {
            return FALSE;
        }
        previous = position;
    }

    return batch.AddLine( previous, center, color, color ) && batch.Submit( IdentityMatrix() );
}

//=========================================================================

xbool render::debug::Circle( vector3 const& center, f32 radius, xcolor color, vector3 const& up,
                             f32 SegmentAngle, primitive_depth_mode depth )
{
    if ( ( radius <= 0.0f ) || ( SegmentAngle <= 0.0f ) || ( up.LengthSquared() <= 0.000001f ) )
    {
        return FALSE;
    }

    s32 const SegmentCount = MAX( 6, static_cast<s32>( R_360 / SegmentAngle ) );
    vector3 axis = up;
    axis.Normalize();
    vector3 perpendicular = ( x_abs( axis.Dot( vector3( 0.0f, 1.0f, 0.0f ) ) ) > 0.001f )
        ? axis.Cross( vector3( 1.0f, 0.0f, 0.0f ) )
        : axis.Cross( vector3( 0.0f, 1.0f, 0.0f ) );
    perpendicular.NormalizeAndScale( radius );

    PrimitiveBatch batch( LineDesc( depth ) );
    batch.Reserve( SegmentCount * 2, SegmentCount * 2 );

    vector3 previous = center + perpendicular;
    for ( s32 i = 1; i <= SegmentCount; ++i )
    {
        quaternion const rotation( axis, R_360 * i / SegmentCount );
        vector3 const position = center + rotation * perpendicular;
        if ( !batch.AddLine( previous, position, color, color ) )
        {
            return FALSE;
        }
        previous = position;
    }

    return batch.Submit( IdentityMatrix() );
}

//=========================================================================

xbool render::debug::Cylinder( vector3 const& center, f32 radius, f32 height, s32 segments, xcolor color,
                               xbool capped, vector3 const& up, primitive_depth_mode depth )
{
    if ( ( radius <= 0.0f ) || ( height <= 0.0f ) || ( up.LengthSquared() <= 0.000001f ) )
    {
        return FALSE;
    }

    segments = MAX( 5, segments );
    vector3 axis = up;
    axis.Normalize();
    vector3 const vertical = axis * ( height * 0.5f );
    vector3 perpendicular = ( x_abs( axis.Dot( vector3( 0.0f, 1.0f, 0.0f ) ) ) > 0.001f )
        ? axis.Cross( vector3( 1.0f, 0.0f, 0.0f ) )
        : axis.Cross( vector3( 0.0f, 1.0f, 0.0f ) );
    perpendicular.NormalizeAndScale( radius );

    PrimitiveBatch batch( TriangleDesc( depth, PRIMITIVE_RASTER_SOLID_NO_CULL ) );
    batch.Reserve( segments * ( capped ? 12 : 6 ), segments * ( capped ? 12 : 6 ) );
    primitive_vertex const TopCenter( center + vertical, vector2( 0.5f, 0.5f ), color );
    primitive_vertex const BottomCenter( center - vertical, vector2( 0.5f, 0.5f ), color );

    for ( s32 i = 0; i < segments; ++i )
    {
        quaternion const rotation0( axis, R_360 * i / segments );
        quaternion const rotation1( axis, R_360 * ( i + 1 ) / segments );
        vector3 const edge0 = rotation0 * perpendicular;
        vector3 const edge1 = rotation1 * perpendicular;
        primitive_vertex const top0( center + vertical + edge0, vector2( 0.0f, 0.0f ), color );
        primitive_vertex const bottom0( center - vertical + edge0, vector2( 0.0f, 1.0f ), color );
        primitive_vertex const top1( center + vertical + edge1, vector2( 1.0f, 0.0f ), color );
        primitive_vertex const bottom1( center - vertical + edge1, vector2( 1.0f, 1.0f ), color );

        if ( !batch.AddTriangle( top0, bottom0, bottom1 ) || !batch.AddTriangle( top0, bottom1, top1 ) )
        {
            return FALSE;
        }

        if ( capped &&
             ( !batch.AddTriangle( TopCenter, top1, top0 ) ||
               !batch.AddTriangle( BottomCenter, bottom0, bottom1 ) ) )
        {
            return FALSE;
        }
    }

    return batch.Submit( IdentityMatrix() );
}

//=========================================================================

xbool render::debug::Volume( vector3 const& position0, vector3 const& position1, f32 width, f32 height, xcolor color,
                             primitive_depth_mode depth )
{
    return SubmitVolumeGeometry( position0, position1, width, height, color, depth, IdentityMatrix() );
}

//=========================================================================

xbool render::debug::Volume( bbox const& bounds, xcolor color, primitive_depth_mode depth )
{
    vector3 position0 = bounds.GetCenter();
    vector3 position1 = position0;
    position0.GetZ() = bounds.Min.GetZ();
    position1.GetZ() = bounds.Max.GetZ();
    vector3 const size = bounds.GetSize();
    return Volume( position0, position1, size.GetX() * 0.5f, size.GetY() * 0.5f, color, depth );
}

//=========================================================================

xbool render::debug::Volume( bbox const& bounds, matrix4 const& LocalToWorld, xcolor color,
                             primitive_depth_mode depth )
{
    vector3 position0 = bounds.GetCenter();
    vector3 position1 = position0;
    position0.GetZ() = bounds.Min.GetZ();
    position1.GetZ() = bounds.Max.GetZ();
    vector3 const size = bounds.GetSize();
    return SubmitVolumeGeometry( position0, position1, size.GetX() * 0.5f, size.GetY() * 0.5f, color, depth,
                                 LocalToWorld );
}

//=========================================================================

xbool render::debug::Ngon( vector3 const* pPoints, s32 nPoints, xcolor color, xbool wire,
                           primitive_depth_mode depth )
{
    if ( !pPoints || ( nPoints < 3 ) )
    {
        return FALSE;
    }

    if ( wire )
    {
        PrimitiveBatch batch( LineDesc( depth ) );
        batch.Reserve( nPoints * 2, nPoints * 2 );
        for ( s32 i = 0; i < nPoints; ++i )
        {
            if ( !batch.AddLine( pPoints[( i + nPoints - 1 ) % nPoints], pPoints[i], color, color ) )
            {
                return FALSE;
            }
        }

        matrix4 const identity = IdentityMatrix();
        return batch.Submit( identity );
    }

    PrimitiveBatch batch( TriangleDesc( depth, PRIMITIVE_RASTER_SOLID_NO_CULL ) );
    batch.Reserve( ( nPoints - 2 ) * 3, ( nPoints - 2 ) * 3 );
    primitive_vertex const first( pPoints[0], vector2( 0.0f, 0.0f ), color );
    for ( s32 i = 1; i < nPoints - 1; ++i )
    {
        if ( !batch.AddTriangle( first, primitive_vertex( pPoints[i], vector2( 0.0f, 0.0f ), color ),
                                 primitive_vertex( pPoints[i + 1], vector2( 0.0f, 0.0f ), color ) ) )
        {
            return FALSE;
        }
    }

    matrix4 const identity = IdentityMatrix();
    return batch.Submit( identity );
}

//=========================================================================

xbool render::debug::Arrow( vector3 const& start, vector3 const& end, xcolor color, primitive_depth_mode depth )
{
    vector3 direction = end - start;
    if ( !direction.SafeNormalize() )
    {
        return FALSE;
    }

    vector3 side = direction.Cross( vector3( 0.0f, 1.0f, 0.0f ) );
    if ( !side.SafeNormalize() )
    {
        side = direction.Cross( vector3( 1.0f, 0.0f, 0.0f ) );
        if ( !side.SafeNormalize() )
        {
            return FALSE;
        }
    }

    f32 const length = ( end - start ).Length();
    vector3 const back = -direction * length * 0.2f;
    vector3 const wing0 = end + back + side * length * 0.1f;
    vector3 const wing1 = end + back - side * length * 0.1f;

    PrimitiveBatch batch( LineDesc( depth ) );
    batch.Reserve( 6, 6 );
    matrix4 const identity = IdentityMatrix();
    return batch.AddLine( start, end, color, color ) && batch.AddLine( end, wing0, color, color ) &&
           batch.AddLine( end, wing1, color, color ) && batch.Submit( identity );
}

//=========================================================================

xbool render::debug::Axis( matrix4 const& LocalToWorld, f32 size, primitive_depth_mode depth )
{
    primitive_vertex const vertices[6] = {
        primitive_vertex( vector3( 0.0f, 0.0f, 0.0f ), vector2( 0.0f, 0.0f ), XCOLOR_WHITE ),
        primitive_vertex( vector3( size, 0.0f, 0.0f ), vector2( 0.0f, 0.0f ), XCOLOR_RED ),
        primitive_vertex( vector3( 0.0f, 0.0f, 0.0f ), vector2( 0.0f, 0.0f ), XCOLOR_WHITE ),
        primitive_vertex( vector3( 0.0f, size, 0.0f ), vector2( 0.0f, 0.0f ), XCOLOR_GREEN ),
        primitive_vertex( vector3( 0.0f, 0.0f, 0.0f ), vector2( 0.0f, 0.0f ), XCOLOR_WHITE ),
        primitive_vertex( vector3( 0.0f, 0.0f, size ), vector2( 0.0f, 0.0f ), XCOLOR_BLUE ) };
    u16 const indices[6] = { 0, 1, 2, 3, 4, 5 };
    return SubmitPrimitives( LineDesc( depth ), LocalToWorld, vertices, ARRAYSIZE( vertices ), indices,
                             ARRAYSIZE( indices ) );
}

//=========================================================================

xbool render::debug::Axis( f32 size, primitive_depth_mode depth )
{
    matrix4 const identity = IdentityMatrix();
    return Axis( identity, size, depth );
}

//=========================================================================

xbool render::debug::Grid( vector3 const& corner, vector3 const& edge0, vector3 const& edge1, xcolor color,
                           s32 subdivisions, primitive_depth_mode depth )
{
    return Grid( corner, edge0, edge1, color, subdivisions, subdivisions, depth );
}

//=========================================================================

xbool render::debug::Grid( vector3 const& corner, vector3 const& edge0, vector3 const& edge1, xcolor color,
                           s32 Edge0Subdivisions, s32 Edge1Subdivisions, primitive_depth_mode depth )
{
    if ( (Edge0Subdivisions < 1) || (Edge1Subdivisions < 1) )
    {
        return FALSE;
    }

    PrimitiveBatch batch( LineDesc( depth ) );
    batch.Reserve( (Edge0Subdivisions + Edge1Subdivisions + 2) * 2,
                   (Edge0Subdivisions + Edge1Subdivisions + 2) * 2 );
    for ( s32 i = 0; i <= Edge0Subdivisions; ++i )
    {
        f32 const t = static_cast<f32>( i ) / static_cast<f32>( Edge0Subdivisions );
        if ( !batch.AddLine( corner + edge0 * t, corner + edge0 * t + edge1, color, color ) )
            return FALSE;
    }
    for ( s32 i = 0; i <= Edge1Subdivisions; ++i )
    {
        f32 const t = static_cast<f32>( i ) / static_cast<f32>( Edge1Subdivisions );
        if ( !batch.AddLine( corner + edge1 * t, corner + edge1 * t + edge0, color, color ) )
            return FALSE;
    }

    matrix4 const identity = IdentityMatrix();
    return batch.Submit( identity );
}

//=========================================================================

xbool render::debug::Frustum( view const& FrustumView, xcolor color, f32 distance, primitive_depth_mode depth )
{
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
    FrustumView.GetViewport( x0, y0, x1, y1 );

    vector3 positions[6];
    positions[0] = FrustumView.GetPosition();
    positions[1] = FrustumView.RayFromScreen( static_cast<f32>( x0 ), static_cast<f32>( y0 ), view::VIEW );
    positions[2] = FrustumView.RayFromScreen( static_cast<f32>( x0 ), static_cast<f32>( y1 ), view::VIEW );
    positions[3] = FrustumView.RayFromScreen( static_cast<f32>( x1 ), static_cast<f32>( y1 ), view::VIEW );
    positions[4] = FrustumView.RayFromScreen( static_cast<f32>( x1 ), static_cast<f32>( y0 ), view::VIEW );
    for ( s32 i = 1; i <= 4; ++i )
    {
        positions[i] *= distance / positions[i].GetZ();
        positions[i] = FrustumView.ConvertV2W( positions[i] );
    }
    positions[5] = ( positions[1] + positions[2] + positions[3] + positions[4] ) * 0.25f;

    PrimitiveBatch batch( LineDesc( depth ) );
    batch.Reserve( 18, 18 );
    matrix4 const identity = IdentityMatrix();
    return batch.AddLine( positions[0], positions[1], color, color ) &&
           batch.AddLine( positions[0], positions[2], color, color ) &&
           batch.AddLine( positions[0], positions[3], color, color ) &&
           batch.AddLine( positions[0], positions[4], color, color ) &&
           batch.AddLine( positions[1], positions[2], color, color ) &&
           batch.AddLine( positions[2], positions[3], color, color ) &&
           batch.AddLine( positions[3], positions[4], color, color ) &&
           batch.AddLine( positions[4], positions[1], color, color ) &&
           batch.AddLine( positions[0], positions[5], XCOLOR_GREY, XCOLOR_GREY ) && batch.Submit( identity );
}

//=========================================================================

xbool render::debug::Marker( vector3 const& position, xcolor color )
{
    vector3 ScreenPosition;
    irect ScreenBounds;
    if ( !g_UIRenderer.IsInitialized() || !GetProjectedPoint( position, ScreenPosition, ScreenBounds ) )
    {
        return FALSE;
    }

    ScreenPosition.GetX() = MAX( static_cast<f32>( ScreenBounds.l ),
                                 MIN( static_cast<f32>( ScreenBounds.r ), ScreenPosition.GetX() ) );
    ScreenPosition.GetY() = MAX( static_cast<f32>( ScreenBounds.t ),
                                 MIN( static_cast<f32>( ScreenBounds.b ), ScreenPosition.GetY() ) );

    vector2 const center( ScreenPosition.GetX(), ScreenPosition.GetY() );
    ui_vertex const vertices[4] = {
        ui_vertex( center + vector2( 0.0f, -8.0f ), vector2( 0.5f, 0.0f ), color ),
        ui_vertex( center + vector2( -8.0f, 0.0f ), vector2( 0.0f, 0.5f ), color ),
        ui_vertex( center + vector2( 0.0f, 8.0f ), vector2( 0.5f, 1.0f ), color ),
        ui_vertex( center + vector2( 8.0f, 0.0f ), vector2( 1.0f, 0.5f ), color ) };
    u32 const indices[6] = { 0, 1, 2, 2, 3, 0 };

    g_UIRenderer.PushScreenSpace( ScreenBounds );
    xbool result = g_UIRenderer.GetDrawList().AddTriangles( ui_material(), vertices, ARRAYSIZE( vertices ), indices,
                                                            ARRAYSIZE( indices ) );
    if ( result && ( ScreenPosition.GetZ() < 0.0f ) )
    {
        result = g_UIRenderer.DrawPoint( center, XCOLOR_BLACK, 8.0f );
    }
    g_UIRenderer.PopScreenSpace();
    return result;
}

//=========================================================================

xbool render::debug::Point( vector3 const& position, xcolor color, s32 size )
{
    vector3 ScreenPosition;
    irect ScreenBounds;
    if ( !g_UIRenderer.IsInitialized() || !GetProjectedPoint( position, ScreenPosition, ScreenBounds ) ||
         ( ScreenPosition.GetZ() < 0.0f ) || ( size <= 0 ) )
    {
        return FALSE;
    }

    g_UIRenderer.PushScreenSpace( ScreenBounds );
    xbool const result = g_UIRenderer.DrawPoint( vector2( ScreenPosition.GetX(), ScreenPosition.GetY() ), color,
                                                 static_cast<f32>( size * 2 ) );
    g_UIRenderer.PopScreenSpace();
    return result;
}

//=========================================================================

xbool render::debug::Label( vector3 const& position, xcolor color, char const* pFormat, ... )
{
    if ( !pFormat || !g_UiMgr || !g_UIRenderer.IsInitialized() )
    {
        return FALSE;
    }

    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    vector3 const ScreenPosition = pView->PointToScreen( position );
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
    pView->GetViewport( x0, y0, x1, y1 );
    if ( ( ScreenPosition.GetZ() < 0.0f ) || ( ScreenPosition.GetX() < x0 ) || ( ScreenPosition.GetX() > x1 ) ||
         ( ScreenPosition.GetY() < y0 ) || ( ScreenPosition.GetY() > y1 ) )
    {
        return FALSE;
    }

    x_va_list arguments;
    x_va_start( arguments, pFormat );
    xstring message;
    message.FormatV( pFormat, arguments );
    x_va_end( arguments );

    const s32 Font = g_UiMgr->FindFont( "small" );
    if( Font < 0 )
        return FALSE;

    irect TextBounds;
    const xwstring Text( (const char*)message );
    g_UiMgr->TextSize( Font, TextBounds, Text, Text.GetLength() );
    TextBounds.Translate( static_cast<s32>( ScreenPosition.GetX() ) - TextBounds.GetWidth() / 2,
                          static_cast<s32>( ScreenPosition.GetY() ) );
    s32 Width, Height;
    eng_GetRes( Width, Height );
    g_UIRenderer.PushScreenSpace( irect( 0, 0, Width, Height ) );
    g_UiMgr->RenderText( Font, TextBounds, ui_font::h_left | ui_font::v_top, color, Text, TRUE, FALSE );
    g_UIRenderer.PopScreenSpace();
    return TRUE;
}
