//==============================================================================
//
//  ecs_package.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ecs_package.hpp"
#include "x_plus.hpp"
#include "x_threads.hpp"
#include "x_time.hpp"

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

namespace
{
constexpr u32 FourCC( char A, char B, char C, char D )
{
    return static_cast<u32>( A ) | ( static_cast<u32>( B ) << 8 ) | ( static_cast<u32>( C ) << 16 ) |
           ( static_cast<u32>( D ) << 24 );
}

//==============================================================================
//  CONSTANTS
//==============================================================================

const u32 s_ChunkTypes[] =
{
    FourCC( 'M', 'E', 'T', 'A' ),
    FourCC( 'C', 'O', 'M', 'B' ),
    FourCC( 'P', 'R', 'O', 'G' ),
    FourCC( 'R', 'E', 'F', 'L' ),
    FourCC( 'B', 'I', 'D', 'X' ),
    FourCC( 'S', 'T', 'R', 'S' ),
    FourCC( 'C', 'O', 'D', 'E' )
};

//------------------------------------------------------------------------------

enum
{
    META,
    COMB,
    PROG,
    REFL,
    BIDX,
    STRS,
    CODE,
    CHUNKS
};

//==============================================================================

u32 CRC( const xarray<byte>& Data )
{
    return x_chksum( Data.GetPtr(), Data.GetCount() );
}

//==============================================================================

xbool Fail( xstring& Error, const char* pMessage )
{
    Error = pMessage;
    return FALSE;
}

//==============================================================================

void ResizeBytes( xarray<byte>& Data, s32 Count )
{
    ASSERT( Count >= 0 );
    if( Count > Data.GetCapacity() )
    {
        const u64 Capacity = MAX( static_cast<u64>( Count ),
                                  static_cast<u64>( Data.GetCapacity() ) + MAX( 128, Data.GetCapacity() / 2 ) );
        Data.SetCapacity( static_cast<s32>( MIN( Capacity, static_cast<u64>( ECS_PACKAGE_MAX_FILE_SIZE ) ) ) );
    }

    Data.SetCount( Count );
}

//==============================================================================

void AppendBytes( xarray<byte>& Data, const byte* pBytes, s32 Count )
{
    const s32 Offset = Data.GetCount();

    ResizeBytes( Data, Offset + Count );

    if( Count )
    {
        x_memcpy( Data.GetPtr() + Offset, pBytes, Count );
    }
}

//==============================================================================

void AlignBytes( xarray<byte>& Data )
{
    const s32 Count   = Data.GetCount();
    const s32 Aligned = ( Count + 15 ) & ~15;

    ResizeBytes( Data, Aligned );

    if( Aligned > Count )
    {
        x_memset( Data.GetPtr() + Count, 0, Aligned - Count );
    }
}

//==============================================================================

xbool EqualBytes( const xarray<byte>& A, const xarray<byte>& B )
{
    return A.GetCount() == B.GetCount() && ( !A.GetCount() || x_memcmp( A.GetPtr(), B.GetPtr(), A.GetCount() ) == 0 );
}

//==============================================================================
//  TYPES
//==============================================================================

struct PackageWriter
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    PackageWriter( void )
        : Data( Storage )
    {
    }

    //==============================================================================

    explicit PackageWriter( xarray<byte>& Bytes )
        : Data( Bytes )
    {
    }

    //==============================================================================

    void U32( u32 Value )
    {
        const u32 Position = Data.GetCount();

        ResizeBytes( Data, Position + 4 );
        x_store_le32( Data.GetPtr() + Position, Value );
    }

    //==============================================================================

    void U64( u64 Value )
    {
        U32( static_cast<u32>( Value ) );
        U32( static_cast<u32>( Value >> 32 ) );
    }

    //==============================================================================

    void Align()
    {
        AlignBytes( Data );
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    xarray<byte>  Storage;
    xarray<byte>& Data;
};

//------------------------------------------------------------------------------

struct PackageStrings
{
    //------------------------------------------------------------------------------
    //  TYPES
    //------------------------------------------------------------------------------

    struct StringEntry
    {
        xstring Text;
        u32     Offset;
    };

    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    PackageStrings( void )
    {
        Data.Append( 0 );
    }

    //==============================================================================

    u32 Add( const xstring& Text )
    {
        if( Text.IsEmpty() )
        {
            return 0;
        }

        if( Text.GetLength() > 4096 || x_strlen( Text ) != Text.GetLength() ||
            Data.GetCount() + Text.GetLength() + 1 > ECS_PACKAGE_MAX_METADATA )
        {
            Valid = FALSE;
            return 0;
        }

        u32 Hash = 2166136261u;

        for( s32 i = 0; i < Text.GetLength(); ++i )
        {
            Hash = ( Hash ^ ( byte )Text.GetAt( i ) ) * 16777619u;
        }

        xarray<StringEntry>& Bucket = Entries[Hash & 255];

        for( s32 i = 0; i < Bucket.GetCount(); ++i )
        {
            if( Bucket[i].Text == Text )
            {
                return Bucket[i].Offset;
            }
        }

        const u32 Offset = static_cast<u32>( Data.GetCount() );

        AppendBytes( Data, ( const byte* )( const char* )Text, Text.GetLength() );
        Data.Append( 0 );

        StringEntry& Entry = Bucket.Append();

        Entry.Text   = Text;
        Entry.Offset = Offset;

        return Offset;
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    xarray<byte> Data;

    xarray<StringEntry> Entries[256];
    xbool               Valid = TRUE;
};

//------------------------------------------------------------------------------

struct PackageRange
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    PackageRange( u64 RangeStart = 0, u64 RangeEnd = 0, u32 RangeGroup = 0 )
        : Start( RangeStart ),
          End( RangeEnd ),
          Group( RangeGroup )
    {
    }

    //==============================================================================

    xbool operator<( const PackageRange& Other ) const
    {
        return Group < Other.Group ||
               ( Group == Other.Group && ( Start < Other.Start || ( Start == Other.Start && End < Other.End ) ) );
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    u64 Start;
    u64 End;
    u32 Group;
};

//------------------------------------------------------------------------------

struct BinaryGroup
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    BinaryGroup( u32 Target      = 0xffffffffu,
                 u32 ShaderStage = 0xffffffffu,
                 u32 Input       = 0xffffffffu,
                 u32 Output      = 0xffffffffu )
        : Format( Target ),
          Stage( ShaderStage ),
          InputInterface( Input ),
          OutputInterface( Output )
    {
    }

    //==============================================================================

    xbool operator<( const BinaryGroup& Other ) const
    {
        if( Format != Other.Format )
        {
            return Format < Other.Format;
        }

        if( Stage != Other.Stage )
        {
            return Stage < Other.Stage;
        }

        if( InputInterface != Other.InputInterface )
        {
            return InputInterface < Other.InputInterface;
        }

        return OutputInterface < Other.OutputInterface;
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    u32 Format;
    u32 Stage;
    u32 InputInterface;
    u32 OutputInterface;
};

//------------------------------------------------------------------------------

struct ContentEntry
{
    x_content_id Identity;
    u32            Format;
    xarray<u32>    Indices;
};

//==============================================================================
//  CONTENT LOOKUP
//==============================================================================

xarray<u32>& ContentCandidates( xarray<ContentEntry>& Bucket, const x_content_id& Identity, u32 Format )
{
    for( s32 i = 0; i < Bucket.GetCount(); ++i )
    {
        if( Bucket[i].Format == Format && Bucket[i].Identity == Identity )
        {
            return Bucket[i].Indices;
        }
    }

    ContentEntry& Entry = Bucket.Append();

    Entry.Identity = Identity;
    Entry.Format   = Format;

    return Entry.Indices;
}

//==============================================================================

struct PackageReader
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    u32 U32()
    {
        if( Position > Data.GetCount() || Data.GetCount() - Position < 4 )
        {
            Valid = FALSE;
            return 0;
        }

        const u32 Value = x_load_le32( Data.GetPtr() + Position );

        Position += 4;

        return Value;
    }

    //==============================================================================

    u64 U64()
    {
        const u64 Low = U32();
        return Low | ( static_cast<u64>( U32() ) << 32 );
    }

    //==============================================================================

    u32 Count( u32 Limit )
    {
        const u32 Value = U32();

        if( Value > Limit )
        {
            Valid = FALSE;
            return 0;
        }

        return Value;
    }

    //==============================================================================

