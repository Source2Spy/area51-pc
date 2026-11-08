//==============================================================================
//
//  ui_renderer.hpp
//
//  Platform-neutral screen-space draw list and RHI renderer.
//
//==============================================================================

#ifndef UI_RENDERER_HPP
#define UI_RENDERER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_array.hpp"
#include "x_color.hpp"
#include "x_math.hpp"

#include "e_RenderBuffer.hpp"
#include "e_RenderState.hpp"
#include "e_VRAM.hpp"
#include "Render/GraphicsPipelineCache.hpp"

namespace render
{
class FrameGraphBuilder;
class RenderPassContext;
struct RenderFrameContext;
}

class texture;

//==============================================================================
//  VIEWPORT
//==============================================================================

class ui_viewport
{
public:
    enum
    {
        CONTENT_WIDTH  = 512,
        CONTENT_HEIGHT = 448,
        CANVAS_WIDTH   = 640,
        CANVAS_HEIGHT  = 560
    };

                    ui_viewport             ( void );

    void            SetOutputSize           ( s32 Width, s32 Height );
    void            SetUserScale            ( f32 Scale );
    void            SetHudUserScale         ( f32 Scale );

    s32             GetOutputWidth          ( void ) const;
    s32             GetOutputHeight         ( void ) const;
    f32             GetScale                ( void ) const;
    f32             GetUserScale            ( void ) const;
    f32             GetHudUserScale         ( void ) const;
    f32             GetHudScale             ( const irect& ScreenViewport ) const;
    rect const&     GetLogicalBounds        ( void ) const;
    irect           GetLogicalClipBounds    ( void ) const;

    vector2         ScreenDeltaToLogical    ( vector2 const& Delta ) const;
    irect           LogicalToScreen         ( irect const& Rect ) const;
    rect            GetHudBounds            ( const irect& ScreenViewport ) const;
    vector2         ScreenToHud             ( const vector2& Position,
                                               const irect& ScreenViewport ) const;
    vector2         HudToScreen             ( const vector2& Position,
                                               const irect& ScreenViewport ) const;
    irect           HudToScreen             ( const irect& Rect,
                                               const irect& ScreenViewport ) const;

protected:
    void            Recalculate             ( void );

    s32             m_OutputWidth;
    s32             m_OutputHeight;
    f32             m_Scale;
    f32             m_UserScale;
    f32             m_HudUserScale;
    rect            m_LogicalBounds;
};

//==============================================================================
//  DRAW DATA
//==============================================================================

enum ui_blend_mode
{
    UI_BLEND_OPAQUE = 0,
    UI_BLEND_ALPHA,
    UI_BLEND_ADDITIVE,
    UI_BLEND_SUBTRACTIVE,
    UI_BLEND_MULTIPLY,
    UI_BLEND_INTENSITY,
    UI_BLEND_COUNT
};

//------------------------------------------------------------------------------

enum ui_sampler_mode
{
    UI_SAMPLER_LINEAR_CLAMP = 0,
    UI_SAMPLER_LINEAR_CLAMP_ATLAS,
    UI_SAMPLER_LINEAR_WRAP,
    UI_SAMPLER_POINT_CLAMP,
    UI_SAMPLER_POINT_WRAP,
    UI_SAMPLER_COUNT
};

//------------------------------------------------------------------------------

enum ui_coordinate_space
{
    UI_COORDINATE_SPACE_LOGICAL = 0,
    UI_COORDINATE_SPACE_SCREEN,
    UI_COORDINATE_SPACE_HUD,
    UI_COORDINATE_SPACE_COUNT
};

//------------------------------------------------------------------------------

inline void ui_GetTextureRectUV( const irect& TextureRect,
                                 s32 TextureWidth,
                                 s32 TextureHeight,
                                 vector2& UV0,
                                 vector2& UV1 )
{
    // Sample the edge texel centers so bilinear filtering cannot reach an
    // adjacent atlas cell. Right and bottom are exclusive irect edges.
    const f32 InvWidth  = 1.0f / (f32)TextureWidth;
    const f32 InvHeight = 1.0f / (f32)TextureHeight;

    UV0.Set( ((f32)TextureRect.l + 0.5f) * InvWidth,
             ((f32)TextureRect.t + 0.5f) * InvHeight );
    UV1.Set( ((f32)TextureRect.r - 0.5f) * InvWidth,
             ((f32)TextureRect.b - 0.5f) * InvHeight );
}

//------------------------------------------------------------------------------

struct ui_vertex
{
    vector2 Position;
    u32     Color;
    vector2 UV;

    ui_vertex( void );
    ui_vertex( const vector2& Position, const vector2& UV, const xcolor& Color );
};

