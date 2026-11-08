//=========================================================================
//
//  FrameCompiler.hpp
//
//  Geometry submission and draw backend.
//
//=========================================================================

#ifndef FRAME_COMPILER_HPP
#define FRAME_COMPILER_HPP

//=========================================================================
//  BASE INCLUDES
//=========================================================================

#include "x_types.hpp"
#include "x_array.hpp"
#include "a51_shader_contract.hpp"

//=========================================================================
//  INCLUDES
//=========================================================================

#include "../../Material.hpp"
#include "../../GeomDraw.hpp"
#include "../../RenderLighting.hpp"
#include "../../LightSystem.hpp"
#include "../../GraphicsPipelineCache.hpp"
#include "../../ProjectionTextureRegistry.hpp"
#include "../../Render.hpp"
#include "../../SkinGeom.hpp"
#include "../../Texture.hpp"

#include "../../Features/Shadow/ShadowRenderer.hpp"
#include "../Resource/ProjectionAtlas.hpp"
#include "../Storage/StaticGeometryStorage.hpp"
#include "../Storage/SkinnedGeometryStorage.hpp"
#include "../FrameStorage.hpp"
#include "../../FrameGraph/FrameGraph.hpp"
#include "../RenderBackendContext.hpp"

#include "e_Engine.hpp"

namespace render
{

//=========================================================================
//  CONSTANTS
//=========================================================================

enum GeomInstFlagBits
{
    GEOM_INST_FLAG_GLOWING              = ( 1u << 20 ),
    GEOM_INST_FLAG_RECEIVE_LOCAL_SHADOW = ( 1u << 21 ),
    GEOM_INST_FLAG_FILTER_LIGHT         = ( 1u << 22 ),
    GEOM_INST_FLAG_PROJ_LIGHT           = ( 1u << 23 ),
    GEOM_INST_FLAG_PROJ_SHADOW          = ( 1u << 27 ),
};

//=========================================================================
//  CONSTANT BUFFER STRUCTURES
//=========================================================================

//-------------------------------------------------------------------------

struct SkinDrawInstance
{
    u32 InstanceIndex;
    u32 BoneRemapOffset;
};

static_assert( sizeof( SkinDrawInstance ) == 8, "Skin draw-instance vertex layout must match HLSL" );

//=========================================================================
//  BATCH DESCRIPTOR STRUCTURES
//=========================================================================

enum geom_depth_mode
{
    GEOM_DEPTH_DEFAULT,
    GEOM_DEPTH_PREPASS,
    GEOM_DEPTH_READ_ONLY
};

//-------------------------------------------------------------------------

struct geom_pass_desc
{
    // Attachment dimensions for screen-space shader constants.
    u32               TargetWidth;
    u32               TargetHeight;
    render_fog_mode   SceneFogMode;
    render_fog_mode   BloomFogMode;
    const RenderView* pView;
    rtarget_format    ColorFormat;
    rtarget_format    DepthFormat;
    u32               SampleCount;
    geom_depth_mode   DepthMode;
    xbool             WriteSceneDepth;
    xbool             WriteGlowMask;
    xbool             WriteNormals;
    xbool             EnableShadows;

    geom_pass_desc( void );
};

//-------------------------------------------------------------------------

struct RigidBatchDesc
{
    const rigid_geom*                pGeom;
    const matrix4*                   pL2W;
    RenderLightingHandle             Lighting;
    const u32*                       pColorInfo;
    xhandle                          hMesh;
    s32                              iSubMesh;
    u32                              RenderFlags;
    render::geom_inst_features       InstFeatures;
    u8                               UOffset;
    u8                               VOffset;
    u8                               Alpha;
    u8                               OverrideMat;
    f32                              SortDepth;
    render::RenderContributionPolicy ContributionPolicy;
};

//-------------------------------------------------------------------------

struct SkinBatchDesc
{
    const skin_geom*                 pGeom;
    const matrix4*                   pBones;
    RenderLightingHandle             Lighting;
    xhandle                          hMesh;
    s32                              iSubMesh;
    u32                              RenderFlags;
    render::geom_inst_features       InstFeatures;
    u8                               UOffset;
    u8                               VOffset;
    u8                               Alpha;
    u8                               OverrideMat;
    f32                              SortDepth;
    render::RenderContributionPolicy ContributionPolicy;
};

//-------------------------------------------------------------------------

struct PacketMetadata
{
    s32                              PacketIndex;
    geom_render_pass                 Pass;
    f32                              SortDepth;
    u32                              Sequence;
    render::RenderContributionPolicy ContributionPolicy;

