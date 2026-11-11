//==============================================================================
//  Final SPIR-V reflection. DXIL reflection never substitutes for this path.
//==============================================================================
#include "ShaderTool.hpp"
#include "spirv_reflect.h"
#include "spirv-tools/libspirv.hpp"
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <map>

typedef std::map<u64, u32> matrix_strides;

static
void ReflectMembers( const char* Buffer, const SpvReflectBlockVariable& Block,
                            const xstring& Prefix, xarray<shader_buffer_member>& Members,
                            const matrix_strides& MatrixStrides, u32 BaseOffset = 0 )
{
    for( u32 i = 0; i < Block.member_count; ++i )
    {
        const SpvReflectBlockVariable& Variable = Block.members[i];
        shader_buffer_member&          Member   = Members.Append();
        Member.Buffer                           = Buffer;
        Member.Name                             = Prefix + ( Variable.name ? Variable.name : "" );
        Member.Offset                           = BaseOffset + Variable.offset;
        Member.Size                             = Variable.size;
        Member.ArrayStride                      = Variable.array.stride;

        if( Variable.type_description && Variable.type_description->op == SpvOpTypeRuntimeArray )
        {
            // BlockVariable omits runtime-array traits; the reflected type retains the final ArrayStride.
            Member.ArrayStride = Variable.type_description->traits.array.stride;
            Member.Size        = Member.ArrayStride;
        }

        Member.MatrixStride = Variable.numeric.matrix.stride;
        // SPIRV-Reflect drops the member MatrixStride decoration on matrix arrays.
        // Read that decoration from the validated final module, rather than infer it.
        if( !Member.MatrixStride && Block.type_description )
        {
            const SpvReflectTypeDescription* Type   = Block.type_description->struct_type_description ? Block.type_description->struct_type_description : Block.type_description;
            const auto                       Stride = MatrixStrides.find( ( static_cast<u64>( Type->id ) << 32 ) | i );

            if( Stride != MatrixStrides.end() )
                Member.MatrixStride = Stride->second;
        }

        const xstring ChildPrefix = Member.Name + ".";
        const u32     ChildOffset = Member.Offset;
        ReflectMembers( Buffer, Variable, ChildPrefix, Members, MatrixStrides, ChildOffset );
    }
}

//==============================================================================

static
void ReflectInterface( const SpvReflectInterfaceVariable& Variable,
                              xarray<shader_interface_variable>& Variables )
{
    if( Variable.member_count )
    {
        for( u32 i = 0; i < Variable.member_count; ++i )
            ReflectInterface( Variable.members[i], Variables );
        return;
    }

    if( Variable.built_in >= 0 )
        return;

    shader_interface_variable& Result = Variables.Append();
    Result.Semantic                   = Variable.semantic ? Variable.semantic : ( Variable.name ? Variable.name : "" );
    Result.Semantic.MakeUpper();
    s32 End   = Result.Semantic.GetLength();
    s32 Begin = End;

    while( Begin && Result.Semantic[Begin - 1] >= '0' && Result.Semantic[Begin - 1] <= '9' )
        --Begin;
    Result.Index         = Begin == End ? 0 : static_cast<u32>( std::strtoul( (const char*)Result.Semantic + Begin, NULL, 10 ) );
    Result.Semantic      = Result.Semantic.Left( Begin );
    Result.Components    = MAX( Variable.numeric.vector.component_count, 1u );
    Result.ComponentType = Variable.type_description &&
                                   ( Variable.type_description->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT )
                               ? 3
                               : ( Variable.numeric.scalar.signedness ? 2 : 1 );
}

//==============================================================================

