//==============================================================================
//
//  DxcCompiler.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderTool.hpp"

#if defined( TARGET_WINDOWS ) || defined( TARGET_LINUX )
#if defined( TARGET_WINDOWS )
#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <oleauto.h>
#endif
#include <dxcapi.h>
#define interface struct
#include <d3d12shader.h>
#undef interface
#include <limits.h>
#include <string>
#include <vector>
#include <filesystem>
#endif

//==============================================================================

#if defined( TARGET_WINDOWS ) || defined( TARGET_LINUX )

template <class T>
class dxc_ptr
{
  public:
    dxc_ptr( void ) : m_p( NULL )
    {
    }

    ~dxc_ptr( void )
    {
        Release();
    }

    T* operator->( void ) const
    {
        return m_p;
    }

    T* Get( void ) const
    {
        return m_p;
    }

    T** Put( void )
    {
        Release();
        return &m_p;
    }

  private:
    void Release( void )
    {
        if( m_p )
        {
            m_p->Release();
            m_p = NULL;
        }
    }

    dxc_ptr( const dxc_ptr& );
    const dxc_ptr& operator=( const dxc_ptr& );

    T* m_p;
};

class dxc_snapshot_include : public IDxcIncludeHandler
{
  public:
    dxc_snapshot_include( IDxcUtils* pUtils ) : m_pUtils( pUtils ), m_pSources( NULL )
    {
        x_AtomicInit( &m_References, 1 );
        m_pUtils->AddRef();
    }

    ~dxc_snapshot_include( void )
    {
        m_pUtils->Release();
    }