    PacketMetadata( void );
};

//=========================================================================
//  TEXTURE SLOTS
//=========================================================================

enum TextureSlot
{
    TEXTURE_SLOT_DIFFUSE          = 0,
    TEXTURE_SLOT_DETAIL           = 1,
    TEXTURE_SLOT_ENVIRONMENT      = 2,
    TEXTURE_SLOT_ENVIRONMENT_CUBE = 3,
};

//=========================================================================
//  FRAME COMPILER CLASS
//=========================================================================

class FrameCompiler
{
public:

    //-------------------------------------------------------------------------
    //  LIFETIME
    //-------------------------------------------------------------------------

    FrameCompiler( void );
    void  Init( const RenderBackendContext& Context );
    void  SetContext( const RenderBackendContext& Context );
    void  Kill( void );
    xbool IsInitialized( void ) const;

    //-------------------------------------------------------------------------
    //  FRAME PREPARATION
    //-------------------------------------------------------------------------

    xbool PrepareOrderedDraws( const xarray<geom_draw_item>& Draws );
    void  WaitForOrderedDraws( void );
    xbool BuildPackets( const xarray<geom_draw_item>&    Draws,
                        const xarray<dynamic_geom_draw>& DynamicDraws,
                        const dynamic_geom_storage&      DynamicStorage,
                        const cubemap*                   pCubeMap,
                        const RenderFrameContext&        Frame );
    void  BeginFrameStorage( void );
    xbool QueueMaterialPipelines( const material& Material, xbool Skin, const geom_pass_desc& Pass );
    xbool QueueDefaultMaterialPipelines( const geom_pass_desc& Pass );
    xbool QueueDynamicPipeline( const geom_pass_desc& Pass, u32 RenderFlags = 0 );
    void  ReleaseLevelPipelines( void );

protected:

    //-------------------------------------------------------------------------
    //  MATERIAL BINDING
    //-------------------------------------------------------------------------

    xbool SetRigidMaterial( const material*       pMaterial,
                            u32                   RenderFlags,
                            u8                    UOffset,
                            u8                    VOffset,
                            u8                    OverrideMat,
                            xbool                 WriteGlowAlpha,
                            const geom_pass_desc& PassData,
                            LocalLightingMode     Lighting,
                            xbool                 Projection,
                            s32                   PacketIndex );
    xbool SetSkinMaterial( const material*       pMaterial,
                           u32                   RenderFlags,
                           u8                    UOffset,
                           u8                    VOffset,
                           u8                    OverrideMat,
                           xbool                 WriteGlowAlpha,
                           const geom_pass_desc& PassData,
                           LocalLightingMode     Lighting,
                           xbool                 Projection,
                           s32                   PacketIndex );

    //-------------------------------------------------------------------------
    //  PROJECTION AND SHADOW STATE
    //-------------------------------------------------------------------------

    xbool ResetProjTextures( void );
    void  SetDistortionState( const radian3& NormalRot );
    void  ResetTransientState( void );

protected:

    //-------------------------------------------------------------------------
    //  TEXTURE BINDING AND INSTANCE DATA
    //-------------------------------------------------------------------------

    void SetBitmap( const texture* pTexture, TextureSlot SlotIndex );
    void SetEnvironmentCubemap( const cubemap* pCubemap );
    void BeginPacketCollection( void );
    void ResetShadowTargets( void );

public:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool                  PrepareProjectionAtlas( FrameGraphBuilder& Graph );
    xbool                  PrepareSharedShadowData( FrameGraphBuilder& Graph );
    xbool                  ApplyPassInputs( const BackendPassInputs& Inputs );
    xbool                  UploadPackets( FrameGraphBuilder& Graph );
    u32                    GetDynamicLightShadowIndex( s32 DynamicLightIndex ) const;
    const shader_resource* GetFaceShadowResource( void ) const;
    const shader_resource* GetFaceShadowDepthResource( void ) const;
    const shader_resource* GetShadowMatricesResource( void ) const;
    const shader_resource* GetShadowDataResource( void ) const;
    rstate_sampler_preset  GetFaceShadowSampler( void ) const;
    s32                    GetPacketCount( void ) const;
    void                   DeclarePacketResources( FrameGraphBuilder& Graph, RenderPassId Pass, s32 PacketIndex ) const;
    void                   DeclarePassResources( FrameGraphBuilder& Graph, RenderPassId Pass ) const;
    s32                    GetRigidInstanceCount( void ) const;
    s32                    GetSkinInstanceCount( void ) const;
    s32                    GetLitInstanceCount( void ) const;
    s32                    GetInstanceLightCount( void ) const;
    s32                    GetLightingRecordCount( void ) const;

    //-------------------------------------------------------------------------
    //  DRAW STATISTICS
    //-------------------------------------------------------------------------

