#include "a51_shader_catalog.hpp"
//==============================================================================
//
//  ui_renderer.cpp
//
//==============================================================================

#include "ui_renderer.hpp"

#include "Render/Texture.hpp"
#include "Render/Pipeline/RenderPipeline.hpp"

#include "e_Engine.hpp"
#include "e_RenderTarget.hpp"
#include "x_debug.hpp"
#include "x_profile.hpp"
#include "Font/FontMgr.hpp"

#include <stddef.h>

static_assert( sizeof(ui_vertex)             == 20, "UI vertex layout must match HLSL" );
static_assert( offsetof(ui_vertex, Position) == 0,  "UI position offset must match PSO" );
static_assert( offsetof(ui_vertex, Color)    == 8,  "UI color offset must match PSO" );
static_assert( offsetof(ui_vertex, UV)       == 12, "UI UV offset must match PSO" );

//==============================================================================
//  GLOBALS
//==============================================================================

ui_renderer g_UIRenderer;

//==============================================================================
//  HELPERS
//==============================================================================

static
u32 ui_PackColor( const xcolor& Color )
{
    return ((u32)Color.A << 24) |
           ((u32)Color.R << 16) |
           ((u32)Color.G <<  8) |
           ((u32)Color.B      );
}

//==============================================================================

static
s32 ui_NextCapacity( s32 Current, s32 Required )
{
    s32 Capacity = (Current > 0) ? Current : 4096;
    while( Capacity < Required )
    {
        if( Capacity > (0x7fffffff / 2) )
            return Required;
        Capacity *= 2;
    }
    return Capacity;
}

//==============================================================================

static
rstate_blend_preset ui_GetBlendPreset( ui_blend_mode Blend )
{
    switch( Blend )
    {
    case UI_BLEND_OPAQUE:       return RSTATE_BLEND_PRESET_NONE;
    case UI_BLEND_ALPHA:        return RSTATE_BLEND_PRESET_ALPHA;
    case UI_BLEND_ADDITIVE:     return RSTATE_BLEND_PRESET_ADD;
    case UI_BLEND_SUBTRACTIVE:  return RSTATE_BLEND_PRESET_SUB;
    case UI_BLEND_MULTIPLY:     return RSTATE_BLEND_PRESET_MULTIPLY;
    case UI_BLEND_INTENSITY:    return RSTATE_BLEND_PRESET_INTENSITY;
    default:                    return RSTATE_BLEND_PRESET_ALPHA;
    }
}

//==============================================================================

static
rstate_sampler_preset ui_GetSamplerPreset( ui_sampler_mode Sampler )
{
    switch( Sampler )
    {
    case UI_SAMPLER_LINEAR_CLAMP:       return RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    case UI_SAMPLER_LINEAR_CLAMP_ATLAS: return RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    case UI_SAMPLER_LINEAR_WRAP:        return RSTATE_SAMPLER_PRESET_LINEAR_WRAP;
    case UI_SAMPLER_POINT_CLAMP:        return RSTATE_SAMPLER_PRESET_POINT_CLAMP;
    case UI_SAMPLER_POINT_WRAP:         return RSTATE_SAMPLER_PRESET_POINT_WRAP;
    default:                            return RSTATE_SAMPLER_PRESET_LINEAR_CLAMP;
    }
}

//==============================================================================
//  VIEWPORT
//==============================================================================

ui_viewport::ui_viewport( void ) :
    m_OutputWidth ( CANVAS_WIDTH ),
    m_OutputHeight( CANVAS_HEIGHT ),
    m_Scale       ( 1.0f ),
    m_UserScale   ( 1.0f ),
    m_HudUserScale( 1.0f ),
    m_LogicalBounds( (f32)(CONTENT_WIDTH  - CANVAS_WIDTH ) * 0.5f,
                     (f32)(CONTENT_HEIGHT - CANVAS_HEIGHT) * 0.5f,
                     (f32)(CONTENT_WIDTH  + CANVAS_WIDTH ) * 0.5f,
                     (f32)(CONTENT_HEIGHT + CANVAS_HEIGHT) * 0.5f )
{
}

//==============================================================================

void ui_viewport::SetOutputSize( s32 Width, s32 Height )
{
    m_OutputWidth  = MAX( Width,  0 );
    m_OutputHeight = MAX( Height, 0 );
    Recalculate();
}

//==============================================================================

void ui_viewport::SetUserScale( f32 Scale )
{
    if( !x_isvalid( Scale ) )
        Scale = 1.0f;

    m_UserScale = x_clamp( Scale, 0.6f, 1.4f );
    Recalculate();
}

//==============================================================================

void ui_viewport::SetHudUserScale( f32 Scale )
{
    if( !x_isvalid( Scale ) )
        Scale = 1.0f;

    m_HudUserScale = x_clamp( Scale, 0.6f, 1.4f );
}

//==============================================================================

void ui_viewport::Recalculate( void )
{
    f32 const ContentCenterX = (f32)CONTENT_WIDTH  * 0.5f;
    f32 const ContentCenterY = (f32)CONTENT_HEIGHT * 0.5f;

    if( (m_OutputWidth == 0) || (m_OutputHeight == 0) )
    {
        m_Scale = 1.0f;
        m_LogicalBounds.Set( ContentCenterX - (f32)CANVAS_WIDTH  * 0.5f,
                             ContentCenterY - (f32)CANVAS_HEIGHT * 0.5f,
                             ContentCenterX + (f32)CANVAS_WIDTH  * 0.5f,
                             ContentCenterY + (f32)CANVAS_HEIGHT * 0.5f );
        return;
    }

    f32 const ScaleX = (f32)m_OutputWidth  / (f32)CANVAS_WIDTH;
    f32 const ScaleY = (f32)m_OutputHeight / (f32)CANVAS_HEIGHT;
    m_Scale = MIN( ScaleX, ScaleY ) * m_UserScale;

    f32 const LogicalWidth  = (f32)m_OutputWidth  / m_Scale;
    f32 const LogicalHeight = (f32)m_OutputHeight / m_Scale;
    f32 const LogicalLeft   = ContentCenterX - LogicalWidth  * 0.5f;
    f32 const LogicalTop    = ContentCenterY - LogicalHeight * 0.5f;
    m_LogicalBounds.Set( LogicalLeft,
                         LogicalTop,
                         LogicalLeft + LogicalWidth,
                         LogicalTop  + LogicalHeight );
}

//==============================================================================

s32 ui_viewport::GetOutputWidth( void ) const
{
    return m_OutputWidth;
}

//==============================================================================

s32 ui_viewport::GetOutputHeight( void ) const
{
    return m_OutputHeight;
}

//==============================================================================

f32 ui_viewport::GetScale( void ) const
{
    return m_Scale;
}

//==============================================================================

f32 ui_viewport::GetUserScale( void ) const
{
    return m_UserScale;
}

//==============================================================================

f32 ui_viewport::GetHudUserScale( void ) const
{
    return m_HudUserScale;
}

//==============================================================================

f32 ui_viewport::GetHudScale( const irect& ScreenViewport ) const
{
    if( ScreenViewport.GetHeight() <= 0 )
        return 1.0f;

    return ((f32)ScreenViewport.GetHeight() / (f32)CONTENT_HEIGHT) * m_HudUserScale;
}

//==============================================================================

rect const& ui_viewport::GetLogicalBounds( void ) const
{
    return m_LogicalBounds;
}

//==============================================================================

irect ui_viewport::GetLogicalClipBounds( void ) const
{
    return irect( (s32)x_floor( m_LogicalBounds.Min.X ),
                  (s32)x_floor( m_LogicalBounds.Min.Y ),
                  (s32)x_ceil ( m_LogicalBounds.Max.X ),
                  (s32)x_ceil ( m_LogicalBounds.Max.Y ) );
}

//==============================================================================

