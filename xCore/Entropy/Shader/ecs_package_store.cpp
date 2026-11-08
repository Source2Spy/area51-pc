//==============================================================================
//
//  ecs_package_store.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ecs_package_store.hpp"
#include "x_threads.hpp"
#include "x_time.hpp"

//==============================================================================
//  TYPES
//==============================================================================

struct ecs_block_request : public xref_counted
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    ecs_block_request( void )
    {
        x_AtomicInit( &State, ECS_PROGRAM_PENDING );
        x_AtomicInit( &Subscribers, 0 );
        x_AtomicInit( &Stale, 0 );
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    ecs_package_ptr      Package;
    u32                  Block = 0;
    xref<ecs_block_data> Data;
    ecs_block_view       View;
    xstring              Error;
    x_atomic_s32         State;
    x_atomic_s32         Subscribers;
    x_atomic_s32         Stale;
    xbool                Installed = FALSE;
    f64                  ReadMs    = 0;
    f64                  DecodeMs  = 0;
};

//------------------------------------------------------------------------------

struct ecs_package_jobs
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    ecs_package_jobs( void )
        : Dispatch( 1, 0 ),
          Continue( 1, 0 )
    {
        x_AtomicInit( &Stop, 0 );
        x_AtomicInit( &InFlightBytes, 0 );
        x_AtomicInit( &PeakInFlightBytes, 0 );
    }

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    xmutex                          Lock;
    xsema                           Dispatch;
    xsema                           Continue;
    xthread*                        pWorker = NULL;
    char*                           Arguments[1];
    xarray<xref<ecs_block_request>> Queue;
    xarray<xref<ecs_block_request>> Requests;
    u32                             Next = 0;
    x_atomic_s32                    Stop;
    x_atomic_u64                    InFlightBytes;
    x_atomic_u64                    PeakInFlightBytes;
};

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

namespace
{

//==============================================================================

xbool ReadSource( void* pContext, u32 Offset, byte* pData, u32 Size )
{
    return static_cast<ecs_package_source*>( pContext )->Read( Offset, pData, Size );
}

//==============================================================================

void AddSubscriber( ecs_block_request& Request, s32 Change )
{
    s32 Previous = x_AtomicLoadAcquire( &Request.Subscribers );

    for( ;; )
    {
        const s32 Actual = x_AtomicCompareExchange( &Request.Subscribers, Previous + Change, Previous );

        if( Actual == Previous )
        {
            return;
        }

        Previous = Actual;
    }
}

//==============================================================================

void PackageReadWorker( s32, char** ppArguments )
{
    ecs_package_jobs& Jobs = *reinterpret_cast<ecs_package_jobs*>( ppArguments[0] );
    ecs_block_context Context;
    xarray<byte>      Stored;

    for( ;; )
    {
        Jobs.Dispatch.Acquire();

        for( ;; )
        {
            Jobs.Lock.Enter();

            if( x_AtomicLoadAcquire( &Jobs.Stop ) )
            {
                Jobs.Lock.Exit();
                return;
            }

            if( Jobs.Next >= static_cast<u32>( Jobs.Queue.GetCount() ) )
            {
                Jobs.Queue.SetCount( 0 );
                Jobs.Next = 0;
                Jobs.Lock.Exit();
                break;
            }

            xref<ecs_block_request> Request = Jobs.Queue[Jobs.Next];

            Jobs.Queue[Jobs.Next++].Clear();
            Jobs.Lock.Exit();

            s32 State = ECS_PROGRAM_CANCELLED;

            if( !x_AtomicLoadAcquire( &Request->Stale ) && x_AtomicLoadAcquire( &Request->Subscribers ) > 0 )
            {
                const ecs_package_index& Index = Request->Package->Index;
                const ecs_package_block& Block = Index.Blocks[Request->Block];
                const u64                Bytes = static_cast<u64>( Block.StoredSize ) + Block.DecodedSize;

                x_AtomicStoreRelease( &Jobs.InFlightBytes, Bytes );

                if( Bytes > x_AtomicLoadAcquire( &Jobs.PeakInFlightBytes ) )
                {
                    x_AtomicStoreRelease( &Jobs.PeakInFlightBytes, Bytes );
                }

                Stored.SetCount( Block.StoredSize );
                Request->Data = new ecs_block_data;

                const xtick ReadStart = x_GetTime();
                const xbool Read = Request->Package->Source->Read( Index.CodeOffset + Block.Offset, Stored.GetPtr(),
                                                                   Block.StoredSize );

                Request->ReadMs = x_TicksToMs( x_GetTime() - ReadStart );

                const xtick DecodeStart = x_GetTime();

                if( !Read )
                {
                    Request->Error = "package block read failed";
                }

                State = Read && ecs_DecodePackageBlock( Context, Index, Request->Block, Stored.GetPtr(),
                                                        Block.StoredSize, Request->Data->Bytes, Request->Error )
                            ? ECS_PROGRAM_READY
                            : ECS_PROGRAM_FAILED;
                Request->DecodeMs = x_TicksToMs( x_GetTime() - DecodeStart );
            }

            Stored.SetCount( 0 );

            if( Stored.GetCapacity() > ECS_BLOCK_TARGET_SIZE )
            {
                Stored.FreeExtra();
            }
            // Requests owns this pending record until submission consumes completion.
            ecs_block_request* pRequest = Request.Get();

            Request.Clear();
            x_AtomicStoreRelease( &pRequest->State, State );
            Jobs.Continue.Acquire();

            if( x_AtomicLoadAcquire( &Jobs.Stop ) )
            {
                return;
            }
        }
    }
}

} // namespace

