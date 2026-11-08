//==============================================================================
//
//  ShaderCache.cpp
//
//  Target reflection and bytecode are cached independently of package layout.
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderTool.hpp"
#include <vector>

#include <filesystem>
#include <algorithm>

namespace
{
//==============================================================================
//  CACHE METADATA SERIALIZATION
//==============================================================================

struct Writer
{
    xarray<byte> Data;
    xbool        Valid = TRUE;

    void Append( const void* pData, u32 Size )
    {
        const u64 Count = static_cast<u64>( Data.GetCount() ) + Size;

        if( !Valid || Count > ECS_PACKAGE_MAX_METADATA )
        {
            Valid = FALSE;
            return;
        }

        if( Count > static_cast<u64>( Data.GetCapacity() ) )
            Data.SetCapacity( static_cast<s32>( MIN( static_cast<u64>( ECS_PACKAGE_MAX_METADATA ),
                                                     MAX( Count, static_cast<u64>( Data.GetCapacity() ) + MAX( 128, Data.GetCapacity() / 2 ) ) ) ) );
        const s32 Offset = Data.GetCount();
        Data.SetCount( static_cast<s32>( Count ) );

        if( Size )
            x_memcpy( Data.GetPtr() + Offset, pData, Size );
    }

    void U32( u32 Value )
    {
        byte Bytes[4];
        x_store_le32( Bytes, Value );
        Append( Bytes, sizeof( Bytes ) );
    }

    void String( const xstring& Value )
    {
        U32( Value.GetLength() );
        Append( (const char*)Value, Value.GetLength() );
    }
};

struct Reader
{
    const byte* Data;
    size_t      Size;
    size_t      Position = 0;
    bool        Valid    = true;

    u32 U32()
    {
        if( Position + 4 > Size )
        {
            Valid = false;
            return 0;
        }

        u32 Value = x_load_le32( Data + Position );
        Position += 4;
        return Value;
    }

    u32 Count( u32 Limit )
    {
        const u32 Value = U32();

        if( Value > Limit )
        {
            Valid = false;
            return 0;
        }

        return Value;
    }

    void String( xstring& Value )
    {
        u32 Length = Count( 4096 );

        if( Position + Length > Size )
        {
            Valid = false;
            return;
        }

        std::vector<char> Text( Length + 1, 0 );

        if( Length )
            x_memcpy( Text.data(), Data + Position, Length );
        Position += Length;
        Value = Text.data();
    }
};

//==============================================================================

void WriteVariables( Writer& File, const xarray<shader_interface_variable>& Variables )
{
    File.U32( Variables.GetCount() );

    for( s32 i = 0; i < Variables.GetCount(); ++i )
    {
        File.String( Variables[i].Semantic );
        File.U32( Variables[i].Index );
        File.U32( Variables[i].Components );
        File.U32( Variables[i].ComponentType );
    }
}

//==============================================================================

void ReadVariables( Reader& File, xarray<shader_interface_variable>& Variables )
{
    const u32 Count = File.Count( 256 );
    Variables.SetCount( Count );

    for( u32 i = 0; i < Count; ++i )
    {
        File.String( Variables[i].Semantic );
        Variables[i].Index         = File.U32();
        Variables[i].Components    = File.U32();
        Variables[i].ComponentType = File.U32();
    }
}

//==============================================================================

xstring CachePath( const shader_script& Script, const shader_entry& Shader, const char* pTarget, u32 Capabilities = 0 )
{
    if( Capabilities )
        return JoinPath( JoinPath( Script.CacheRoot.IsEmpty() ? Script.OutRoot : Script.CacheRoot, Script.Config ), xstring( xfs( "%s.%s.%s.fp16.cache", (const char*)Shader.Name, (const char*)Shader.StageShort, pTarget ) ) );

    return JoinPath( JoinPath( Script.CacheRoot.IsEmpty() ? Script.OutRoot : Script.CacheRoot, Script.Config ), xstring( xfs( "%s.%s.%s.cache", (const char*)Shader.Name, (const char*)Shader.StageShort, pTarget ) ) );
}

//==============================================================================

x_content_id CacheKey( const shader_script& Script, const shader_entry& Shader,
                         const compile_options& Options, const char* pTarget )
{
    // Hash compiler inputs only. Package layout changes do not alter stage code.
    Writer Inputs;
    Inputs.U32( 5 );
    Inputs.U32( Shader.ColorArithmetic );
    Inputs.U32( Shader.CompileCapabilities );
    Inputs.U32( static_cast<u32>( Script.AbiId ) );
    Inputs.U32( static_cast<u32>( Script.AbiId >> 32 ) );
    Inputs.U32( static_cast<u32>( Options.CompilerFingerprint ) );
    Inputs.U32( static_cast<u32>( Options.CompilerFingerprint >> 32 ) );
    Inputs.U32( Script.BindingModel );
    Inputs.U32( Script.DiscoverBindings ? 1 : 0 );
    Inputs.String( Script.Config );
    Inputs.String( pTarget );
    Inputs.String( Shader.Name );
    Inputs.String( std::filesystem::weakly_canonical( (const char*)Shader.Source ).string().c_str() );

    if( Script.Config == "debug" )
        Inputs.String( Shader.Source );
    Inputs.String( Shader.Stage );
    Inputs.String( Shader.Entry );
    Inputs.String( Shader.Profile );
    Inputs.String( Shader.CommonArgs );
    Inputs.String( Shader.VulkanArgs );
    Inputs.String( Shader.D3D12Args );
    Inputs.Append( Shader.SourceContent.GetPtr(), 32 );
    x_content_hash Hash;
    Hash.Update( Inputs.Data.GetPtr(), Inputs.Data.GetCount() );

    if( Script.Sources )
        Hash.Update( Script.Sources->Generated.GetPtr(), Script.Sources->Generated.GetCount() );
    return Hash.Finish();
}
}