vector2 ui_viewport::ScreenDeltaToLogical( vector2 const& Delta ) const
{
    return Delta / m_Scale;
}

//==============================================================================

irect ui_viewport::LogicalToScreen( irect const& Rect ) const
{
    irect ScreenRect;
    ScreenRect.l = (s32)x_floor( ((f32)Rect.l - m_LogicalBounds.Min.X) * m_Scale );
    ScreenRect.t = (s32)x_floor( ((f32)Rect.t - m_LogicalBounds.Min.Y) * m_Scale );
    ScreenRect.r = (s32)x_ceil ( ((f32)Rect.r - m_LogicalBounds.Min.X) * m_Scale );
    ScreenRect.b = (s32)x_ceil ( ((f32)Rect.b - m_LogicalBounds.Min.Y) * m_Scale );

    ScreenRect.l = MAX( ScreenRect.l, 0 );
    ScreenRect.t = MAX( ScreenRect.t, 0 );
    ScreenRect.r = MIN( ScreenRect.r, m_OutputWidth  );
    ScreenRect.b = MIN( ScreenRect.b, m_OutputHeight );
    return ScreenRect;
}

//==============================================================================

rect ui_viewport::GetHudBounds( const irect& ScreenViewport ) const
{
    if( (ScreenViewport.GetWidth() <= 0) || (ScreenViewport.GetHeight() <= 0) )
        return rect( 0.0f, 0.0f, (f32)CONTENT_WIDTH, (f32)CONTENT_HEIGHT );

    const f32 Scale  = GetHudScale( ScreenViewport );
    const f32 Width  = (f32)ScreenViewport.GetWidth()  / Scale;
    const f32 Height = (f32)ScreenViewport.GetHeight() / Scale;
    const f32 Left   = ((f32)CONTENT_WIDTH  - Width ) * 0.5f;
    const f32 Top    = ((f32)CONTENT_HEIGHT - Height) * 0.5f;
    return rect( Left, Top, Left + Width, Top + Height );
}

//==============================================================================

vector2 ui_viewport::ScreenToHud( const vector2& Position,
                                  const irect& ScreenViewport ) const
{
    const rect HudBounds = GetHudBounds( ScreenViewport );
    if( ScreenViewport.GetHeight() <= 0 )
        return Position;

    const f32 Scale = GetHudScale( ScreenViewport );
    return vector2( HudBounds.Min.X + (Position.X - (f32)ScreenViewport.l) / Scale,
                    HudBounds.Min.Y + (Position.Y - (f32)ScreenViewport.t) / Scale );
}

//==============================================================================

vector2 ui_viewport::HudToScreen( const vector2& Position,
                                  const irect& ScreenViewport ) const
{
    const rect HudBounds = GetHudBounds( ScreenViewport );
    if( ScreenViewport.GetHeight() <= 0 )
        return Position;

    const f32 Scale = GetHudScale( ScreenViewport );
    return vector2( (f32)ScreenViewport.l + (Position.X - HudBounds.Min.X) * Scale,
                    (f32)ScreenViewport.t + (Position.Y - HudBounds.Min.Y) * Scale );
}

//==============================================================================

irect ui_viewport::HudToScreen( const irect& Rect,
                                const irect& ScreenViewport ) const
{
    if( ScreenViewport.GetHeight() <= 0 )
        return Rect;

    const vector2 TopLeft = HudToScreen( vector2( (f32)Rect.l, (f32)Rect.t ),
                                         ScreenViewport );
    const vector2 BottomRight = HudToScreen( vector2( (f32)Rect.r, (f32)Rect.b ),
                                             ScreenViewport );
    return irect( (s32)x_floor( TopLeft.X ),
                  (s32)x_floor( TopLeft.Y ),
                  (s32)x_ceil ( BottomRight.X ),
                  (s32)x_ceil ( BottomRight.Y ) );
}

//==============================================================================
//  UI VERTEX / MATERIAL
//==============================================================================

ui_vertex::ui_vertex( void ) :
    Position( 0.0f, 0.0f ),
    Color   ( 0xffffffffu ),
    UV      ( 0.0f, 0.0f )
{
}

//==============================================================================

ui_vertex::ui_vertex( const vector2& InPosition,
                      const vector2& InUV,
                      const xcolor&  InColor ) :
    Position( InPosition ),
    Color   ( ui_PackColor( InColor ) ),
    UV      ( InUV )
{
}

//==============================================================================

ui_material::ui_material( void ) :
    Texture (),
    Blend   ( UI_BLEND_ALPHA ),
    Sampler ( UI_SAMPLER_LINEAR_CLAMP ),
    Coverage( FALSE )
{
}

//==============================================================================

ui_material::ui_material( const texture& InTexture,
                          ui_blend_mode BlendMode,
                          ui_sampler_mode SamplerMode ) :
    Texture (),
    Blend   ( BlendMode ),
    Sampler ( SamplerMode ),
    Coverage( FALSE )
{
    const shader_resource* pResource = InTexture.GetShaderResource();
    if( pResource )
        Texture = *pResource;
}

//==============================================================================

ui_material::ui_material( const shader_resource& InTexture,
                          ui_blend_mode BlendMode,
                          ui_sampler_mode SamplerMode ) :
    Texture ( InTexture ),
    Blend   ( BlendMode ),
    Sampler ( SamplerMode ),
    Coverage( FALSE )
{
}

//==============================================================================
//  DRAW LIST
//==============================================================================

ui_draw_list::command::command( void ) :
    Material       (),
    ClipRect       (),
    CoordinateSpace( UI_COORDINATE_SPACE_LOGICAL ),
    CoordinateViewport(),
    StartIndex     ( 0 ),
    IndexCount     ( 0 )
{
}

//==============================================================================

ui_draw_list::coordinate_state::coordinate_state( void ) :
    Space    ( UI_COORDINATE_SPACE_LOGICAL ),
    Viewport (),
    ClipDepth( 0 )
{
}

//==============================================================================

ui_draw_list::ui_draw_list( void ) :
    m_vertices(),
    m_indices (),
    m_Commands(),
    m_ClipStack(),
    m_CoordinateStack()
{
}

//==============================================================================

void ui_draw_list::BeginFrame( irect const& Bounds )
{
    ASSERTS( AreStacksBalanced(),
             "UI coordinate or clip stack leaked across frames" );

    m_vertices.SetCount( 0 );
    m_indices.SetCount ( 0 );
    m_Commands.SetCount( 0 );
    m_ClipStack.SetCount( 0 );
    m_CoordinateStack.SetCount( 0 );

    if( (Bounds.GetWidth() > 0) && (Bounds.GetHeight() > 0) )
    {
        m_ClipStack.Append() = Bounds;
        coordinate_state& State = m_CoordinateStack.Append();
        State.Space = UI_COORDINATE_SPACE_LOGICAL;
        State.Viewport.Clear();
        State.ClipDepth = m_ClipStack.GetCount();
    }
}

//==============================================================================

void ui_draw_list::Clear( void )
{
    m_vertices.Clear();
    m_indices.Clear();
    m_Commands.Clear();
    m_ClipStack.Clear();
    m_CoordinateStack.Clear();
}

//==============================================================================

//==============================================================================

void ui_draw_list::PushClipRect( const irect& Rect )
{
    ASSERTS( (m_CoordinateStack.GetCount() > 0) && (m_ClipStack.GetCount() > 0),
             "Cannot push a UI clip rect without an active coordinate space" );
    if( (m_CoordinateStack.GetCount() == 0) || (m_ClipStack.GetCount() == 0) )
        return;

    irect Clipped = Rect;
    if( m_ClipStack.GetCount() > 0 )
    {
        const irect& Parent = m_ClipStack[m_ClipStack.GetCount() - 1];
        Clipped.l = iMax( Clipped.l, Parent.l );
        Clipped.t = iMax( Clipped.t, Parent.t );
        Clipped.r = iMin( Clipped.r, Parent.r );
        Clipped.b = iMin( Clipped.b, Parent.b );
    }

    if( Clipped.r < Clipped.l ) Clipped.r = Clipped.l;
    if( Clipped.b < Clipped.t ) Clipped.b = Clipped.t;
    m_ClipStack.Append() = Clipped;
}