    void SetSources( const shader_source_snapshot* pSources )
    {
        m_pSources = pSources;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID Id, void** ppObject ) override
    {
        if( !ppObject )
            return E_POINTER;
        *ppObject = NULL;

        if( Id != __uuidof( IUnknown ) && Id != __uuidof( IDxcIncludeHandler ) )
            return E_NOINTERFACE;
        *ppObject = static_cast<IDxcIncludeHandler*>( this );
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef( void ) override
    {
        s32 Previous = x_AtomicLoadAcquire( &m_References );

        for( ;; )
        {
            const s32 Actual = x_AtomicCompareExchange( &m_References, Previous + 1, Previous );

            if( Actual == Previous )
                return Previous + 1;

            Previous = Actual;
        }
    }

    ULONG STDMETHODCALLTYPE Release( void ) override
    {
        s32 Previous = x_AtomicLoadAcquire( &m_References );

        for( ;; )
        {
            const s32 Actual = x_AtomicCompareExchange( &m_References, Previous - 1, Previous );

            if( Actual == Previous )
            {
                if( Previous == 1 )
                    delete this;
                return Previous - 1;
            }

            Previous = Actual;
        }
    }

    HRESULT STDMETHODCALLTYPE LoadSource( LPCWSTR pFilename, IDxcBlob** ppSource ) override
    {
        if( !pFilename || !ppSource || !m_pSources )
            return E_INVALIDARG;
        *ppSource = NULL;
#if defined( TARGET_WINDOWS )
        const std::filesystem::path Path = std::filesystem::absolute( pFilename ).lexically_normal();
#else
        CW2A Converted( pFilename );

        if( !Converted.m_psz )
            return E_FAIL;

        const std::filesystem::path Path   = std::filesystem::absolute( Converted.m_psz ).lexically_normal();
#endif
        const xarray<byte>*         pBytes = NULL;

        if( Path.filename() == "a51_shader_contract.hlsl" )
            pBytes = &m_pSources->Generated;
        else
        {
            const std::string Name = Path.string();

            for( s32 i = 0; i < m_pSources->Files.GetCount(); ++i )
                if( m_pSources->Files[i].Path == Name.c_str() )
                {
                    pBytes = &m_pSources->Files[i].Bytes;
                    break;
                }
        }

        if( !pBytes )
            return E_FAIL;

        IDxcBlobEncoding* pBlob  = NULL;
        const HRESULT     Result = m_pUtils->CreateBlob( pBytes->GetPtr(), pBytes->GetCount(), DXC_CP_UTF8, &pBlob );

        if( SUCCEEDED( Result ) )
            *ppSource = pBlob;
        return Result;
    }

  private:
    IDxcUtils*                    m_pUtils;
    const shader_source_snapshot* m_pSources;
    x_atomic_s32                  m_References;
};

struct dxc_worker_context
{
    dxc_ptr<IDxcUtils>          Utils;
    dxc_ptr<IDxcCompiler3>      Compiler;
    dxc_ptr<IDxcIncludeHandler> IncludeHandler;
    dxc_ptr<IDxcIncludeHandler> SnapshotHandler;
};

static thread_local dxc_worker_context s_DxcWorker;

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static
std::wstring ToWide( const xstring& Text )
{
#if defined( TARGET_WINDOWS )
    const s32 Length = MultiByteToWideChar( CP_ACP,
                                            0,
                                            (const char*)Text,
                                            Text.GetLength(),
                                            NULL,
                                            0 );
    if( Length <= 0 )
        return std::wstring();

    std::wstring Result;
    Result.resize( Length );
    MultiByteToWideChar( CP_ACP,
                         0,
                         (const char*)Text,
                         Text.GetLength(),
                         &Result[0],
                         Length );

    return Result;
#else
    CA2W Converted( (const char*)Text );

    if( !Converted.m_psz )
        return std::wstring();

    return std::wstring( Converted.m_psz );
#endif
}

//==============================================================================

static
void SplitArgString( const xstring& Text, xarray<xstring>& Args )
{
    xstring Current;
    xbool   InQuote = FALSE;

    for( s32 i = 0; i < Text.GetLength(); i++ )
    {
        const char C = Text[i];

        if( C == '"' )
        {
            InQuote = !InQuote;
        }
        else if( !InQuote && x_isspace( C ) )
        {
            if( !Current.IsEmpty() )
                Args.Append( Current );

            Current.Clear();
        }
        else
        {
            Current += C;
        }
    }

    if( !Current.IsEmpty() )
        Args.Append( Current );
}

//==============================================================================

static
void AppendArgs( xarray<xstring>& Args, const xstring& Text )
{
    if( !Text.IsEmpty() )
        SplitArgString( Text, Args );
}

//==============================================================================

static
const char* StageDefine( const xstring& Stage )
{
    if( Stage == "vertex" )
        return "A51_SHADER_STAGE_VERTEX=1";

    if( ( Stage == "pixel" ) || ( Stage == "fragment" ) )
        return "A51_SHADER_STAGE_PIXEL=1";

    if( Stage == "compute" )
        return "A51_SHADER_STAGE_COMPUTE=1";

    if( Stage == "geometry" )
        return "A51_SHADER_STAGE_GEOMETRY=1";

    if( ( Stage == "hull" ) || ( Stage == "tess_control" ) )
        return "A51_SHADER_STAGE_HULL=1";

    if( ( Stage == "domain" ) || ( Stage == "tess_evaluation" ) )
        return "A51_SHADER_STAGE_DOMAIN=1";

    if( Stage == "mesh" )
        return "A51_SHADER_STAGE_MESH=1";

    if( ( Stage == "amplification" ) || ( Stage == "task" ) )
        return "A51_SHADER_STAGE_AMPLIFICATION=1";

    return "A51_SHADER_STAGE_UNKNOWN=1";
}

//==============================================================================

static
void AppendBindingArgs( const shader_script& Script,
                               const shader_entry&  Shader,
                               xarray<xstring>&     Args )
{
    Args.Append( "-D" );

    if( Script.BindingModel == SHADER_BINDING_SDL )
        Args.Append( "A51_SHADER_BINDING_SDL=1" );
    else if( Script.BindingModel == SHADER_BINDING_NATIVE )
        Args.Append( "A51_SHADER_BINDING_NATIVE=1" );

    Args.Append( "-D" );
    Args.Append( StageDefine( Shader.Stage ) );
}

//==============================================================================

static
void BuildDxcArgs( const shader_script& Script,
                          const shader_entry&  Shader,
                          const char*          pTarget,
                          const xstring&       D3D12PdbPath,
                          xarray<xstring>&     Args )
{
    const xbool Debug = ( Script.Config != "release" );

    Args.Append( "-I" );
    Args.Append( GetDirectory( Shader.Source ) );

    Args.Append( "-I" );
    Args.Append( JoinPath( JoinPath( Script.OutRoot, Script.Config ), "generated" ) );
    AppendBindingArgs( Script, Shader, Args );

    if( Shader.ColorArithmetic )
        Args.Append( "-DA51_COLOR_ARITHMETIC=1" );
    if( Shader.CompileCapabilities & ECS_SHADER_CAPABILITY_FLOAT16 )
    {
        Args.Append( "-enable-16bit-types" );
        Args.Append( "-DA51_NATIVE_FLOAT16=1" );
    }

    if( Script.BindingModel == SHADER_BINDING_SDL )
    {
        for( u32 i = 0; i < 64; ++i )
        {
            Args.Append( xstring( xfs( "-DA51_RESOURCE_SLOT_%u=%u", i, Shader.ResourceSlots[i] ) ) );
            Args.Append( xstring( xfs( "-DA51_UNIFORM_SLOT_%u=%u", i, Shader.UniformSlots[i] ) ) );
        }
    }

    if( x_stricmp( pTarget, "vulkan" ) == 0 )
    {
        Args.Append( "-spirv" );
        Args.Append( "-fspv-reflect" );
        Args.Append( "-fspv-target-env=vulkan1.0" );
    }

    if( Debug )
    {
        Args.Append( "-Zi" );
        // Specialization must remove unused resources in both targets. Keep
        // debug information without changing the compact resource contract.
        Args.Append( "-O3" );

        if( x_stricmp( pTarget, "d3d12" ) == 0 )
        {
            Args.Append( "-Qembed_debug" );
            Args.Append( "-Fd" );
            Args.Append( D3D12PdbPath );
        }
    }
    else
    {
        Args.Append( "-O3" );
    }

    AppendArgs( Args, Shader.CommonArgs );

    if( x_stricmp( pTarget, "vulkan" ) == 0 )
        AppendArgs( Args, Shader.VulkanArgs );
    else if( x_stricmp( pTarget, "d3d12" ) == 0 )
        AppendArgs( Args, Shader.D3D12Args );
    else if( x_stricmp( pTarget, "metal" ) == 0 )
        AppendArgs( Args, Shader.MetalArgs );
    else
        x_printf( "Error: Unknown target '%s' in BuildDxcArgs\n", pTarget );
}

//==============================================================================

static
xbool WriteBlob( const xstring& PathName, IDxcBlob* pBlob )
{
    if( !pBlob || ( pBlob->GetBufferSize() == 0 ) )
        return FALSE;

    return WriteBinaryFile( PathName,
                            pBlob->GetBufferPointer(),
                            (s32)pBlob->GetBufferSize() );
}

//==============================================================================

static
void PrintErrors( IDxcResult* pResult )
{
    dxc_ptr<IDxcBlobUtf8> Errors;

    if( SUCCEEDED( pResult->GetOutput( DXC_OUT_ERRORS,
                                       __uuidof( IDxcBlobUtf8 ),
                                       (void**)Errors.Put(),
                                       NULL ) ) &&
        Errors.Get() &&
        Errors->GetStringLength() )
    {
        ReportShaderDiagnostics( Errors->GetStringPointer() );
    }
}

//==============================================================================

static
xbool IsUnboundedBinding( const D3D12_SHADER_INPUT_BIND_DESC& Binding )
{
    return ( Binding.BindCount == UINT_MAX );
}

//==============================================================================

static
u32 BindingEnd( const D3D12_SHADER_INPUT_BIND_DESC& Binding )
{
    if( IsUnboundedBinding( Binding ) )
        return 0;

    return Binding.BindPoint + Binding.BindCount;
}

//==============================================================================

static
xbool IsStorageBufferType( D3D_SHADER_INPUT_TYPE Type )
{
    return ( Type == D3D_SIT_TBUFFER ) ||
           ( Type == D3D_SIT_STRUCTURED ) ||
           ( Type == D3D_SIT_BYTEADDRESS ) ||
           ( Type == D3D_SIT_UAV_RWSTRUCTURED ) ||
           ( Type == D3D_SIT_UAV_RWBYTEADDRESS ) ||
           ( Type == D3D_SIT_UAV_APPEND_STRUCTURED ) ||
           ( Type == D3D_SIT_UAV_CONSUME_STRUCTURED ) ||
           ( Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER ) ||
           ( Type == D3D_SIT_RTACCELERATIONSTRUCTURE );
}

//==============================================================================

static
xbool IsReadWriteStorageBufferType( D3D_SHADER_INPUT_TYPE Type )
{
    return ( Type == D3D_SIT_UAV_RWSTRUCTURED ) ||
           ( Type == D3D_SIT_UAV_RWBYTEADDRESS ) ||
           ( Type == D3D_SIT_UAV_APPEND_STRUCTURED ) ||
           ( Type == D3D_SIT_UAV_CONSUME_STRUCTURED ) ||
           ( Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER );
}

//==============================================================================

static
xbool IsStorageTextureType( D3D_SHADER_INPUT_TYPE Type )
{
    return ( Type == D3D_SIT_UAV_RWTYPED ) ||
           ( Type == D3D_SIT_UAV_FEEDBACKTEXTURE );
}

//==============================================================================

static
xbool HasSamplerSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                             u32                                         Slot,
                             u32                                         Space )
{
    for( s32 i = 0; i < Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];

        if( ( Binding.Type == D3D_SIT_SAMPLER ) &&
            ( Binding.Space == Space ) &&
            ( Slot >= Binding.BindPoint ) &&
            ( Slot < BindingEnd( Binding ) ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool IsSampledTexture( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                               const D3D12_SHADER_INPUT_BIND_DESC&         Texture )
{
    const u32 End = BindingEnd( Texture );

    if( End == 0 )
        return FALSE;

    for( u32 Slot = Texture.BindPoint; Slot < End; Slot++ )
    {
        if( !HasSamplerSlot( Bindings, Slot, Texture.Space ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool HasTextureSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                             u32                                         Slot,
                             u32                                         Space )
{
    for( s32 i = 0; i < Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];

        if( ( Binding.Type == D3D_SIT_TEXTURE ) &&
            ( Binding.Space == Space ) &&
            ( Slot >= Binding.BindPoint ) &&
            ( Slot < BindingEnd( Binding ) ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool HasUniformBufferSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                                   u32                                         Slot,
                                   u32                                         Space )
{
    for( s32 i = 0; i < Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];

        if( ( Binding.Type == D3D_SIT_CBUFFER ) &&
            ( Binding.Space == Space ) &&
            ( Slot >= Binding.BindPoint ) &&
            ( Slot < BindingEnd( Binding ) ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool HasStorageBufferSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                                   u32                                         Slot,
                                   u32                                         Space )
{
    for( s32 i = 0; i < Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];

        if( IsStorageBufferType( Binding.Type ) &&
            ( Binding.Space == Space ) &&
            ( Slot >= Binding.BindPoint ) &&
            ( Slot < BindingEnd( Binding ) ) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
void AddReflectionBinding( xarray<shader_reflection_binding>& Bindings,
                                  shader_reflection_binding_type     Type,
                                  const char*                        pName,
                                  u32                                Slot,
                                  u32                                Count )
{
    if( !pName || !pName[0] || ( Count == 0 ) )
        return;

    shader_reflection_binding& Binding = Bindings.Append();
    Binding.Type                       = Type;
    Binding.Name                       = pName;
    Binding.Slot                       = Slot;
    Binding.Count                      = Count;
}

//==============================================================================

static
xbool DxilTypeSize( ID3D12ShaderReflectionType* Type, xbool Uniform, u32& Size )
{
    D3D12_SHADER_TYPE_DESC Desc;

    if( !Type || FAILED( Type->GetDesc( &Desc ) ) )
        return FALSE;

    Size = 0;

    if( Desc.Class == D3D_SVC_STRUCT )
    {
        for( u32 i = 0; i < Desc.Members; ++i )
        {
            ID3D12ShaderReflectionType* Child = Type->GetMemberTypeByIndex( i );
            D3D12_SHADER_TYPE_DESC      ChildDesc;
            u32                         ChildSize;

            if( FAILED( Child->GetDesc( &ChildDesc ) ) || !DxilTypeSize( Child, Uniform, ChildSize ) )
                return FALSE;

            Size = MAX( Size, ChildDesc.Offset + ChildSize );
        }

        if( Uniform )
            Size = ( Size + 15 ) & ~15u;
    }
    else if( Desc.Class == D3D_SVC_MATRIX_ROWS || Desc.Class == D3D_SVC_MATRIX_COLUMNS )
    {
        const u32 Components = Desc.Class == D3D_SVC_MATRIX_COLUMNS ? Desc.Rows : Desc.Columns;
        const u32 Vectors    = Desc.Class == D3D_SVC_MATRIX_COLUMNS ? Desc.Columns : Desc.Rows;
        Size                 = ( Uniform ? 16 : Components * 4 ) * Vectors;
    }
    else if( Desc.Class == D3D_SVC_SCALAR || Desc.Class == D3D_SVC_VECTOR )
        Size = MAX( 1u, Desc.Columns ) * ( Desc.Type == D3D_SVT_DOUBLE ? 8 : 4 );
    else
        return FALSE;

    if( Desc.Elements )
        Size = ( Uniform ? ( Size + 15 ) & ~15u : Size ) * Desc.Elements;
    return TRUE;
}

//==============================================================================

static
xbool ReflectDxilMembers( const char* Buffer, ID3D12ShaderReflectionType* Type,
                                 xstring const& Prefix, u32 BaseOffset, xbool Uniform, xarray<shader_buffer_member>& Members )
{
    D3D12_SHADER_TYPE_DESC Desc;

    if( FAILED( Type->GetDesc( &Desc ) ) )
        return FALSE;

    for( u32 i = 0; i < Desc.Members; ++i )
    {
        ID3D12ShaderReflectionType* Child = Type->GetMemberTypeByIndex( i );
        D3D12_SHADER_TYPE_DESC      ChildDesc;
        u32                         Size;

        if( FAILED( Child->GetDesc( &ChildDesc ) ) || !DxilTypeSize( Child, Uniform, Size ) )
            return FALSE;

        const char* Name = Type->GetMemberTypeName( i );

        if( !Name )
            return FALSE;

        shader_buffer_member& Member = Members.Append();
        Member.Buffer                = Buffer;
        Member.Name                  = Prefix + Name;
        Member.Offset                = BaseOffset + ChildDesc.Offset;
        Member.Size                  = Size;
        Member.ArrayStride           = ChildDesc.Elements ? Size / ChildDesc.Elements : 0;
        Member.MatrixStride          = ChildDesc.Class == D3D_SVC_MATRIX_ROWS || ChildDesc.Class == D3D_SVC_MATRIX_COLUMNS ? ( Uniform ? 16 : ( ChildDesc.Class == D3D_SVC_MATRIX_COLUMNS ? ChildDesc.Rows : ChildDesc.Columns ) * 4 ) : 0;
        const xstring ChildPrefix    = Member.Name + ".";
        const u32     Offset         = Member.Offset;

        if( !ReflectDxilMembers( Buffer, Child, ChildPrefix, Offset, Uniform, Members ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool ReflectComputeResources( ID3D12ShaderReflection&                     Reflection,
                                      const shader_entry&                         Shader,
                                      const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                                      shader_compile_output&                      Output )
{
    const shader_reflection_binding_type Types[] =
        {
            SHADER_REFLECT_SAMPLED_TEXTURE, SHADER_REFLECT_STORAGE_TEXTURE, SHADER_REFLECT_STORAGE_BUFFER,
            SHADER_REFLECT_READ_WRITE_TEXTURE, SHADER_REFLECT_READ_WRITE_BUFFER, SHADER_REFLECT_UNIFORM_BUFFER };
    u32         Counts[ARRAYSIZE( Types )] = {};
    xarray<u32> Categories;
    Categories.SetCount( Bindings.GetCount() );
    u32 Samplers = 0;

    for( s32 i = 0; i < Bindings.GetCount(); ++i )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        u32                                 Category;
        u32                                 Space;

        if( Binding.BindCount != 1 )
        {
            x_printf( "Error: compute descriptor arrays are unsupported in %s\n", (const char*)Shader.Name );
            return FALSE;
        }

        if( Binding.Type == D3D_SIT_SAMPLER )
        {
            if( Binding.Space != 0 || Binding.BindCount > 16 || Samplers > 16 - Binding.BindCount )
            {
                return FALSE;
            }

            Samplers += Binding.BindCount;
            Categories[i] = ARRAYSIZE( Types );
            continue;
        }

        if( Binding.Type == D3D_SIT_CBUFFER )
        {
            Category = 5;
            Space    = 2;
        }
        else if( Binding.Type == D3D_SIT_TEXTURE )
        {
            Category = IsSampledTexture( Bindings, Binding ) ? 0 : 1;
            Space    = 0;
        }
        else if( Binding.Type == D3D_SIT_STRUCTURED || Binding.Type == D3D_SIT_BYTEADDRESS )
        {
            Category = 2;
            Space    = 0;
        }
        else if( Binding.Type == D3D_SIT_UAV_RWTYPED && Binding.Dimension != D3D_SRV_DIMENSION_BUFFER )
        {
            Category = 3;
            Space    = 1;
        }
        else if( Binding.Type == D3D_SIT_UAV_RWSTRUCTURED || Binding.Type == D3D_SIT_UAV_RWBYTEADDRESS )
        {
            Category = 4;
            Space    = 1;
        }
        else
        {
            x_printf( "Error: unsupported compute resource '%s' in %s\n", Binding.Name, (const char*)Shader.Name );
            return FALSE;
        }

        if( Binding.Space != Space || Binding.BindCount > 16 || Counts[Category] > 16 - Binding.BindCount )
        {
            x_printf( "Error: invalid compute resource space or count for '%s' in %s\n",
                      Binding.Name, (const char*)Shader.Name );
            return FALSE;
        }

        Categories[i] = Category;
        Counts[Category] += Binding.BindCount;
    }

    if( Counts[0] > 16 || Samplers != Counts[0] || Counts[1] > 8 || Counts[2] > 8 ||
        Counts[3] > 8 || Counts[4] > 8 || Counts[5] > 4 )
    {
        x_printf( "Error: compute resource limits exceeded in %s\n", (const char*)Shader.Name );
        return FALSE;
    }

    const u32 Bases[]                      = { 0, Counts[0], Counts[0] + Counts[1], 0, Counts[3], 0 };
    xbool     Used[ARRAYSIZE( Types )][16] = {};

    for( s32 i = 0; i < Bindings.GetCount(); ++i )
    {
        const u32                           Category = Categories[i];
        const D3D12_SHADER_INPUT_BIND_DESC& Binding  = Bindings[i];

        if( Category == ARRAYSIZE( Types ) )
        {
            if( BindingEnd( Binding ) > Samplers )
            {
                return FALSE;
            }

            continue;
        }

        if( Binding.BindPoint < Bases[Category] ||
            BindingEnd( Binding ) > Bases[Category] + Counts[Category] )
        {
            x_printf( "Error: non-contiguous compute binding '%s' in %s\n", Binding.Name, (const char*)Shader.Name );
            return FALSE;
        }

        const u32 Slot = Binding.BindPoint - Bases[Category];

        for( u32 Element = 0; Element < Binding.BindCount; ++Element )
        {
            if( Used[Category][Slot + Element] )
            {
                return FALSE;
            }

            Used[Category][Slot + Element] = TRUE;
        }

        AddReflectionBinding( Output.Bindings, Types[Category], Binding.Name, Slot, Binding.BindCount );
    }

    for( u32 Slot = 0; Slot < Samplers; ++Slot )
    {
        if( !HasSamplerSlot( Bindings, Slot, 0 ) )
        {
            return FALSE;
        }
    }

    Output.Resources                     = shader_resource_counts();
    Output.Resources.SamplerCount        = Counts[0];
    Output.Resources.StorageTextureCount = Counts[1];
    Output.Resources.StorageBufferCount  = Counts[2];
    Output.Resources.UniformBufferCount  = Counts[5];
    Output.Compute.ReadWriteTextureCount = Counts[3];
    Output.Compute.ReadWriteBufferCount  = Counts[4];
    Reflection.GetThreadGroupSize( &Output.Compute.ThreadCountX, &Output.Compute.ThreadCountY,
                                   &Output.Compute.ThreadCountZ );
    const u32 X = Output.Compute.ThreadCountX;
    const u32 Y = Output.Compute.ThreadCountY;
    const u32 Z = Output.Compute.ThreadCountZ;

    if( !X || !Y || !Z || X > 1024 || Y > 1024 || Z > 64 || static_cast<u64>( X ) * Y * Z > 1024 )
    {
        x_printf( "Error: invalid compute thread group in %s\n", (const char*)Shader.Name );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool ReflectResources( IDxcUtils*                         pUtils,
                               IDxcResult*                        pResult,
                               const shader_script&               Script,
                               const shader_entry&                Shader,
                               shader_resource_counts&            Resources,
                               xarray<shader_reflection_binding>& OutBindings,
                               shader_compile_output*             Output = NULL )
{
    OutBindings.Clear();

    dxc_ptr<IDxcBlob> ReflectionData;
    HRESULT           hr = pResult->GetOutput( DXC_OUT_REFLECTION,
                                               __uuidof( IDxcBlob ),
                                               (void**)ReflectionData.Put(),
                                               NULL );
    if( FAILED( hr ) || !ReflectionData.Get() )
    {
        x_printf( "Error: DXC produced no reflection data for %s\n", (const char*)Shader.Name );
        return FALSE;
    }

    DxcBuffer ReflectionBuffer;
    ReflectionBuffer.Ptr      = ReflectionData->GetBufferPointer();
    ReflectionBuffer.Size     = ReflectionData->GetBufferSize();
    ReflectionBuffer.Encoding = 0;

    dxc_ptr<ID3D12ShaderReflection> Reflection;
    hr = pUtils->CreateReflection( &ReflectionBuffer,
                                   __uuidof( ID3D12ShaderReflection ),
                                   (void**)Reflection.Put() );
    if( FAILED( hr ) || !Reflection.Get() )
    {
        x_printf( "Error: DXC reflection creation failed for %s: 0x%08X\n",
                  (const char*)Shader.Name,
                  (u32)hr );
        return FALSE;
    }

    D3D12_SHADER_DESC Desc;
    hr = Reflection->GetDesc( &Desc );

    if( FAILED( hr ) )
        return FALSE;

    xarray<D3D12_SHADER_INPUT_BIND_DESC> Bindings;
    Bindings.SetCount( (s32)Desc.BoundResources );

    for( u32 i = 0; i < Desc.BoundResources; i++ )
    {
        hr = Reflection->GetResourceBindingDesc( i, &Bindings[(s32)i] );

        if( FAILED( hr ) )
        {
            x_printf( "Error: Failed to get resource binding desc %u in shader %s: 0x%08X\n",
                      i, (const char*)Shader.Name, (u32)hr );
            return FALSE;
        }

        if( IsUnboundedBinding( Bindings[(s32)i] ) )
        {
            x_printf( "Error: Unsupported unbounded resource '%s' in shader %s\n",
                      Bindings[(s32)i].Name, (const char*)Shader.Name );
            return FALSE;
        }

        if( Bindings[(s32)i].BindCount == 0 )
        {
            x_printf( "Error: Resource '%s' in shader %s has zero bind count (DXC reflection anomaly)\n",
                      Bindings[(s32)i].Name, (const char*)Shader.Name );
            return FALSE;
        }
    }

    if( Output )
    {
        for( u32 Direction = 0; Direction < 2; ++Direction )
        {
            const u32 Count = Direction ? Desc.OutputParameters : Desc.InputParameters;

            for( u32 i = 0; i < Count; ++i )
            {
                D3D12_SIGNATURE_PARAMETER_DESC Parameter;
                HRESULT                        Result = Direction ? Reflection->GetOutputParameterDesc( i, &Parameter ) : Reflection->GetInputParameterDesc( i, &Parameter );

                if( FAILED( Result ) )
                    return FALSE;

                if( Parameter.SystemValueType != D3D_NAME_UNDEFINED )
                    continue;
                shader_interface_variable& Variable = Direction ? Output->Outputs.Append() : Output->Inputs.Append();
                Variable.Semantic                   = Parameter.SemanticName;
                Variable.Semantic.MakeUpper();
                Variable.Index         = Parameter.SemanticIndex;
                Variable.ComponentType = Parameter.ComponentType;
                Variable.Components    = 0;

                for( u32 Bit = 0; Bit < 4; ++Bit )
                    Variable.Components += ( Parameter.Mask >> Bit ) & 1;
            }
        }

        for( u32 i = 0; i < Desc.ConstantBuffers; ++i )
        {
            ID3D12ShaderReflectionConstantBuffer* Buffer = Reflection->GetConstantBufferByIndex( i );
            D3D12_SHADER_BUFFER_DESC              BufferDesc;

            if( FAILED( Buffer->GetDesc( &BufferDesc ) ) )
                return FALSE;

            if( BufferDesc.Type != D3D_CT_CBUFFER && BufferDesc.Type != D3D_CT_RESOURCE_BIND_INFO )
                continue;
            const xbool Uniform = BufferDesc.Type == D3D_CT_CBUFFER;

            if( Shader.StageShort == "cs" && !Uniform )
                continue;
            for( u32 j = 0; j < BufferDesc.Variables; ++j )
            {
                ID3D12ShaderReflectionVariable* Variable = Buffer->GetVariableByIndex( j );
                D3D12_SHADER_VARIABLE_DESC      VariableDesc;
                D3D12_SHADER_TYPE_DESC          TypeDesc;

                if( FAILED( Variable->GetDesc( &VariableDesc ) ) || FAILED( Variable->GetType()->GetDesc( &TypeDesc ) ) )
                    return FALSE;

                shader_buffer_member& Member = Output->Members.Append();
                Member.Buffer                = BufferDesc.Name;
                Member.Name                  = Uniform ? VariableDesc.Name : "";
                Member.Offset                = VariableDesc.StartOffset;
                Member.Size                  = VariableDesc.Size;
                Member.MatrixStride          = ( TypeDesc.Class == D3D_SVC_MATRIX_ROWS || TypeDesc.Class == D3D_SVC_MATRIX_COLUMNS ) ? 16 : 0;
                Member.ArrayStride           = Uniform ? ( TypeDesc.Elements ? ( ( VariableDesc.Size / TypeDesc.Elements + 15 ) & ~15u ) : 0 ) : BufferDesc.Size;
                const xstring Prefix         = Member.Name.IsEmpty() ? xstring( "." ) : Member.Name + ".";

                if( !ReflectDxilMembers( BufferDesc.Name, Variable->GetType(), Prefix, VariableDesc.StartOffset, Uniform, Output->Members ) )
                    return FALSE;
            }
        }
    }

    if( Shader.StageShort == "cs" )
    {
        if( Script.BindingModel != SHADER_BINDING_SDL || !Output )
        {
            return FALSE;
        }

        return ReflectComputeResources( *Reflection.Get(), Shader, Bindings, *Output );
    }

    Resources = shader_resource_counts();

    u32 ResourceSpace = 0;
    u32 UniformSpace  = 0;

    if( Script.BindingModel == SHADER_BINDING_SDL )
    {
        if( Shader.StageShort == "vs" )
        {
            ResourceSpace = 0;
            UniformSpace  = 1;
        }
        else if( Shader.StageShort == "ps" )
        {
            ResourceSpace = 2;
            UniformSpace  = 3;
        }

        // TODO: GS: Implement this.
        else
        {
            x_printf( "Error: SDL graphics reflection is not defined for stage '%s'\n",
                      (const char*)Shader.Stage );
            return FALSE;
        }
    }

    for( s32 i = 0; i < Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        const u32                           End     = BindingEnd( Binding );

        if( Binding.Type == D3D_SIT_SAMPLER )
        {
            if( ( Script.BindingModel == SHADER_BINDING_SDL ) && ( Binding.Space != ResourceSpace ) )
            {
                x_printf( "Error: SDL sampler '%s' uses space%u; expected space%u in shader %s\n",
                          Binding.Name, Binding.Space, ResourceSpace, (const char*)Shader.Name );
                return FALSE;
            }

            Resources.SamplerCount = MAX( Resources.SamplerCount, End );
        }
        else if( Binding.Type == D3D_SIT_CBUFFER )
        {
            if( ( Script.BindingModel == SHADER_BINDING_SDL ) && ( Binding.Space != UniformSpace ) )
            {
                x_printf( "Error: SDL uniform buffer '%s' uses space%u; expected space%u in shader %s\n",
                          Binding.Name, Binding.Space, UniformSpace, (const char*)Shader.Name );
                return FALSE;
            }

            Resources.UniformBufferCount = MAX( Resources.UniformBufferCount, End );
            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_UNIFORM_BUFFER,
                                  Binding.Name,
                                  Binding.BindPoint,
                                  Binding.BindCount );
        }
    }

    if( ( Script.BindingModel == SHADER_BINDING_SDL ) && !Script.DiscoverBindings && ( Resources.UniformBufferCount > 4 ) )
    {
        x_printf( "Error: SDL supports at most four uniform buffers per graphics stage; shader %s needs %u\n",
                  (const char*)Shader.Name,
                  Resources.UniformBufferCount );
        return FALSE;
    }

    for( s32 i = 0; i < Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];

        if( Binding.Type != D3D_SIT_TEXTURE )
            continue;

        const u32 End = BindingEnd( Binding );

        if( IsSampledTexture( Bindings, Binding ) )
        {
            if( End > Resources.SamplerCount )
            {
                x_printf( "Error: Sampled texture '%s' at slot %u has no matching sampler in shader %s\n",
                          Binding.Name, End - 1, (const char*)Shader.Name );
                return FALSE;
            }

            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_SAMPLED_TEXTURE,
                                  Binding.Name,
                                  Binding.BindPoint,
                                  Binding.BindCount );
        }
        else if( Script.BindingModel == SHADER_BINDING_SDL )
        {
            if( ( Binding.Space != ResourceSpace ) || ( Binding.BindPoint < Resources.SamplerCount ) )
            {
                x_printf( "Error: SDL storage texture '%s' must follow %u sampled textures in space%u in shader %s\n",
                          Binding.Name, Resources.SamplerCount, ResourceSpace, (const char*)Shader.Name );
                return FALSE;
            }

            Resources.StorageTextureCount = MAX( Resources.StorageTextureCount,
                                                 End - Resources.SamplerCount );
            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_STORAGE_TEXTURE,
                                  Binding.Name,
                                  Binding.BindPoint - Resources.SamplerCount,
                                  Binding.BindCount );
        }
        else if( Script.BindingModel == SHADER_BINDING_NATIVE )
        {
            Resources.StorageTextureCount = MAX( Resources.StorageTextureCount, End );
            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_STORAGE_TEXTURE,
                                  Binding.Name,
                                  Binding.BindPoint,
                                  Binding.BindCount );
        }
    }

    const u32 StorageBufferBase = Resources.SamplerCount + Resources.StorageTextureCount;

    for( s32 i = 0; i < Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        const u32                           End     = BindingEnd( Binding );

        if( IsStorageBufferType( Binding.Type ) )
        {
            if( Script.BindingModel == SHADER_BINDING_SDL )
            {
                if( IsReadWriteStorageBufferType( Binding.Type ) )
                {
                    x_printf( "Error: SDL graphics shaders cannot use read-write storage buffer '%s'\n",
                              Binding.Name );
                    return FALSE;
                }

                if( ( Binding.Space != ResourceSpace ) || ( Binding.BindPoint < StorageBufferBase ) )
                {
                    x_printf( "Error: SDL storage buffer '%s' must follow sampled/storage textures at t%u in space%u in shader %s\n",
                              Binding.Name, StorageBufferBase, ResourceSpace, (const char*)Shader.Name );
                    return FALSE;
                }

                Resources.StorageBufferCount = MAX( Resources.StorageBufferCount,
                                                    End - StorageBufferBase );
                AddReflectionBinding( OutBindings,
                                      SHADER_REFLECT_STORAGE_BUFFER,
                                      Binding.Name,
                                      Binding.BindPoint - StorageBufferBase,
                                      Binding.BindCount );
            }
            else if( Script.BindingModel == SHADER_BINDING_NATIVE )
            {
                Resources.StorageBufferCount = MAX( Resources.StorageBufferCount, End );
                AddReflectionBinding( OutBindings,
                                      SHADER_REFLECT_STORAGE_BUFFER,
                                      Binding.Name,
                                      Binding.BindPoint,
                                      Binding.BindCount );
            }
        }
        else if( IsStorageTextureType( Binding.Type ) )
        {
            if( Script.BindingModel == SHADER_BINDING_SDL )
            {
                x_printf( "Error: SDL graphics shaders cannot use UAV resource '%s'\n", Binding.Name );
                return FALSE;
            }

            Resources.StorageTextureCount = MAX( Resources.StorageTextureCount, End );
        }
    }

    if( Script.BindingModel == SHADER_BINDING_SDL && !Script.DiscoverBindings )
    {
        for( u32 Slot = 0; Slot < Resources.SamplerCount; Slot++ )
        {
            if( !HasSamplerSlot( Bindings, Slot, ResourceSpace ) ||
                !HasTextureSlot( Bindings, Slot, ResourceSpace ) )
            {
                x_printf( "Error: SDL sampled-texture slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }

        for( u32 Slot = 0; Slot < Resources.UniformBufferCount; Slot++ )
        {
            if( !HasUniformBufferSlot( Bindings, Slot, UniformSpace ) )
            {
                x_printf( "Error: SDL uniform-buffer slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }

        for( u32 Slot = 0; Slot < Resources.StorageTextureCount; Slot++ )
        {
            const u32 BindingSlot = Resources.SamplerCount + Slot;

            if( !HasTextureSlot( Bindings, BindingSlot, ResourceSpace ) ||
                HasSamplerSlot( Bindings, BindingSlot, ResourceSpace ) )
            {
                x_printf( "Error: SDL storage-texture slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }

        for( u32 Slot = 0; Slot < Resources.StorageBufferCount; Slot++ )
        {
            const u32 BindingSlot = StorageBufferBase + Slot;

            if( !HasStorageBufferSlot( Bindings, BindingSlot, ResourceSpace ) )
            {
                x_printf( "Error: SDL storage-buffer slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool CompileDxc( const shader_script&   Script,
                         const shader_entry&    Shader,
                         const compile_options& Options,
                         const char*            pTarget,
                         dxc_ptr<IDxcUtils>&    Utils,
                         dxc_ptr<IDxcResult>&   Result,
                         xarray<xstring>&       Args )
{
    BuildDxcArgs( Script, Shader, pTarget, PdbPath( Script, Shader ), Args );

    if( Options.Verbose )
    {
        x_printf( "  DXC %s:", pTarget );

        for( s32 i = 0; i < Args.GetCount(); i++ )
            x_printf( " %s", (const char*)Args[i] );
        x_printf( "\n" );
    }

    xarray<byte>        Source;
    const xarray<byte>* pSource = NULL;

    if( Script.Sources )
    {
        const std::string Path = std::filesystem::weakly_canonical( (const char*)Shader.Source ).string();

        for( s32 i = 0; i < Script.Sources->Files.GetCount(); ++i )
            if( Script.Sources->Files[i].Path == Path.c_str() )
            {
                pSource = &Script.Sources->Files[i].Bytes;
                break;
            }
    }

    if( !pSource && !LoadBinaryFile( Shader.Source, Source ) )
    {
        x_printf( "Error: Failed to read shader source %s\n", (const char*)Shader.Source );
        return FALSE;
    }

    HRESULT hr = S_OK;

    if( !s_DxcWorker.Utils.Get() )
        hr = DxcCreateInstance( CLSID_DxcUtils, __uuidof( IDxcUtils ), (void**)s_DxcWorker.Utils.Put() );
    if( SUCCEEDED( hr ) && !s_DxcWorker.Compiler.Get() )
        hr = DxcCreateInstance( CLSID_DxcCompiler, __uuidof( IDxcCompiler3 ), (void**)s_DxcWorker.Compiler.Put() );
    if( SUCCEEDED( hr ) && !s_DxcWorker.IncludeHandler.Get() )
        hr = s_DxcWorker.Utils->CreateDefaultIncludeHandler( s_DxcWorker.IncludeHandler.Put() );
    if( FAILED( hr ) )
    {
        x_printf( "Error: DXC worker initialization failed: 0x%08X\n", (u32)hr );
        return FALSE;
    }

    IDxcIncludeHandler* pIncludes = s_DxcWorker.IncludeHandler.Get();

    if( Script.Sources )
    {
        if( !s_DxcWorker.SnapshotHandler.Get() )
            *s_DxcWorker.SnapshotHandler.Put() = new dxc_snapshot_include( s_DxcWorker.Utils.Get() );
        static_cast<dxc_snapshot_include*>( s_DxcWorker.SnapshotHandler.Get() )->SetSources( Script.Sources.Get() );
        pIncludes = s_DxcWorker.SnapshotHandler.Get();
    }
    *Utils.Put() = s_DxcWorker.Utils.Get();
    Utils->AddRef();

    std::vector<std::wstring> WideArgs;
    std::vector<LPCWSTR>      ArgPtrs;
    WideArgs.reserve( Args.GetCount() );
    ArgPtrs.reserve( Args.GetCount() );

    for( s32 i = 0; i < Args.GetCount(); i++ )
        WideArgs.push_back( ToWide( Args[i] ) );
    for( size_t i = 0; i < WideArgs.size(); i++ )
        ArgPtrs.push_back( WideArgs[i].c_str() );

    std::wstring SourceName = ToWide( Shader.Source );
    std::wstring Entry      = ToWide( Shader.Entry );
    std::wstring Profile    = ToWide( Shader.CompileCapabilities ? Shader.StageShort + "_6_2" : Shader.Profile );

    dxc_ptr<IDxcCompilerArgs> CompilerArgs;
    hr = Utils->BuildArguments( SourceName.c_str(),
                                Entry.c_str(),
                                Profile.c_str(),
                                ArgPtrs.empty() ? NULL : &ArgPtrs[0],
                                (UINT32)ArgPtrs.size(),
                                NULL,
                                0,
                                CompilerArgs.Put() );
    if( FAILED( hr ) )
    {
        x_printf( "Error: DXC BuildArguments failed: 0x%08X\n", (u32)hr );
        return FALSE;
    }

    DxcBuffer SourceBuffer;
    SourceBuffer.Ptr      = pSource ? pSource->GetPtr() : Source.GetPtr();
    SourceBuffer.Size     = pSource ? pSource->GetCount() : Source.GetCount();
    SourceBuffer.Encoding = DXC_CP_UTF8;

    const xtick CompileStart = x_GetTime();
    hr                       = s_DxcWorker.Compiler->Compile( &SourceBuffer,
                                                              CompilerArgs->GetArguments(),
                                                              CompilerArgs->GetCount(),
                                                              pIncludes,
                                                              __uuidof( IDxcResult ),
                                                              (void**)Result.Put() );
    if( Script.Metrics )
    {
        const f64 Elapsed = x_TicksToMs( x_GetTime() - CompileStart );

        if( Script.DiscoverBindings )
        {
            ++Script.Metrics->DiscoveryCalls;
            Script.Metrics->DiscoveryMs += Elapsed;
        }
        else if( x_stricmp( pTarget, "vulkan" ) == 0 )
        {
            ++Script.Metrics->SpirvCalls;
            Script.Metrics->SpirvMs += Elapsed;
        }
        else
        {
            ++Script.Metrics->DxilCalls;
            Script.Metrics->DxilMs += Elapsed;
        }
    }

    if( FAILED( hr ) || !Result.Get() )
    {
        x_printf( "Error: DXC failed to start %s compile: 0x%08X\n", pTarget, (u32)hr );
        return FALSE;
    }

    PrintErrors( Result.Get() );

    HRESULT Status = S_OK;
    hr             = Result->GetStatus( &Status );

    if( FAILED( hr ) || FAILED( Status ) )
    {
        x_printf( "Error: %s compile failed: 0x%08X\n", pTarget, (u32)( FAILED( hr ) ? hr : Status ) );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool CompileDxcTarget( const shader_script&   Script,
                        const shader_entry&    Shader,
                        const compile_options& Options,
                        const char*            pTarget,
                        shader_compile_output& Output,
                        xbool                  Reflect )
{
    Output                      = shader_compile_output();
    Output.Target               = pTarget;
    Output.RequiredCapabilities = Shader.CompileCapabilities;
    Output.ShaderName           = Shader.Name;
    Output.Pdb                  = PdbPath( Script, Shader );
    Output.TempPdb              = TempPathFor( Output.Pdb );

    if( Options.Verbose )
        x_printf( "  compile %s\n", pTarget );

    xarray<xstring>     Args;
    dxc_ptr<IDxcUtils>  Utils;
    dxc_ptr<IDxcResult> Result;

    if( !CompileDxc( Script, Shader, Options, pTarget, Utils, Result, Args ) )
        return FALSE;

    if( Reflect && x_stricmp( pTarget, "d3d12" ) == 0 )
    {
        if( !ReflectResources( Utils.Get(), Result.Get(), Script, Shader, Output.Resources, Output.Bindings, &Output ) )
            return FALSE;
    }

    dxc_ptr<IDxcBlob> Object;
    HRESULT           hr = Result->GetOutput( DXC_OUT_OBJECT,
                                              __uuidof( IDxcBlob ),
                                              (void**)Object.Put(),
                                              NULL );
    if( FAILED( hr ) || !Object.Get() )
    {
        x_printf( "Error: %s produced no object output\n", pTarget );
        return FALSE;
    }

    const size_t ObjectSize = Object->GetBufferSize();

    if( ( ObjectSize == 0 ) || ( ObjectSize > 0x7FFFFFFFu ) )
    {
        x_printf( "Error: Invalid %s object size\n", pTarget );
        return FALSE;
    }

    Output.Code.SetCount( (s32)ObjectSize );
    x_memcpy( Output.Code.GetPtr(), Object->GetBufferPointer(), ObjectSize );

    if( Reflect && x_stricmp( pTarget, "vulkan" ) == 0 && !ReflectSpirvShader( Script, Shader, Output ) )
    {
        x_printf( "Error: final SPIR-V reflection failed for %s\n", (const char*)Shader.Name );
        return FALSE;
    }

    if( ( x_stricmp( pTarget, "d3d12" ) == 0 ) && Result->HasOutput( DXC_OUT_PDB ) )
    {
        dxc_ptr<IDxcBlob> PdbBlob;
        hr = Result->GetOutput( DXC_OUT_PDB,
                                __uuidof( IDxcBlob ),
                                (void**)PdbBlob.Put(),
                                NULL );
        if( FAILED( hr ) || !PdbBlob.Get() || !PdbBlob->GetBufferSize() )
        {
            x_printf( "Error: DXC reported an invalid PDB for %s\n", (const char*)Shader.Name );
            return FALSE;
        }

        RemoveFile( Output.TempPdb );

        if( !WriteBlob( Output.TempPdb, PdbBlob.Get() ) )
        {
            x_printf( "Error: Failed to write temp PDB %s\n", (const char*)Output.TempPdb );
            return FALSE;
        }

        Output.HasPdb = TRUE;
    }

    return TRUE;
}

//==============================================================================

xbool ReflectDxcShader( const shader_script&               Script,
                        const shader_entry&                Shader,
                        const compile_options&             Options,
                        shader_resource_counts&            Resources,
                        xarray<shader_reflection_binding>& Bindings )
{
    xarray<xstring>     Args;
    dxc_ptr<IDxcUtils>  Utils;
    dxc_ptr<IDxcResult> Result;

    if( !CompileDxc( Script, Shader, Options, "d3d12", Utils, Result, Args ) )
        return FALSE;

    return ReflectResources( Utils.Get(), Result.Get(), Script, Shader, Resources, Bindings );
}

#else

xbool CompileDxcTarget( const shader_script&,
                        const shader_entry&,
                        const compile_options&,
                        const char*,
                        shader_compile_output&,
                        xbool )
{
    x_printf( "Error: DXC API backend is not configured for this platform.\n" );
    return FALSE;
}

//==============================================================================

xbool ReflectDxcShader( const shader_script&,
                        const shader_entry&,
                        const compile_options&,
                        shader_resource_counts&,
                        xarray<shader_reflection_binding>& )
{
    x_printf( "Error: DXC reflection backend is not configured for this platform.\n" );
    return FALSE;
}

#endif