//==============================================================================

static
u64 HashCompilerFile( const xstring& File )
{
    X_FILE* pInput = x_fopen( File, "rb" );

    if( !pInput )
        return 0;

    byte Bytes[64 * 1024];
    u64  Hash = 14695981039346656037ull;
    s32  Count;

    while( ( Count = x_fread( Bytes, 1, sizeof( Bytes ), pInput ) ) > 0 )
        for( s32 i = 0; i < Count; ++i )
            Hash = ( Hash ^ Bytes[i] ) * 1099511628211ull;
    const xbool Complete = x_ftell( pInput ) == x_flength( pInput );
    x_fclose( pInput );

    if( !Complete )
        return 0;

    return Hash;
}

//==============================================================================

u64 ShaderCompilerFingerprint()
{
    const u64 Library = HashCompilerFile( A51_DXC_LIBRARY_FILE );

    if( !Library )
        return 0;

    // Bump when compilation/binding discovery/reflection changes. Packaging and
    // catalog reports must not invalidate unchanged compiler intermediates.
    constexpr u64 CompileContractRevision = 0x202610310001ull;
    return Library ^ CompileContractRevision;
}

namespace
{
xbool ReadCachedBlob( const xstring& Path, Reader& File, xarray<byte>& Bytes, xbool Retain )
{
    // The metadata records both the byte count and the content identity.
    const u32      Size = File.Count( 128 * 1024 * 1024 );
    const u32      CRC  = File.U32();
    x_content_id Identity;

    for( u32 i = 0; i < 8; ++i )
        x_store_le32( Identity.GetPtr() + i * 4, File.U32() );
    if( !File.Valid )
        return FALSE;

    X_FILE* pInput = x_fopen( Path, "rb" );

    if( !pInput )
        return FALSE;

    if( x_flength( pInput ) != static_cast<s32>( Size ) )
    {
        x_fclose( pInput );
        return FALSE;
    }

    // Validation-only callers stream the blob without retaining its bytecode.
    if( Retain )
    {
        Bytes.SetCount( Size );
        const xbool Read = x_fread( Bytes.GetPtr(), 1, Size, pInput ) == static_cast<s32>( Size );
        x_fclose( pInput );
        return Read && x_chksum( Bytes.GetPtr(), Size ) == CRC && x_HashContent( Bytes.GetPtr(), Size ) == Identity;
    }

    x_content_hash Hash;
    byte             Buffer[64 * 1024];
    u32              Remaining = Size;

    while( Remaining )
    {
        const u32 Count = MIN( Remaining, static_cast<u32>( sizeof( Buffer ) ) );

        if( x_fread( Buffer, 1, Count, pInput ) != static_cast<s32>( Count ) )
        {
            x_fclose( pInput );
            return FALSE;
        }

        Hash.Update( Buffer, Count );
        Remaining -= Count;
    }

    x_fclose( pInput );
    return Hash.Finish() == Identity;
}

//==============================================================================

xbool WriteCachedBlob( const xstring& Path, Writer& File, const xarray<byte>& Bytes )
{
    File.U32( Bytes.GetCount() );
    File.U32( x_chksum( Bytes.GetPtr(), Bytes.GetCount() ) );
    const x_content_id Identity = x_HashContent( Bytes.GetPtr(), Bytes.GetCount() );

    for( u32 i = 0; i < 8; ++i )
        File.U32( x_load_le32( Identity.GetPtr() + i * 4 ) );
    const xstring Temporary = TempPathFor( Path );
    return WriteBinaryFile( Temporary, Bytes.GetPtr(), Bytes.GetCount() ) && ReplaceFile( Temporary, Path );
}
}