//==============================================================================

void ui_draw_list::PopClipRect( void )
{
    ASSERTS( (m_CoordinateStack.GetCount() > 0) && (m_ClipStack.GetCount() > 0),
             "Cannot pop a UI clip rect without an active coordinate space" );
    if( (m_CoordinateStack.GetCount() == 0) || (m_ClipStack.GetCount() == 0) )
        return;

    const s32 CoordinateClipDepth = GetCoordinateClipDepth();
    ASSERTS( m_ClipStack.GetCount() > CoordinateClipDepth,
             "Cannot pop the root clip rect for the current UI coordinate space" );
    if( m_ClipStack.GetCount() > CoordinateClipDepth )
        m_ClipStack.Delete( m_ClipStack.GetCount() - 1 );
}

//==============================================================================

const irect& ui_draw_list::GetClipRect( void ) const
{
    ASSERT( m_ClipStack.GetCount() > 0 );
    return m_ClipStack[m_ClipStack.GetCount() - 1];
}

//==============================================================================

void ui_draw_list::PushCoordinateSpace( ui_coordinate_space CoordinateSpace,
                                        const irect& Bounds,
                                        const irect& Viewport )
{
    const xbool IsValid =
        (CoordinateSpace >= 0) && (CoordinateSpace < UI_COORDINATE_SPACE_COUNT) &&
        (m_CoordinateStack.GetCount() > 0) &&
        (m_ClipStack.GetCount() > 0) &&
        (Bounds.GetWidth() > 0) && (Bounds.GetHeight() > 0);
    ASSERTS( IsValid, "Cannot push an invalid UI coordinate space" );
    if( !IsValid )
        return;

    m_ClipStack.Append() = Bounds;
    coordinate_state& State = m_CoordinateStack.Append();
    State.Space = CoordinateSpace;
    State.Viewport = Viewport;
    State.ClipDepth = m_ClipStack.GetCount();
}

//==============================================================================

void ui_draw_list::PopCoordinateSpace( void )
{
    ASSERTS( m_CoordinateStack.GetCount() > 1,
             "Cannot pop the default UI coordinate space" );
    if( m_CoordinateStack.GetCount() <= 1 )
        return;

    const s32 CoordinateClipDepth = GetCoordinateClipDepth();
    ASSERTS( m_ClipStack.GetCount() == CoordinateClipDepth,
             "UI coordinate space contains an unbalanced clip rect" );

    while( m_ClipStack.GetCount() >= CoordinateClipDepth )
        m_ClipStack.Delete( m_ClipStack.GetCount() - 1 );

    m_CoordinateStack.Delete( m_CoordinateStack.GetCount() - 1 );
}

//==============================================================================

ui_coordinate_space ui_draw_list::GetCoordinateSpace( void ) const
{
    ASSERT( m_CoordinateStack.GetCount() > 0 );
    return m_CoordinateStack[m_CoordinateStack.GetCount() - 1].Space;
}

//==============================================================================

const irect& ui_draw_list::GetCoordinateViewport( void ) const
{
    ASSERT( m_CoordinateStack.GetCount() > 0 );
    return m_CoordinateStack[m_CoordinateStack.GetCount() - 1].Viewport;
}

//==============================================================================

s32 ui_draw_list::GetCoordinateClipDepth( void ) const
{
    ASSERT( m_CoordinateStack.GetCount() > 0 );
    return m_CoordinateStack[m_CoordinateStack.GetCount() - 1].ClipDepth;
}

//==============================================================================

xbool ui_draw_list::AreStacksBalanced( void ) const
{
    if( m_CoordinateStack.GetCount() == 0 )
    {
        return m_ClipStack.GetCount() == 0;
    }

    return (m_CoordinateStack.GetCount() == 1) &&
           (m_ClipStack.GetCount() == 1) &&
           (m_CoordinateStack[0].ClipDepth == 1);
}

//==============================================================================

xbool ui_draw_list::IsSameCommand( const command& A, const command& B ) const
{
    return (A.Material.Texture.pBackend == B.Material.Texture.pBackend) &&
           (A.Material.Blend    == B.Material.Blend   ) &&
           (A.Material.Sampler  == B.Material.Sampler ) &&
           (A.Material.Coverage == B.Material.Coverage) &&
           (A.CoordinateSpace   == B.CoordinateSpace  ) &&
           (A.CoordinateViewport == B.CoordinateViewport) &&
           (A.ClipRect          == B.ClipRect         ) &&
           ((A.StartIndex + A.IndexCount) == B.StartIndex);
}

//==============================================================================

void ui_draw_list::AppendCommand( const ui_material& Material,
                                  u32 StartIndex,
                                  u32 IndexCount )
{
    command NewCommand;
    NewCommand.Material   = Material;
    NewCommand.ClipRect   = GetClipRect();
    NewCommand.CoordinateSpace = GetCoordinateSpace();
    NewCommand.CoordinateViewport = GetCoordinateViewport();
    NewCommand.StartIndex = StartIndex;
    NewCommand.IndexCount = IndexCount;

    if( m_Commands.GetCount() > 0 )
    {
        command& Last = m_Commands[m_Commands.GetCount() - 1];
        if( IsSameCommand( Last, NewCommand ) )
        {
            Last.IndexCount += IndexCount;
            return;
        }
    }

    m_Commands.Append() = NewCommand;
}

//==============================================================================

xbool ui_draw_list::AddTriangles( const ui_material& Material,
                                  const ui_vertex*   pVertices,
                                  s32                VertexCount,
                                  const u32*         pIndices,
                                  s32                IndexCount )
{
    if( render::GetActiveRenderScopeDesc().Purpose == render::RENDER_VIEW_SCENE_CAPTURE )
        return TRUE;

    if( !pVertices || !pIndices || (VertexCount <= 0) || (IndexCount <= 0) )
        return FALSE;

    if( (Material.Blend < 0) || (Material.Blend >= UI_BLEND_COUNT) ||
        (Material.Sampler < 0) || (Material.Sampler >= UI_SAMPLER_COUNT) ||
        (m_ClipStack.GetCount() == 0) ||
        (m_CoordinateStack.GetCount() == 0) )
    {
        return FALSE;
    }

    const irect& Clip = GetClipRect();
    if( (Clip.r <= Clip.l) || (Clip.b <= Clip.t) )
        return TRUE;

    for( s32 i = 0; i < IndexCount; i++ )
    {
        if( pIndices[i] >= (u32)VertexCount )
            return FALSE;
    }

    const u32 BaseVertex = (u32)m_vertices.GetCount();
    const u32 StartIndex = (u32)m_indices.GetCount();

    if( (BaseVertex > (0xffffffffu - (u32)VertexCount)) ||
        (StartIndex > (0xffffffffu - (u32)IndexCount)) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < VertexCount; i++ )
        m_vertices.Append() = pVertices[i];

    for( s32 i = 0; i < IndexCount; i++ )
        m_indices.Append() = BaseVertex + pIndices[i];

    AppendCommand( Material, StartIndex, (u32)IndexCount );
    return TRUE;
}

//==============================================================================