//------------------------------------------------------------------------------

struct ui_material
{
    shader_resource        Texture;
    ui_blend_mode          Blend;
    ui_sampler_mode        Sampler;
    xbool                  Coverage;

    ui_material( void );
    ui_material( const texture& Texture,
                 ui_blend_mode BlendMode = UI_BLEND_ALPHA,
                 ui_sampler_mode SamplerMode = UI_SAMPLER_LINEAR_CLAMP );
    ui_material( const shader_resource& Texture,
                 ui_blend_mode BlendMode = UI_BLEND_ALPHA,
                 ui_sampler_mode SamplerMode = UI_SAMPLER_LINEAR_CLAMP );
};

//==============================================================================
//  DRAW LIST
//==============================================================================

class ui_draw_list
{
public:
                    ui_draw_list       ( void );

    void            BeginFrame         ( irect const& Bounds );
    void            Clear              ( void );

    void            PushClipRect       ( const irect& Rect );
    void            PopClipRect        ( void );
    const irect&    GetClipRect        ( void ) const;

    xbool           AddTriangles       ( const ui_material& Material,
                                          const ui_vertex*   pVertices,
                                          s32                VertexCount,
                                          const u32*         pIndices,
                                          s32                IndexCount );
    xbool           AddImage           ( const ui_material& Material,
                                          const vector2&     Position,
                                          const vector2&     Size,
                                          const vector2&     UV0,
                                          const vector2&     UV1,
                                          const xcolor&      Color,
                                          radian             Rotation = 0.0f );
    xbool           AddRect            ( const irect& Rect,
                                          const xcolor& Color,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );
    xbool           AddGradientRect    ( const irect& Rect,
                                          const xcolor& TopLeft,
                                          const xcolor& TopRight,
                                          const xcolor& BottomRight,
                                          const xcolor& BottomLeft,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );
    xbool           AddRectOutline     ( const rect& Rect,
                                          f32 Width,
                                          const xcolor& TopLeft,
                                          const xcolor& TopRight,
                                          const xcolor& BottomRight,
                                          const xcolor& BottomLeft,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );
    xbool           AddLine            ( const vector2& Start,
                                          const vector2& End,
                                          const xcolor&  Color,
                                          f32            Width = 1.0f,
                                          ui_blend_mode  Blend = UI_BLEND_ALPHA );
    xbool           AddPoint           ( const vector2& Position,
                                          const xcolor& Color,
                                          f32 Size = 1.0f,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );

    s32             GetVertexCount     ( void ) const;
    s32             GetIndexCount      ( void ) const;
    s32             GetCommandCount    ( void ) const;

protected:
    struct command
    {
        ui_material Material;
        irect       ClipRect;
        ui_coordinate_space
                    CoordinateSpace;
        irect       CoordinateViewport;
        u32         StartIndex;
        u32         IndexCount;

        command( void );
    };

    struct coordinate_state
    {
        ui_coordinate_space Space;
        irect               Viewport;
        s32                 ClipDepth;

        coordinate_state( void );
    };

    xbool           IsSameCommand      ( const command& A, const command& B ) const;
    void            AppendCommand      ( const ui_material& Material,
                                          u32 StartIndex,
                                          u32 IndexCount );
    void            PushCoordinateSpace( ui_coordinate_space CoordinateSpace,
                                          const irect& Bounds,
                                          const irect& Viewport );
    void            PopCoordinateSpace ( void );
public:
    ui_coordinate_space
                    GetCoordinateSpace ( void ) const;
    const irect&    GetCoordinateViewport( void ) const;
protected:
    s32             GetCoordinateClipDepth( void ) const;
    xbool           AreStacksBalanced      ( void ) const;

protected:
    xarray<ui_vertex> m_vertices;
    xarray<u32>       m_indices;
    xarray<command>   m_Commands;
    xarray<irect>     m_ClipStack;
    xarray<coordinate_state>
                       m_CoordinateStack;

    friend class ui_renderer;
};

//==============================================================================
//  RENDERER
//==============================================================================

class ui_renderer
{
public:
                    ui_renderer        ( void );

    xbool           Init               ( void );
    void            Kill               ( void );
    xbool           IsInitialized      ( void ) const;

    void            RefreshViewport    ( void );
    void            SetUserScale       ( f32 Scale );
    void            SetHudUserScale    ( f32 Scale );
    ui_viewport const&
                    GetViewport        ( void ) const;

    ui_draw_list&   GetDrawList        ( void );