    u32 GetForwardOpaqueGpuDrawCount( void ) const;
    u32 GetForwardOpaqueInstanceCount( void ) const;
    u32 GetForwardOpaqueSkinSectionDrawCount( void ) const;
    u64 GetForwardOpaqueSubmittedIndexCount( void ) const;
    u32 GetForwardOpaqueRigidIndirectRunCount( void ) const;
    u32 GetForwardOpaqueRigidIndirectCommandCount( void ) const;
    u32 GetForwardOpaqueSkinIndirectRunCount( void ) const;
    u32 GetForwardOpaqueSkinIndirectCommandCount( void ) const;
    s32 GetOpaquePacketCount( void ) const;
    s32 GetOrderedPacketCount( void ) const;

    //-------------------------------------------------------------------------
    //  PACKET ACCESS AND EXECUTION
    //-------------------------------------------------------------------------

    geom_render_pass           GetPacketPass( s32 PacketIndex ) const;
    xbool                      IsFirstPersonPacket( s32 PacketIndex ) const;
    xbool                      IsFirstPersonDepthPacket( s32 PacketIndex ) const;
    u32                        GetPacketSequence( s32 PacketIndex ) const;
    xbool                      PacketReadsSceneColor( s32 PacketIndex ) const;
    const GraphicsPipelineKey& GetPacketPipelineKey( s32 PacketIndex ) const;
    f32                        GetPacketSortDepth( s32 PacketIndex ) const;
    xbool                      IsPacketGlowCapable( s32 PacketIndex ) const;
    xbool                      CanBatchForwardPackets( s32 FirstPacket, s32 NextPacket ) const;
    void                       BeginForwardBatches( void );
    s32                        AppendForwardCommands( s32 PacketIndex );
    u32                        GetForwardCommandCount( void ) const;
    xbool                      UploadForwardCommands( FrameGraphBuilder& Graph );
    xbool ExecutePacket( s32                      PacketIndex,
                         const geom_pass_desc&    PassData,
                         const BackendPassInputs& Inputs,
                         u32                      FirstCommand = 0,
                         u32                      CommandCount = 0 );
    xbool                      PreparePassConstants( const RenderView* pView, u32 Width, u32 Height );
    xbool                      ExecuteForwardOpaque( const geom_pass_desc& PassData, const BackendPassInputs& Inputs );
    void                       ClearPackets( void );

protected:

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    xbool HasRigidBatch( void ) const;
    xbool CanAppendRigidBatch( const RigidBatchDesc& Desc ) const;
    void  AddRigidBatchInst( const RigidBatchDesc& Desc );
    void  FlushRigidBatch( const material* pMaterial, u8 MaterialOverride, geom_render_pass PassData, u32 Sequence );
    xbool HasSkinBatch( void ) const;
    xbool CanAppendSkinBatch( const SkinBatchDesc& Desc ) const;
    void  AddSkinBatchInst( const SkinBatchDesc& Desc );
    void  FlushSkinBatch( const material* pMaterial, u8 MaterialOverride, geom_render_pass PassData, u32 Sequence );

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct RenderStateSelection
    {
        rstate_blend_preset   Blend;
        rstate_depth_preset   Depth;
        rstate_raster_preset  Raster;
        rstate_sampler_preset Sampler;
        xbool                 WriteSceneDepth;
        ShaderProgramKey      Program;
        rtarget_format        ColorFormat;
        rtarget_format        DepthFormat;
        u32                   SampleCount;

        RenderStateSelection( void );
    };

    //-------------------------------------------------------------------------

    enum GeomShaderKind
    {
        GEOM_SHADER_NONE = -1,
        GEOM_SHADER_RIGID,
        GEOM_SHADER_SKIN,
        GEOM_SHADER_DYNAMIC,
        GEOM_SHADER_COUNT
    };

    //-------------------------------------------------------------------------

    enum GeomPacketKind
    {
        GEOM_PACKET_RIGID = 0,
        GEOM_PACKET_SKIN,
        GEOM_PACKET_DYNAMIC
    };

    //-------------------------------------------------------------------------

    struct OrderedDrawJob
    {
        const geom_draw_item*  pDraws;
        const geom_draw_item** pOrderedDraws;
        s32                    DrawCount;
    };

    //-------------------------------------------------------------------------

    struct SkinPose
    {
        const matrix4* pBones;
        s32            BoneCount;
        u32            FirstBone;

        SkinPose( void );
    };

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    struct SkinRemapCacheEntry
    {
        s32                              MeshHandle;
        u32                              FirstRemap;
        SkinnedGeometryStorage::MeshView View;

        SkinRemapCacheEntry( void );
    };

    //-------------------------------------------------------------------------

    struct GeomResourceSnapshot
    {
        const shader_resource* pDiffuse;
        const shader_resource* pDetail;
        const shader_resource* pEnvironment;
        const shader_resource* pEnvironmentCube;
        xbool                  isDistortionActive;
        radian3                DistortionNormalRot;