xbool ui_draw_list::AddImage( const ui_material& Material,
                              const vector2&     Position,
                              const vector2&     Size,
                              const vector2&     UV0,
                              const vector2&     UV1,
                              const xcolor&      Color,
                              radian             Rotation )
{
    if( (Size.X == 0.0f) || (Size.Y == 0.0f) )
        return TRUE;

    vector2 Positions[4] =
    {
        Position,
        vector2( Position.X + Size.X, Position.Y ),
        Position + Size,
        vector2( Position.X, Position.Y + Size.Y )
    };

    if( Rotation != 0.0f )
    {
        const vector2 Center = Position + (Size * 0.5f);
        f32 Sin;
        f32 Cos;
        // Match the rotation convention of the replaced draw_SpriteUV API.
        x_sincos( -Rotation, Sin, Cos );

        for( s32 i = 0; i < 4; i++ )
        {
            const vector2 Delta = Positions[i] - Center;
            Positions[i].X = Center.X + Delta.X * Cos - Delta.Y * Sin;
            Positions[i].Y = Center.Y + Delta.X * Sin + Delta.Y * Cos;
        }
    }

    const ui_vertex Vertices[4] =
    {
        ui_vertex( Positions[0], vector2( UV0.X, UV0.Y ), Color ),
        ui_vertex( Positions[1], vector2( UV1.X, UV0.Y ), Color ),
        ui_vertex( Positions[2], vector2( UV1.X, UV1.Y ), Color ),
        ui_vertex( Positions[3], vector2( UV0.X, UV1.Y ), Color )
    };

    static const u32 Indices[6] = { 0, 1, 2, 2, 3, 0 };
    return AddTriangles( Material, Vertices, 4, Indices, 6 );
}

//==============================================================================

xbool ui_draw_list::AddRect( const irect& Rect,
                             const xcolor& Color,
                             ui_blend_mode Blend )
{
    return AddGradientRect( Rect, Color, Color, Color, Color, Blend );
}

//==============================================================================

xbool ui_draw_list::AddGradientRect( const irect& Rect,
                                     const xcolor& TopLeft,
                                     const xcolor& TopRight,
                                     const xcolor& BottomRight,
                                     const xcolor& BottomLeft,
                                     ui_blend_mode Blend )
{
    const ui_vertex Vertices[4] =
    {
        ui_vertex( vector2( (f32)Rect.l, (f32)Rect.t ), vector2( 0.0f, 0.0f ), TopLeft     ),
        ui_vertex( vector2( (f32)Rect.r, (f32)Rect.t ), vector2( 1.0f, 0.0f ), TopRight    ),
        ui_vertex( vector2( (f32)Rect.r, (f32)Rect.b ), vector2( 1.0f, 1.0f ), BottomRight ),
        ui_vertex( vector2( (f32)Rect.l, (f32)Rect.b ), vector2( 0.0f, 1.0f ), BottomLeft  )
    };

    static const u32 Indices[6] = { 0, 1, 2, 2, 3, 0 };
    ui_material Material;
    Material.Blend = Blend;
    return AddTriangles( Material, Vertices, 4, Indices, 6 );
}

//==============================================================================

xbool ui_draw_list::AddRectOutline( const rect& Rect,
                                    f32 Width,
                                    const xcolor& TopLeft,
                                    const xcolor& TopRight,
                                    const xcolor& BottomRight,
                                    const xcolor& BottomLeft,
                                    ui_blend_mode Blend )
{
    if( (Rect.GetWidth() <= 0.0f) || (Rect.GetHeight() <= 0.0f) || (Width <= 0.0f) )
        return TRUE;

    const ui_vertex Vertices[8] =
    {
        ui_vertex( vector2( Rect.Min.X,         Rect.Min.Y         ), vector2( 0.0f, 0.0f ), TopLeft     ),
        ui_vertex( vector2( Rect.Max.X,         Rect.Min.Y         ), vector2( 1.0f, 0.0f ), TopRight    ),
        ui_vertex( vector2( Rect.Max.X,         Rect.Max.Y         ), vector2( 1.0f, 1.0f ), BottomRight ),
        ui_vertex( vector2( Rect.Min.X,         Rect.Max.Y         ), vector2( 0.0f, 1.0f ), BottomLeft  ),
        ui_vertex( vector2( Rect.Min.X + Width, Rect.Min.Y + Width ), vector2( 0.0f, 0.0f ), TopLeft     ),
        ui_vertex( vector2( Rect.Max.X - Width, Rect.Min.Y + Width ), vector2( 1.0f, 0.0f ), TopRight    ),
        ui_vertex( vector2( Rect.Max.X - Width, Rect.Max.Y - Width ), vector2( 1.0f, 1.0f ), BottomRight ),
        ui_vertex( vector2( Rect.Min.X + Width, Rect.Max.Y - Width ), vector2( 0.0f, 1.0f ), BottomLeft  )
    };

    static const u32 OutlineIndices[24] =
    {
        0, 1, 5, 5, 4, 0,
        1, 2, 6, 6, 5, 1,
        2, 3, 7, 7, 6, 2,
        3, 0, 4, 4, 7, 3
    };
    static const u32 FillIndices[6] = { 0, 1, 2, 2, 3, 0 };
    const xbool Filled = (Rect.GetWidth() <= Width * 2.0f) || (Rect.GetHeight() <= Width * 2.0f);
    ui_material Material;
    Material.Blend = Blend;
    return AddTriangles( Material, Vertices, Filled ? 4 : 8,
                         Filled ? FillIndices : OutlineIndices, Filled ? 6 : 24 );
}

//==============================================================================

xbool ui_draw_list::AddLine( const vector2& Start,
                             const vector2& End,
                             const xcolor&  Color,
                             f32            Width,
                             ui_blend_mode  Blend )
{
    const vector2 Direction = End - Start;
    const f32 Length = Direction.Length();
    if( (Length <= 0.0001f) || (Width <= 0.0f) )
        return TRUE;

    const f32 Scale = (Width * 0.5f) / Length;
    const vector2 Normal( -Direction.Y * Scale, Direction.X * Scale );

    const ui_vertex Vertices[4] =
    {
        ui_vertex( Start - Normal, vector2( 0.0f, 0.0f ), Color ),
        ui_vertex( End   - Normal, vector2( 1.0f, 0.0f ), Color ),
        ui_vertex( End   + Normal, vector2( 1.0f, 1.0f ), Color ),
        ui_vertex( Start + Normal, vector2( 0.0f, 1.0f ), Color )
    };

    static const u32 Indices[6] = { 0, 1, 2, 2, 3, 0 };
    ui_material Material;
    Material.Blend = Blend;
    return AddTriangles( Material, Vertices, 4, Indices, 6 );
}

//==============================================================================

xbool ui_draw_list::AddPoint( const vector2& Position,
                              const xcolor& Color,
                              f32 Size,
                              ui_blend_mode Blend )
{
    const f32 Half = Size * 0.5f;
    const irect Rect( (s32)(Position.X - Half),
                      (s32)(Position.Y - Half),
                      (s32)(Position.X + Half + 1.0f),
                      (s32)(Position.Y + Half + 1.0f) );
    return AddRect( Rect, Color, Blend );
}

//==============================================================================

s32 ui_draw_list::GetVertexCount( void ) const { return m_vertices.GetCount(); }
s32 ui_draw_list::GetIndexCount ( void ) const { return m_indices.GetCount();  }
s32 ui_draw_list::GetCommandCount( void ) const { return m_Commands.GetCount(); }

//==============================================================================
//  RENDERER
//==============================================================================

ui_renderer::ui_renderer( void ) :
    m_Viewport            (),
    m_DrawList            (),
    m_vertexBuffer        (),
    m_indexBuffer         (),
    m_VertexCapacity      ( 0 ),
    m_IndexCapacity       ( 0 ),
    m_PreparedVertices    ( 0 ),
    m_PreparedIndices     ( 0 ),
    m_vertexShader        (),
    m_pixelShader         (),
    m_DrawUniformSlot     ( 0xffffffffu ),
    m_PixelDrawUniformSlot( 0xffffffffu ),
    m_textureSlot         ( 0xffffffffu ),
    m_whiteTexture        (),
    m_pipelines           (),
    m_prewarmedFormat     ( RTARGET_FORMAT_COUNT ),
    m_bPrepared           ( FALSE ),
    m_isInitialized        ( FALSE )
{
}

