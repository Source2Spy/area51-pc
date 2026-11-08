//==============================================================================
//
//  ecs_package_store.hpp
//
//==============================================================================

#ifndef ECS_PACKAGE_STORE_HPP
#define ECS_PACKAGE_STORE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ecs_block_cache.hpp"

//==============================================================================
//  CONSTS
//==============================================================================

#if defined( TARGET_ANDROID )
inline constexpr u64 ECS_DECODED_CACHE_BUDGET = 32ull * 1024 * 1024;
#else
inline constexpr u64 ECS_DECODED_CACHE_BUDGET = 128ull * 1024 * 1024;
#endif

//==============================================================================
//  TYPES
//==============================================================================

class ecs_package_source : public xref_counted
{
public:

    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    virtual ~ecs_package_source( void )                     = default;
    virtual u32   Size( void ) const                        = 0;
    virtual xbool Read( u32 Offset, byte* pData, u32 Size ) = 0;
};

//------------------------------------------------------------------------------

struct ecs_loaded_package;

// Views must retain both the lazy file source and its validated metadata.
typedef xref<ecs_package_source>       ecs_source_ptr;
typedef xref<const ecs_loaded_package> ecs_package_ptr;

//------------------------------------------------------------------------------

struct ecs_loaded_package : public xref_counted
{
    ecs_package_index Index;
    ecs_source_ptr    Source;
};

//------------------------------------------------------------------------------

struct ecs_program_view
{
    //------------------------------------------------------------------------------
    //  FUNCTIONS
    //------------------------------------------------------------------------------

    const byte*               Code( void ) const;
    const ecs_package_layout* Layout( void ) const;
    void                      Reset( void );

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    ecs_package_ptr            Package;
    const ecs_package_program* Program = NULL;
    ecs_block_view             Block;
};

//------------------------------------------------------------------------------

enum ecs_program_request_status
{
    ECS_PROGRAM_PENDING,
    ECS_PROGRAM_READY,
    ECS_PROGRAM_FAILED,
    ECS_PROGRAM_CANCELLED
};

//==============================================================================
//  FORWARD DECLARATIONS
//==============================================================================

struct ecs_block_request;
struct ecs_package_jobs;

//==============================================================================
//  CLASSES
//==============================================================================

class ecs_program_request : public xref_counted
{
public:

    //------------------------------------------------------------------------------
    //  LIFETIME
    //------------------------------------------------------------------------------

    ecs_program_request( void );
    ~ecs_program_request( void );

private:

    friend class ecs_package_store;

    //------------------------------------------------------------------------------
    //  HELPER FUNCTIONS
    //------------------------------------------------------------------------------

    void Detach( void );

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    ecs_package_ptr            m_Package;
    const ecs_package_program* m_pProgram;
    xref<ecs_block_request>    m_Block;
    ecs_block_view             m_View;
    xbool                      m_isCancelled;
    xbool                      m_isAttached;
};

typedef xref<ecs_program_request> ecs_request_ptr;

//------------------------------------------------------------------------------

class ecs_package_store
{
public:

    //------------------------------------------------------------------------------
    //  LIFETIME
    //------------------------------------------------------------------------------

    explicit ecs_package_store( u64 DecodedBudget = ECS_DECODED_CACHE_BUDGET );
    ~ecs_package_store( void );

    //------------------------------------------------------------------------------
    //  PACKAGES AND PROGRAMS
    //------------------------------------------------------------------------------

    xbool Open( const xstring& File, ecs_source_ptr Source, u64 CatalogId, ecs_package_ptr& Package, xstring& Error );
    ecs_package_ptr Find( const xstring& File, u64 CatalogId ) const;

    xbool AcquireProgram( const ecs_package_ptr& Package,
                          u64                    ProgramId,
                          u32                    Format,
                          ecs_program_view&      View,
                          xstring&               Error );
    xbool AcquireCombo( const ecs_package_ptr& Package,
                        u32                    Kind,
                        u64                    Key,
                        u32                    Stage,
                        u32                    Format,
                        ecs_program_view&      View,
                        xstring&               Error );

    xbool RequestProgram( const ecs_package_ptr& Package,
                          u64                    ProgramId,
                          u32                    Format,
                          ecs_request_ptr&       Request,
                          xstring&               Error,
                          u32                    Capabilities = 0 );
    ecs_program_request_status PollProgram( const ecs_request_ptr& Request, ecs_program_view& View, xstring& Error );
    void                       CancelRequest( const ecs_request_ptr& Request );
    void                       ReleaseRequest( ecs_request_ptr& Request );

    void Clear( void );
    void Close( const xstring& File, u64 CatalogId );

    ecs_block_cache_stats GetStats( void ) const;

private:

    //------------------------------------------------------------------------------
    //  TYPES
    //------------------------------------------------------------------------------

    struct PackageEntry
    {
        u64             CatalogId = 0;
        xstring         File;
        ecs_package_ptr Package;
    };

    //------------------------------------------------------------------------------
    //  HELPER FUNCTIONS
    //------------------------------------------------------------------------------

    ecs_package_store( const ecs_package_store& );
    ecs_package_store& operator=( const ecs_package_store& );
    void               PumpCompletions( void );

    //------------------------------------------------------------------------------
    //  DATA
    //------------------------------------------------------------------------------

    xarray<PackageEntry> m_Packages;
    ecs_block_cache      m_Blocks;
    ecs_package_jobs*    m_pJobs = NULL;
};

//==============================================================================
#endif // ECS_PACKAGE_STORE_HPP
//==============================================================================