//==============================================================================
//  FUNCTIONS
//==============================================================================
//  REQUEST LIFETIME
//==============================================================================

ecs_program_request::ecs_program_request( void )
    : m_pProgram( NULL ),
      m_isCancelled( FALSE ),
      m_isAttached( FALSE )
{
}

//==============================================================================

ecs_program_request::~ecs_program_request( void )
{
    Detach();
}

//==============================================================================

void ecs_program_request::Detach( void )
{
    if( m_isAttached )
    {
        AddSubscriber( *m_Block, -1 );
        m_isAttached = FALSE;
    }
}

//==============================================================================
//  PROGRAM VIEWS
//==============================================================================

const byte* ecs_program_view::Code( void ) const
{
    if( !Package || !Program || !Block.Data )
    {
        return NULL;
    }

    const ecs_package_binary& Binary = Package->Index.Binaries[Program->Binary];

    if( static_cast<u64>( Binary.Offset ) + Binary.Size > Block.Data->Bytes.GetCount() )
    {
        return NULL;
    }

    return Block.Data->Bytes.GetPtr() + Binary.Offset;
}

//==============================================================================

const ecs_package_layout* ecs_program_view::Layout( void ) const
{
    return Package && Program ? &Package->Index.Layouts[Program->Layout] : NULL;
}

//==============================================================================

void ecs_program_view::Reset( void )
{
    Program = NULL;
    Block.Reset();
    Package.Clear();
}

//==============================================================================
//  STORE LIFETIME
//==============================================================================

ecs_package_store::ecs_package_store( u64 DecodedBudget )
    : m_Blocks( DecodedBudget )
{
}

//==============================================================================

ecs_package_store::~ecs_package_store( void )
{
    Clear();
}

//==============================================================================
//  PACKAGE LOOKUP
//==============================================================================

ecs_package_ptr ecs_package_store::Find( const xstring& File, u64 CatalogId ) const
{
    for( s32 i = 0; i < m_Packages.GetCount(); ++i )
    {
        if( m_Packages[i].CatalogId == CatalogId && m_Packages[i].File == File )
        {
            return m_Packages[i].Package;
        }
    }

    return NULL;
}

//==============================================================================
//  PACKAGE LOADING
//==============================================================================

xbool ecs_package_store::Open( const xstring&   File,
                               ecs_source_ptr   Source,
                               u64              CatalogId,
                               ecs_package_ptr& Package,
                               xstring&         Error )
{
    Package.Clear();
    Error.Clear();

    if( File.IsEmpty() || !Source || !CatalogId )
    {
        Error = "invalid package store request";
        return FALSE;
    }

    if( auto Existing = Find( File, CatalogId ) )
    {
        Package = Existing;
        return TRUE;
    }

    xref<ecs_loaded_package> Loaded( new ecs_loaded_package );

    if( !ecs_ReadPackageIndex( ReadSource, Source.Get(), Source->Size(), CatalogId, Loaded->Index, Error ) )
    {
        return FALSE;
    }

    for( s32 i = 0; i < m_Packages.GetCount(); ++i )
    {
        if( m_Packages[i].CatalogId == CatalogId && m_Packages[i].Package->Index.PackageId == Loaded->Index.PackageId )
        {
            // One catalog cannot publish two physical definitions of a family.
            Error = "duplicate package identity under another filename";
            return FALSE;
        }
    }

    Loaded->Source = Source;

    PackageEntry& Entry = m_Packages.Append();

    Entry.CatalogId = CatalogId;
    Entry.File      = File;
    Entry.Package   = Loaded;
    Package         = Loaded;

    return TRUE;
}

//==============================================================================
//  SYNCHRONOUS ACQUISITION
//==============================================================================