//==============================================================================

xbool ui_renderer::Init( void )
{
    if( m_isInitialized )
        return TRUE;

    if( !LoadShaders() ||
        !CreateSamplers() ||
        !CreateWhiteTexture() )
    {
        Kill();
        RENDER_THROW( "UIRenderer", "initialization failed" );
        return FALSE;
    }

    m_isInitialized = TRUE;
    return TRUE;
}

//==============================================================================

void ui_renderer::Kill( void )
{
    m_DrawList.Clear();
    DestroyBuffers();
    DestroyPipelines();

    vram_DestroyTexture( m_whiteTexture );
    for( s32 i = 0; i < UI_SAMPLER_COUNT; i++ )
        rstate_DestroySampler( m_samplers[i] );

    shader_Destroy( m_pixelShader );
    shader_Destroy( m_vertexShader );

    m_DrawUniformSlot  = 0xffffffffu;
    m_PixelDrawUniformSlot = 0xffffffffu;
    m_textureSlot      = 0xffffffffu;
    m_PreparedVertices = 0;
    m_PreparedIndices  = 0;
    m_bPrepared        = FALSE;
    m_isInitialized     = FALSE;
}

//==============================================================================

xbool ui_renderer::IsInitialized( void ) const
{
    return m_isInitialized;
}

//==============================================================================

void ui_renderer::RefreshViewport( void )
{
    s32 Width;
    s32 Height;
    eng_GetRes( Width, Height );
    m_Viewport.SetOutputSize( Width, Height );
}

//==============================================================================

void ui_renderer::SetUserScale( f32 Scale )
{
    m_Viewport.SetUserScale( Scale );
}

//==============================================================================

void ui_renderer::SetHudUserScale( f32 Scale )
{
    m_Viewport.SetHudUserScale( Scale );
}

//==============================================================================

ui_viewport const& ui_renderer::GetViewport( void ) const
{
    return m_Viewport;
}

//==============================================================================

ui_draw_list& ui_renderer::GetDrawList( void )
{
    return m_DrawList;
}

//==============================================================================

void ui_renderer::PushClipRect( const irect& Rect )
{
    m_DrawList.PushClipRect( Rect );
}

//==============================================================================

void ui_renderer::PopClipRect( void )
{
    m_DrawList.PopClipRect();
}

//==============================================================================

void ui_renderer::PushScreenSpace( const irect& Bounds )
{
    m_DrawList.PushCoordinateSpace( UI_COORDINATE_SPACE_SCREEN, Bounds, Bounds );
}

//==============================================================================

void ui_renderer::PopScreenSpace( void )
{
    m_DrawList.PopCoordinateSpace();
}

//==============================================================================

void ui_renderer::PushHudSpace( const irect& ScreenViewport )
{
    const rect HudBounds = m_Viewport.GetHudBounds( ScreenViewport );
    const irect HudClip( (s32)x_floor( HudBounds.Min.X ),
                         (s32)x_floor( HudBounds.Min.Y ),
                         (s32)x_ceil ( HudBounds.Max.X ),
                         (s32)x_ceil ( HudBounds.Max.Y ) );

    #ifndef X_RETAIL
    const vector2 ScreenCenter( ((f32)ScreenViewport.l + (f32)ScreenViewport.r) * 0.5f,
                                ((f32)ScreenViewport.t + (f32)ScreenViewport.b) * 0.5f );
    const vector2 RoundTrip = m_Viewport.HudToScreen(
        m_Viewport.ScreenToHud( ScreenCenter, ScreenViewport ),
        ScreenViewport );
    ASSERTS( (x_abs( RoundTrip.X - ScreenCenter.X ) < 0.01f) &&
             (x_abs( RoundTrip.Y - ScreenCenter.Y ) < 0.01f),
             "HUD/screen point conversion failed to round-trip" );
    #endif

    m_DrawList.PushCoordinateSpace( UI_COORDINATE_SPACE_HUD, HudClip, ScreenViewport );
}

//==============================================================================

void ui_renderer::PopHudSpace( void )
{
    m_DrawList.PopCoordinateSpace();
}

//==============================================================================

xbool ui_renderer::DrawImage( const texture& Texture,
                              const vector2& Position,
                              const vector2& Size,
                              const vector2& UV0,
                              const vector2& UV1,
                              const xcolor&  Color,
                              radian Rotation,
                              ui_blend_mode Blend,
                              ui_sampler_mode Sampler )
{
    return m_DrawList.AddImage( ui_material( Texture, Blend, Sampler ),
                                Position, Size, UV0, UV1, Color, Rotation );
}

//==============================================================================

xbool ui_renderer::DrawImage( const shader_resource& Texture,
                              const vector2& Position,
                              const vector2& Size,
                              const vector2& UV0,
                              const vector2& UV1,
                              const xcolor&  Color,
                              radian Rotation,
                              ui_blend_mode Blend,
                              ui_sampler_mode Sampler )
{
    return m_DrawList.AddImage( ui_material( Texture, Blend, Sampler ),
                                Position, Size, UV0, UV1, Color, Rotation );
}

//==============================================================================

xbool ui_renderer::DrawRect( const irect& Rect,
                             const xcolor& Color,
                             xbool Wire,
                             ui_blend_mode Blend )
{
    if( !Wire )
        return m_DrawList.AddRect( Rect, Color, Blend );

    return DrawRectOutline( Rect, Color, Color, Color, Color, Blend );
}

//==============================================================================

xbool ui_renderer::DrawRect( const rect& Rect,
                             const xcolor& Color,
                             xbool Wire,
                             ui_blend_mode Blend )
{
    return DrawRect( irect( (s32)Rect.Min.X,
                            (s32)Rect.Min.Y,
                            (s32)Rect.Max.X,
                            (s32)Rect.Max.Y ),
                     Color,
                     Wire,
                     Blend );
}

//==============================================================================

xbool ui_renderer::DrawGradientRect( const irect& Rect,
                                     const xcolor& TopLeft,
                                     const xcolor& TopRight,
                                     const xcolor& BottomRight,
                                     const xcolor& BottomLeft,
                                     xbool Wire,
                                     ui_blend_mode Blend )
{
    if( !Wire )
        return m_DrawList.AddGradientRect( Rect, TopLeft, TopRight, BottomRight, BottomLeft, Blend );

    return DrawRectOutline( Rect, TopLeft, TopRight, BottomRight, BottomLeft, Blend );
}

//==============================================================================

xbool ui_renderer::DrawRectOutline( const irect& Rect,
                                    const xcolor& TopLeft,
                                    const xcolor& TopRight,
                                    const xcolor& BottomRight,
                                    const xcolor& BottomLeft,
                                    ui_blend_mode Blend )
{
    if( (Rect.GetWidth() <= 0) || (Rect.GetHeight() <= 0) )
        return TRUE;

    f32 Scale = 1.0f;
    vector2 Origin( 0.0f, 0.0f );
    if( m_DrawList.GetCoordinateSpace() == UI_COORDINATE_SPACE_LOGICAL )
    {
        Scale = m_Viewport.GetScale();
        Origin = m_Viewport.GetLogicalBounds().Min;
    }
    else if( m_DrawList.GetCoordinateSpace() == UI_COORDINATE_SPACE_HUD )
    {
        const irect& Viewport = m_DrawList.GetCoordinateViewport();
        Scale = m_Viewport.GetHudScale( Viewport );
        Origin = m_Viewport.GetHudBounds(Viewport).Min - vector2((f32)Viewport.l, (f32)Viewport.t) / Scale;
    }

    rect Aligned;
    Aligned.Set( Origin.X + x_floor(((f32)Rect.l - Origin.X) * Scale + 0.5f) / Scale,
                 Origin.Y + x_floor(((f32)Rect.t - Origin.Y) * Scale + 0.5f) / Scale,
                 Origin.X + x_floor(((f32)Rect.r - Origin.X) * Scale + 0.5f) / Scale,
                 Origin.Y + x_floor(((f32)Rect.b - Origin.Y) * Scale + 0.5f) / Scale );
    const f32 Width = MAX( 1.0f, x_floor(Scale + 0.5f) ) / Scale;
    return m_DrawList.AddRectOutline( Aligned, Width, TopLeft, TopRight, BottomRight, BottomLeft, Blend );
}