        GeomResourceSnapshot( void );
    };

    //-------------------------------------------------------------------------

    struct GeomDrawPacket
    {
        GeomPacketKind                             Kind;
        const material*                            pMaterial;
        xhandle                                    hMesh;
        u32                                        RenderFlags;
        LocalLightingMode                          LightingMode;
        xbool                                      Projection;
        xbool                                      Shadows;
        render::geom_inst_features                 InstFeatures;
        render::RenderContributionPolicy           ContributionPolicy;
        u32                                        FirstInstance;
        u32                                        InstanceCount;
        u8                                         UOffset;
        u8                                         VOffset;
        u8                                         MaterialOverride;
        geom_render_pass                           Pass;
        u32                                        Sequence;
        f32                                        SortDepth;
        u32                                        FirstSkinDrawInstance;
        const SkinnedGeometryStorage::SoftSection* pSkinSections;
        s32                                        SkinSectionCount;
        u32                                        FirstDynamicVertex;
        const dynamic_geom_gpu_mesh*               pDynamicGpuMesh;
        u32                                        FirstDynamicIndex;
        u32                                        DynamicIndexCount;
        u32                                        DynamicLightingIndex;
        xbool                                      CoverageUsesTextureAlpha;
        const shader_resource*                     pDamageMask;
        vram_texture*                              pDamageTexture;
        const u8*                                  pDamageUpload;
        xbool*                                     pDamageUploadPending;
        s32                                        DamageUploadX;
        s32                                        DamageUploadY;
        s32                                        DamageUploadWidth;
        s32                                        DamageUploadHeight;
        GeomResourceSnapshot                       Resources;
        ShaderProgramKey                           SceneProgram;
        s32                                        ScenePipelineIndex;
        render_pipeline*                           pDepthPipeline;
        u64                                        NativePipelineSerial;
        u32                                        SubmissionIndex;
        StaticGeometryStorage::mesh_range          Range;

        GeomDrawPacket( void );
    };

    //-------------------------------------------------------------------------

    struct SkinIndirectRun
    {
        u32 FirstPacket;
        u32 FirstCommand;
        u32 CommandCount;
        s32 VertexPool;
        s32 IndexPool;

        SkinIndirectRun( void );
    };

    //-------------------------------------------------------------------------

    struct RigidIndirectRun
    {
        u32 FirstPacket;
        u32 LastPacket;
        u32 FirstCommand;
        u32 CommandCount;
        s32 VertexPool;
        s32 IndexPool;

        RigidIndirectRun( void );
    };

    //-------------------------------------------------------------------------

    struct ShaderBindingLayout
    {
        u32 FrameConstantsVertex;
        u32 FrameConstantsPixel;
        u32 DynamicConstantsVertex;
        u32 DamageTexturePixel;
        u32 FogConstantsPixel;
        u32 FogPalettePixel;
        u32 ProjTexturesPixel;
        u32 ShadowMatricesPixel;
        u32 ShadowDataPixel;
        u32 DiffuseTexturePixel;
        u32 DetailTexturePixel;
        u32 EnvironmentTexturePixel;
        u32 EnvironmentCubeTexturePixel;
        u32 FaceShadowTexturePixel;
        u32 FaceShadowDepthTexturePixel;
        u32 DistortionTexturePixel;
        u32 ProjectionAtlasTexturePixel;
        u32 AmbientVisibilityTexturePixel;
        u32 InstanceDataVertex;
        u32 AuxiliaryDataVertex;
        u32 LightingDataPixel;
        u32 ProjectionLightingDataPixel;
        u32 AmbientDataPixel;
        u32 BoneRemapDataVertex;

        ShaderBindingLayout( void );
    };

    //-------------------------------------------------------------------------

    struct SurfaceStage
    {
        const char* pFile;
        shader      Shader;
    };

    //-------------------------------------------------------------------------

    struct SurfaceProgram
    {
        SurfaceStage*       pVertex;
        SurfaceStage*       pPixel;
        ShaderBindingLayout Bindings;
    };

    struct ScenePipelineRequirement
    {
        GraphicsPipelineKey  Key;
        u64                  Hash;
        u64                  NativeSerial;
        RenderStateSelection MaterialState;
        render_pipeline*     pPipeline;
        ScenePipelineRequirement( void );
    };

    //-------------------------------------------------------------------------
    //  FUNCTIONS
    //-------------------------------------------------------------------------

    SurfaceProgram* GetSurfaceProgram( const ShaderProgramKey& Key, xbool IsPrewarm = FALSE );
    SurfaceStage* LoadSurfaceStage( const char*  pFile,
                                    u64          Key,
                                    u64          ProgramId,
                                    shader_stage ShaderStage,
                                    xbool        IsPrewarm );
    void            InitSurfacePrograms( void );
    void            DestroySurfacePrograms( void );