xbool ReflectSpirvShader( const shader_script& Script, const shader_entry& Shader,
                          shader_compile_output& Output )
{
    spvtools::SpirvTools Validator( SPV_ENV_VULKAN_1_0 );
    Validator.SetMessageConsumer( []( spv_message_level_t, const char*, const spv_position_t& Position, const char* Message )
                                  { x_printf( "SPIR-V validation at %u:%u: %s\n", (u32)Position.line, (u32)Position.column, Message ); } );
    if( !Validator.Validate( reinterpret_cast<const u32*>( Output.Code.GetPtr() ), Output.Code.GetCount() / 4 ) )
        return FALSE;

    matrix_strides MatrixStrides;
    const u32*     Words     = reinterpret_cast<const u32*>( Output.Code.GetPtr() );
    const u32      WordCount = Output.Code.GetCount() / 4;

    for( u32 Word = 5; Word < WordCount; Word += Words[Word] >> 16 )
    {
        if( ( Words[Word] & 0xffff ) == SpvOpMemberDecorate && ( Words[Word] >> 16 ) == 5 &&
            Words[Word + 3] == SpvDecorationMatrixStride )
            MatrixStrides[( static_cast<u64>( Words[Word + 1] ) << 32 ) | Words[Word + 2]] = Words[Word + 4];
    }

    SpvReflectShaderModule Module = {};

    if( spvReflectCreateShaderModule( Output.Code.GetCount(), Output.Code.GetPtr(), &Module ) != SPV_REFLECT_RESULT_SUCCESS )
        return FALSE;

    struct ModuleGuard
    {
        SpvReflectShaderModule* Module;

        ~ModuleGuard()
        {
            spvReflectDestroyShaderModule( Module );
        }
    } Guard = { &Module };

    if( Script.BindingModel != SHADER_BINDING_SDL )
    {
        x_printf( "Error: final SPIR-V reflection requires the SDL resource contract\n" );
        return FALSE;
    }

    const xbool Compute     = Shader.StageShort == "cs";
    const u32   ResourceSet = Compute || Shader.StageShort == "vs" ? 0 : 2;
    const u32   UniformSet  = Compute ? 2 : ResourceSet + 1;

    if( ( !Compute && Shader.StageShort != "vs" && Shader.StageShort != "ps" ) ||
        Module.entry_point_count != 1 || Module.push_constant_block_count )
        return FALSE;

    if( Compute )
    {
        if( Module.entry_points[0].spirv_execution_model != SpvExecutionModelGLCompute )
        {
            return FALSE;
        }

        Output.Compute.ThreadCountX = Module.entry_points[0].local_size.x;
        Output.Compute.ThreadCountY = Module.entry_points[0].local_size.y;
        Output.Compute.ThreadCountZ = Module.entry_points[0].local_size.z;
        const u32 X                 = Output.Compute.ThreadCountX;
        const u32 Y                 = Output.Compute.ThreadCountY;
        const u32 Z                 = Output.Compute.ThreadCountZ;

        if( !X || !Y || !Z || X > 1024 || Y > 1024 || Z > 64 || static_cast<u64>( X ) * Y * Z > 1024 )
        {
            return FALSE;
        }
    }

    std::vector<const SpvReflectDescriptorBinding*> Bindings;

    for( u32 i = 0; i < Module.descriptor_binding_count; ++i )
        Bindings.push_back( &Module.descriptor_bindings[i] );
    std::sort( Bindings.begin(), Bindings.end(), []( const SpvReflectDescriptorBinding* A, const SpvReflectDescriptorBinding* B )
               { return A->set == B->set ? A->binding < B->binding : A->set < B->set; } );
    u32   NextResource    = 0;
    u32   NextUniform     = 0;
    u32   LastCategory    = 0;
    u32   NextWritable    = 0;
    xbool WritableBuffers = FALSE;

    for( const SpvReflectDescriptorBinding* Binding : Bindings )
    {
        if( !Binding->count || !Binding->name || ( Compute && Binding->count != 1 ) )
            return FALSE;

        shader_reflection_binding_type Type;
        u32                            Slot;

        if( Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
        {
            if( Binding->set != UniformSet || Binding->binding != NextUniform )
                return FALSE;

            Type = SHADER_REFLECT_UNIFORM_BUFFER;
            Slot = NextUniform;
            NextUniform += Binding->count;
            Output.Resources.UniformBufferCount = NextUniform;
        }
        else if( Compute && Binding->set == 1 )
        {
            if( Binding->binding != NextWritable )
                return FALSE;

            if( Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE )
            {
                if( WritableBuffers )
                    return FALSE;

                Type = SHADER_REFLECT_READ_WRITE_TEXTURE;
                Slot = Output.Compute.ReadWriteTextureCount;
                Output.Compute.ReadWriteTextureCount += Binding->count;
            }
            else if( Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER )
            {
                if( Binding->resource_type & SPV_REFLECT_RESOURCE_FLAG_SRV )
                    return FALSE;

                WritableBuffers = TRUE;
                Type            = SHADER_REFLECT_READ_WRITE_BUFFER;
                Slot            = Output.Compute.ReadWriteBufferCount;
                Output.Compute.ReadWriteBufferCount += Binding->count;
            }
            else
                return FALSE;

            NextWritable += Binding->count;
        }
        else
        {
            if( Binding->set != ResourceSet || Binding->binding != NextResource )
                return FALSE;

            if( Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            {
                if( LastCategory > 0 )
                    return FALSE;

                Type = SHADER_REFLECT_SAMPLED_TEXTURE;
                Slot = Output.Resources.SamplerCount;
                Output.Resources.SamplerCount += Binding->count;
            }
            else if( Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE )
            {
                if( LastCategory > 1 )
                    return FALSE;

                LastCategory = 1;
                Type         = SHADER_REFLECT_STORAGE_TEXTURE;
                Slot         = Output.Resources.StorageTextureCount;
                Output.Resources.StorageTextureCount += Binding->count;
            }
            else if( Binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER )
            {
                if( Compute && !( Binding->resource_type & SPV_REFLECT_RESOURCE_FLAG_SRV ) )
                    return FALSE;

                LastCategory = 2;
                Type         = SHADER_REFLECT_STORAGE_BUFFER;
                Slot         = Output.Resources.StorageBufferCount;
                Output.Resources.StorageBufferCount += Binding->count;
            }
            else
                return FALSE;

            NextResource += Binding->count;
        }

        shader_reflection_binding& Reflected = Output.Bindings.Append();
        Reflected.Type                       = Type;
        Reflected.Name                       = Binding->name;
        Reflected.Slot                       = Slot;
        Reflected.Count                      = Binding->count;
        Reflected.ByteSize                   = Binding->block.padded_size;

        if( !Compute || Type == SHADER_REFLECT_UNIFORM_BUFFER )
        {
            ReflectMembers( Binding->name, Binding->block, "", Output.Members, MatrixStrides );
        }
    }

    if( NextUniform > 4 || Output.Resources.SamplerCount > 16 ||
        Output.Resources.StorageTextureCount > 8 || Output.Resources.StorageBufferCount > 8 ||
        Output.Compute.ReadWriteTextureCount > 8 || Output.Compute.ReadWriteBufferCount > 8 )
        return FALSE;

    for( u32 i = 0; i < Module.input_variable_count; ++i )
        ReflectInterface( *Module.input_variables[i], Output.Inputs );
    for( u32 i = 0; i < Module.output_variable_count; ++i )
        ReflectInterface( *Module.output_variables[i], Output.Outputs );
    return TRUE;
}

//==============================================================================

xbool ValidateTargetReflections( const shader_entry& Shader, const shader_compile_output& A,
                                 const shader_compile_output& B )
{
    if( A.Compute.ReadWriteTextureCount != B.Compute.ReadWriteTextureCount ||
        A.Compute.ReadWriteBufferCount != B.Compute.ReadWriteBufferCount ||
        A.Compute.ThreadCountX != B.Compute.ThreadCountX || A.Compute.ThreadCountY != B.Compute.ThreadCountY ||
        A.Compute.ThreadCountZ != B.Compute.ThreadCountZ )
    {
        x_printf( "Error: target compute layouts disagree for %s\n", (const char*)Shader.Name );
        return FALSE;
    }

    if( A.Resources.SamplerCount != B.Resources.SamplerCount ||
        A.Resources.UniformBufferCount != B.Resources.UniformBufferCount ||
        A.Resources.StorageTextureCount != B.Resources.StorageTextureCount ||
        A.Resources.StorageBufferCount != B.Resources.StorageBufferCount ||
        A.Bindings.GetCount() != B.Bindings.GetCount() )
    {
        x_printf( "Error: target resource counts disagree for %s\n", (const char*)Shader.Name );
        return FALSE;
    }

    for( s32 i = 0; i < A.Bindings.GetCount(); ++i )
    {
        const shader_reflection_binding& Binding = A.Bindings[i];
        xbool                            Found   = FALSE;

        for( s32 j = 0; j < B.Bindings.GetCount(); ++j )
        {
            const shader_reflection_binding& Other = B.Bindings[j];

            if( Binding.Name == Other.Name && Binding.Type == Other.Type && Binding.Slot == Other.Slot && Binding.Count == Other.Count )
                Found = TRUE;
        }

        if( !Found )
        {
            x_printf( "Error: target binding '%s' disagrees for %s\n", (const char*)Binding.Name, (const char*)Shader.Name );
            return FALSE;
        }
    }

    // Uniform layouts are portable only when the final binaries agree.
    for( s32 i = 0; i < A.Members.GetCount(); ++i )
    {
        const shader_buffer_member& Member = A.Members[i];
        xbool                       Found  = FALSE;

        for( s32 j = 0; j < B.Members.GetCount(); ++j )
        {
            const shader_buffer_member& Other    = B.Members[j];
            const xbool                 SameName = Member.Name == Other.Name ||
                                   ( !Member.Name.IsEmpty() && Other.Name == xstring( "." ) + Member.Name ) ||
                                   ( !Other.Name.IsEmpty() && Member.Name == xstring( "." ) + Other.Name );
            if( Member.Buffer == Other.Buffer && SameName && Member.Offset == Other.Offset &&
                ( Member.Name.IsEmpty() || Member.Size == Other.Size ) &&
                Member.ArrayStride == Other.ArrayStride && Member.MatrixStride == Other.MatrixStride )
                Found = TRUE;
        }

        if( !Found )
        {
            x_printf( "Error: final buffer member '%s.%s' layout differs for %s (%s offset=%u size=%u array=%u matrix=%u)\n",
                      (const char*)Member.Buffer, (const char*)Member.Name, (const char*)Shader.Name,
                      (const char*)A.Target, Member.Offset, Member.Size, Member.ArrayStride, Member.MatrixStride );
            for( s32 j = 0; j < B.Members.GetCount(); ++j )
            {
                const shader_buffer_member& Other = B.Members[j];

                if( Member.Buffer == Other.Buffer )
                    x_printf( "  %s member '%s' offset=%u size=%u array=%u matrix=%u\n", (const char*)B.Target,
                              (const char*)Other.Name, Other.Offset, Other.Size, Other.ArrayStride, Other.MatrixStride );
            }

            return FALSE;
        }
    }

    return TRUE;
}