    void            PushClipRect       ( const irect& Rect );
    void            PopClipRect        ( void );
    void            PushScreenSpace    ( const irect& Bounds );
    void            PopScreenSpace     ( void );
    void            PushHudSpace       ( const irect& ScreenViewport );
    void            PopHudSpace        ( void );

    xbool           DrawImage          ( const texture& Texture,
                                          const vector2& Position,
                                          const vector2& Size,
                                          const vector2& UV0,
                                          const vector2& UV1,
                                          const xcolor&  Color = XCOLOR_WHITE,
                                          radian Rotation = 0.0f,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA,
                                          ui_sampler_mode Sampler = UI_SAMPLER_LINEAR_CLAMP );
    xbool           DrawImage          ( const shader_resource& Texture,
                                          const vector2& Position,
                                          const vector2& Size,
                                          const vector2& UV0,
                                          const vector2& UV1,
                                          const xcolor&  Color = XCOLOR_WHITE,
                                          radian Rotation = 0.0f,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA,
                                          ui_sampler_mode Sampler = UI_SAMPLER_LINEAR_CLAMP );
    xbool           DrawRect           ( const irect& Rect,
                                          const xcolor& Color,
                                          xbool Wire = FALSE,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );
    xbool           DrawRect           ( const rect& Rect,
                                          const xcolor& Color,
                                          xbool Wire = FALSE,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );
    xbool           DrawGradientRect   ( const irect& Rect,
                                          const xcolor& TopLeft,
                                          const xcolor& TopRight,
                                          const xcolor& BottomRight,
                                          const xcolor& BottomLeft,
                                          xbool Wire = FALSE,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );
    xbool           DrawLine           ( const vector2& Start,
                                          const vector2& End,
                                          const xcolor& Color,
                                          f32 Width = 1.0f,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );
    xbool           DrawPoint          ( const vector2& Position,
                                          const xcolor& Color,
                                          f32 Size = 1.0f,
                                          ui_blend_mode Blend = UI_BLEND_ALPHA );

    void            BeginFrame         ( void );
    xbool           Prepare            ( void );
    xbool           HasPendingCommands ( void ) const;
    xbool           DeclarePass        ( render::FrameGraphBuilder& Graph, render::RenderFrameContext const& Frame );

protected:

    xbool           DrawRectOutline    ( const irect& Rect,
                                          const xcolor& TopLeft,
                                          const xcolor& TopRight,
                                          const xcolor& BottomRight,
                                          const xcolor& BottomLeft,
                                          ui_blend_mode Blend );
    xbool           LoadShaders        ( void );
    xbool           CreateSamplers     ( void );
    xbool           CreateWhiteTexture ( void );
    xbool           PrewarmPipelinesForFormat( rtarget_format Format );
    void            DestroyPipelines   ( void );
    void            DestroyBuffers     ( void );
    xbool           EnsureBufferCapacity( s32 VertexCount, s32 IndexCount );
    xbool           CreateBuffers      ( s32 VertexCapacity, s32 IndexCapacity );
    xbool           BindPipeline       ( rtarget_format Format, ui_blend_mode Blend );
    xbool           ExecuteGraphPass   ( render::RenderPassContext& Context );
    static xbool    ExecuteGraphPassThunk( render::RenderPassContext& Context, ui_renderer* pRenderer );
    render_pipeline*
                    GetOrCreatePipeline( rtarget_format Format,
                                          ui_blend_mode Blend,
                                          xbool         isPrewarm = FALSE );
    xbool           BuildPipelineDesc  ( render_pipeline_desc& Out,
                                          rtarget_format       Format,
                                          ui_blend_mode        Blend ) const;
    const rstate_sampler*
                    GetSampler         ( ui_sampler_mode Sampler ) const;

protected:
    ui_viewport      m_Viewport;
    ui_draw_list     m_DrawList;
    rbuffer          m_vertexBuffer;
    rbuffer          m_indexBuffer;
    s32              m_VertexCapacity;
    s32              m_IndexCapacity;
    s32              m_PreparedVertices;
    s32              m_PreparedIndices;

    shader           m_vertexShader;
    shader           m_pixelShader;
    u32              m_DrawUniformSlot;
    u32              m_PixelDrawUniformSlot;
    u32              m_textureSlot;
    rstate_sampler   m_samplers[UI_SAMPLER_COUNT];
    vram_texture     m_whiteTexture;
    render::GraphicsPipelineCache
                     m_pipelines;
    rtarget_format   m_prewarmedFormat;

    xbool            m_bPrepared;
    xbool            m_isInitialized;
};

//==============================================================================

extern ui_renderer g_UIRenderer;

//==============================================================================
#endif // UI_RENDERER_HPP
//==============================================================================