//==============================================================================

xbool ui_renderer::DrawLine( const vector2& Start,
                              const vector2& End,
                             const xcolor& Color,
                             f32 Width,
                             ui_blend_mode Blend )
{
    return m_DrawList.AddLine( Start, End, Color, Width, Blend );
}

//==============================================================================

xbool ui_renderer::DrawPoint( const vector2& Position,
                              const xcolor& Color,
                              f32 Size,
                              ui_blend_mode Blend )
{
    return m_DrawList.AddPoint( Position, Color, Size, Blend );
}

//==============================================================================

void ui_renderer::BeginFrame( void )
{
    if( !m_isInitialized )
        return;

    ASSERTS( !m_bPrepared, "Prepared UI data survived the previous frame" );
    RefreshViewport();
    m_DrawList.BeginFrame( m_Viewport.GetLogicalClipBounds() );
    m_PreparedVertices = 0;
    m_PreparedIndices  = 0;
    m_bPrepared = FALSE;
}

//==============================================================================

xbool ui_renderer::Prepare( void )
{
    X_PROFILE_SCOPE_CATEGORY( "UI", "UI/Prepare" );

    if( !m_isInitialized )
        return TRUE;

    if( !g_FontMgr.Prepare() )
    {
        RENDER_THROW( "UIRenderer", "font atlas upload failed" );
        return FALSE;
    }

    ASSERTS( m_DrawList.AreStacksBalanced(),
             "UI coordinate or clip stack is unbalanced at prepare" );
    if ( !m_DrawList.AreStacksBalanced() )
    {
        RENDER_THROW( "UIRenderer", "coordinate or clip stack is unbalanced" );
        return FALSE;
    }

    if( m_DrawList.GetCommandCount() == 0 )
        return TRUE;

    const s32 VertexCount = m_DrawList.GetVertexCount();
    const s32 IndexCount  = m_DrawList.GetIndexCount();
    if( (VertexCount <= 0) || (IndexCount <= 0) )
    {
        RENDER_THROW( "UIRenderer", "UI command list has no drawable geometry" );
        return FALSE;
    }

    if( !EnsureBufferCapacity( VertexCount, IndexCount ) )
    {
        RENDER_THROW( "UIRenderer", "failed to allocate frame submission buffers" );
        return FALSE;
    }

    const u32 VertexBytes = (u32)VertexCount * (u32)sizeof(ui_vertex);
    const u32 IndexBytes  = (u32)IndexCount  * (u32)sizeof(u32);

    rbuffer_upload_batch_item Uploads[2];
    Uploads[0].pBuffer       = &m_vertexBuffer;
    Uploads[0].Upload.pData  = m_DrawList.m_vertices.GetPtr();
    Uploads[0].Upload.Size   = VertexBytes;
    Uploads[0].Upload.bCycle = TRUE;
    Uploads[1].pBuffer       = &m_indexBuffer;
    Uploads[1].Upload.pData  = m_DrawList.m_indices.GetPtr();
    Uploads[1].Upload.Size   = IndexBytes;
    Uploads[1].Upload.bCycle = TRUE;
    if( !rbuffer_UploadBatch( Uploads, ARRAYSIZE( Uploads ) ) )
    {
        RENDER_THROW( "UIRenderer", "failed to upload frame submission" );
        return FALSE;
    }

    m_PreparedVertices = VertexCount;
    m_PreparedIndices  = IndexCount;
    m_bPrepared        = TRUE;
    return TRUE;
}

//==============================================================================

xbool ui_renderer::HasPendingCommands( void ) const
{
    return m_DrawList.GetCommandCount() > 0;
}

//==============================================================================

xbool ui_renderer::DeclarePass( render::FrameGraphBuilder& Graph, render::RenderFrameContext const& Frame )
{
    if( !m_isInitialized || !m_bPrepared )
        return TRUE;
    render::RenderPassDesc desc;
    desc.pDebugName = "UI";
    desc.Color = Frame.FinalColor;
    desc.ColorLoadOp = RTARGET_LOAD_LOAD;
    render::RenderPassId const pass = Graph.AddPass( desc, ExecuteGraphPassThunk, this );
    m_bPrepared = FALSE;
    return pass != render::INVALID_RENDER_PASS;
}

//==============================================================================

xbool ui_renderer::ExecuteGraphPassThunk( render::RenderPassContext& Context, ui_renderer* pRenderer )
{
    if ( !pRenderer )
    {
        RENDER_THROW( "UIRenderer", "UI pass received a null payload" );
        return FALSE;
    }
    return pRenderer->ExecuteGraphPass( Context );
}

//=============================================================================