    //-------------------------------------------------------------------------
    //  RENDER STATE HELPERS
    //-------------------------------------------------------------------------

    RenderStateSelection ResolveRenderStates( const material* pMaterial,
                                              u32             RenderFlags,
                                              u8              OverrideMat,
                                              xbool           WriteGlowAlpha ) const;
    RenderStateSelection BuildMaterialPipelineState( const material*       pMaterial,
                                                     u32                   RenderFlags,
                                                     u8                    OverrideMat,
                                                     xbool                 WriteGlowAlpha,
                                                     const geom_pass_desc& Pass,
                                                     LocalLightingMode     Lighting,
                                                     xbool                 Projection ) const;
    xbool BuildGeometryPipelineDesc( render_pipeline_desc&       Desc,
                                     const RenderStateSelection& State,
                                     VertexContract              Vertex,
                                     xbool                       IsPrewarm = FALSE );
    render_pipeline*     GetRigidPipeline( const RenderStateSelection& PipelineState, xbool isPrewarm = FALSE );
    render_pipeline*     GetSkinPipeline( const RenderStateSelection& PipelineState, xbool isPrewarm = FALSE );
    xbool                BindRigidPipeline( const RenderStateSelection& PipelineState, render_pipeline* pPrepared );
    xbool                BindSkinPipeline( const RenderStateSelection& PipelineState, render_pipeline* pPrepared );
    void                 DestroyRigidPipelines( void );
    void                 DestroySkinPipelines( void );
    render_pipeline*     FindPreparedMaterialPipeline( s32 PacketIndex, const RenderStateSelection& State ) const;

    //-------------------------------------------------------------------------
    //  RESOURCE INITIALIZATION
    //-------------------------------------------------------------------------

    xbool InitRigidShaders( void );
    void  KillRigidShaders( void );
    xbool InitSkinShaders( void );
    void  KillSkinShaders( void );
    xbool InitDynamicGeom( void );
    void  KillDynamicGeom( void );
    xbool InitShaderBindings( void );
    void  KillShaderBindings( void );
    xbool ResolveShaderBindings( ShaderBindingLayout& Bindings,
                                 const shader&        VertexShader,
                                 const shader&        PixelShader,
                                 A51ResourceSemantic  Instance,
                                 A51ResourceSemantic  Auxiliary,
                                 A51ResourceSemantic  Remap );
    xbool InitShadowMaps( void );
    void  KillShadowMaps( void );
    xbool InitSamplers( void );
    void  KillSamplers( void );

    //-------------------------------------------------------------------------
    //  INTERNAL HELPERS
    //-------------------------------------------------------------------------

