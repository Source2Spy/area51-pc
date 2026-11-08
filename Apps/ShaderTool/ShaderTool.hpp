//==============================================================================
//
//  ShaderTool.hpp
//
//==============================================================================

#ifndef SHADERTOOL_HPP
#define SHADERTOOL_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "x_ref.hpp"
#include "Auxiliary/CommandLine/CommandLine.hpp"
#include "Entropy/Shader/ecs_package.hpp"
#include "Entropy/e_Shader.hpp"
#include "Parsing/tokenizer.hpp"
#include "ShaderComboSchema.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define SHADER_TARGET_VULKAN ( 1 << 0 )
#define SHADER_TARGET_D3D12 ( 1 << 1 )
#define SHADER_TARGET_METAL ( 1 << 2 )
#define SHADER_TARGET_ALL ( SHADER_TARGET_VULKAN | SHADER_TARGET_D3D12 ) //|SHADER_TARGET_METAL) // TODO: GS: Implement this.

//==============================================================================
//  ENUMS
//==============================================================================

enum shader_binding_model
{
    SHADER_BINDING_NATIVE,
    SHADER_BINDING_SDL,
};

//------------------------------------------------------------------------------

enum shader_reflection_binding_type
{
    SHADER_REFLECT_SAMPLED_TEXTURE = 0,
    SHADER_REFLECT_UNIFORM_BUFFER,
    SHADER_REFLECT_STORAGE_TEXTURE,
    SHADER_REFLECT_STORAGE_BUFFER,
    SHADER_REFLECT_READ_WRITE_TEXTURE,
    SHADER_REFLECT_READ_WRITE_BUFFER
};

//==============================================================================
//  STRUCTS
//==============================================================================

struct shader_reflection_binding
{
    shader_reflection_binding_type Type;
    xstring                        Name;
    u32                            Slot;
    u32                            Count;
    u32                            ByteSize;

