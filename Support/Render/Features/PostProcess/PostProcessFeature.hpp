//=========================================================================
//
//  PostProcessFeature.hpp
//
//=========================================================================

#ifndef POST_PROCESS_FEATURE_HPP
#define POST_PROCESS_FEATURE_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../Texture.hpp"
#include "../../Render.hpp"
#include "../../RenderFog.hpp"
#include "e_RenderState.hpp"
#include "e_Composite.hpp"

#include "../Bloom/BloomEffect.hpp"

namespace render
{

//=========================================================================
//  FORWARD DECLARATIONS
//=========================================================================

class GraphicsPipelineCache;

//=========================================================================
//  CONSTANTS
//=========================================================================

#define DEPTH_BUFFER_BITS 19
#define MAX_POST_MIPS 3
#define MAX_POST_SCREEN_WARPS 8

//=========================================================================
//  POST-PROCESSING PARAMETER STRUCTURES
//=========================================================================

struct PostGlowParams
{
    f32 MotionBlurIntensity;
    s32 Cutoff;

    PostGlowParams();
};

//-------------------------------------------------------------------------

struct PostMotionBlurParams
{
    f32 Intensity;

    PostMotionBlurParams();
};

//-------------------------------------------------------------------------

struct PostRadialBlurParams
{
    f32    Zoom;
    radian Angle;
    f32    AlphaSub;
    f32    AlphaScale;

    PostRadialBlurParams();
};

//-------------------------------------------------------------------------

struct PostScreenWarpParams
{
    s32     Count;
    vector3 WorldPos[MAX_POST_SCREEN_WARPS];
    f32     Radius[MAX_POST_SCREEN_WARPS];
    f32     Amount[MAX_POST_SCREEN_WARPS];

    PostScreenWarpParams();
};

//-------------------------------------------------------------------------

struct PostFogFilterParams
{
    render::post_falloff_fn Fn[5];
    s32                     PaletteIndex;
    xcolor                  GeneratedColor;
    f32                     GeneratedParam1;
    f32                     GeneratedParam2;

    PostFogFilterParams();
};

//-------------------------------------------------------------------------

struct PostMipFilterParams
{
    render::post_falloff_fn Fn[4];
    xcolor                  Color[4];
    f32                     Param1[4];
    f32                     Param2[4];
    s32                     Count[4];
    f32                     Offset[4];
    s32                     PaletteIndex;

    PostMipFilterParams();
};

//-------------------------------------------------------------------------

struct PostSimpleParams
{
    xcolor NoiseColor;
    xcolor FadeColor;

    PostSimpleParams();
};

//-------------------------------------------------------------------------

struct PostChainRequests
{
    // Allow debugger overrides.
    u32 Override : 1;

    // Effect enable flags
    u32 DoMotionBlur : 1;
    u32 DoSelfIllumGlow : 1;
    u32 DoRadialBlur : 1;
    u32 DoZFogFn : 1;
    u32 DoZFogCustom : 1;
    u32 DoMipFn : 1;
    u32 DoMipCustom : 1;
    u32 DoNoise : 1;
    u32 DoScreenFade : 1;

    PostChainRequests();
};

//=========================================================================
//  POST-PROCESSING FEATURE
//=========================================================================

class PostProcessFeature : public IRenderFeature
{
    friend class BloomEffect;

public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    PostProcessFeature( void );

    virtual const char* GetName( void ) const;
    virtual u32         GetDependencyCount( void ) const;
    virtual const char* GetDependency( u32 Index ) const;
    virtual u32         GetGraphStageMask( void ) const;

    BloomEffect& GetBloomFeature( void );

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    virtual xbool Init( void );
    virtual void  Kill( void );
    void          Update( f32 DeltaTime );
    xbool QueuePipelines( GraphicsPipelineCache&    Cache,
                          const RenderFormatPolicy& Formats,
                          rtarget_format            OutputFormat,
                          u32                       OutputSamples );
    xbool         IsInitialized( void ) const;

    void                         SetSceneColorGrade( const SceneColorGradeParams& Params );
    const SceneColorGradeParams& GetSceneColorGrade( void ) const;

    //-------------------------------------------------------------------------
    //  FRAME PIPELINE
    //-------------------------------------------------------------------------