xbool ecs_package_store::AcquireProgram( const ecs_package_ptr& Package,
                                         u64                    ProgramId,
                                         u32                    Format,
                                         ecs_program_view&      View,
                                         xstring&               Error )
{
    View.Reset();

    ecs_request_ptr Request;

    if( !RequestProgram( Package, ProgramId, Format, Request, Error ) )
    {
        return FALSE;
    }

    ecs_program_request_status Status;

    while( ( Status = PollProgram( Request, View, Error ) ) == ECS_PROGRAM_PENDING )
    {
        x_DelayThread( 1 );
    }

    ReleaseRequest( Request );

    return Status == ECS_PROGRAM_READY;
}

//==============================================================================

xbool ecs_package_store::AcquireCombo( const ecs_package_ptr& Package,
                                       u32                    Kind,
                                       u64                    Key,
                                       u32                    Stage,
                                       u32                    Format,
                                       ecs_program_view&      View,
                                       xstring&               Error )
{
    View.Reset();
    Error.Clear();

    if( !Package || Stage > 1 )
    {
        Error = "invalid combo request";
        return FALSE;
    }

    const auto* pCombo = ecs_FindPackageCombo( Package->Index, Kind, Key );

    if( !pCombo )
    {
        Error = "combo is missing from package";
        return FALSE;
    }

    return AcquireProgram( Package, Stage ? pCombo->Pixel : pCombo->Vertex, Format, View, Error );
}

//==============================================================================
//  ASYNCHRONOUS REQUESTS
//==============================================================================

void ecs_package_store::PumpCompletions( void )
{
    if( !m_pJobs )
    {
        return;
    }

    for( s32 i = m_pJobs->Requests.GetCount() - 1; i >= 0; --i )
    {
        ecs_block_request& Block = *m_pJobs->Requests[i];
        const s32          State = x_AtomicLoadAcquire( &Block.State );

        if( State == ECS_PROGRAM_PENDING )
        {
            continue;
        }

        if( !Block.Installed )
        {
            if( State == ECS_PROGRAM_READY && !x_AtomicLoadAcquire( &Block.Stale ) &&
                x_AtomicLoadAcquire( &Block.Subscribers ) > 0 )
            {
                m_Blocks.Insert( Block.Package->Index, Block.Block, Block.Data, Block.View, Block.ReadMs,
                                 Block.DecodeMs );
            }

            Block.Data.Clear();
            Block.Installed = TRUE;
            x_AtomicStoreRelease( &m_pJobs->InFlightBytes, 0 );
            m_pJobs->Continue.Release( X_TH_NOBLOCK );
        }

        if( x_AtomicLoadAcquire( &Block.Subscribers ) == 0 )
        {
            const s32 Last = m_pJobs->Requests.GetCount() - 1;

            if( i != Last )
            {
                m_pJobs->Requests[i] = m_pJobs->Requests[Last];
            }

            m_pJobs->Requests[Last].Clear();
            m_pJobs->Requests.SetCount( Last );
        }
    }
}

//==============================================================================

xbool ecs_package_store::RequestProgram( const ecs_package_ptr& Package,
                                         u64                    ProgramId,
                                         u32                    Format,
                                         ecs_request_ptr&       Request,
                                         xstring&               Error,
                                         u32                    Capabilities )
{
    ReleaseRequest( Request );
    Error.Clear();
    PumpCompletions();

    if( !Package || !Package->Source )
    {
        Error = "package is not open";
        return FALSE;
    }

    const ecs_package_program* pProgram = ecs_FindPackageProgram( Package->Index, ProgramId, Format, Capabilities );

    if( !pProgram )
    {
        Error = "program or target is missing from package";
        return FALSE;
    }

    Request             = new ecs_program_request;
    Request->m_Package  = Package;
    Request->m_pProgram = pProgram;

    const u32 Block = Package->Index.Binaries[pProgram->Binary].Block;

    if( m_Blocks.Find( Package->Index, Block, Request->m_View ) )
    {
        return TRUE;
    }

    if( !m_pJobs )
    {
        m_pJobs               = new ecs_package_jobs;
        m_pJobs->Arguments[0] = reinterpret_cast<char*>( m_pJobs );
        m_pJobs->pWorker      = new xthread( PackageReadWorker, "ECS decode", 128 * 1024, 0, 1, m_pJobs->Arguments );
    }

    // Reuse a pending decode for the same block.
    for( s32 i = 0; i < m_pJobs->Requests.GetCount(); ++i )
    {
        const xref<ecs_block_request>& Existing = m_pJobs->Requests[i];

        if( Existing->Package == Package && Existing->Block == Block && !x_AtomicLoadAcquire( &Existing->Stale ) &&
            x_AtomicLoadAcquire( &Existing->Subscribers ) > 0 &&
            x_AtomicLoadAcquire( &Existing->State ) != ECS_PROGRAM_CANCELLED )
        {
            Request->m_Block = Existing;
            AddSubscriber( *Existing, 1 );
            Request->m_isAttached = TRUE;

            return TRUE;
        }
    }

    Request->m_Block          = new ecs_block_request;
    Request->m_Block->Package = Package;
    Request->m_Block->Block   = Block;
    AddSubscriber( *Request->m_Block, 1 );
    Request->m_isAttached = TRUE;
    m_pJobs->Requests.Append( Request->m_Block );
    m_pJobs->Lock.Enter();
    m_pJobs->Queue.Append( Request->m_Block );
    m_pJobs->Lock.Exit();
    m_pJobs->Dispatch.Release( X_TH_NOBLOCK );

    return TRUE;
}