xbool ui_renderer::ExecuteGraphPass( render::RenderPassContext& Context )
{
    X_PROFILE_SCOPE_CATEGORY( "UI", "UI/ExecuteGraphPass" );

    const rtarget* pBackBuffer = Context.Write( Context.GetFrame().FinalColor );

    if( !pBackBuffer ||
        (m_Viewport.GetOutputWidth() <= 0) ||
        (m_Viewport.GetOutputHeight() <= 0) ||
        !rbuffer_BindVertex( m_vertexBuffer, 0 ) ||
        !rbuffer_BindIndex( m_indexBuffer, RBUFFER_INDEX_FORMAT_U32 ) )
    {
        m_bPrepared = FALSE;
        RENDER_THROW( "UIRenderer", "UI pass target, viewport, or buffers are unavailable" );
        return FALSE;
    }

    if( !PrewarmPipelinesForFormat( pBackBuffer->Desc.Format ) )
    {
        RENDER_THROW( "UIRenderer", "failed to prewarm UI pipelines for format=%d",
                      pBackBuffer->Desc.Format );
        return FALSE;
    }

    ui_coordinate_space ActiveCoordinateSpace = UI_COORDINATE_SPACE_COUNT;
    irect ActiveCoordinateViewport;
    ActiveCoordinateViewport.Clear();
    ui_blend_mode ActiveBlend = UI_BLEND_COUNT;
    s32 ActiveCoverage = -1;
    const shader_resource* pActiveTexture = NULL;
    const rstate_sampler* pActiveSampler = NULL;
    xbool bHaveScissor = FALSE;
    rdraw_scissor ActiveScissor;

    for( s32 i = 0; i < m_DrawList.m_Commands.GetCount(); i++ )
    {
        const ui_draw_list::command& Command = m_DrawList.m_Commands[i];
        if( (Command.IndexCount == 0) ||
            (Command.StartIndex + Command.IndexCount > (u32)m_PreparedIndices) )
        {
            RENDER_THROW( "UIRenderer", "invalid draw command index=%d start=%u count=%u prepared=%d",
                          i, Command.StartIndex, Command.IndexCount, m_PreparedIndices );
            return FALSE;
        }

        if( (Command.CoordinateSpace != ActiveCoordinateSpace) ||
            (Command.CoordinateViewport != ActiveCoordinateViewport) ||
            ((s32)Command.Material.Coverage != ActiveCoverage) )
        {
            UIDrawConstants Constants = {};
            Constants.Coverage = Command.Material.Coverage ? 1 : 0;
            if( Command.CoordinateSpace == UI_COORDINATE_SPACE_HUD )
            {
                const irect& ScreenViewport = Command.CoordinateViewport;
                const rect HudBounds = m_Viewport.GetHudBounds( ScreenViewport );
                const f32 Scale = m_Viewport.GetHudScale( ScreenViewport );
                Constants.LogicalOrigin[0] = HudBounds.Min.X - (f32)ScreenViewport.l / Scale;
                Constants.LogicalOrigin[1] = HudBounds.Min.Y - (f32)ScreenViewport.t / Scale;
                Constants.InverseLogicalSize[0] = Scale / (f32)m_Viewport.GetOutputWidth();
                Constants.InverseLogicalSize[1] = Scale / (f32)m_Viewport.GetOutputHeight();
            }
            else if( Command.CoordinateSpace == UI_COORDINATE_SPACE_SCREEN )
            {
                Constants.LogicalOrigin[0]      = 0.0f;
                Constants.LogicalOrigin[1]      = 0.0f;
                Constants.InverseLogicalSize[0] = 1.0f / (f32)m_Viewport.GetOutputWidth();
                Constants.InverseLogicalSize[1] = 1.0f / (f32)m_Viewport.GetOutputHeight();
            }
            else
            {
                rect const& LogicalBounds = m_Viewport.GetLogicalBounds();
                Constants.LogicalOrigin[0]      = LogicalBounds.Min.X;
                Constants.LogicalOrigin[1]      = LogicalBounds.Min.Y;
                Constants.InverseLogicalSize[0] = 1.0f / LogicalBounds.GetWidth();
                Constants.InverseLogicalSize[1] = 1.0f / LogicalBounds.GetHeight();
            }

            if( !shader_PushUniformData( SHADER_STAGE_VERTEX,
                                         m_DrawUniformSlot,
                                         &Constants,
                                         sizeof(Constants) ) ||
                !shader_PushUniformData( SHADER_STAGE_PIXEL, m_PixelDrawUniformSlot, &Constants, sizeof(Constants) ) )
            {
                RENDER_THROW( "UIRenderer", "failed to upload draw constants command=%d", i );
                return FALSE;
            }

            ActiveCoverage = Command.Material.Coverage;
            ActiveCoordinateSpace = Command.CoordinateSpace;
            ActiveCoordinateViewport = Command.CoordinateViewport;
        }

        irect Clip;
        if( Command.CoordinateSpace == UI_COORDINATE_SPACE_HUD )
        {
            Clip = m_Viewport.HudToScreen( Command.ClipRect, Command.CoordinateViewport );
            Clip.l = x_clamp( Clip.l, Command.CoordinateViewport.l, Command.CoordinateViewport.r );
            Clip.t = x_clamp( Clip.t, Command.CoordinateViewport.t, Command.CoordinateViewport.b );
            Clip.r = x_clamp( Clip.r, Command.CoordinateViewport.l, Command.CoordinateViewport.r );
            Clip.b = x_clamp( Clip.b, Command.CoordinateViewport.t, Command.CoordinateViewport.b );
        }
        else if( Command.CoordinateSpace == UI_COORDINATE_SPACE_SCREEN )
        {
            Clip = Command.ClipRect;
            Clip.l = x_clamp( Clip.l, 0, m_Viewport.GetOutputWidth()  );
            Clip.t = x_clamp( Clip.t, 0, m_Viewport.GetOutputHeight() );
            Clip.r = x_clamp( Clip.r, 0, m_Viewport.GetOutputWidth()  );
            Clip.b = x_clamp( Clip.b, 0, m_Viewport.GetOutputHeight() );
        }
        else
        {
            Clip = m_Viewport.LogicalToScreen( Command.ClipRect );
        }

        if( (Clip.r <= Clip.l) || (Clip.b <= Clip.t) )
            continue;

        if( Command.Material.Blend != ActiveBlend )
        {
            if( !BindPipeline( pBackBuffer->Desc.Format, Command.Material.Blend ) )
            {
                RENDER_THROW( "UIRenderer", "failed to bind pipeline command=%d format=%d blend=%d", i,
                              pBackBuffer->Desc.Format, Command.Material.Blend );
                return FALSE;
            }
            pActiveTexture = NULL;
            pActiveSampler = NULL;
        }
        ActiveBlend = Command.Material.Blend;

        const shader_resource* pTexture = &Command.Material.Texture;
        if( !*pTexture )
            pTexture = vram_GetShaderResource( m_whiteTexture );

        const rstate_sampler* pSampler = GetSampler( Command.Material.Sampler );
        if( !pTexture || !pSampler )
        {
            RENDER_THROW( "UIRenderer", "failed to bind texture command=%d", i );
            return FALSE;
        }
        if( ( !pActiveTexture || ( pActiveTexture->pBackend != pTexture->pBackend ) ||
              ( pActiveSampler != pSampler ) ) &&
            !shader_BindSampler( shader_sampler_binding( SHADER_STAGE_PIXEL,
                                                         m_textureSlot,
                                                         pTexture,
                                                         pSampler ) ) )
        {
            RENDER_THROW( "UIRenderer", "failed to bind texture command=%d", i );
            return FALSE;
        }
        pActiveTexture = pTexture;
        pActiveSampler = pSampler;

        rdraw_scissor Scissor;
        Scissor.X      = Clip.l;
        Scissor.Y      = Clip.t;
        Scissor.Width  = Clip.r - Clip.l;
        Scissor.Height = Clip.b - Clip.t;
        if( !bHaveScissor || ( Scissor.X != ActiveScissor.X ) || ( Scissor.Y != ActiveScissor.Y ) ||
            ( Scissor.Width != ActiveScissor.Width ) || ( Scissor.Height != ActiveScissor.Height ) )
        {
            if( !rdraw_SetScissor( Scissor ) )
            {
                RENDER_THROW( "UIRenderer", "failed to set scissor command=%d", i );
                return FALSE;
            }
            ActiveScissor = Scissor;
            bHaveScissor = TRUE;
        }

        rdraw_DrawIndexed( (s32)Command.IndexCount, (s32)Command.StartIndex, 0 );
    }

    rdraw_scissor FullScissor;
    FullScissor.X      = 0;
    FullScissor.Y      = 0;
    FullScissor.Width  = (s32)pBackBuffer->Desc.Width;
    FullScissor.Height = (s32)pBackBuffer->Desc.Height;
    if ( !rdraw_SetScissor( FullScissor ) )
    {
        RENDER_THROW( "UIRenderer", "failed to restore full scissor" );
        return FALSE;
    }

    m_bPrepared = FALSE;
    return TRUE;
}

//==============================================================================

xbool ui_renderer::LoadShaders( void )
{
    shader_LoadComboStage( m_vertexShader, A51FixedPrograms[A51FixedProgram_ui].PackageFile, SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_ui].Key, SHADER_STAGE_VERTEX );
    shader_LoadComboStage( m_pixelShader, A51FixedPrograms[A51FixedProgram_ui].PackageFile, SHADER_COMBO_FIXED, A51FixedPrograms[A51FixedProgram_ui].Key, SHADER_STAGE_PIXEL );

    if( !m_vertexShader || !m_pixelShader )
        return FALSE;

    if( !shader_GetResourceSlot( m_vertexShader, A51Resource_cbUIDraw, m_DrawUniformSlot ) )
    {
        return FALSE;
    }

    return shader_GetResourceSlot( m_pixelShader, A51Resource_cbUIDraw, m_PixelDrawUniformSlot ) &&
           shader_GetResourceSlot( m_pixelShader, A51Resource_txUI, m_textureSlot );
}

//==============================================================================