    void          BeginFrame( void );
    virtual void  BeginFrame( const RenderFrameContext& Frame );
    xbool         DeclareFogPass( FrameGraphBuilder& Graph, const RenderFrameContext& FrameData );
    xbool         DeclarePostChain( FrameGraphBuilder& Graph, RenderFrameContext& FrameData );
    xbool         DeclareScreenFadePass( FrameGraphBuilder& Graph, const RenderFrameContext& FrameData );
    virtual void  EndFrame( void );
    virtual void  AbortFrame( void );
    virtual xbool Prepare( RenderFeaturePrepareContext& Context );
    virtual xbool Declare( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool Build( FrameGraphBuilder& Graph, const RenderFrameContext& Frame );
    virtual xbool BuildGraphStage( RenderFeatureGraphStage Stage, FrameGraphBuilder& Graph, RenderFrameContext& Frame );
    virtual u32   GetSceneTextureRequirements( void ) const;

    //-------------------------------------------------------------------------
    //  POST-EFFECT REQUESTS
    //-------------------------------------------------------------------------

    void ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff );
    void MotionBlur( f32 Intensity );
    void AddScreenWarp( const vector3& WorldPos, f32 Radius, f32 WarpAmount );
    void MipFilter( s32                     nFilters,
                    f32                     Offset,
                    render::post_falloff_fn Fn,
                    xcolor                  Color,
                    f32                     Param1,
                    f32                     Param2,
                    s32                     PaletteIndex );
    void MipFilter( s32                     nFilters,
                    f32                     Offset,
                    render::post_falloff_fn Fn,
                    const texture::handle&  Texture,
                    s32                     PaletteIndex );
    void NoiseFilter( xcolor Color );
    void ScreenFade( xcolor Color );
    void RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale );
    void SetFrameHasForcedGlow( xbool HasForcedGlow );

    //-------------------------------------------------------------------------
    //  FOG REQUESTS AND QUERIES
    //-------------------------------------------------------------------------

    void SetCustomFogPalette( const texture::handle& Texture,
                              xbool                  ImmediateSwitch,
                              s32                    PaletteIndex,
                              const view*            pRenderView );
    void                   ZFogFilter( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 );
    void                   ZFogFilter( render::post_falloff_fn Fn, s32 PaletteIndex );
    xcolor                 GetFogValue( const vector3& WorldPos, s32 PaletteIndex, const view* pRenderView );
    void                   GetFogConstants( RenderFogConstants& Constants ) const;
    void                   SetXboxFogEnabled( xbool Enabled );
    const shader_resource* GetFogPaletteResource( void ) const;
    const rstate_sampler*  GetFogPaletteSampler( void ) const;

protected:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct TemporalHistoryState
    {
        u64            Key;
        u64            ResetToken     = 0;
        u64            LastUsedSerial = 0;
        u32            Width;
        u32            Height;
        rtarget_format Format;
        xbool          Valid;

        TemporalHistoryState( void );
    };

    //-------------------------------------------------------------------------

    struct PreparedWarpView
    {
        u32                 ViewIndex;
        PostFilterConstants Constants;
    };

    //-------------------------------------------------------------------------

    struct FogResources
    {

        //-------------------------------------------------------------------------
        //  FUNCTIONS
        //-------------------------------------------------------------------------

        FogResources();

        // Resource lifetime
        xbool Initialize( void );
        void  Shutdown( void );

        // Shader state updates
        void                   UpdateConstants( const RenderFogConstants& Constants );
        xbool                  UpdatePaletteTexture( FrameGraphBuilder& Graph, const u8* pPalette );
        xbool                  BindConstants( const shader& PixelShader ) const;
        const shader_resource* GetPaletteResource( void ) const;
        const rstate_sampler*  GetPaletteSampler( void ) const;

        //-------------------------------------------------------------------------
        //  DATA
        //-------------------------------------------------------------------------

        // GPU resources
        vram_texture       PaletteTexture;
        shader             CompositePS;
        rstate_sampler     PaletteSampler;
        RenderFogConstants Constants;
    };

    //-------------------------------------------------------------------------

    struct GlowResources
    {