//==============================================================================

ecs_program_request_status ecs_package_store::PollProgram( const ecs_request_ptr& Request,
                                                           ecs_program_view&      View,
                                                           xstring&               Error )
{
    View.Reset();
    Error.Clear();
    PumpCompletions();

    if( !Request || Request->m_isCancelled )
    {
        return ECS_PROGRAM_CANCELLED;
    }

    if( Request->m_Block )
    {
        if( x_AtomicLoadAcquire( &Request->m_Block->Stale ) )
        {
            return ECS_PROGRAM_CANCELLED;
        }

        const s32 State = x_AtomicLoadAcquire( &Request->m_Block->State );

        if( State != ECS_PROGRAM_READY )
        {
            if( State == ECS_PROGRAM_FAILED )
            {
                Error = Request->m_Block->Error;
            }

            return static_cast<ecs_program_request_status>( State );
        }

        if( !Request->m_Block->Installed )
        {
            return ECS_PROGRAM_PENDING;
        }

        View.Block = Request->m_Block->View;
    }
    else
    {
        View.Block = Request->m_View;
    }

    View.Package = Request->m_Package;
    View.Program = Request->m_pProgram;

    return ECS_PROGRAM_READY;
}

//==============================================================================

void ecs_package_store::CancelRequest( const ecs_request_ptr& Request )
{
    if( !Request )
    {
        return;
    }

    Request->m_isCancelled = TRUE;
    Request->Detach();
    Request->m_View.Reset();
}

//==============================================================================

void ecs_package_store::ReleaseRequest( ecs_request_ptr& Request )
{
    if( Request )
    {
        Request->Detach();
    }

    Request.Clear();
    PumpCompletions();
}

//==============================================================================
//  PACKAGE LIFETIME
//==============================================================================

void ecs_package_store::Close( const xstring& File, u64 CatalogId )
{
    for( s32 i = 0; i < m_Packages.GetCount(); ++i )
    {
        if( m_Packages[i].CatalogId != CatalogId || m_Packages[i].File != File )
        {
            continue;
        }

        if( m_pJobs )
        {
            for( s32 j = 0; j < m_pJobs->Requests.GetCount(); ++j )
            {
                if( m_pJobs->Requests[j]->Package == m_Packages[i].Package )
                {
                    x_AtomicStoreRelease( &m_pJobs->Requests[j]->Stale, 1 );
                }
            }
        }

        m_Blocks.RemovePackage( CatalogId, m_Packages[i].Package->Index.PackageId );

        const s32 Last = m_Packages.GetCount() - 1;

        if( i != Last )
        {
            m_Packages[i] = m_Packages[Last];
        }

        m_Packages[Last].Package.Clear();
        m_Packages[Last].File.Clear();
        m_Packages.SetCount( Last );

        return;
    }
}

//==============================================================================

void ecs_package_store::Clear( void )
{
    if( m_pJobs )
    {
        for( s32 i = 0; i < m_pJobs->Requests.GetCount(); ++i )
        {
            x_AtomicStoreRelease( &m_pJobs->Requests[i]->Stale, 1 );
        }

        x_AtomicStoreRelease( &m_pJobs->Stop, 1 );
        m_pJobs->Dispatch.Release( X_TH_NOBLOCK );
        m_pJobs->Continue.Release( X_TH_NOBLOCK );

        while( m_pJobs->pWorker->IsActive() )
        {
            x_DelayThread( 1 );
        }

        delete m_pJobs->pWorker;
        delete m_pJobs;
        m_pJobs = NULL;
    }

    m_Packages.Clear();
    m_Blocks.Clear();
}

//==============================================================================
//  STATISTICS
//==============================================================================

ecs_block_cache_stats ecs_package_store::GetStats( void ) const
{
    ecs_block_cache_stats Stats = m_Blocks.GetStats();

    if( m_pJobs )
    {
        Stats.InFlightBytes     = x_AtomicLoadAcquire( &m_pJobs->InFlightBytes );
        Stats.PeakInFlightBytes = x_AtomicLoadAcquire( &m_pJobs->PeakInFlightBytes );
    }

    return Stats;
}