    shader_reflection_binding( void ) : Type( SHADER_REFLECT_SAMPLED_TEXTURE ),
                                        Name(),
                                        Slot( 0 ),
                                        Count( 0 ),
                                        ByteSize( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_interface_variable
{
    xstring Semantic;
    u32     Index;
    u32     Components;
    u32     ComponentType;
};

struct shader_buffer_member
{
    xstring Buffer;
    xstring Name;
    u32     Offset;
    u32     Size;
    u32     ArrayStride;
    u32     MatrixStride;
};

struct shader_compile_output
{
    u32                               RequiredCapabilities = 0;
    xstring                           Target;
    xstring                           ShaderName;
    xarray<byte>                      Code;
    u32                               BinarySize = 0;
    xstring                           Pdb;
    xstring                           TempPdb;
    xbool                             HasPdb;
    xarray<shader_interface_variable> Inputs;
    xarray<shader_interface_variable> Outputs;
    xarray<shader_buffer_member>      Members;
    shader_resource_counts            Resources;
    shader_compute_desc               Compute;
    xarray<shader_reflection_binding>
        Bindings;

    shader_compile_output( void ) : HasPdb( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct compile_options
{
    xbool           Clean;
    xbool           Verbose;
    xbool           GenerateOnly;
    xstring         StageFilter;
    xstring         Config;
    u32             WorkerCount         = 1;
    u64             CompilerFingerprint = 0;
    xarray<xstring> Scripts;

    compile_options( void ) : Clean( FALSE ),
                              Verbose( FALSE ),
                              GenerateOnly( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_entry
{
    xstring        Name;
    xstring        Stage;
    xstring        StageShort;
    xstring        Source;
    xstring        Entry;
    xstring        Profile;
    xstring        CommonArgs;
    xstring        VulkanArgs;
    xstring        D3D12Args;
    xbool          ColorArithmetic     = FALSE;
    u32            CompileCapabilities = 0;
    u64            ProgramId           = 0;
    ecs_content_id SourceContent       = {};
    u32            ResourceSlots[64];
    u32            UniformSlots[64];

    shader_entry( void )
    {
        for( u32 i = 0; i < 64; ++i )
            ResourceSlots[i] = UniformSlots[i] = i;
    }

    xstring MetalArgs;
};

//------------------------------------------------------------------------------

struct shader_contract_field
{
    xstring Type;
    xstring Name;
    xstring CppName;
    u32     Count;
    u32     Offset;
    u32     Size;
};

struct shader_contract_buffer
{
    xstring                       HlslName;
    xstring                       CppName;
    xstring                       Resource;
    u32                           Size;
    xarray<shader_contract_field> Fields;
};

struct shader_program_recipe
{
    shader_combo_kind Kind = SHADER_COMBO_FIXED;
    xstring           Name;
    xstring           Vertex;
    xstring           Pixel;
    u64               Key = 0;
    xstring           Package;
};

struct shader_surface_family
{
    xstring Recipe;
    xstring Vertex;
    xstring Source;
    xstring PixelSource;
    xstring Package;
};

struct shader_package_assignment
{
    xstring         Name;
    xarray<xstring> Programs;
    xarray<xstring> Families;
};

struct shader_interface_field
{
    xstring Type;
    xstring Name;
    xstring Semantic;
    xstring Interpolation;
    xstring Condition;
};

struct shader_contract_interface
{
    xstring                        Name;
    xarray<shader_interface_field> Fields;
};

//------------------------------------------------------------------------------

struct shader_source_file
{
    xstring      Path;
    xarray<byte> Bytes;
};

struct shader_source_snapshot : public xref_counted
{
    xarray<shader_source_file> Files;
    xarray<byte>               Generated;
};

struct shader_compile_metrics
{
    u32 DiscoveryCalls = 0;
    u32 DxilCalls      = 0;
    u32 SpirvCalls     = 0;
    f64 DiscoveryMs    = 0;
    f64 DxilMs         = 0;
    f64 SpirvMs        = 0;
};

struct shader_script
{
    xstring                           ScriptPath;
    xstring                           ScriptDir;
    xstring                           SourceRoot;
    xstring                           OutRoot;
    xstring                           CacheRoot;
    xstring                           Config;
    xstring                           Contract;
    xstring                           Abi;
    u64                               CatalogId;
    u64                               AbiId               = 0;
    u64                               CompilerFingerprint = 0;
    xarray<shader_contract_buffer>    Buffers;
    xarray<xstring>                   ResourceSemantics;
    xarray<shader_program_recipe>     Programs;
    xarray<shader_surface_family>     SurfaceFamilies;
    xarray<shader_package_assignment> Packages;
    std::vector<shader_combo_desc>    Combos;
    xarray<shader_contract_interface> Interfaces;
    xarray<shader_compile_output>     FinalOutputs;
    xref<shader_source_snapshot>      Sources;
    shader_compile_metrics*           Metrics = NULL;
    u32                               TargetMask;
    shader_binding_model              BindingModel;
    xbool                             DiscoverBindings;
    xarray<shader_entry>              Shaders;
    xarray<shader_entry>              ComputeShaders;

    shader_script( void ) : Config( "debug" ),
                            TargetMask( SHADER_TARGET_ALL ),
                            BindingModel( SHADER_BINDING_NATIVE ),
                            DiscoverBindings( FALSE ),
                            CatalogId( 0 )
    {
    }
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

void  DisplayHelp( void );
xbool WriteShaderPackages( const shader_script& Script, const compile_options& Options );
xbool BuildShaderPackage( const shader_script& Script, const compile_options& Options,
                          const xstring& PackageName, const xarray<s32>& Recipes,
                          xarray<byte>& File, xstring& Error );
xbool ParseCommandLine( s32 argc, char** argv, compile_options& Options );
xbool ParseScript( const char* pFileName, shader_script& Script );
xbool ProcessScript( const char* pFileName, const compile_options& Options );
xbool CompileShader( shader_script&         Script,
                     const shader_entry&    Shader,
                     const compile_options& Options );
xbool CompileDxcTarget( const shader_script&   Script,
                        const shader_entry&    Shader,
                        const compile_options& Options,
                        const char*            pTarget,
                        shader_compile_output& Output,
                        xbool                  Reflect = TRUE );
xbool WriteComputeShaderHeaders( const shader_script& Script, const compile_options& Options );
xbool ReflectDxcShader( const shader_script&               Script,
                        const shader_entry&                Shader,
                        const compile_options&             Options,
                        shader_resource_counts&            Resources,
                        xarray<shader_reflection_binding>& Bindings );

xstring     ToLower( const xstring& Text );
xbool       IsAbsolutePath( const xstring& Path );
xstring     JoinPath( const xstring& Path, const xstring& File );
xstring     NormalizePath( const xstring& Path );
xstring     MakeRelativePath( const xstring& BasePath, const xstring& Path );
xstring     GetDirectory( const xstring& PathName );
xbool       EnsureParentDir( const xstring& PathName );
xbool       LoadBinaryFile( const xstring& PathName, xarray<byte>& Data );
xbool       WriteBinaryFile( const xstring& PathName, const void* pData, s32 Size );
xbool       RemoveFile( const xstring& PathName );
xbool       ReplaceFile( const xstring& TempPath, const xstring& FinalPath );
xstring     MetaPath( const shader_script& Script, const shader_entry& Shader );
xstring     BlobPath( const shader_script& Script,
                      const shader_entry&  Shader,
                      const char*          pTarget );
xstring     PdbPath( const shader_script& Script, const shader_entry& Shader );
xstring     TempPathFor( const xstring& PathName );
u64         ShaderCompilerFingerprint();
void        InitializeShaderConsole( void );
void        SetShaderConsoleVerbose( xbool Verbose );
xbool       ShaderConsoleVerbose( void );
void        BeginShaderConsole( const char* pFile, const shader_script& Script, const compile_options& Options );
void        FinishShaderConsole( xbool Success, f64 Seconds );
void        SetShaderConsoleOutput( const xstring& Output );
void        ReportShaderDiagnostics( const char* pText );
void        BeginShaderPackageProgress( const char* pName );
void        UpdateShaderPackageProgress( const char* pName, u32 Completed, u32 Total, u32 Zstd, u32 Cached, u32 Raw,
                                         const char* pPhase, xbool Finished = FALSE, f64 ElapsedMs = 0 );
void        DisplayShaderProgress( const char* pPhase, u32 Completed, u32 Total, f64 ElapsedMs, xbool Finished = FALSE );
xbool       LoadShaderCache( const shader_script&, const shader_entry&, const compile_options&, xarray<shader_compile_output>&,
                             xbool LoadCode = TRUE, xbool RestorePdb = FALSE );
xbool       WriteShaderCache( const shader_script&, const shader_entry&, const compile_options&, const xarray<shader_compile_output>& );
xbool       ShaderKitIsCurrent( const shader_script& Script );
xbool       WriteShaderKitState( const shader_script& Script );
xbool       WriteShaderKitManifest( const shader_script& Script );
xbool       WriteShaderCatalog( const shader_script& Script );
xbool       GenerateShaderContract( shader_script& Script, xbool WriteFiles );
xbool       ShaderSourcesUnchanged( const shader_script& Script );
xbool       ExpandSurfaceRecipes( shader_script& Script );
u32         ShaderInterfaceId( const xarray<shader_interface_variable>& Variables );
xbool       ValidateDeclaredBuffers( const shader_script& Script, const shader_compile_output& Output );
xbool       ReflectSpirvShader( const shader_script& Script, const shader_entry& Shader, shader_compile_output& Output );
xbool       ValidateTargetReflections( const shader_entry& Shader, const shader_compile_output& A, const shader_compile_output& B );
const char* BindingModelName( shader_binding_model Model );

//==============================================================================
#endif // SHADERTOOL_HPP
//==============================================================================