        //-------------------------------------------------------------------------
        //  FUNCTIONS
        //-------------------------------------------------------------------------

        GlowResources();

        // Resource lifetime
        xbool Initialize( void );
        void  Shutdown( void );

        void ResetFrame( void );
        void InvalidateHistory( void );

        //-------------------------------------------------------------------------
        //  DATA
        //-------------------------------------------------------------------------

        u32            BufferWidth;
        u32            BufferHeight;
        rtarget_format BufferFormat;
        xbool          isHistoryValid;
        f32            PulseScale;
        f32            PulseDirection;

        // GPU resources
        shader         PrefilterPS;
        shader         DownsamplePS;
        shader         BlurHPS;
        shader         BlurVPS;
        shader         CombinePS;
        shader         CompositePS;
        rstate_sampler AuxSampler;
    };

    //-------------------------------------------------------------------------

    struct FilterResources
    {

        //-------------------------------------------------------------------------
        //  FUNCTIONS
        //-------------------------------------------------------------------------

        FilterResources();

        // Resource lifetime
        xbool Initialize( void );
        void  Shutdown( void );

        void InvalidateHistory( void );

        // Shader state updates
        xbool UpdateMipRampTexture( const xbitmap* pBitmap );

        //-------------------------------------------------------------------------
        //  DATA
        //-------------------------------------------------------------------------

        // GPU resources
        shader         MotionBlurPS;
        shader         MipReducePS;
        shader         MipDownsamplePS;
        shader         PainBlurPS;
        shader         MipCompositePS;
        shader         MipCompositeCustomPS;
        shader         RadialBlurPS;
        shader         ScreenWarpPS;
        shader         WarpMaskPS;
        shader         WarpMaskBlurPS;
        shader         NoisePS;
        shader         ScreenFadePS;
        vram_texture   MipRampTexture;
        rstate_sampler LinearSampler;
        rstate_sampler PointSampler;

        xbool isHistoryValid;
    };

    //-------------------------------------------------------------------------
    //  INTERNAL EFFECT EXECUTION
    //-------------------------------------------------------------------------

    xbool ExecuteMotionBlur( const rtarget* pHistory );
    xbool ExecuteRadialBlur( const rtarget* pSource, const rtarget* pBlurSource, const RenderFrameContext& FrameData );
    xbool ExecuteScreenWarps( const rtarget*            pSource,
                              const rtarget*            pSceneDepth,
                              const RenderFrameContext& FrameData,
                              const RenderView*         pRenderView,
                              const rtarget*            pMask0,
                              const rtarget*            pMask1 );
    xbool ExecuteZFogFilter( const rtarget* pSceneDepth );
    xbool ExecuteMipComposite( const rtarget* pMipTarget, const rtarget* pSceneColor, const rtarget* pSceneDepth );
    xbool ExecuteNoiseFilter( void );
    static xbool ExecuteScreenFadePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteScreenFadeCapture( RenderPassContext& Context, PostProcessFeature* pPost );

    static xbool ExecuteFogPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteMipDownsamplePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteRadialMipPass( RenderPassContext& Context, PostProcessFeature* pPost );
    xbool        DeclarePainFilterPasses( FrameGraphBuilder& Graph, const RenderFrameContext& FrameData );
    static xbool ExecutePainDownsamplePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecutePainHorizontalPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecutePainVerticalPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteMipCompositePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteMotionPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteGlowDownsamplePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteGlowBlurPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteGlowCombinePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteGlowHistoryPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteBloomCompositePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteWarpSourcePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteRadialPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteWarpMaskPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteWarpMaskBlurPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteWarpPass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteResolvePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteNoisePass( RenderPassContext& Context, PostProcessFeature* pPost );
    static xbool ExecuteHistoryPass( RenderPassContext& Context, PostProcessFeature* pPost );

    //-------------------------------------------------------------------------
    //  PER-FRAME MAINTENANCE
    //-------------------------------------------------------------------------

    void  UpdateGlowStageBegin( void );
    xbool DeclareFilterHistoryPass( FrameGraphBuilder& Graph, const RenderFrameContext& FrameData );
    xbool DeclareMipFilterPasses( FrameGraphBuilder& Graph, const RenderFrameContext& FrameData );
    xbool DeclareWarpPasses( FrameGraphBuilder& Graph, const RenderFrameContext& FrameData );
    void  InvalidateTemporalHistory( void );
    void  ResetFrameRequests( void );