//==============================================================================

xbool LoadShaderCache( const shader_script& Script, const shader_entry& Shader,
                       const compile_options& Options, xarray<shader_compile_output>& Outputs, xbool LoadCode, xbool RestorePdb )
{
    Outputs.Clear();

    if( Options.Clean || !Options.CompilerFingerprint )
        return FALSE;

    for( u32 iTarget = 0; iTarget < 3; ++iTarget )
    {
        const u32 Target = iTarget == 0 ? SHADER_TARGET_D3D12 : SHADER_TARGET_VULKAN;

        if( !( Script.TargetMask & Target ) || ( iTarget == 2 && ( !Shader.ColorArithmetic || Script.DiscoverBindings ) ) )
            continue;
        shader_entry Variant        = Shader;
        Variant.CompileCapabilities = iTarget == 2 ? ECS_SHADER_CAPABILITY_FLOAT16 : 0;
        const char*   pTarget       = Target == SHADER_TARGET_VULKAN ? "vulkan" : "d3d12";
        const xstring Path          = CachePath( Script, Shader, pTarget, Variant.CompileCapabilities );
        xarray<byte>  Bytes;

        if( !LoadBinaryFile( Path, Bytes ) || Bytes.GetCount() < 44 )
            return FALSE;

        const byte*          Header = Bytes.GetPtr();
        const x_content_id Key    = CacheKey( Script, Variant, Options, pTarget );

        if( x_load_le32( Header ) != 0x34434153 || x_memcmp( Header + 4, Key.GetPtr(), 32 ) ||
            x_load_le32( Header + 36 ) != static_cast<u32>( Bytes.GetCount() - 44 ) ||
            x_load_le32( Header + 40 ) != x_chksum( Header + 44, Bytes.GetCount() - 44 ) )
            return FALSE;

        Reader                 File   = { Header + 44, static_cast<size_t>( Bytes.GetCount() - 44 ) };
        shader_compile_output& Output = Outputs.Append();
        Output.RequiredCapabilities   = Variant.CompileCapabilities;
        File.String( Output.Target );
        File.String( Output.ShaderName );
        const size_t CodePosition = File.Position;

        if( !ReadCachedBlob( Path + ".code", File, Output.Code, LoadCode ) )
            return FALSE;

        Output.BinarySize                    = x_load_le32( File.Data + CodePosition );
        Output.Resources.SamplerCount        = File.U32();
        Output.Resources.UniformBufferCount  = File.U32();
        Output.Resources.StorageTextureCount = File.U32();
        Output.Resources.StorageBufferCount  = File.U32();
        const u32 Bindings                   = File.Count( 256 );
        Output.Bindings.SetCount( Bindings );

        for( u32 j = 0; j < Bindings; ++j )
        {
            shader_reflection_binding& Binding = Output.Bindings[j];
            Binding.Type                       = (shader_reflection_binding_type)File.U32();
            File.String( Binding.Name );
            Binding.Slot     = File.U32();
            Binding.Count    = File.U32();
            Binding.ByteSize = File.U32();
        }

        ReadVariables( File, Output.Inputs );
        ReadVariables( File, Output.Outputs );
        const u32 Members = File.Count( 8192 );
        Output.Members.SetCount( Members );

        for( u32 j = 0; j < Members; ++j )
        {
            shader_buffer_member& Member = Output.Members[j];
            File.String( Member.Buffer );
            File.String( Member.Name );
            Member.Offset       = File.U32();
            Member.Size         = File.U32();
            Member.ArrayStride  = File.U32();
            Member.MatrixStride = File.U32();
        }

        const u32 HasPdb = File.Count( 1 );

        if( HasPdb )
        {
            xarray<byte> Pdb;

            if( !ReadCachedBlob( Path + ".pdb", File, Pdb, !LoadCode || RestorePdb ) )
                return FALSE;

            Output.Pdb = PdbPath( Script, Shader );

            if( LoadCode && RestorePdb )
            {
                Output.TempPdb = TempPathFor( Output.Pdb );
                Output.HasPdb  = TRUE;

                if( !WriteBinaryFile( Output.TempPdb, Pdb.GetPtr(), Pdb.GetCount() ) )
                    return FALSE;
            }
            else if( !LoadCode && !WriteBinaryFile( Output.Pdb, Pdb.GetPtr(), Pdb.GetCount() ) )
                return FALSE;
        }

        if( !File.Valid || File.Position != File.Size || Output.ShaderName != Shader.Name ||
            Output.Target != pTarget || !Output.BinarySize )
            return FALSE;
    }

    return Outputs.GetCount() != 0;
}