    xbool UpdateRigidConstants( const material*       pMaterial,
                                u8                    UOffset,
                                u8                    VOffset,
                                u8                    OverrideMat,
                                const geom_pass_desc& PassData );
    xbool UpdateSkinConstants( const material*       pMaterial,
                               u8                    UOffset,
                               u8                    VOffset,
                               u8                    OverrideMat,
                               const geom_pass_desc& PassData );
    xbool      UpdateProjTextures( void );
    s32        FindSkinPoseSlot( const matrix4* pBones, s32 BoneCount ) const;
    xbool      BuildShadowMapConstants( FrameGraphBuilder& Graph );
    static u32 BuildBatchStateFlags( u32 RenderFlags );
    static u32 BuildInstFlags( render::geom_inst_features InstFeatures );
    static f32 BuildInstFadeAlpha( u32 RenderFlags, u8 Alpha );
    xbool      ReserveFrameArrays( const xarray<geom_draw_item>& Draws, const xarray<dynamic_geom_draw>& DynamicDraws );
    xbool      BuildLightingData( void );
    xbool      ReserveUploadBuffers( void );
    xbool      ResolveLightingIndex( const RenderLightingHandle& Lighting, u32& OutIndex );
    xbool      RequestLightCookies( const xarray<RenderLightingHandle>& Lighting );
    xbool CopyLightingData( GeomLightingConstants&           Destination,
                            GeomProjectionLightingConstants& Projection,
                            GeomAmbientConstants&            Ambient,
                            const cb_geom_lighting&          Source ) const;
    const ShaderBindingLayout* GetShaderBindings( GeomShaderKind Kind ) const;
    const rstate_sampler*      GetSampler( rstate_sampler_preset Preset ) const;
    xbool                      PushFrameConstants( GeomShaderKind Kind, const GeomFrameConstants& Frame ) const;
    static vector4             BuildFilterLightColor( void );
    xbool                      PushFogConstants( GeomShaderKind Kind, const geom_pass_desc& PassData ) const;
    xbool    BindPixelTexture( u32 SlotIndex, const shader_resource* pResource, const rstate_sampler* pSampler ) const;
    xbool    BindPacketResources( const GeomDrawPacket& Packet, xbool DepthOnly );
    xbool    BindRigidFrameBuffers( xbool DepthOnly ) const;
    xbool    BindRigidGeomBuffers( const StaticGeometryStorage::mesh_range& Range ) const;
    xbool    BindSkinFrameBuffers( xbool DepthOnly ) const;
    xbool    BindSkinGeomBuffers( const GeomDrawPacket& Packet ) const;
    xbool    BuildDynamicPackets( const xarray<dynamic_geom_draw>& Draws );
    xbool    ReserveDynamicBuffers( void );
    xbool    UploadDynamicGeom( FrameGraphBuilder& Graph );
    rbuffer& GetFrameBuffer( FrameStorage::Stream StreamId ) const;
    void     CommitFrameBufferCapacities( void ) const;
    xbool    ExecuteDynamicPacket( const GeomDrawPacket& Packet, const geom_pass_desc& PassData );
    render_pipeline* GetDynamicPipeline( const RenderStateSelection& State, xbool IsPrewarm = FALSE );
    RenderStateSelection BuildDynamicPipelineState( u32                   Flags,
                                                    const geom_pass_desc& Pass,
                                                    LocalLightingMode     Lighting,
                                                    xbool                 Projection ) const;
    xbool BuildDynamicPipelineDesc( render_pipeline_desc&       Desc,
                                    const RenderStateSelection& State,
                                    xbool                       IsPrewarm = FALSE );
    ShaderProgramKey BuildDynamicProgram( u32                   RenderFlags,
                                          const geom_pass_desc& Pass,
                                          LocalLightingMode     Lighting,
                                          xbool                 Projection ) const;
    GeomResourceSnapshot CaptureResourceSnapshot( void ) const;
    void ConfigureDrawResources( const material*  pMaterial,
                                 geom_render_pass PassData,
                                 const radian3&   DistortionNormalRot,
                                 const cubemap*   pCubeMap );
    static void          BuildOrderedDraws( void* pData );
    static s32           CompareForwardOpaqueDraws( const void* pA, const void* pB );
    static s32           CompareScenePackets( const void* pA, const void* pB );
    xbool                BuildRigidIndirectRuns( void );
    xbool CanAppendRigidIndirectRun( const RigidIndirectRun& Run, u32 PacketIndex, const GeomDrawPacket& Packet ) const;
    xbool ExecuteRigidIndirectRun( const RigidIndirectRun& Run, const geom_pass_desc& PassData );
    xbool BuildSkinDrawData( void );
    xbool CanAppendSkinIndirectRun( const SkinIndirectRun& Run, u32 PacketIndex, const GeomDrawPacket& Packet ) const;
    xbool ExecuteSkinIndirectRun( const SkinIndirectRun& Run, const geom_pass_desc& PassData );
    void  ResetRigidBatch( void );
    void  ResetSkinBatch( void );
    GeomFrameConstants BuildFrameConstants( const material*       pMaterial,
                                            u8                    UOffset,
                                            u8                    VOffset,
                                            u8                    OverrideMat,
                                            const geom_pass_desc& PassData ) const;

protected:

    //-------------------------------------------------------------------------
    //  RENDERER STATE
    //-------------------------------------------------------------------------

    GeomFrameConstants          m_PassFrameConstants;
    RenderFogConstants          m_PassFogConstants;
    xbool                       m_isInitialized;
    const RenderBackendContext* m_pContext;
    GraphicsPipelineCache       m_RigidPipelines;
    GraphicsPipelineCache       m_SkinPipelines;

    //-------------------------------------------------------------------------
    //  RIGID GEOMETRY RESOURCES
    //-------------------------------------------------------------------------

    xarray<u32>                      m_RigidInstanceIndices;
    u32                              m_RigidInstanceCapacity;
    u32                              m_RigidInstanceIndexCapacity;
    u32                              m_RigidColorCapacity;
    u32                              m_RigidIndirectCapacity;
    xarray<RigidInstanceData>        m_lRigidFrameInstances;
    xarray<RenderLightingHandle>     m_lRigidFrameLighting;
    xarray<u32>                      m_lRigidFrameColors;
    u32                              m_RigidBatchFirstInstance;
    xhandle                          m_hRigidBatchMesh;
    u32                              m_RigidBatchFlags;
    xbool                            m_isRigidBatchForcedGlow;
    u8                               m_RigidBatchUOffset;
    u8                               m_RigidBatchVOffset;
    u8                               m_RigidBatchOverrideMat;
    f32                              m_RigidBatchSortDepth;
    render::RenderContributionPolicy m_RigidBatchContributionPolicy;