    //-------------------------------------------------------------------------
    //  INTERNAL HELPERS
    //-------------------------------------------------------------------------

    void                       BuildFogPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 );
    void                       UpdatePostView( const RenderFrameContext& FrameData );
    const PostFilterConstants* GetScreenWarpConstants( const RenderFrameContext& Frame,
                                                       const RenderView*         pView,
                                                       const rtarget&            Source );
    xbool                      AcceptRequest( const char* pName ) const;
    void BuildMipPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex );
    RenderResourceHandle CreateFilterTarget( FrameGraphBuilder&  Graph,
                                             const RenderExtent& ExtentData,
                                             rtarget_format      FormatData,
                                             const char*         pDebugName );

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool PrepareTemporalHistory( u64                       Key,
                                  const RenderExtent&       Extent,
                                  rtarget_format            Format,
                                  const RenderFrameContext& Frame );
    void MarkTemporalHistoryValid( u64                       Key,
                                   const RenderExtent&       Extent,
                                   rtarget_format            Format,
                                   const RenderFrameContext& Frame );
    void  InvalidateTemporalHistoryKey( u64 Key );

protected:

    //-------------------------------------------------------------------------
    //  FEATURE STATE
    //-------------------------------------------------------------------------

    xbool       m_isInitialized;
    BloomEffect m_Bloom;
    xbool       m_isRequestsSealed;
    xbool       m_hasMotionBlur;
    xbool       m_hasForcedGlow;
    f32         m_FrameDeltaTime;

    // View and depth state for the current pass.
    s32 m_PostViewL, m_PostViewT, m_PostViewR, m_PostViewB;
    f32 m_PostNearZ, m_PostFarZ;

    // Requests are sealed before graph execution.
    PostChainRequests    m_Requests;
    PostMotionBlurParams m_MotionBlur;
    PostGlowParams       m_Glow;
    PostRadialBlurParams m_RadialBlur;
    PostScreenWarpParams m_ScreenWarp;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<PreparedWarpView> m_PreparedWarpViews;
    PostFogFilterParams      m_FogFilter;
    PostMipFilterParams      m_MipFilter;
    PostMipFilterParams      m_PainFilter;
    SceneColorGradeParams    m_SceneColorGrade;

    const xbitmap*   m_pMipTexture;
    PostSimpleParams m_Simple;

    //-------------------------------------------------------------------------
    //  FOG RESOURCES
    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xbool               m_isXboxFogEnabled;
    xbool               m_isFogValid[5];
    u8                  m_FogSourcePalette[5][64 * 4];
    u8                  m_FogPalette[5][256 * 4];
    vector4             m_FogColor[5];
    vector4             m_FogConst[5];
    f32                 m_FogStart[5];
    FogResources        m_FogResources;
    PostFogFilterParams m_GeneratedFog;
    f32                 m_GeneratedFogNearZ;
    f32                 m_GeneratedFogFarZ;
    xbool               m_isGeneratedFogValid;

    //-------------------------------------------------------------------------
    //  GLOW RESOURCES
    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    GlowResources        m_GlowResources;
    RenderResourceHandle m_GlowDownsample[3];
    RenderResourceHandle m_GlowBlur[2];
    RenderResourceHandle m_GlowCombined;
    RenderResourceHandle m_GlowHistory;
    RenderResourceHandle m_GlowResult;
    f32                  m_GlowHistoryRetention;
    f32                  m_GlowCurrentWeight;
    xbool                m_shouldStoreGlowHistory;

    //-------------------------------------------------------------------------
    //  FILTER RESOURCES
    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    FilterResources      m_FilterResources;
    RenderResourceHandle m_FilterPost[2];
    RenderResourceHandle m_FilterMip[MAX_POST_MIPS];
    RenderResourceHandle m_FilterRadialMip;
    RenderResourceHandle m_FilterPain;
    RenderResourceHandle m_FilterPainScratch;
    RenderResourceHandle m_FilterMipSource;
    RenderResourceHandle m_FilterWarpSource;
    RenderResourceHandle m_FilterWarpResult;
    RenderResourceHandle m_WarpMaskRaw[2];
    RenderResourceHandle m_WarpMaskScratch[2];
    RenderResourceHandle m_WarpMask[2];
    RenderResourceHandle m_ScreenFadeSource;
    s32                  m_nFilterMips;

    //-------------------------------------------------------------------------
    RenderResourceHandle         m_FilterHistoryHandle;
    u64                          m_FilterHistoryKey;
    u64                          m_GlowHistoryKey;
    xarray<TemporalHistoryState> m_TemporalHistories;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