//==============================================================================

xbool WriteShaderCache( const shader_script& Script, const shader_entry& Shader,
                        const compile_options& Options, const xarray<shader_compile_output>& Outputs )
{
    for( s32 i = 0; i < Outputs.GetCount(); ++i )
    {
        const shader_compile_output& Output = Outputs[i];

        if( !Output.Code.GetCount() )
            continue;
        shader_entry Variant        = Shader;
        Variant.CompileCapabilities = Output.RequiredCapabilities;
        const xstring Path          = CachePath( Script, Shader, Output.Target, Output.RequiredCapabilities );
        Writer        File;
        File.String( Output.Target );
        File.String( Output.ShaderName );

        if( !WriteCachedBlob( Path + ".code", File, Output.Code ) )
            return FALSE;

        File.U32( Output.Resources.SamplerCount );
        File.U32( Output.Resources.UniformBufferCount );
        File.U32( Output.Resources.StorageTextureCount );
        File.U32( Output.Resources.StorageBufferCount );
        File.U32( Output.Bindings.GetCount() );

        for( s32 j = 0; j < Output.Bindings.GetCount(); ++j )
        {
            const shader_reflection_binding& Binding = Output.Bindings[j];
            File.U32( Binding.Type );
            File.String( Binding.Name );
            File.U32( Binding.Slot );
            File.U32( Binding.Count );
            File.U32( Binding.ByteSize );
        }

        WriteVariables( File, Output.Inputs );
        WriteVariables( File, Output.Outputs );
        File.U32( Output.Members.GetCount() );

        for( s32 j = 0; j < Output.Members.GetCount(); ++j )
        {
            const shader_buffer_member& Member = Output.Members[j];
            File.String( Member.Buffer );
            File.String( Member.Name );
            File.U32( Member.Offset );
            File.U32( Member.Size );
            File.U32( Member.ArrayStride );
            File.U32( Member.MatrixStride );
        }

        File.U32( Output.HasPdb ? 1 : 0 );

        if( Output.HasPdb )
        {
            xarray<byte> Pdb;

            if( ( !LoadBinaryFile( Output.TempPdb, Pdb ) && !LoadBinaryFile( Output.Pdb, Pdb ) ) || !WriteCachedBlob( Path + ".pdb", File, Pdb ) )
                return FALSE;
        }

        if( !File.Valid )
            return FALSE;

        xarray<byte> Bytes;
        Bytes.SetCount( 44 + File.Data.GetCount() );
        x_store_le32( Bytes.GetPtr(), 0x34434153 );
        const x_content_id Key = CacheKey( Script, Variant, Options, Output.Target );
        x_memcpy( Bytes.GetPtr() + 4, Key.GetPtr(), 32 );
        x_store_le32( Bytes.GetPtr() + 36, File.Data.GetCount() );
        x_store_le32( Bytes.GetPtr() + 40, x_chksum( File.Data.GetPtr(), File.Data.GetCount() ) );
        x_memcpy( Bytes.GetPtr() + 44, File.Data.GetPtr(), File.Data.GetCount() );
        const xstring Temporary = TempPathFor( Path );

        if( !WriteBinaryFile( Temporary, Bytes.GetPtr(), Bytes.GetCount() ) || !ReplaceFile( Temporary, Path ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

namespace
{
xbool CollectKitFiles( const xstring& Folder, std::vector<std::filesystem::path>& Files, xbool IncludeManifest )
{
    std::error_code Error;

    if( !std::filesystem::is_directory( (const char*)Folder, Error ) || Error )
        return FALSE;

    for( std::filesystem::recursive_directory_iterator It( (const char*)Folder, Error ), End;
         !Error && It != End; It.increment( Error ) )
    {
        if( !It->is_regular_file( Error ) )
            continue;
        const std::string Name = It->path().filename().string();

        if( Name == ".buildstate" || ( !IncludeManifest && Name == "shaderkit.json" ) )
            continue;
        Files.push_back( It->path() );
    }

    if( Error || Files.empty() )
        return FALSE;

    std::sort( Files.begin(), Files.end() );
    return TRUE;
}

//==============================================================================

xbool HashKitFile( const std::filesystem::path& Path, x_content_id& Digest )
{
    X_FILE* pInput = x_fopen( Path.string().c_str(), "rb" );

    if( !pInput )
        return FALSE;

    x_content_hash Hash;
    byte             Buffer[64 * 1024];
    s32              Count;

    while( ( Count = x_fread( Buffer, 1, sizeof( Buffer ), pInput ) ) > 0 )
        Hash.Update( Buffer, Count );
    const xbool Complete = x_ftell( pInput ) == x_flength( pInput );
    x_fclose( pInput );

    if( !Complete )
        return FALSE;

    Digest = Hash.Finish();
    return TRUE;
}

//==============================================================================

xbool KitState( const shader_script& Script, xarray<byte>& State )
{
    Writer File;
    File.U32( 6 );
    File.U32( static_cast<u32>( Script.CatalogId ) );
    File.U32( static_cast<u32>( Script.CatalogId >> 32 ) );
    File.U32( Script.TargetMask );
    File.String( Script.Config );
    compile_options Options;
    Options.CompilerFingerprint = Script.CompilerFingerprint;
    x_content_hash Inputs;

    for( s32 i = 0; i < Script.Shaders.GetCount(); ++i )
    {
        const x_content_id Key = CacheKey( Script, Script.Shaders[i], Options, "kit" );
        Inputs.Update( Key.GetPtr(), 32 );
    }

    const x_content_id InputIdentity = Inputs.Finish();
    File.Append( InputIdentity.GetPtr(), 32 );
    const xstring                      Folder = JoinPath( Script.OutRoot, Script.Config );
    std::vector<std::filesystem::path> Files;

    if( !CollectKitFiles( Folder, Files, TRUE ) )
        return FALSE;

    for( const auto& Path : Files )
    {
        File.String( Path.lexically_relative( (const char*)Folder ).generic_string().c_str() );
        x_content_id Digest;

        if( !HashKitFile( Path, Digest ) )
            return FALSE;

        File.Append( Digest.GetPtr(), 32 );
    }

    if( !File.Valid )
        return FALSE;

    State.SetCount( File.Data.GetCount() );
    x_memcpy( State.GetPtr(), File.Data.GetPtr(), State.GetCount() );
    return TRUE;
}
}

//==============================================================================

xbool ShaderKitIsCurrent( const shader_script& Script )
{
    xarray<byte> Expected, Actual;

    if( !LoadBinaryFile( JoinPath( JoinPath( Script.OutRoot, Script.Config ), ".buildstate" ), Expected ) ||
        !KitState( Script, Actual ) || Expected.GetCount() != Actual.GetCount() )
        return FALSE;

    return !x_memcmp( Expected.GetPtr(), Actual.GetPtr(), Actual.GetCount() );
}

//==============================================================================

xbool WriteShaderKitState( const shader_script& Script )
{
    xarray<byte> State;
    return KitState( Script, State ) &&
           WriteBinaryFile( JoinPath( JoinPath( Script.OutRoot, Script.Config ), ".buildstate" ), State.GetPtr(), State.GetCount() );
}

//==============================================================================

xbool WriteShaderKitManifest( const shader_script& Script )
{
    const xstring                      Folder = JoinPath( Script.OutRoot, Script.Config );
    std::vector<std::filesystem::path> Files;

    if( !CollectKitFiles( Folder, Files, FALSE ) )
        return FALSE;

    xstring Json( xfs( "{\n  \"format\": 1,\n  \"catalog_id\": \"%016llx\",\n  \"config\": \"%s\",\n  \"files\": [\n",
                       (unsigned long long)Script.CatalogId, (const char*)Script.Config ) );
    for( size_t i = 0; i < Files.size(); ++i )
    {
        x_content_id Digest;

        if( !HashKitFile( Files[i], Digest ) )
            return FALSE;

        xstring Hex;

        for( u32 j = 0; j < 32; ++j )
            Hex += xstring( xfs( "%02x", Digest[j] ) );
        const std::string Relative = Files[i].lexically_relative( (const char*)Folder ).generic_string();
        Json += xstring( xfs( "    {\"path\": \"%s\", \"sha256\": \"%s\"}%s\n",
                              Relative.c_str(), (const char*)Hex, i + 1 == Files.size() ? "" : "," ) );
    }

    Json += "  ]\n}\n";
    return WriteBinaryFile( JoinPath( Folder, "shaderkit.json" ), (const char*)Json, Json.GetLength() );
}