    xbool Done() const
    {
        return Valid && Position == Data.GetCount();
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    const xarray<byte>& Data;
    u32                 Position = 0;
    xbool               Valid    = TRUE;
};

//==============================================================================
//  METADATA SERIALIZATION
//==============================================================================

xbool ReadString( const xarray<byte>& StringData, u32 Offset, xstring& Text, xbool Required )
{
    if( Offset >= StringData.GetCount() || ( Required && !Offset ) || ( Offset && StringData[Offset - 1] ) )
    {
        return FALSE;
    }

    const byte* pBegin = StringData.GetPtr() + Offset;
    const byte* pEnd   = static_cast<const byte*>( x_memchr( ( void* )pBegin, 0, StringData.GetCount() - Offset ) );

    if( !pEnd || pEnd - pBegin > 4096 || ( Required && pBegin == pEnd ) )
    {
        return FALSE;
    }

    Text = xstring( ( const char* )pBegin, ( s32 )( pEnd - pBegin ) );

    return TRUE;
}

//==============================================================================

void WriteLayout( PackageWriter& Out, PackageStrings& Text, const ecs_package_layout& Layout )
{
    const u32 Size = 40 + Layout.Bindings.GetCount() * 20 + Layout.Members.GetCount() * 24 +
                     ( Layout.Inputs.GetCount() + Layout.Outputs.GetCount() ) * 16;

    ResizeBytes( Out.Data, Out.Data.GetCount() + Size );
    Out.Data.SetCount( Out.Data.GetCount() - Size );
    Out.U32( Layout.InputInterface );
    Out.U32( Layout.OutputInterface );

    for( u32 Count : Layout.Resources )
    {
        Out.U32( Count );
    }

    Out.U32( static_cast<u32>( Layout.Bindings.GetCount() ) );
    Out.U32( static_cast<u32>( Layout.Members.GetCount() ) );
    Out.U32( static_cast<u32>( Layout.Inputs.GetCount() ) );
    Out.U32( static_cast<u32>( Layout.Outputs.GetCount() ) );

    for( s32 iBinding = 0; iBinding < Layout.Bindings.GetCount(); ++iBinding )
    {
        const ecs_package_binding& Binding = Layout.Bindings[iBinding];

        Out.U32( Binding.Kind );
        Out.U32( Binding.Slot );
        Out.U32( Binding.Count );
        Out.U32( Binding.ByteSize );
        Out.U32( Text.Add( Binding.Name ) );
    }

    for( s32 iMember = 0; iMember < Layout.Members.GetCount(); ++iMember )
    {
        const ecs_package_member& Member = Layout.Members[iMember];

        Out.U32( Text.Add( Member.Buffer ) );
        Out.U32( Text.Add( Member.Name ) );
        Out.U32( Member.Offset );
        Out.U32( Member.Size );
        Out.U32( Member.ArrayStride );
        Out.U32( Member.MatrixStride );
    }

    for( s32 iSide = 0; iSide < 2; ++iSide )
    {
        const xarray<ecs_package_variable>* pVariables = iSide ? &Layout.Outputs : &Layout.Inputs;

        for( s32 iVariable = 0; iVariable < pVariables->GetCount(); ++iVariable )
        {
            const ecs_package_variable& Variable = ( *pVariables )[iVariable];

            Out.U32( Text.Add( Variable.Semantic ) );
            Out.U32( Variable.Index );
            Out.U32( Variable.Components );
            Out.U32( Variable.ComponentType );
        }
    }
}

//==============================================================================

struct PackageBlockTask
{
    u32               First = 0;
    u32               Count = 0;
    ecs_package_block Block;
    xarray<byte>      Stored;
    xstring           Error;
    xbool             Cached = FALSE;
};

//------------------------------------------------------------------------------

struct PackageBlockJobs
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    PackageBlockJobs( xarray<PackageBlockTask>&          BlockTasks,
                      const xarray<u32>&                 Code,
                      const xarray<const xarray<byte>*>& Bytes,
                      const xarray<ecs_package_binary>&  BinaryIndex,
                      ecs_package_build_options&         BuildOptions )
        : Tasks( BlockTasks ),
          UniqueCode( Code ),
          Bytecode( Bytes ),
          Binaries( BinaryIndex ),
          Options( BuildOptions ),
          Dispatch( 12, 0 ),
          Completed( 12, 0 )
    {
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    xarray<PackageBlockTask>&          Tasks;
    const xarray<u32>&                 UniqueCode;
    const xarray<const xarray<byte>*>& Bytecode;
    const xarray<ecs_package_binary>&  Binaries;
    ecs_package_build_options&         Options;
    xmutex                             Lock;
    xsema                              Dispatch;
    xsema                              Completed;
    u32                                Next = 0;
    xbool                              Stop = FALSE;
};

//==============================================================================
//  BLOCK COMPRESSION
//==============================================================================

void CompressPackageBlock( PackageBlockJobs&  Jobs,
                           PackageBlockTask&  Task,
                           ecs_block_context& Context,
                           xarray<byte>&      Decoded,
                           xarray<byte>&      Verified )
{
    x_content_hash Hash;
    byte             Header[16];

    x_store_le32( Header, 1 );
    x_store_le32( Header + 4, 3 );
    x_store_le32( Header + 8, Task.Block.DecodedSize );
    x_store_le32( Header + 12, Task.Count );
    Hash.Update( Header, sizeof( Header ) );

    for( u32 i = Task.First; i < Task.First + Task.Count; ++i )
    {
        const ecs_package_binary& Binary = Jobs.Binaries[i];

        Hash.Update( Binary.Content.GetPtr(), 32 );
        x_store_le32( Header, Binary.Offset );
        x_store_le32( Header + 4, Binary.Size );
        Hash.Update( Header, 8 );
    }

    const x_content_id Key    = Hash.Finish();
    ecs_package_block    Cached = Task.Block;

    if( Jobs.Options.LoadBlock && Jobs.Options.LoadBlock( Jobs.Options.pContext, Key, Cached, Task.Stored ) )
    {
        Task.Block  = Cached;
        Task.Cached = TRUE;
        return;
    }

    // Build and compress the decoded block.
    Decoded.SetCount( Task.Block.DecodedSize );
    x_memset( Decoded.GetPtr(), 0, Decoded.GetCount() );

    for( u32 i = Task.First; i < Task.First + Task.Count; ++i )
    {
        const ecs_package_binary& Binary = Jobs.Binaries[i];
        x_memcpy( Decoded.GetPtr() + Binary.Offset, Jobs.Bytecode[Jobs.UniqueCode[i]]->GetPtr(), Binary.Size );
    }

    Task.Block.DecodedCRC = CRC( Decoded );
    Task.Stored.SetCount( ecs_CompressBound( Task.Block.DecodedSize ) );

    u32 Size = Task.Stored.GetCount();

    if( !Context.Compress( Decoded.GetPtr(), Task.Block.DecodedSize, Task.Stored.GetPtr(), Size ) )
    {
        Task.Error = "block compression failed";
        return;
    }

    if( Size < Task.Block.DecodedSize )
    {
        Task.Block.Codec = ECS_BLOCK_ZSTD;
        Task.Stored.SetCount( Size );
    }
    else
    {
        Task.Block.Codec = ECS_BLOCK_RAW;
        Task.Stored.SetCount( Task.Block.DecodedSize );
        x_memcpy( Task.Stored.GetPtr(), Decoded.GetPtr(), Decoded.GetCount() );
    }

    Task.Block.StoredSize = Task.Stored.GetCount();
    Task.Block.StoredCRC  = CRC( Task.Stored );

    // Verify the stored block before publishing it.
    Verified.SetCount( Task.Block.DecodedSize );

    if( !Context.Decode( Task.Block.Codec, Task.Stored.GetPtr(), Task.Block.StoredSize, Verified.GetPtr(),
                         Task.Block.DecodedSize ) ||
        !EqualBytes( Decoded, Verified ) )
    {
        Task.Error = "compressed block verification failed";
        return;
    }

    if( Jobs.Options.SaveBlock && !Jobs.Options.SaveBlock( Jobs.Options.pContext, Key, Task.Block, Task.Stored ) )
    {
        Task.Error = "compressed block cache write failed";
    }
}

//==============================================================================

void PackageBlockWorker( s32, char** ppArguments )
{
    PackageBlockJobs& Jobs = *reinterpret_cast<PackageBlockJobs*>( ppArguments[0] );
    ecs_block_context Context;
    xarray<byte>      Decoded, Verified;

    for( ;; )
    {
        Jobs.Dispatch.Acquire();
        Jobs.Lock.Enter();

        const xbool Stop  = Jobs.Stop;
        const u32   Index = Jobs.Next++;

        Jobs.Lock.Exit();

        if( Stop )
        {
            return;
        }

        CompressPackageBlock( Jobs, Jobs.Tasks[Index], Context, Decoded, Verified );

        // Capacity is retained between ordinary blocks, but not across large waves.
        Decoded.SetCount( 0 );
        Verified.SetCount( 0 );

        if( Decoded.GetCapacity() > ECS_BLOCK_TARGET_SIZE )
        {
            Decoded.FreeExtra();
        }

        if( Verified.GetCapacity() > ECS_BLOCK_TARGET_SIZE )
        {
            Verified.FreeExtra();
        }

        Jobs.Completed.Release();
    }
}

//==============================================================================

xbool CompressPackageBlocks( xarray<PackageBlockTask>&          Tasks,
                             const xarray<u32>&                 UniqueCode,
                             const xarray<const xarray<byte>*>& Bytecode,
                             const xarray<ecs_package_binary>&  Binaries,
                             ecs_package_build_options&         Options,
                             xarray<byte>&                      Code,
                             xstring&                           Error )
{
    const u32 Workers = MIN( MIN( MAX( Options.WorkerCount, 1u ), 12u ), static_cast<u32>( Tasks.GetCount() ) );

    if( Options.TemporaryBudget < 4ull * ECS_BLOCK_MAX_SIZE )
    {
        return Fail( Error, "package temporary memory budget is too small" );
    }

    PackageBlockJobs Jobs( Tasks, UniqueCode, Bytecode, Binaries, Options );
    xthread*         Threads[12] = {};
    char*            Arguments[] = { reinterpret_cast<char*>( &Jobs ) };

    for( u32 i = 0; i < Workers; ++i )
    {
        Threads[i] = new xthread( PackageBlockWorker, "ECS compression", 128 * 1024, 0, 1, Arguments );
    }

    xbool Success  = TRUE;
    u64   CodeSize = 0;

    if( Options.Progress )
    {
        Options.Progress( Options.pContext, 0, Tasks.GetCount() );
    }

    // Dispatch waves bounded by the temporary memory budget.
    for( u32 First = 0; First < Tasks.GetCount() && Success; )
    {
        u32 Count  = 0;
        u64 Memory = 0;

        while( Count < Workers && First + Count < Tasks.GetCount() )
        {
            const u64 Required = 3ull * Tasks[First + Count].Block.DecodedSize +
                                 ecs_CompressBound( Tasks[First + Count].Block.DecodedSize );

            if( Memory + Required > Options.TemporaryBudget )
            {
                break;
            }

            Memory += Required;
            ++Count;
        }

        if( !Count )
        {
            Error   = "block exceeds temporary memory budget";
            Success = FALSE;
            break;
        }

        Options.PeakTemporaryBytes = MAX( Options.PeakTemporaryBytes, Memory );

        for( u32 i = 0; i < Count; ++i )
        {
            Jobs.Dispatch.Release();
        }

        for( u32 i = 0; i < Count; ++i )
        {
            Jobs.Completed.Acquire();
        }

        for( u32 i = First; i < First + Count; ++i )
        {
            PackageBlockTask& Task = Tasks[i];

            if( !Task.Error.IsEmpty() )
            {
                Error   = Task.Error;
                Success = FALSE;
                break;
            }

            if( Task.Cached )
            {
                ++Options.CachedBlocks;
            }
            else
            {
                ++Options.CompressedBlocks;
            }

            if( !Task.Cached )
            {
                if( Task.Block.Codec == ECS_BLOCK_ZSTD )
                {
                    ++Options.ZstdBlocks;
                }
                else
                {
                    ++Options.RawBlocks;
                }
            }

            CodeSize = ( ( CodeSize + 15 ) & ~u64( 15 ) ) + Task.Block.StoredSize;

            if( CodeSize > ECS_PACKAGE_MAX_FILE_SIZE )
            {
                Error   = "CODE exceeds file limit";
                Success = FALSE;
                break;
            }

            AlignBytes( Code );
            Task.Block.Offset = Code.GetCount();
            AppendBytes( Code, Task.Stored.GetPtr(), Task.Stored.GetCount() );
            Task.Stored.Clear();
        }

        First += Count;

        if( Success && Options.Progress )
        {
            Options.Progress( Options.pContext, First, Tasks.GetCount() );
        }
    }

    // Stop and join the workers before returning.
    Jobs.Lock.Enter();
    Jobs.Stop = TRUE;
    Jobs.Lock.Exit();

    for( u32 i = 0; i < Workers; ++i )
    {
        Jobs.Dispatch.Release();
    }

    for( u32 i = 0; i < Workers; ++i )
    {
        while( Threads[i]->IsActive() )
        {
            x_DelayThread( 1 );
        }

        delete Threads[i];
    }

    return Success;
}

//==============================================================================
//  INDEX VALIDATION
//==============================================================================

xbool ValidIndex( const ecs_package_index& Index, xstring& Error )
{
    if( !Index.CatalogId || !Index.PackageId || Index.Name.IsEmpty() || !Index.TargetMask ||
        ( Index.TargetMask & ~3u ) || ( Index.Programs.GetCount() == 0 ) ||
        Index.Programs.GetCount() > ECS_PACKAGE_MAX_RECORDS || Index.Combos.GetCount() > ECS_PACKAGE_MAX_RECORDS ||
        Index.Layouts.GetCount() > ECS_PACKAGE_MAX_RECORDS || Index.Binaries.GetCount() > ECS_PACKAGE_MAX_RECORDS ||
        Index.Blocks.GetCount() > ECS_PACKAGE_MAX_RECORDS )
    {
        return Fail( Error, "invalid package identity or table count" );
    }

    u32                        Targets        = 0;
    const ecs_package_program* pLogical       = NULL;
    u32                        LogicalTargets = 0;

    for( s32 i = 0; i < Index.Programs.GetCount(); ++i )
    {
        const ecs_package_program& Program = Index.Programs[i];

        if( ( Program.RequiredCapabilities & ~ECS_SHADER_CAPABILITY_FLOAT16 ) ||
            ( Program.RequiredCapabilities && Program.Format != 1 ) || !Program.Id || Program.Stage > 1 ||
            Program.Format < 1 || Program.Format > 2 || Program.Entry.IsEmpty() ||
            Program.Layout >= ( u32 )Index.Layouts.GetCount() || Program.Binary >= ( u32 )Index.Binaries.GetCount() ||
            Index.Binaries[Program.Binary].Format != Program.Format )
        {
            return Fail( Error, "invalid program reference" );
        }

        Targets |= 1u << ( Program.Format - 1 );

        if( pLogical && pLogical->Id != Program.Id )
        {
            if( LogicalTargets != Index.TargetMask )
            {
                return Fail( Error, "missing logical program target" );
            }

            LogicalTargets = 0;
            pLogical       = NULL;
        }

        if( pLogical &&
            ( pLogical->Stage != Program.Stage || pLogical->Name != Program.Name || pLogical->Entry != Program.Entry ) )
        {
            return Fail( Error, "inconsistent logical program targets" );
        }

        if( !Program.RequiredCapabilities )
        {
            LogicalTargets |= 1u << ( Program.Format - 1 );
        }
        else
        {
            const ecs_package_program* pBase = ecs_FindPackageProgram( Index, Program.Id, Program.Format );

            if( !pBase || pBase->Layout != Program.Layout )
            {
                return Fail( Error, "capability variant changes shader interface" );
            }
        }

        pLogical = &Program;
    }

    if( LogicalTargets != Index.TargetMask )
    {
        return Fail( Error, "missing logical program target" );
    }

    if( Targets != Index.TargetMask )
    {
        return Fail( Error, "target mask does not match programs" );
    }

    // Validate reflection interfaces and binding ranges.
    for( s32 iLayout = 0; iLayout < Index.Layouts.GetCount(); ++iLayout )
    {
        const ecs_package_layout& Layout = Index.Layouts[iLayout];

        if( Layout.Bindings.GetCount() > 256 || Layout.Members.GetCount() > 8192 || Layout.Inputs.GetCount() > 256 ||
            Layout.Outputs.GetCount() > 256 )
        {
            return Fail( Error, "reflection table exceeds limits" );
        }

        for( u32 Count : Layout.Resources )
        {
            if( Count > 256 )
            {
                return Fail( Error, "resource count exceeds format limit" );
            }
        }

        for( s32 iSide = 0; iSide < 2; ++iSide )
        {
            const xarray<ecs_package_variable>* pVariables = iSide ? &Layout.Outputs : &Layout.Inputs;

            for( s32 i = 0; i < pVariables->GetCount(); ++i )
            {
                const ecs_package_variable& Variable = ( *pVariables )[i];

                if( Variable.Semantic.IsEmpty() || !Variable.Components || Variable.Components > 4 )
                {
                    return Fail( Error, "invalid stage interface" );
                }

                for( s32 j = 0; j < i; ++j )
                {
                    if( ( *pVariables )[j].Semantic == Variable.Semantic && ( *pVariables )[j].Index == Variable.Index )
                    {
                        return Fail( Error, "invalid stage interface" );
                    }
                }
            }
        }

        xarray<xstring> Names;
        xbool           Slots[4][256];

        x_memset( Slots, 0, sizeof( Slots ) );

        for( s32 iBinding = 0; iBinding < Layout.Bindings.GetCount(); ++iBinding )
        {
            const ecs_package_binding& Binding = Layout.Bindings[iBinding];

            if( Binding.Kind > 3 || !Binding.Count || Binding.Name.IsEmpty() ||
                Binding.Slot > Layout.Resources[Binding.Kind] ||
                Binding.Count > Layout.Resources[Binding.Kind] - Binding.Slot || Names.Find( Binding.Name ) != -1 )
            {
                return Fail( Error, "invalid resource layout" );
            }

            Names.Append( Binding.Name );

            for( u32 i = 0; i < Binding.Count; ++i )
            {
                if( Slots[Binding.Kind][Binding.Slot + i] )
                {
                    return Fail( Error, "overlapping resource bindings" );
                }

                Slots[Binding.Kind][Binding.Slot + i] = TRUE;
            }
        }

        for( u32 Kind = 0; Kind < 4; ++Kind )
        {
            for( u32 Slot = 0; Slot < Layout.Resources[Kind]; ++Slot )
            {
                if( !Slots[Kind][Slot] )
                {
                    return Fail( Error, "resource layout is not compact" );
                }
            }
        }

        xarray<const ecs_package_member*> Members;

        Members.SetCount( Layout.Members.GetCount() );

        for( s32 i = 0; i < Members.GetCount(); ++i )
        {
            Members[i] = &Layout.Members[i];
        }

        x_qsort( Members.GetPtr(), Members.GetCount(),
                 []( const ecs_package_member* pA, const ecs_package_member* pB )
                 {
                     if( pA->Buffer != pB->Buffer )
                     {
                         return pA->Buffer < pB->Buffer ? -1 : 1;
                     }

                     return pA->Name < pB->Name ? -1 : ( pB->Name < pA->Name ? 1 : 0 );
                 } );

        for( s32 i = 1; i < Members.GetCount(); ++i )
        {
            if( Members[i - 1]->Buffer == Members[i]->Buffer && Members[i - 1]->Name == Members[i]->Name )
            {
                return Fail( Error, "invalid buffer member" );
            }
        }

        for( s32 iMember = 0; iMember < Layout.Members.GetCount(); ++iMember )
        {
            const ecs_package_member& Member = Layout.Members[iMember];

            if( Member.Buffer.IsEmpty() || !Member.Size || ( Member.Name.IsEmpty() && Member.Offset != 0 ) ||
                static_cast<u64>( Member.Offset ) + Member.Size > 0xffffffffull )
            {
                return Fail( Error, "invalid buffer member" );
            }

            const ecs_package_binding* pBinding = NULL;

            for( s32 iBinding = 0; iBinding < Layout.Bindings.GetCount(); ++iBinding )
            {
                const ecs_package_binding& Binding = Layout.Bindings[iBinding];

                if( Binding.Name == Member.Buffer && ( Binding.Kind == 1 || Binding.Kind == 3 ) )
                {
                    pBinding = &Binding;
                    break;
                }
            }

            if( !pBinding ||
                ( pBinding->ByteSize && static_cast<u64>( Member.Offset ) + Member.Size > pBinding->ByteSize ) )
            {
                return Fail( Error, "buffer member exceeds resource layout" );
            }
        }
    }

    // Check binary extents and block ownership.
    for( s32 iBinary = 0; iBinary < Index.Binaries.GetCount(); ++iBinary )
    {
        const ecs_package_binary& Binary = Index.Binaries[iBinary];

        if( Binary.Block >= Index.Blocks.GetCount() || ( Binary.Offset & 15 ) || !Binary.Size || Binary.Format < 1 ||
            Binary.Format > 2 ||
            static_cast<u64>( Binary.Offset ) + Binary.Size > Index.Blocks[Binary.Block].DecodedSize )
        {
            return Fail( Error, "invalid binary extent" );
        }
    }

    xarray<PackageRange> BinaryRanges;

    for( s32 iBinary = 0; iBinary < Index.Binaries.GetCount(); ++iBinary )
    {
        const ecs_package_binary& Binary = Index.Binaries[iBinary];
        BinaryRanges.Append(
            PackageRange( Binary.Offset, static_cast<u64>( Binary.Offset ) + Binary.Size, Binary.Block ) );
    }

    x_qsort( BinaryRanges.GetPtr(), BinaryRanges.GetCount(),
             []( const PackageRange& A, const PackageRange& B ) { return A < B ? -1 : ( B < A ? 1 : 0 ); } );

    for( u32 i = 1; i < BinaryRanges.GetCount(); ++i )
    {
        if( BinaryRanges[i - 1].Group == BinaryRanges[i].Group && BinaryRanges[i].Start < BinaryRanges[i - 1].End )
        {
            return Fail( Error, "overlapping binary extents" );
        }
    }

    // Check compressed block ranges.
    xarray<PackageRange> Ranges;

    for( s32 iBlock = 0; iBlock < Index.Blocks.GetCount(); ++iBlock )
    {
        const ecs_package_block& Block = Index.Blocks[iBlock];

        if( ( Block.Offset & 15 ) || !Block.StoredSize || !Block.DecodedSize ||
            Block.DecodedSize > ECS_BLOCK_MAX_SIZE ||
            ( Block.Codec != ECS_BLOCK_RAW && Block.Codec != ECS_BLOCK_ZSTD ) ||
            ( Block.Codec == ECS_BLOCK_RAW && Block.StoredSize != Block.DecodedSize ) ||
            ( Block.Codec == ECS_BLOCK_ZSTD && Block.StoredSize > ecs_CompressBound( Block.DecodedSize ) ) ||
            static_cast<u64>( Block.Offset ) + Block.StoredSize > Index.CodeSize )
        {
            return Fail( Error, "invalid block extent or codec" );
        }

        Ranges.Append( PackageRange( Block.Offset, static_cast<u64>( Block.Offset ) + Block.StoredSize ) );
    }

    x_qsort( Ranges.GetPtr(), Ranges.GetCount(),
             []( const PackageRange& A, const PackageRange& B ) { return A < B ? -1 : ( B < A ? 1 : 0 ); } );

    for( u32 i = 1; i < Ranges.GetCount(); ++i )
    {
        if( Ranges[i].Start < Ranges[i - 1].End )
        {
            return Fail( Error, "overlapping code blocks" );
        }
    }

    // Validate vertex and pixel stage pairings.
    for( s32 iCombo = 0; iCombo < Index.Combos.GetCount(); ++iCombo )
    {
        const ecs_package_combo& Combo = Index.Combos[iCombo];

        if( Combo.Kind > 1 )
        {
            return Fail( Error, "unknown combo kind" );
        }

        for( u32 Format = 1; Format <= 2; ++Format )
        {
            if( !( Index.TargetMask & ( 1u << ( Format - 1 ) ) ) )
            {
                continue;
            }

            const auto* pVertex = ecs_FindPackageProgram( Index, Combo.Vertex, Format );
            const auto* pPixel  = ecs_FindPackageProgram( Index, Combo.Pixel, Format );

            if( !pVertex || !pPixel || pVertex->Stage != 0 || pPixel->Stage != 1 )
            {
                return Fail( Error, "invalid combo stage pair" );
            }

            const auto& Provided = Index.Layouts[pVertex->Layout].Outputs;

            for( s32 iRequired = 0; iRequired < ( Index.Layouts[pPixel->Layout].Inputs ).GetCount(); ++iRequired )
            {
                const ecs_package_variable& Required = ( Index.Layouts[pPixel->Layout].Inputs )[iRequired];

                xbool Found = FALSE;

                for( s32 iVariable = 0; iVariable < Provided.GetCount(); ++iVariable )
                {
                    const ecs_package_variable& Variable = Provided[iVariable];

                    if( Variable.Semantic == Required.Semantic && Variable.Index == Required.Index &&
                        Variable.ComponentType == Required.ComponentType && Variable.Components >= Required.Components )
                    {
                        Found = TRUE;
                        break;
                    }
                }

                if( !Found )
                {
                    return Fail( Error, "incompatible combo interfaces" );
                }
            }
        }
    }

    return TRUE;
}

} // namespace

//==============================================================================
//  PACKAGE LOOKUP
//==============================================================================

const ecs_package_combo* ecs_FindPackageCombo( const ecs_package_index& Index, u32 Kind, u64 Key )
{
    s32 Low  = 0;
    s32 High = Index.Combos.GetCount();

    while( Low < High )
    {
        const s32                Mid   = Low + ( High - Low ) / 2;
        const ecs_package_combo& Combo = Index.Combos[Mid];

        if( Combo.Kind < Kind || ( Combo.Kind == Kind && Combo.Key < Key ) )
        {
            Low = Mid + 1;
        }
        else
        {
            High = Mid;
        }
    }

    return Low < Index.Combos.GetCount() && Index.Combos[Low].Kind == Kind && Index.Combos[Low].Key == Key
               ? &Index.Combos[Low]
               : NULL;
}

//==============================================================================

const ecs_package_program* ecs_FindPackageProgram( const ecs_package_index& Index,
                                                   u64                      Id,
                                                   u32                      Format,
                                                   u32                      Capabilities )
{
    s32 Low  = 0;
    s32 High = Index.Programs.GetCount();

    while( Low < High )
    {
        const s32                  Mid     = Low + ( High - Low ) / 2;
        const ecs_package_program& Program = Index.Programs[Mid];

        if( Program.Id < Id || ( Program.Id == Id && Program.Format < Format ) )
        {
            Low = Mid + 1;
        }
        else
        {
            High = Mid;
        }
    }

    const ecs_package_program* pSelected = NULL;

    while( Low < Index.Programs.GetCount() && Index.Programs[Low].Id == Id && Index.Programs[Low].Format == Format )
    {
        const ecs_package_program& Program = Index.Programs[Low++];

        if( !( Program.RequiredCapabilities & ~Capabilities ) &&
            ( !pSelected || Program.RequiredCapabilities > pSelected->RequiredCapabilities ) )
        {
            pSelected = &Program;
        }
    }

    return pSelected;
}

//==============================================================================
//  PACKAGE BUILDING
//==============================================================================

xbool ecs_BuildPackage( ecs_package_index&                 Index,
                        const xarray<const xarray<byte>*>& Bytecode,
                        xarray<byte>&                      File,
                        xstring&                           Error,
                        ecs_package_build_options*         pOptions )
{
    File.SetCount( 0 );
    Error.Clear();

    if( ( Bytecode.GetCount() == 0 ) || Bytecode.GetCount() != Index.Binaries.GetCount() ||
        Bytecode.GetCount() > ECS_PACKAGE_MAX_RECORDS || Index.Programs.GetCount() > ECS_PACKAGE_MAX_RECORDS ||
        Index.Combos.GetCount() > ECS_PACKAGE_MAX_RECORDS || Index.Layouts.GetCount() > ECS_PACKAGE_MAX_RECORDS )
    {
        return Fail( Error, "binary data count does not match index" );
    }

    const xtick                PreparationStart = x_GetTime();
    ecs_package_build_options  Defaults;
    ecs_package_build_options& Options = pOptions ? *pOptions : Defaults;

    Options.CachedBlocks = Options.CompressedBlocks = 0;
    Options.ZstdBlocks = Options.RawBlocks = 0;
    Options.PeakTemporaryBytes             = 0;

    // Logical programs can share reflection and code without losing identity.
    PackageStrings             LayoutStrings;
    xarray<ContentEntry>       LayoutLookup[256];
    xarray<xarray<byte>>       LayoutBytes;
    xarray<ecs_package_layout> Layouts;

    LayoutBytes.SetCapacity( Index.Layouts.GetCount() );
    Layouts.SetCapacity( Index.Layouts.GetCount() );

    xarray<u32> LayoutMap;

    LayoutMap.SetCount( Index.Layouts.GetCount() );

    for( u32 i = 0; i < Index.Layouts.GetCount(); ++i )
    {
        const ecs_package_layout& Layout = Index.Layouts[i];

        if( Layout.Bindings.GetCount() > 256 || Layout.Members.GetCount() > 8192 || Layout.Inputs.GetCount() > 256 ||
            Layout.Outputs.GetCount() > 256 )
        {
            return Fail( Error, "reflection exceeds package limits" );
        }

        PackageWriter Canonical;

        WriteLayout( Canonical, LayoutStrings, Layout );

        if( !LayoutStrings.Valid )
        {
            return Fail( Error, "invalid reflection string" );
        }

        const x_content_id Identity   = x_HashContent( Canonical.Data.GetPtr(), Canonical.Data.GetCount() );
        xarray<u32>&         Candidates = ContentCandidates( LayoutLookup[Identity[0]], Identity, 0 );
        s32                  Existing   = -1;

        for( s32 j = 0; j < Candidates.GetCount(); ++j )
        {
            if( EqualBytes( LayoutBytes[Candidates[j]], Canonical.Data ) )
            {
                Existing = Candidates[j];
                break;
            }
        }

        if( Existing >= 0 )
        {
            LayoutMap[i] = Existing;
        }
        else
        {
            LayoutMap[i] = Layouts.GetCount();
            Candidates.Append( LayoutMap[i] );
            Layouts.Append( Layout );
            LayoutBytes.Append( Canonical.Data );
        }
    }

    // Deduplicate code by target and shader interface.
    xarray<u32>                UniqueCode;
    xarray<ecs_package_binary> Binaries;
    xarray<u32>                BinaryMap;

    BinaryMap.SetCount( Bytecode.GetCount() );

    xarray<ContentEntry>   BinaryLookup[256];
    xarray<u32>            Order;
    xarray<x_content_id> Identities;

    UniqueCode.SetCapacity( Bytecode.GetCount() );
    Binaries.SetCapacity( Bytecode.GetCount() );
    Order.SetCapacity( Bytecode.GetCount() );
    Identities.SetCapacity( Bytecode.GetCount() );

    for( u32 i = 0; i < Bytecode.GetCount(); ++i )
    {
        if( ( Bytecode[i]->GetCount() == 0 ) || Bytecode[i]->GetCount() > ECS_BLOCK_MAX_SIZE )
        {
            return Fail( Error, "binary exceeds block limit" );
        }

        Identities.Append( x_HashContent( Bytecode[i]->GetPtr(), static_cast<u32>( Bytecode[i]->GetCount() ) ) );
        Order.Append( i );
    }

    xarray<BinaryGroup> Groups;

    Groups.SetCount( Bytecode.GetCount() );

    for( s32 i = 0; i < Groups.GetCount(); ++i )
    {
        Groups[i] = BinaryGroup( 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu );
    }

    for( s32 iProgram = 0; iProgram < Index.Programs.GetCount(); ++iProgram )
    {
        const ecs_package_program& Program = Index.Programs[iProgram];

        if( Program.Layout >= Index.Layouts.GetCount() || Program.Binary >= Groups.GetCount() )
        {
            return Fail( Error, "invalid program reference" );
        }

        const ecs_package_layout& Layout = Index.Layouts[Program.Layout];
        const BinaryGroup         Group( Program.Format, Program.Stage, Layout.InputInterface, Layout.OutputInterface );

        if( Group < Groups[Program.Binary] )
        {
            Groups[Program.Binary] = Group;
        }
    }

    for( s32 iGroup = 0; iGroup < Groups.GetCount(); ++iGroup )
    {
        const BinaryGroup& Group = Groups[iGroup];

        if( Group.Format == 0xffffffffu )
        {
            return Fail( Error, "unreferenced package binary" );
        }
    }

    x_qsort( Order.GetPtr(), Order.GetCount(),
             [&]( u32 A, u32 B )
             {
                 if( Groups[A] < Groups[B] )
                 {
                     return -1;
                 }

                 if( Groups[B] < Groups[A] )
                 {
                     return 1;
                 }

                 return Identities[A] < Identities[B] ? -1 : ( Identities[B] < Identities[A] ? 1 : 0 );
             } );

    for( s32 iOrder = 0; iOrder < Order.GetCount(); ++iOrder )
    {
        u32 i = Order[iOrder];

        xarray<u32>& Candidates =
            ContentCandidates( BinaryLookup[Identities[i][0]], Identities[i], Index.Binaries[i].Format );
        s32 Found = -1;

        for( s32 iCandidate = 0; iCandidate < Candidates.GetCount(); ++iCandidate )
        {
            if( EqualBytes( *Bytecode[UniqueCode[Candidates[iCandidate]]], *Bytecode[i] ) )
            {
                Found = iCandidate;
                break;
            }
        }

        if( Found >= 0 )
        {
            BinaryMap[i] = Candidates[Found];
        }
        else
        {
            BinaryMap[i] = static_cast<u32>( UniqueCode.GetCount() );
            Candidates.Append( BinaryMap[i] );
            UniqueCode.Append( i );
            Binaries.Append( Index.Binaries[i] );
        }
    }

    for( s32 iProgram = 0; iProgram < Index.Programs.GetCount(); ++iProgram )
    {
        ecs_package_program& Program = Index.Programs[iProgram];

        if( Program.Layout >= LayoutMap.GetCount() || Program.Binary >= BinaryMap.GetCount() )
        {
            return Fail( Error, "invalid program reference" );
        }

        Program.Layout = LayoutMap[Program.Layout];
        Program.Binary = BinaryMap[Program.Binary];
    }

    Index.Layouts  = Layouts;
    Index.Binaries = Binaries;
    Layouts.Clear();
    LayoutBytes.Clear();
    x_qsort( Index.Programs.GetPtr(), Index.Programs.GetCount(),
             []( const ecs_package_program& A, const ecs_package_program& B )
             {
                 if( A.Id != B.Id )
                 {
                     return A.Id < B.Id ? -1 : 1;
                 }

                 if( A.Format != B.Format )
                 {
                     return A.Format < B.Format ? -1 : 1;
                 }

                 return A.RequiredCapabilities < B.RequiredCapabilities
                            ? -1
                            : ( B.RequiredCapabilities < A.RequiredCapabilities ? 1 : 0 );
             } );
    x_qsort( Index.Combos.GetPtr(), Index.Combos.GetCount(),
             []( const ecs_package_combo& A, const ecs_package_combo& B )
             {
                 if( A.Kind != B.Kind )
                 {
                     return A.Kind < B.Kind ? -1 : 1;
                 }

                 return A.Key < B.Key ? -1 : ( B.Key < A.Key ? 1 : 0 );
             } );

    for( u32 i = 1; i < Index.Programs.GetCount(); ++i )
    {
        if( Index.Programs[i - 1].Id == Index.Programs[i].Id &&
            Index.Programs[i - 1].Format == Index.Programs[i].Format &&
            Index.Programs[i - 1].RequiredCapabilities == Index.Programs[i].RequiredCapabilities )
        {
            return Fail( Error, "duplicate program identity" );
        }
    }

    for( u32 i = 1; i < Index.Combos.GetCount(); ++i )
    {
        if( Index.Combos[i - 1].Kind == Index.Combos[i].Kind && Index.Combos[i - 1].Key == Index.Combos[i].Key )
        {
            return Fail( Error, "duplicate combo identity" );
        }
    }

    // Group binary extents into independently compressed blocks.
    PackageWriter  Chunks[CHUNKS];
    PackageStrings Text;

    Index.Blocks.Clear();

    xarray<PackageBlockTask> Tasks;

    for( u32 i = 0; i < UniqueCode.GetCount(); ++i )
    {
        const u32           Original = UniqueCode[i];
        const xarray<byte>& Bytes    = *Bytecode[Original];
        const u32 Aligned = Tasks.GetCount() ? ( Tasks[Tasks.GetCount() - 1].Block.DecodedSize + 15 ) & ~u32( 15 ) : 0;

        if( !Tasks.GetCount() || Index.Binaries[i - 1].Format != Index.Binaries[i].Format ||
            ( Aligned + Bytes.GetCount() > ECS_BLOCK_TARGET_SIZE ) )
        {
            if( Tasks.GetCount() == Tasks.GetCapacity() )
            {
                Tasks.SetCapacity( Tasks.GetCapacity() + MAX( 16, Tasks.GetCapacity() / 2 ) );
            }

            PackageBlockTask& Task = Tasks.Append();

            Task.First = i;
        }

        PackageBlockTask&   Task   = Tasks[Tasks.GetCount() - 1];
        ecs_package_binary& Binary = Index.Binaries[i];

        Binary.Block           = Tasks.GetCount() - 1;
        Binary.Offset          = ( Task.Block.DecodedSize + 15 ) & ~u32( 15 );
        Binary.Size            = Bytes.GetCount();
        Binary.Content         = Identities[Original];
        Binary.CRC             = CRC( Bytes );
        Task.Block.DecodedSize = Binary.Offset + Binary.Size;
        ++Task.Count;
    }

    Options.PreparationMs = x_TicksToMs( x_GetTime() - PreparationStart );

    const xtick CompressionStart = x_GetTime();

    if( !CompressPackageBlocks( Tasks, UniqueCode, Bytecode, Index.Binaries, Options, Chunks[CODE].Data, Error ) )
    {
        return FALSE;
    }

    Options.CompressionMs = x_TicksToMs( x_GetTime() - CompressionStart );

    const xtick SerializationStart = x_GetTime();

    Index.Blocks.SetCount( Tasks.GetCount() );

    for( s32 i = 0; i < Tasks.GetCount(); ++i )
    {
        Index.Blocks[i] = Tasks[i].Block;
    }

    Index.CodeSize = static_cast<u32>( Chunks[CODE].Data.GetCount() );

    if( !ValidIndex( Index, Error ) )
    {
        return FALSE;
    }

    // Serialize metadata before assigning chunk offsets.
    Chunks[META].Data.SetCapacity( 32 );
    Chunks[COMB].Data.SetCapacity( Index.Combos.GetCount() * ECS_PACKAGE_COMBO_SIZE );
    Chunks[PROG].Data.SetCapacity( Index.Programs.GetCount() * ECS_PACKAGE_PROGRAM_SIZE );
    Chunks[BIDX].Data.SetCapacity( 8 + Index.Binaries.GetCount() * ECS_PACKAGE_BINARY_SIZE +
                                   Index.Blocks.GetCount() * ECS_PACKAGE_BLOCK_SIZE );
    Chunks[META].U64( Index.CatalogId );
    Chunks[META].U64( Index.PackageId );
    Chunks[META].U32( ECS_PACKAGE_ABI );
    Chunks[META].U32( Index.TargetMask );
    Chunks[META].U32( Text.Add( Index.Name ) );
    Chunks[META].U32( 0 );

    for( s32 iCombo = 0; iCombo < Index.Combos.GetCount(); ++iCombo )
    {
        const ecs_package_combo& Combo = Index.Combos[iCombo];

        Chunks[COMB].U64( Combo.Key );
        Chunks[COMB].U64( Combo.Vertex );
        Chunks[COMB].U64( Combo.Pixel );
        Chunks[COMB].U32( Combo.Kind );
        Chunks[COMB].U32( Combo.Policies );
    }

    for( s32 iProgram = 0; iProgram < Index.Programs.GetCount(); ++iProgram )
    {
        const ecs_package_program& Program = Index.Programs[iProgram];

        Chunks[PROG].U64( Program.Id );
        Chunks[PROG].U32( Program.Stage );
        Chunks[PROG].U32( Program.Format );
        Chunks[PROG].U32( Text.Add( Program.Entry ) );
        Chunks[PROG].U32( Program.Layout );
        Chunks[PROG].U32( Program.Binary );
        Chunks[PROG].U32( Text.Add( Program.Name ) );
        Chunks[PROG].U32( Program.RequiredCapabilities );

        for( u32 i = 0; i < 7; ++i )
        {
            Chunks[PROG].U32( 0 );
        }
    }

    Chunks[REFL].U32( static_cast<u32>( Index.Layouts.GetCount() ) );

    for( s32 iLayout = 0; iLayout < Index.Layouts.GetCount(); ++iLayout )
    {
        const ecs_package_layout& Layout = Index.Layouts[iLayout];
        WriteLayout( Chunks[REFL], Text, Layout );
    }

    Chunks[BIDX].U32( static_cast<u32>( Index.Binaries.GetCount() ) );
    Chunks[BIDX].U32( static_cast<u32>( Index.Blocks.GetCount() ) );

    for( s32 iBinary = 0; iBinary < Index.Binaries.GetCount(); ++iBinary )
    {
        const ecs_package_binary& Binary = Index.Binaries[iBinary];

        AppendBytes( Chunks[BIDX].Data, Binary.Content.GetPtr(), 32 );
        Chunks[BIDX].U32( Binary.Format );
        Chunks[BIDX].U32( Binary.Block );
        Chunks[BIDX].U32( Binary.Offset );
        Chunks[BIDX].U32( Binary.Size );
        Chunks[BIDX].U32( Binary.CRC );

        for( u32 i = 0; i < 3; ++i )
        {
            Chunks[BIDX].U32( 0 );
        }
    }

    for( s32 iBlock = 0; iBlock < Index.Blocks.GetCount(); ++iBlock )
    {
        const ecs_package_block& Block = Index.Blocks[iBlock];

        Chunks[BIDX].U32( Block.Offset );
        Chunks[BIDX].U32( Block.StoredSize );
        Chunks[BIDX].U32( Block.DecodedSize );
        Chunks[BIDX].U32( Block.Codec );
        Chunks[BIDX].U32( Block.StoredCRC );
        Chunks[BIDX].U32( Block.DecodedCRC );
        Chunks[BIDX].U32( 0 );
        Chunks[BIDX].U32( 0 );
    }

    Chunks[STRS].Data = Text.Data;

    if( !Text.Valid )
    {
        return Fail( Error, "invalid package string" );
    }

    PackageWriter Out( File );
    u64           FileSize = ECS_PACKAGE_HEADER_SIZE + CHUNKS * ECS_PACKAGE_CHUNK_SIZE;

    for( u32 i = 0; i < CHUNKS; ++i )
    {
        FileSize = ( ( FileSize + 15 ) & ~u64( 15 ) ) + Chunks[i].Data.GetCount();
    }

    if( FileSize > ECS_PACKAGE_MAX_FILE_SIZE )
    {
        return Fail( Error, "package exceeds file limit" );
    }

    Out.Data.SetCapacity( static_cast<s32>( FileSize ) );
    Out.Data.SetCount( ECS_PACKAGE_HEADER_SIZE + CHUNKS * ECS_PACKAGE_CHUNK_SIZE );
    x_memset( Out.Data.GetPtr(), 0, Out.Data.GetCount() );

    u64 MetadataSize = 0;

    for( u32 i = 0; i < CHUNKS; ++i )
    {
        if( i != CODE )
        {
            MetadataSize += Chunks[i].Data.GetCount();
        }

        if( MetadataSize > ECS_PACKAGE_MAX_METADATA )
        {
            return Fail( Error, "metadata exceeds limit" );
        }

        Out.Align();

        const u32 Offset = static_cast<u32>( Out.Data.GetCount() );

        AppendBytes( Out.Data, Chunks[i].Data.GetPtr(), Chunks[i].Data.GetCount() );

        if( Out.Data.GetCount() > ECS_PACKAGE_MAX_FILE_SIZE )
        {
            return Fail( Error, "package exceeds file limit" );
        }

        byte* pDesc = Out.Data.GetPtr() + ECS_PACKAGE_HEADER_SIZE + i * ECS_PACKAGE_CHUNK_SIZE;

        x_store_le32( pDesc + ECS_PACKAGE_CHUNK_TYPE, s_ChunkTypes[i] );
        x_store_le32( pDesc + ECS_PACKAGE_CHUNK_ABI, ECS_PACKAGE_ABI );
        x_store_le32( pDesc + ECS_PACKAGE_CHUNK_FLAGS, 1 );
        x_store_le32( pDesc + ECS_PACKAGE_CHUNK_OFFSET, Offset );
        x_store_le32( pDesc + ECS_PACKAGE_CHUNK_STORED_SIZE, static_cast<u32>( Chunks[i].Data.GetCount() ) );
        x_store_le32( pDesc + ECS_PACKAGE_CHUNK_DECODED_SIZE, static_cast<u32>( Chunks[i].Data.GetCount() ) );
        x_store_le32( pDesc + ECS_PACKAGE_CHUNK_CRC, i == CODE ? 0 : CRC( Chunks[i].Data ) );
    }

    // Write the header after final offsets are known.
    byte* pHeader = Out.Data.GetPtr();

    x_store_le32( pHeader + ECS_PACKAGE_HEADER_MAGIC, FourCC( 'E', 'C', 'S', 0 ) );
    x_store_le32( pHeader + ECS_PACKAGE_HEADER_VERSION, ECS_PACKAGE_MAJOR );
    x_store_le32( pHeader + ECS_PACKAGE_HEADER_SIZE_FIELD, ECS_PACKAGE_HEADER_SIZE );
    x_store_le32( pHeader + ECS_PACKAGE_HEADER_FILE_SIZE, static_cast<u32>( Out.Data.GetCount() ) );
    x_store_le32( pHeader + ECS_PACKAGE_HEADER_CHUNK_COUNT, CHUNKS );
    x_store_le32( pHeader + ECS_PACKAGE_HEADER_CHUNK_STRIDE, ECS_PACKAGE_CHUNK_SIZE );
    Options.SerializationMs = x_TicksToMs( x_GetTime() - SerializationStart );

    return TRUE;
}

//==============================================================================
//  INDEX READING
//==============================================================================

xbool ecs_ReadPackageIndex( ecs_package_read   Read,
                            void*              pContext,
                            u32                FileSize,
                            u64                ExpectedCatalog,
                            ecs_package_index& Result,
                            xstring&           Error )
{
    Result = {};
    Error.Clear();

    ecs_package_index Index;

    if( !Read || FileSize < ECS_PACKAGE_HEADER_SIZE || FileSize > ECS_PACKAGE_MAX_FILE_SIZE )
    {
        return Fail( Error, "invalid physical file size" );
    }

    // Validate the physical header and chunk directory.
    byte Header[ECS_PACKAGE_HEADER_SIZE];

    if( !Read( pContext, 0, Header, sizeof( Header ) ) ||
        x_load_le32( Header + ECS_PACKAGE_HEADER_MAGIC ) != FourCC( 'E', 'C', 'S', 0 ) ||
        x_load_le32( Header + ECS_PACKAGE_HEADER_VERSION ) != ECS_PACKAGE_MAJOR ||
        x_load_le32( Header + ECS_PACKAGE_HEADER_SIZE_FIELD ) != ECS_PACKAGE_HEADER_SIZE ||
        x_load_le32( Header + ECS_PACKAGE_HEADER_FILE_SIZE ) != FileSize ||
        x_load_le32( Header + ECS_PACKAGE_HEADER_CHUNK_STRIDE ) != ECS_PACKAGE_CHUNK_SIZE ||
        x_load_le32( Header + ECS_PACKAGE_HEADER_FLAGS ) || x_load_le32( Header + ECS_PACKAGE_HEADER_RESERVED ) )
    {
        return Fail( Error, "invalid ECS package header/version" );
    }

    const u32 Count = x_load_le32( Header + ECS_PACKAGE_HEADER_CHUNK_COUNT );

    if( Count < CHUNKS || Count > 64 ||
        static_cast<u64>( ECS_PACKAGE_HEADER_SIZE ) + Count * ECS_PACKAGE_CHUNK_SIZE > FileSize )
    {
        return Fail( Error, "invalid chunk directory size" );
    }

    xarray<byte> Directory;

    Directory.SetCount( Count * ECS_PACKAGE_CHUNK_SIZE );

    if( !Read( pContext, ECS_PACKAGE_HEADER_SIZE, Directory.GetPtr(), static_cast<u32>( Directory.GetCount() ) ) )
    {
        return Fail( Error, "chunk directory read failed" );
    }

    // Load the required metadata chunks.
    xarray<byte>         Chunks[CHUNKS];
    xbool                Seen[CHUNKS] = {};
    xarray<PackageRange> Ranges;
    u64                  MetadataSize = 0;

    for( u32 i = 0; i < Count; ++i )
    {
        const byte* pDesc = Directory.GetPtr() + i * ECS_PACKAGE_CHUNK_SIZE;
        const u32   Type  = x_load_le32( pDesc + ECS_PACKAGE_CHUNK_TYPE ),
                  Flags   = x_load_le32( pDesc + ECS_PACKAGE_CHUNK_FLAGS );
        const u32 Offset  = x_load_le32( pDesc + ECS_PACKAGE_CHUNK_OFFSET ),
                  Size    = x_load_le32( pDesc + ECS_PACKAGE_CHUNK_STORED_SIZE );

        if( Flags & ~1u || x_load_le32( pDesc + ECS_PACKAGE_CHUNK_RESERVED ) ||
            Size != x_load_le32( pDesc + ECS_PACKAGE_CHUNK_DECODED_SIZE ) || ( Offset & 15 ) ||
            Offset < ECS_PACKAGE_HEADER_SIZE + Count * ECS_PACKAGE_CHUNK_SIZE ||
            static_cast<u64>( Offset ) + Size > FileSize )
        {
            return Fail( Error, "invalid chunk extent" );
        }

        Ranges.Append( PackageRange( Offset, static_cast<u64>( Offset ) + Size ) );

        u32 Kind = 0;

        while( Kind < CHUNKS && s_ChunkTypes[Kind] != Type )
        {
            ++Kind;
        }

        if( Kind == CHUNKS )
        {
            if( Flags & 1 )
            {
                return Fail( Error, "unknown required chunk" );
            }

            continue;
        }

        if( Seen[Kind] || x_load_le32( pDesc + ECS_PACKAGE_CHUNK_ABI ) != ECS_PACKAGE_ABI || Flags != 1 )
        {
            return Fail( Error, "invalid chunk ABI or duplicate chunk" );
        }

        Seen[Kind] = TRUE;

        if( Kind == CODE )
        {
            if( x_load_le32( pDesc + ECS_PACKAGE_CHUNK_CRC ) )
            {
                return Fail( Error, "CODE checksum must be carried by block records" );
            }

            Index.CodeOffset = Offset;
            Index.CodeSize   = Size;
            continue;
        }

        MetadataSize += Size;

        if( MetadataSize > ECS_PACKAGE_MAX_METADATA )
        {
            return Fail( Error, "metadata allocation limit" );
        }

        Chunks[Kind].SetCount( Size );

        if( Size && !Read( pContext, Offset, Chunks[Kind].GetPtr(), Size ) )
        {
            return Fail( Error, "metadata read failed" );
        }

        if( CRC( Chunks[Kind] ) != x_load_le32( pDesc + ECS_PACKAGE_CHUNK_CRC ) )
        {
            return Fail( Error, "metadata CRC mismatch" );
        }
    }

    for( xbool Found : Seen )
    {
        if( !Found )
        {
            return Fail( Error, "missing required package table" );
        }
    }

    x_qsort( Ranges.GetPtr(), Ranges.GetCount(),
             []( const PackageRange& A, const PackageRange& B ) { return A < B ? -1 : ( B < A ? 1 : 0 ); } );

    for( u32 i = 1; i < Ranges.GetCount(); ++i )
    {
        if( Ranges[i].Start < Ranges[i - 1].End )
        {
            return Fail( Error, "overlapping chunks" );
        }
    }

    if( Chunks[META].GetCount() != 32 || ( Chunks[STRS].GetCount() == 0 ) || Chunks[STRS][0] )
    {
        return Fail( Error, "invalid META/STRS" );
    }

    PackageReader Meta{ Chunks[META] };

    Index.CatalogId = Meta.U64();
    Index.PackageId = Meta.U64();

    if( Meta.U32() != ECS_PACKAGE_ABI )
    {
        return Fail( Error, "shader ABI mismatch" );
    }

    Index.TargetMask = Meta.U32();

    if( !ReadString( Chunks[STRS], Meta.U32(), Index.Name, TRUE ) || Meta.U32() ||
        ( ExpectedCatalog && Index.CatalogId != ExpectedCatalog ) )
    {
        return Fail( Error, "catalog identity mismatch" );
    }

    if( Chunks[COMB].GetCount() % ECS_PACKAGE_COMBO_SIZE || Chunks[PROG].GetCount() % ECS_PACKAGE_PROGRAM_SIZE ||
        Chunks[COMB].GetCount() / ECS_PACKAGE_COMBO_SIZE > ECS_PACKAGE_MAX_RECORDS ||
        Chunks[PROG].GetCount() / ECS_PACKAGE_PROGRAM_SIZE > ECS_PACKAGE_MAX_RECORDS )
    {
        return Fail( Error, "invalid record table size" );
    }

    PackageReader Combos{ Chunks[COMB] };

    Index.Combos.SetCapacity( Chunks[COMB].GetCount() / ECS_PACKAGE_COMBO_SIZE );

    while( Combos.Position < Chunks[COMB].GetCount() )
    {
        ecs_package_combo Combo;

        Combo.Key      = Combos.U64();
        Combo.Vertex   = Combos.U64();
        Combo.Pixel    = Combos.U64();
        Combo.Kind     = Combos.U32();
        Combo.Policies = Combos.U32();

        if( Index.Combos.GetCount() )
        {
            const ecs_package_combo& Previous = Index.Combos[Index.Combos.GetCount() - 1];

            if( Previous.Kind > Combo.Kind || ( Previous.Kind == Combo.Kind && Previous.Key >= Combo.Key ) )
            {
                return Fail( Error, "unsorted or duplicate combos" );
            }
        }

        Index.Combos.Append( Combo );
    }

    PackageReader Programs{ Chunks[PROG] };

    Index.Programs.SetCapacity( Chunks[PROG].GetCount() / ECS_PACKAGE_PROGRAM_SIZE );

    while( Programs.Position < Chunks[PROG].GetCount() )
    {
        ecs_package_program Program;

        Program.Id     = Programs.U64();
        Program.Stage  = Programs.U32();
        Program.Format = Programs.U32();

        if( !ReadString( Chunks[STRS], Programs.U32(), Program.Entry, TRUE ) )
        {
            return Fail( Error, "invalid entry string" );
        }

        Program.Layout = Programs.U32();
        Program.Binary = Programs.U32();

        if( !ReadString( Chunks[STRS], Programs.U32(), Program.Name, FALSE ) )
        {
            return Fail( Error, "invalid program name" );
        }

        Program.RequiredCapabilities = Programs.U32();

        for( u32 i = 0; i < 7; ++i )
        {
            if( Programs.U32() )
            {
                return Fail( Error, "nonzero program reserved field" );
            }
        }

        if( Index.Programs.GetCount() )
        {
            const ecs_package_program& Previous = Index.Programs[Index.Programs.GetCount() - 1];

            if( Previous.Id > Program.Id ||
                ( Previous.Id == Program.Id && ( Previous.Format > Program.Format ||
                                                 ( Previous.Format == Program.Format &&
                                                   Previous.RequiredCapabilities >= Program.RequiredCapabilities ) ) ) )
            {
                return Fail( Error, "unsorted or duplicate programs" );
            }
        }

        Index.Programs.Append( Program );
    }

    // Read reflection layouts and resource bindings.
    PackageReader Reflection{ Chunks[REFL] };
    const u32     LayoutCount = Reflection.Count( ECS_PACKAGE_MAX_RECORDS );

    if( LayoutCount > Chunks[REFL].GetCount() / 40 )
    {
        return Fail( Error, "invalid reflection count" );
    }

    Index.Layouts.SetCapacity( LayoutCount );

    for( u32 i = 0; i < LayoutCount; ++i )
    {
        ecs_package_layout Layout;

        Layout.InputInterface  = Reflection.U32();
        Layout.OutputInterface = Reflection.U32();

        for( u32& Count : Layout.Resources )
        {
            Count = Reflection.U32();
        }

        const u32 Bindings = Reflection.Count( 256 ), Members = Reflection.Count( 8192 );
        const u32 Inputs = Reflection.Count( 256 ), Outputs = Reflection.Count( 256 );

        Layout.Bindings.SetCapacity( Bindings );
        Layout.Members.SetCapacity( Members );
        Layout.Inputs.SetCapacity( Inputs );
        Layout.Outputs.SetCapacity( Outputs );

        for( u32 j = 0; j < Bindings; ++j )
        {
            ecs_package_binding Binding;

            Binding.Kind     = Reflection.U32();
            Binding.Slot     = Reflection.U32();
            Binding.Count    = Reflection.U32();
            Binding.ByteSize = Reflection.U32();

            if( !ReadString( Chunks[STRS], Reflection.U32(), Binding.Name, TRUE ) )
            {
                return Fail( Error, "invalid binding name" );
            }

            Layout.Bindings.Append( Binding );
        }

        for( u32 j = 0; j < Members; ++j )
        {
            ecs_package_member Member;

            if( !ReadString( Chunks[STRS], Reflection.U32(), Member.Buffer, TRUE ) ||
                !ReadString( Chunks[STRS], Reflection.U32(), Member.Name, FALSE ) )
            {
                return Fail( Error, "invalid member name" );
            }

            Member.Offset       = Reflection.U32();
            Member.Size         = Reflection.U32();
            Member.ArrayStride  = Reflection.U32();
            Member.MatrixStride = Reflection.U32();
            Layout.Members.Append( Member );
        }

        for( u32 Side = 0; Side < 2; ++Side )
        {
            auto& Variables = Side ? Layout.Outputs : Layout.Inputs;

            for( u32 j = 0; j < ( Side ? Outputs : Inputs ); ++j )
            {
                ecs_package_variable Variable;

                if( !ReadString( Chunks[STRS], Reflection.U32(), Variable.Semantic, TRUE ) )
                {
                    return Fail( Error, "invalid interface semantic" );
                }

                Variable.Index         = Reflection.U32();
                Variable.Components    = Reflection.U32();
                Variable.ComponentType = Reflection.U32();
                Variables.Append( Variable );
            }
        }

        Index.Layouts.Append( Layout );
    }

    if( !Reflection.Done() )
    {
        return Fail( Error, "reflection table extent mismatch" );
    }

    // Read binary and compressed-block records.
    PackageReader BinaryIndex{ Chunks[BIDX] };
    const u32     BinaryCount = BinaryIndex.Count( ECS_PACKAGE_MAX_RECORDS ),
              BlockCount      = BinaryIndex.Count( ECS_PACKAGE_MAX_RECORDS );

    if( !BinaryIndex.Valid || 8ull + static_cast<u64>( BinaryCount ) * ECS_PACKAGE_BINARY_SIZE +
                                      static_cast<u64>( BlockCount ) * ECS_PACKAGE_BLOCK_SIZE !=
                                  Chunks[BIDX].GetCount() )
    {
        return Fail( Error, "binary index extent mismatch" );
    }

    Index.Binaries.SetCapacity( BinaryCount );
    Index.Blocks.SetCapacity( BlockCount );

    for( u32 i = 0; i < BinaryCount; ++i )
    {
        ecs_package_binary Binary;

        x_memcpy( Binary.Content.GetPtr(), Chunks[BIDX].GetPtr() + BinaryIndex.Position, 32 );
        BinaryIndex.Position += 32;
        Binary.Format = BinaryIndex.U32();
        Binary.Block  = BinaryIndex.U32();
        Binary.Offset = BinaryIndex.U32();
        Binary.Size   = BinaryIndex.U32();
        Binary.CRC    = BinaryIndex.U32();

        for( u32 j = 0; j < 3; ++j )
        {
            if( BinaryIndex.U32() )
            {
                return Fail( Error, "nonzero binary reserved field" );
            }
        }

        Index.Binaries.Append( Binary );
    }

    for( u32 i = 0; i < BlockCount; ++i )
    {
        ecs_package_block Block;

        Block.Offset      = BinaryIndex.U32();
        Block.StoredSize  = BinaryIndex.U32();
        Block.DecodedSize = BinaryIndex.U32();
        Block.Codec       = static_cast<ecs_block_codec>( BinaryIndex.U32() );
        Block.StoredCRC   = BinaryIndex.U32();
        Block.DecodedCRC  = BinaryIndex.U32();

        if( BinaryIndex.U32() || BinaryIndex.U32() )
        {
            return Fail( Error, "nonzero block reserved field" );
        }

        Index.Blocks.Append( Block );
    }

    if( !ValidIndex( Index, Error ) )
    {
        return FALSE;
    }

    // Build the block-to-binary lookup after validating all references.
    Index.BlockBinaryOffsets.SetCount( BlockCount + 1 );
    x_memset( Index.BlockBinaryOffsets.GetPtr(), 0, ( BlockCount + 1 ) * sizeof( u32 ) );

    for( u32 i = 0; i < BinaryCount; ++i )
    {
        ++Index.BlockBinaryOffsets[Index.Binaries[i].Block + 1];
    }

    for( u32 i = 1; i <= BlockCount; ++i )
    {
        Index.BlockBinaryOffsets[i] += Index.BlockBinaryOffsets[i - 1];
    }

    xarray<u32> Next = Index.BlockBinaryOffsets;

    Index.BlockBinaries.SetCount( BinaryCount );

    for( u32 i = 0; i < BinaryCount; ++i )
    {
        Index.BlockBinaries[Next[Index.Binaries[i].Block]++] = i;
    }

    Result = Index;

    return TRUE;
}

//==============================================================================
//  BLOCK READING
//==============================================================================

xbool ecs_ReadPackageBlock( ecs_package_read         Read,
                            void*                    pContext,
                            const ecs_package_index& Index,
                            u32                      BlockIndex,
                            xarray<byte>&            Decoded,
                            xstring&                 Error )
{
    Decoded.SetCount( 0 );
    Error.Clear();

    if( !Read || BlockIndex >= Index.Blocks.GetCount() )
    {
        return Fail( Error, "invalid block request" );
    }

    const ecs_package_block& Block = Index.Blocks[BlockIndex];

    if( !Block.DecodedSize || Block.DecodedSize > ECS_BLOCK_MAX_SIZE || !Block.StoredSize ||
        Block.StoredSize > ecs_CompressBound( Block.DecodedSize ) || ( Block.Offset & 15 ) ||
        ( Block.Codec != ECS_BLOCK_RAW && Block.Codec != ECS_BLOCK_ZSTD ) ||
        ( Block.Codec == ECS_BLOCK_RAW && Block.StoredSize != Block.DecodedSize ) ||
        static_cast<u64>( Block.Offset ) + Block.StoredSize > Index.CodeSize ||
        static_cast<u64>( Index.CodeOffset ) + Block.Offset + Block.StoredSize > ECS_PACKAGE_MAX_FILE_SIZE )
    {
        return Fail( Error, "block allocation limit" );
    }

    xarray<byte> Stored;

    Stored.SetCount( Block.StoredSize );

    if( !Read( pContext, Index.CodeOffset + Block.Offset, Stored.GetPtr(), Block.StoredSize ) )
    {
        return Fail( Error, "block read or stored CRC failed" );
    }

    ecs_block_context Context;

    return ecs_DecodePackageBlock( Context, Index, BlockIndex, Stored.GetPtr(), Block.StoredSize, Decoded, Error );
}

//==============================================================================
//  BLOCK VALIDATION AND DECODING
//==============================================================================

xbool ecs_DecodePackageBlock( ecs_block_context&       Context,
                              const ecs_package_index& Index,
                              u32                      BlockIndex,
                              const byte*              pStored,
                              u32                      StoredSize,
                              xarray<byte>&            Decoded,
                              xstring&                 Error )
{
    Decoded.SetCount( 0 );
    Error.Clear();

    if( !pStored || BlockIndex >= Index.Blocks.GetCount() ||
        Index.BlockBinaryOffsets.GetCount() != Index.Blocks.GetCount() + 1 ||
        Index.BlockBinaries.GetCount() != Index.Binaries.GetCount() )
    {
        return Fail( Error, "invalid block decode request" );
    }

    const ecs_package_block& Block = Index.Blocks[BlockIndex];

    if( !Block.DecodedSize || Block.DecodedSize > ECS_BLOCK_MAX_SIZE || StoredSize != Block.StoredSize ||
        x_chksum( pStored, StoredSize ) != Block.StoredCRC )
    {
        return Fail( Error, "block allocation limit or stored CRC failed" );
    }

    Decoded.SetCount( Block.DecodedSize );

    if( !Context.Decode( Block.Codec, pStored, StoredSize, Decoded.GetPtr(), Block.DecodedSize ) ||
        CRC( Decoded ) != Block.DecodedCRC )
    {
        Decoded.Clear();
        return Fail( Error, "block decode or decoded CRC failed" );
    }

    const u32 Begin = Index.BlockBinaryOffsets[BlockIndex];
    const u32 End   = Index.BlockBinaryOffsets[BlockIndex + 1];

    if( Begin > End || End > static_cast<u32>( Index.BlockBinaries.GetCount() ) )
    {
        Decoded.SetCount( 0 );
        return Fail( Error, "invalid block binary range" );
    }

    for( u32 i = Begin; i < End; ++i )
    {
        if( Index.BlockBinaries[i] >= static_cast<u32>( Index.Binaries.GetCount() ) )
        {
            Decoded.SetCount( 0 );
            return Fail( Error, "invalid block binary index" );
        }

        const ecs_package_binary& Binary = Index.Binaries[Index.BlockBinaries[i]];

        if( Binary.Block != BlockIndex || static_cast<u64>( Binary.Offset ) + Binary.Size > Decoded.GetCount() ||
            x_chksum( Decoded.GetPtr() + Binary.Offset, Binary.Size ) != Binary.CRC ||
            x_HashContent( Decoded.GetPtr() + Binary.Offset, Binary.Size ) != Binary.Content )
        {
            Decoded.Clear();
            return Fail( Error, "binary content identity mismatch" );
        }
    }

    return TRUE;
}