PostGlowParams::PostGlowParams()
    : MotionBlurIntensity( 0.0f ),
      Cutoff( 0 )
{
}

//=========================================================================

inline
PostMotionBlurParams::PostMotionBlurParams()
    : Intensity( 0.0f )
{
}

//=========================================================================

inline
PostRadialBlurParams::PostRadialBlurParams()
    : Zoom( 1.0f ),
      Angle( 0.0f ),
      AlphaSub( 0.0f ),
      AlphaScale( 1.0f )
{
}

//=========================================================================

inline
PostScreenWarpParams::PostScreenWarpParams()
    : Count( 0 )
{
}

//=========================================================================

inline
PostFogFilterParams::PostFogFilterParams()
    : PaletteIndex( -1 ),
      GeneratedColor( 0, 0, 0, 0 ),
      GeneratedParam1( 0.0f ),
      GeneratedParam2( 0.0f )
{
    for( s32 i = 0; i < 5; i++ )
    {
        Fn[i] = render::FALLOFF_CONSTANT;
    }
}

//=========================================================================

inline
PostMipFilterParams::PostMipFilterParams()
    : PaletteIndex( -1 )
{
    for( s32 i = 0; i < 4; i++ )
    {
        Fn[i]     = render::FALLOFF_CONSTANT;
        Color[i]  = xcolor( 255, 255, 255, 255 );
        Param1[i] = 0.0f;
        Param2[i] = 0.0f;
        Count[i]  = 0;
        Offset[i] = 0.0f;
    }
}

//=========================================================================

inline
PostSimpleParams::PostSimpleParams()
    : NoiseColor( 255, 255, 255, 255 ),
      FadeColor( 0, 0, 0, 0 )
{
}

//=========================================================================

inline
PostChainRequests::PostChainRequests()
{
    x_memset( this, 0, sizeof( PostChainRequests ) );
}

//=========================================================================

inline
const char* PostProcessFeature::GetName( void ) const
{
    return "PostProcess";
}

//=========================================================================

inline
u32 PostProcessFeature::GetDependencyCount( void ) const
{
    return 1;
}

//=========================================================================

inline
const char* PostProcessFeature::GetDependency( u32 Index ) const
{
    return ( Index == 0 ) ? "Geometry" : NULL;
}

//=========================================================================

inline
u32 PostProcessFeature::GetGraphStageMask( void ) const
{
    return RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_FOG ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_POST_PROCESS ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_FILM_GRAIN ) |
           RenderFeatureGraphStageMask( RENDER_FEATURE_STAGE_SCREEN_FADE );
}

//=========================================================================

inline
BloomEffect& PostProcessFeature::GetBloomFeature( void )
{
    return m_Bloom;
}

//=========================================================================

inline
xbool PostProcessFeature::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
u32 PostProcessFeature::GetSceneTextureRequirements( void ) const
{
    return SCENE_TEXTURE_DEPTH;
}

//=========================================================================

inline
void PostProcessFeature::SetFrameHasForcedGlow( xbool HasForcedGlow )
{
    m_hasForcedGlow = HasForcedGlow;
}

//=========================================================================

inline
void PostProcessFeature::SetXboxFogEnabled( xbool Enabled )
{
    m_isXboxFogEnabled = Enabled;
}

//=========================================================================

inline
PostProcessFeature::TemporalHistoryState::TemporalHistoryState( void )
    : Key( 0 ),
      Width( 0 ),
      Height( 0 ),
      Format( RTARGET_FORMAT_COUNT ),
      Valid( FALSE )
{
}

} // namespace render

#endif // POST_PROCESS_FEATURE_HPP
