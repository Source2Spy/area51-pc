//==============================================================================
//
//  ecs_package_source.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_ECS.hpp"
#include "x_stdio.hpp"
#include "x_threads.hpp"
#include "IOManager/io_filesystem.hpp"
#include "IOManager/io_request.hpp"

//==============================================================================
//  TYPES
//==============================================================================

namespace
{
class FileSource final : public ecs_package_source
{
public:

    //==============================================================================

    FileSource( X_FILE* pFile, u32 Size )
        : m_pFile( pFile ),
          m_nBytes( Size )
    {
    }

    //==============================================================================

    ~FileSource( void ) override
    {
        x_fclose( m_pFile );
    }

    //==============================================================================

    u32 Size( void ) const override
    {
        return m_nBytes;
    }

    //==============================================================================

    xbool Read( u32 Offset, byte* pData, u32 Size ) override
    {
        if( !pData || Offset > m_nBytes || Size > m_nBytes - Offset )
        {
            return FALSE;
        }

        if( g_IOFSMgr.OwnsFile( m_pFile ) )
        {
            io_request Request;

            if( !g_IOFSMgr.ReadAsync( reinterpret_cast<io_open_file*>( m_pFile ), Request, pData, Offset, Size ) )
            {
                return FALSE;
            }

            Request.AcquireSemaphore();

            return Request.GetStatus() == io_request::COMPLETED;
        }

        m_Lock.Enter();

        const xbool Read = x_fseek( m_pFile, static_cast<s32>( Offset ), X_SEEK_SET ) == 0 &&
                           x_fread( pData, 1, static_cast<s32>( Size ), m_pFile ) == static_cast<s32>( Size );

        m_Lock.Exit();

        return Read;
    }

private:

    xmutex  m_Lock;
    X_FILE* m_pFile;
    u32     m_nBytes;
};
} // namespace

//==============================================================================
//  FUNCTIONS
//==============================================================================

ecs_source_ptr ecs_OpenPackageSource( const char* pFileName )
{
    if( !pFileName || !pFileName[0] || pFileName[0] == '/' || pFileName[0] == '\\' || pFileName[1] == ':' )
    {
        x_DebugMsg( "ECS: package path must be relative to the mounted SHADERS filesystem\n" );
        return {};
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );

    if( !pFile )
    {
        return {};
    }

    // ECS checks arbitrarily aligned blocks independently of DFS sector checksums.
    if( g_IOFSMgr.OwnsFile( pFile ) )
    {
        g_IOFSMgr.EnableChecksum( pFile, FALSE );
    }

    const s32 Size = x_flength( pFile );

    if( Size <= 0 )
    {
        x_fclose( pFile );
        return {};
    }

    return ecs_source_ptr( new FileSource( pFile, static_cast<u32>( Size ) ) );
}