    //-------------------------------------------------------------------------
    //  SKIN GEOMETRY RESOURCES
    //-------------------------------------------------------------------------

    u32                                    m_SkinInstanceCapacity;
    u32                                    m_SkinBoneCapacity;
    u32                                    m_SkinDrawInstanceCapacity;
    u32                                    m_SkinIndirectCapacity;
    xarray<SkinInstanceData>               m_lSkinFrameInstances;
    xarray<RenderLightingHandle>           m_lSkinFrameLighting;
    xarray<matrix4>                        m_lSkinFrameBones;
    xarray<SkinPose>                       m_SkinPoses;
    xarray<s32>                            m_SkinPoseSlots;
    xarray<SkinDrawInstance>               m_lSkinDrawInstances;
    xarray<rdraw_indexed_indirect_command> m_lSkinIndirectCommands;
    xarray<SkinIndirectRun>                m_lSkinIndirectRuns;
    xarray<SkinRemapCacheEntry>            m_lSkinRemapCache;
    xarray<s32>                            m_lSkinRemapSlots;
    u32                                    m_SkinBatchFirstInstance;
    xhandle                                m_hSkinBatchMesh;
    u32                                    m_SkinBatchFlags;
    xbool                                  m_isSkinBatchForcedGlow;
    render::RenderContributionPolicy       m_SkinBatchContributionPolicy;
    u8                                     m_SkinBatchUOffset;
    u8                                     m_SkinBatchVOffset;
    u8                                     m_SkinBatchOverrideMat;
    f32                                    m_SkinBatchSortDepth;
    u32                                    m_nLitInstances;
    u32                                    m_nInstanceLights;
    u32                                    m_nForwardOpaqueGpuDraws;
    u32                                    m_nForwardOpaqueInstances;
    u32                                    m_nForwardOpaqueSkinSectionDraws;
    u64                                    m_nForwardOpaqueSubmittedIndices;
    xarray<rdraw_indexed_indirect_command> m_lRigidIndirectCommands;
    xarray<RigidIndirectRun>               m_lRigidIndirectRuns;
    xarray<rdraw_indexed_indirect_command> m_ForwardCommands;
    rbuffer                                m_ForwardCommandBuffer;
    u32                                    m_ForwardCommandCapacity = 0;

    //-------------------------------------------------------------------------
    //  DYNAMIC GEOMETRY RESOURCES
    //-------------------------------------------------------------------------

    GraphicsPipelineCache        m_DynamicPipelines;
    u32                          m_DynamicVertexCapacity;
    u32                          m_DynamicIndexCapacity;
    dynamic_geom_storage         m_DynamicStorage;
    xarray<RenderLightingHandle> m_lDynamicFrameLighting;

    u32                                     m_LightingDataCapacity;
    u32                                     m_ProjectionLightingCapacity = 0;
    u32                                     m_AmbientLightingCapacity    = 0;
    xarray<GeomProjectionLightingConstants> m_lProjectionLighting;
    xarray<GeomAmbientConstants>            m_lAmbientLighting;
    xarray<GeomLightingConstants>           m_lFrameLighting;
    xarray<u32>                             m_lLightingRemap;
    u64                                     m_RequestedLightCookieMask;

    //-------------------------------------------------------------------------
    //  SHADER BIND STATE
    //-------------------------------------------------------------------------