xbool ui_renderer::CreateSamplers( void )
{
    for( s32 i = 0; i < UI_SAMPLER_COUNT; i++ )
    {
        const ui_sampler_mode Sampler = (ui_sampler_mode)i;
        rstate_sampler_desc Desc = rstate_GetSamplerDesc( ui_GetSamplerPreset( Sampler ) );
        Desc.pDebugName = (Sampler == UI_SAMPLER_LINEAR_CLAMP_ATLAS)
                        ? "UIAtlasSampler"
                        : "UISampler";

        // Atlas mipmaps blend unrelated cells and their registration pixels.
        // Keep bilinear minification within the base atlas image instead.
        if( Sampler == UI_SAMPLER_LINEAR_CLAMP_ATLAS )
        {
            Desc.MinLOD = 0.0f;
            Desc.MaxLOD = 0.0f;
        }

        if( !rstate_CreateSampler( m_samplers[i], Desc ) )
        {
            return FALSE;
        }
    }
    return TRUE;
}

//==============================================================================

xbool ui_renderer::CreateWhiteTexture( void )
{
    vram_texture_desc Desc;
    Desc.Width      = 1;
    Desc.Height     = 1;
    Desc.Format     = VRAM_TEXTURE_FORMAT_RGBA8;
    Desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    Desc.pDebugName = "UIWhiteTexture";

    if( !vram_CreateTexture( m_whiteTexture, Desc ) )
        return FALSE;

    const u32 WhitePixel = 0xffffffffu;
    vram_texture_upload_desc Upload;
    Upload.Region.Width  = 1;
    Upload.Region.Height = 1;
    Upload.Region.Depth  = 1;
    Upload.pData         = &WhitePixel;
    Upload.Size          = sizeof(WhitePixel);
    Upload.RowPitch      = sizeof(WhitePixel);
    Upload.SlicePitch    = sizeof(WhitePixel);
    return vram_UploadTexture( m_whiteTexture, Upload );
}

//==============================================================================

void ui_renderer::DestroyPipelines( void )
{
    m_pipelines.Reset();
    m_prewarmedFormat = RTARGET_FORMAT_COUNT;
}

//==============================================================================

xbool ui_renderer::PrewarmPipelinesForFormat( rtarget_format Format )
{
    if( Format == RTARGET_FORMAT_COUNT )
        return FALSE;

    if( m_prewarmedFormat == Format )
        return TRUE;

    for( s32 Blend = 0; Blend < UI_BLEND_COUNT; ++Blend )
    {
        if( !GetOrCreatePipeline( Format,
                                  (ui_blend_mode)Blend,
                                  TRUE ) )
            return FALSE;
    }

    m_prewarmedFormat = Format;

    x_DebugMsg( "UIRenderer: prewarmed %d graphics pipeline variants for format %d\n",
                UI_BLEND_COUNT,
                Format );
    return TRUE;
}

//==============================================================================

void ui_renderer::DestroyBuffers( void )
{
    rbuffer_Destroy( m_indexBuffer );
    rbuffer_Destroy( m_vertexBuffer );
    m_VertexCapacity   = 0;
    m_IndexCapacity    = 0;
    m_PreparedVertices = 0;
    m_PreparedIndices  = 0;
}

//==============================================================================

xbool ui_renderer::EnsureBufferCapacity( s32 VertexCount, s32 IndexCount )
{
    if( (VertexCount <= m_VertexCapacity) && (IndexCount <= m_IndexCapacity) )
        return TRUE;

    return CreateBuffers( ui_NextCapacity( m_VertexCapacity, VertexCount ),
                          ui_NextCapacity( m_IndexCapacity,  IndexCount  ) );
}

//==============================================================================

xbool ui_renderer::CreateBuffers( s32 VertexCapacity, s32 IndexCapacity )
{
    if( (VertexCapacity <= 0) || (IndexCapacity <= 0) ||
        ((u32)VertexCapacity > (0xffffffffu / (u32)sizeof(ui_vertex))) ||
        ((u32)IndexCapacity  > (0xffffffffu / (u32)sizeof(u32))) )
    {
        return FALSE;
    }

    DestroyBuffers();

    rbuffer_desc VertexDesc;
    VertexDesc.Size       = (u32)VertexCapacity * (u32)sizeof(ui_vertex);
    VertexDesc.Stride     = sizeof(ui_vertex);
    VertexDesc.UsageFlags = RBUFFER_USAGE_VERTEX;
    VertexDesc.pDebugName = "UIVertexBuffer";

    rbuffer_desc IndexDesc;
    IndexDesc.Size       = (u32)IndexCapacity * (u32)sizeof(u32);
    IndexDesc.Stride     = sizeof(u32);
    IndexDesc.UsageFlags = RBUFFER_USAGE_INDEX;
    IndexDesc.pDebugName = "UIIndexBuffer";

    if( !rbuffer_Create( m_vertexBuffer, VertexDesc ) ||
        !rbuffer_Create( m_indexBuffer, IndexDesc ) )
    {
        DestroyBuffers();
        return FALSE;
    }

    m_VertexCapacity = VertexCapacity;
    m_IndexCapacity  = IndexCapacity;
    return TRUE;
}

//==============================================================================

render_pipeline* ui_renderer::GetOrCreatePipeline( rtarget_format Format,
                                                   ui_blend_mode  Blend,
                                                   xbool          isPrewarm )
{
    render_pipeline_desc Desc;
    if( !BuildPipelineDesc( Desc, Format, Blend ) )
        return NULL;

    return isPrewarm
         ? m_pipelines.Prewarm( Desc )
         : m_pipelines.GetOrCreate( Desc );
}

//==============================================================================

xbool ui_renderer::BindPipeline( rtarget_format Format, ui_blend_mode Blend )
{
    render_pipeline* pPipeline = GetOrCreatePipeline( Format, Blend );
    return pPipeline && render_BindPipeline( *pPipeline );
}

//==============================================================================

xbool ui_renderer::BuildPipelineDesc( render_pipeline_desc& Desc,
                                      rtarget_format       Format,
                                      ui_blend_mode        Blend ) const
{
    if( (Format < 0) || (Format >= RTARGET_FORMAT_COUNT) ||
        (Blend < 0) || (Blend >= UI_BLEND_COUNT) )
    {
        return FALSE;
    }

    static const shader_vertex_buffer_desc VertexBuffer = []()
    {
        shader_vertex_buffer_desc Desc;
        Desc.Slot   = 0;
        Desc.Stride = sizeof(ui_vertex);
        return Desc;
    }();

    static const shader_vertex_element Layout[] =
    {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT2,       offsetof(ui_vertex, Position) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof(ui_vertex, Color)    ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2,       offsetof(ui_vertex, UV)       )
    };

    Desc = render_pipeline_desc();
    Desc.Shader.pVertexShader     = &m_vertexShader;
    Desc.Shader.pPixelShader      = &m_pixelShader;
    Desc.Shader.pVertexBuffers    = &VertexBuffer;
    Desc.Shader.VertexBufferCount = 1;
    Desc.Shader.pInputElements    = Layout;
    Desc.Shader.InputElementCount = ARRAYSIZE(Layout);
    Desc.Shader.Topology          = SHADER_TOPOLOGY_TRIANGLE_LIST;
    Desc.Depth                    = rstate_GetDepthDesc( RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE );
    Desc.Raster                   = rstate_GetRasterDesc( RSTATE_RASTER_PRESET_SOLID_NO_CULL );
    Desc.ColorCount               = 1;
    Desc.ColorTargets[0].Format   = Format;
    Desc.ColorTargets[0].Blend    = rstate_GetBlendDesc( ui_GetBlendPreset( Blend ) );
    Desc.DepthFormat              = RTARGET_FORMAT_COUNT;
    Desc.SampleCount              = 1;
    Desc.pDebugName               = "UIPipeline";
    return TRUE;
}

//==============================================================================

const rstate_sampler* ui_renderer::GetSampler( ui_sampler_mode Sampler ) const
{
    if( (Sampler < 0) || (Sampler >= UI_SAMPLER_COUNT) || !m_samplers[Sampler] )
        return NULL;
    return &m_samplers[Sampler];
}