    GeomShaderKind                m_ActiveShaderKind;
    const ShaderBindingLayout*    m_pActiveSurfaceBindings = NULL;
    xarray<SurfaceProgram*>       m_SurfacePrograms;
    xarray<SurfaceStage*>         m_SurfaceStages;
    rstate_sampler_preset         m_ActiveSamplerPreset;
    rstate_sampler                m_Samplers[RSTATE_SAMPLER_PRESET_COUNT];
    vram_texture                  m_WhiteTexture;
    vram_texture                  m_NeutralDetailTexture;
    vram_texture                  m_BlackTexture;
    vram_texture                  m_BlackCubeTexture;
    xbool                         m_isDistortionStateActive;
    radian3                       m_DistortionNormalRot;
    const shader_resource*        m_pDistortionSceneResource;
    const shader_resource*        m_pAmbientVisibilityResource;
    xbool                         m_hasAmbientVisibility;
    const shader_resource*        m_pDiffuseResource;
    const shader_resource*        m_pDetailResource;
    const shader_resource*        m_pEnvironmentResource;
    const shader_resource*        m_pEnvironmentCubeResource;
    const shader_resource*        m_pCachedFaceShadowResource;
    const shader_resource*        m_pCachedFaceShadowDepthResource;
    rstate_sampler_preset         m_FaceShadowSamplerPreset;
    rbuffer                       m_ShadowMatrixBuffer;
    rbuffer                       m_ShadowDataBuffer;
    matrix4                       m_FaceShadowMatrices[MAX_SHADOW_SOURCES];
    CbShadowMapData               m_ShadowMapData;
    u32                           m_DynamicLightShadowIndex[LightSystem::MAX_DYNAMIC_LIGHTS];
    xbool                         m_areShadowMapConstantsValid;
    xarray<const geom_draw_item*>  m_OrderedDraws;
    OrderedDrawJob                 m_OrderedDrawJob;
    xhandle                       m_OrderedDrawJobHandle;
    xbool                         m_areOrderedDrawsPrepared;
    xbool                         m_isOrderedDrawJobSubmitted;

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    xarray<ScenePipelineRequirement> m_ScenePipelineRequirements;
    xarray<s32>                      m_ScenePipelineSlots;
    xarray<GeomDrawPacket>           m_lDrawPackets;
    material                         m_DefaultDistortionMaterial;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
geom_pass_desc::geom_pass_desc( void )
    : TargetWidth( 0 ),
      TargetHeight( 0 ),
      SceneFogMode( RENDER_FOG_NONE ),
      BloomFogMode( RENDER_FOG_NONE ),
      pView( NULL ),
      ColorFormat( RTARGET_FORMAT_RGBA8 ),
      DepthFormat( RTARGET_FORMAT_DEPTH_STENCIL ),
      SampleCount( 1 ),
      DepthMode( GEOM_DEPTH_DEFAULT ),
      WriteSceneDepth( FALSE ),
      WriteGlowMask( FALSE ),
      WriteNormals( FALSE ),
      EnableShadows( TRUE )
{
}

//=========================================================================

inline
PacketMetadata::PacketMetadata( void )
    : PacketIndex( -1 ),
      Pass( GEOM_PASS_FORWARD_OPAQUE ),
      SortDepth( 0.0f ),
      Sequence( 0 ),
      ContributionPolicy( render::RENDER_CONTRIBUTION_DEFAULT )
{
}

//=========================================================================

inline
void FrameCompiler::SetContext( const RenderBackendContext& Context )
{
    m_pContext = &Context;
}

//=========================================================================

inline
xbool FrameCompiler::IsInitialized( void ) const
{
    return m_isInitialized;
}

//=========================================================================

inline
u32 FrameCompiler::GetForwardCommandCount( void ) const
{
    return m_ForwardCommands.GetCount();
}

//=========================================================================

inline
FrameCompiler::RenderStateSelection::RenderStateSelection( void )
    : Blend( RSTATE_BLEND_PRESET_NONE ),
      Depth( RSTATE_DEPTH_PRESET_NORMAL ),
      Raster( RSTATE_RASTER_PRESET_SOLID ),
      Sampler( RSTATE_SAMPLER_PRESET_MATERIAL_WRAP ),
      WriteSceneDepth( FALSE ),
      Program(),
      ColorFormat( RTARGET_FORMAT_RGBA8 ),
      DepthFormat( RTARGET_FORMAT_DEPTH_STENCIL ),
      SampleCount( 1 )
{
}

//=========================================================================

inline
FrameCompiler::SkinPose::SkinPose( void )
    : pBones( NULL ),
      BoneCount( 0 ),
      FirstBone( 0 )
{
}

//=========================================================================

inline
FrameCompiler::SkinRemapCacheEntry::SkinRemapCacheEntry( void )
    : MeshHandle( HNULL ),
      FirstRemap( 0 ),
      View()
{
}

//=========================================================================

inline
FrameCompiler::SkinIndirectRun::SkinIndirectRun( void )
    : FirstPacket( 0 ),
      FirstCommand( 0 ),
      CommandCount( 0 ),
      VertexPool( -1 ),
      IndexPool( -1 )
{
}

//=========================================================================

inline
FrameCompiler::RigidIndirectRun::RigidIndirectRun( void )
    : FirstPacket( 0 ),
      LastPacket( 0 ),
      FirstCommand( 0 ),
      CommandCount( 0 ),
      VertexPool( -1 ),
      IndexPool( -1 )
{
}

//=========================================================================

inline
rbuffer& FrameCompiler::GetFrameBuffer( FrameStorage::Stream StreamId ) const
{
    ASSERT( m_pContext && m_pContext->pStorage );
    return m_pContext->pStorage->GetGpuBuffer( StreamId );
}

//=========================================================================

inline
FrameCompiler::ScenePipelineRequirement::ScenePipelineRequirement( void )
    : Key(),
      Hash( 0 ),
      NativeSerial( 0 ),
      pPipeline( NULL )
{
}

} // namespace render

#endif // FRAME_COMPILER_HPP
//=========================================================================
