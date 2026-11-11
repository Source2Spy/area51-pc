//=========================================================================
//
//  FrameGraph_Resources.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "FrameGraph_Private.hpp"
#include "e_Engine.hpp"
#include "x_debug.hpp"
#include "x_plus.hpp"

namespace render
{

//=========================================================================

static
xbool ResourceDiagnosticsEnabled( void )
{
    static const xbool Enabled = []()
    {
        const char* pValue = x_getenv( "A51_RENDER_RESOURCE_DIAGNOSTICS" );
        return pValue && ( pValue[0] == '1' ) && ( pValue[1] == 0 );
    }();

    return Enabled;
}

//=========================================================================

static
void LogResourceEvent( const char* pEvent, const RenderResourceDesc& Desc,
                              u64 FrameSerial, u32 FrameSlot, u64 IdleAge, f32 TimeMs )
{
    if( !ResourceDiagnosticsEnabled() )
        return;

    x_DebugMsg( "A51 resource: event=%s frame=%llu slot=%u name='%s' type=%d size=%ux%u "
                          "format=%d mips=%u samples=%u usage=0x%x lifetime=%d key=0x%llx buffer-bytes=%llu age=%llu ms=%.3f\n",
                  pEvent, static_cast<unsigned long long>( FrameSerial ), FrameSlot,
                  Desc.pDebugName ? Desc.pDebugName : "unnamed", static_cast<s32>( Desc.Type ),
                  Desc.Extent.Width, Desc.Extent.Height, static_cast<s32>( Desc.Format ), Desc.MipCount, Desc.SampleCount,
                  Desc.Usage, static_cast<s32>( Desc.Lifetime ),
                  static_cast<unsigned long long>( Desc.PersistentKey ),
                  static_cast<unsigned long long>( Desc.Buffer.Size ),
                  static_cast<unsigned long long>( IdleAge ), TimeMs );
}

//=========================================================================
//  RESOURCE POOL
//=========================================================================

struct FrameResourcePool::Resource
{
    Resource( void )
        : Target(),
          Desc(),
          Generation( 0 ),
          LastUsedFrame( 0 ),
          IsImported( FALSE ),
          pExternal( NULL ),
          pExternalMutable( NULL ),
          pExternalBackend( NULL ),
          LastUsedFrameSerial( 0 ),
          LastUsedTime( 0 ),
          FrameSlot( 0 ),
          IsRetired( FALSE ),
          RetireFrameSerial( 0 ),
          IsCacheEvictable( FALSE )
    {
    }

    rtarget            Target;
    rbuffer            Buffer;
    RenderResourceDesc Desc;
    u32                Generation;
    u32                LastUsedFrame;
    xbool              IsImported;
    const rtarget*     pExternal;
    rtarget*           pExternalMutable;
    rtarget_backend*   pExternalBackend;
    u64                LastUsedFrameSerial;
    xtick              LastUsedTime;
    u32                FrameSlot;
    xbool              IsRetired;
    u64                RetireFrameSerial;
    xbool              IsCacheEvictable;

    //-------------------------------------------------------------------------

    void Reset( void )
    {
        Target              = rtarget();
        Buffer              = rbuffer();
        Desc                = RenderResourceDesc();
        Generation          = 0;
        LastUsedFrame       = 0;
        IsImported          = FALSE;
        pExternal           = NULL;
        pExternalMutable    = NULL;
        pExternalBackend    = NULL;
        LastUsedFrameSerial = 0;
        LastUsedTime        = 0;
        FrameSlot           = 0;
        IsRetired           = FALSE;
        RetireFrameSerial   = 0;
        IsCacheEvictable    = FALSE;
    }
};

//=========================================================================

static
void HashResourceDescBytes( u32& Hash, const void* pData, s32 ByteCount )
{
    const u8* pBytes = static_cast<const u8*>( pData );

    for( s32 i = 0; i < ByteCount; ++i )
    {
        Hash ^= pBytes[i];
        Hash *= 16777619u;
    }
}

//=========================================================================

static
u32 HashResourceDesc( const RenderResourceDesc& Desc )
{
    u32 Hash = 2166136261u;
    HashResourceDescBytes( Hash, &Desc.Type, sizeof( Desc.Type ) );
    HashResourceDescBytes( Hash, &Desc.Buffer.Size, sizeof( Desc.Buffer.Size ) );
    HashResourceDescBytes( Hash, &Desc.Buffer.Stride, sizeof( Desc.Buffer.Stride ) );
    HashResourceDescBytes( Hash, &Desc.Buffer.UsageFlags, sizeof( Desc.Buffer.UsageFlags ) );
    HashResourceDescBytes( Hash, &Desc.Extent, sizeof( Desc.Extent ) );
    HashResourceDescBytes( Hash, &Desc.Format, sizeof( Desc.Format ) );
    HashResourceDescBytes( Hash, &Desc.MipCount, sizeof( Desc.MipCount ) );
    HashResourceDescBytes( Hash, &Desc.SampleCount, sizeof( Desc.SampleCount ) );
    HashResourceDescBytes( Hash, &Desc.ArrayLayers, sizeof( Desc.ArrayLayers ) );
    HashResourceDescBytes( Hash, &Desc.ViewMask, sizeof( Desc.ViewMask ) );
    HashResourceDescBytes( Hash, &Desc.Usage, sizeof( Desc.Usage ) );
    HashResourceDescBytes( Hash, &Desc.Lifetime, sizeof( Desc.Lifetime ) );
    HashResourceDescBytes( Hash, &Desc.PersistentKey, sizeof( Desc.PersistentKey ) );
    HashResourceDescBytes( Hash, &Desc.PreserveContents, sizeof( Desc.PreserveContents ) );

    return Hash;
}

//=========================================================================

static
u64 EstimateResourceBytes( const RenderResourceDesc& Desc )
{
    if( Desc.IsBuffer() )
    {
        return Desc.Buffer.Size;
    }

    u64 BytesPerSample = 0;

    switch( Desc.Format )
    {
        case RTARGET_FORMAT_RGBA8:
        case RTARGET_FORMAT_BGRA8:
        case RTARGET_FORMAT_RGB10A2:
        case RTARGET_FORMAT_R32_UINT:
        case RTARGET_FORMAT_R32F:
        case RTARGET_FORMAT_DEPTH32F:
        {
            BytesPerSample = 4;
            break;
        }
        case RTARGET_FORMAT_RGBA16F:
        {
            BytesPerSample = 8;
            break;
        }
        case RTARGET_FORMAT_RGBA32F:
        {
            BytesPerSample = 16;
            break;
        }
        case RTARGET_FORMAT_R8_UINT:
        case RTARGET_FORMAT_R8:
        {
            BytesPerSample = 1;
            break;
        }
        case RTARGET_FORMAT_RG16F:
        {
            BytesPerSample = 4;
            break;
        }
        case RTARGET_FORMAT_DEPTH_STENCIL:
        {
            // D24S8 is normally 4 bytes; use the largest backend choice
            // (D32FS8) so the cache estimate remains conservative.
            BytesPerSample = 8;
            break;
        }
        default:
        {
            return ~static_cast<u64>( 0 );
        }
    }

    const u64 MaxValue      = ~static_cast<u64>( 0 );
    const u64 BytesPerPixel = BytesPerSample * MAX( Desc.SampleCount, 1u );
    const u64 LayerCount    = MAX( Desc.ArrayLayers, 1u );

    if( ( BytesPerPixel == 0 ) || ( BytesPerPixel > ( MaxValue / LayerCount ) ) )
    {
        return MaxValue;
    }

    const u64 BytesPerLayer = BytesPerPixel * LayerCount;

    u64 ByteCount = 0;
    u32 Width     = Desc.Extent.Width;
    u32 Height    = Desc.Extent.Height;

    for( u32 Mip = 0; Mip < Desc.MipCount; ++Mip )
    {
        const u64 PixelCount = static_cast<u64>( Width ) * Height;
        if( PixelCount > ( ( MaxValue - ByteCount ) / BytesPerLayer ) )
        {
            return MaxValue;
        }

        ByteCount += PixelCount * BytesPerLayer;
        Width      = MAX( Width >> 1, 1u );
        Height     = MAX( Height >> 1, 1u );
    }

    return ByteCount;
}

//=========================================================================

u32 FrameResourcePool::AddResource( Resource* pResource )
{
    if( m_FreeResourceSlots.GetCount() > 0 )
    {
        const s32 Last  = m_FreeResourceSlots.GetCount() - 1;
        const s32 Index = m_FreeResourceSlots[Last];
        m_FreeResourceSlots.SetCount( Last );
        ASSERT( !m_Resources[Index] );
        m_Resources[Index] = pResource;

        return static_cast<u32>( Index );
    }

    m_Resources.Append() = pResource;
    return static_cast<u32>( m_Resources.GetCount() - 1 );
}

//=========================================================================

void FrameResourcePool::RebuildDescriptorLookup( void )
{
    const u32 ResourceCount      = static_cast<u32>( m_Resources.GetCount() );
    const u64 RequestedTableSize = MAX( u64( 8 ), static_cast<u64>( ResourceCount ) * 2 );

    if( RequestedTableSize > 0x7fffffffu )
    {
        m_DescriptorLookup.Clear();
        m_PersistentLookup.Clear();
        m_DescriptorLookupMask = 0;
        m_nDescriptorLookups   = 0;

        return;
    }

    u32 TableSize = 8;

    while( static_cast<u64>( TableSize ) < RequestedTableSize )
    {
        if( TableSize > ( 0x7fffffffu / 2u ) )
        {
            m_DescriptorLookup.Clear();
            m_PersistentLookup.Clear();
            m_DescriptorLookupMask = 0;
            m_nDescriptorLookups   = 0;

            return;
        }

        TableSize <<= 1;
    }

    m_DescriptorLookup.SetCount( ResourceCount ? TableSize : 0 );
    m_PersistentLookup.SetCount( ResourceCount ? TableSize : 0 );
    m_DescriptorLookupMask = ResourceCount ? ( TableSize - 1u ) : 0;
    m_nDescriptorLookups   = 0;

    for( s32 i = 0; i < m_DescriptorLookup.GetCount(); ++i )
    {
        m_DescriptorLookup[i] = -1;
        m_PersistentLookup[i] = -1;
    }

    if( !ResourceCount )
    {
        return;
    }

    for( u32 ResourceIndex = 0; ResourceIndex < ResourceCount; ++ResourceIndex )
    {
        Resource* pResource = m_Resources[static_cast<s32>( ResourceIndex )];

        if( !pResource || pResource->IsImported )
        {
            continue;
        }

        if( pResource->Desc.Lifetime == RENDER_RESOURCE_PERSISTENT && !pResource->IsRetired )
        {
            const u64 Key  = pResource->Desc.PersistentKey;
            u32       Slot = static_cast<u32>( Key ^ ( Key >> 32 ) ) & m_DescriptorLookupMask;

            while( m_PersistentLookup[Slot] >= 0 )
            {
                Slot = ( Slot + 1u ) & m_DescriptorLookupMask;
            }

            m_PersistentLookup[Slot] = static_cast<s32>( ResourceIndex );
        }

        u32 SlotIndex = HashResourceDesc( pResource->Desc ) & m_DescriptorLookupMask;

        while( m_DescriptorLookup[static_cast<s32>( SlotIndex )] >= 0 )
        {
            SlotIndex = ( SlotIndex + 1u ) & m_DescriptorLookupMask;
        }

        m_DescriptorLookup[static_cast<s32>( SlotIndex )] = static_cast<s32>( ResourceIndex );
        ++m_nDescriptorLookups;
    }
}

//=========================================================================

s32 FrameResourcePool::FindPersistentResource( u64 Key ) const
{
    if( !Key || !m_PersistentLookup.GetCount() )
    {
        return -1;
    }

    u32 Slot = static_cast<u32>( Key ^ ( Key >> 32 ) ) & m_DescriptorLookupMask;

    for( s32 i = 0; i < m_PersistentLookup.GetCount(); ++i )
    {
        const s32 Index = m_PersistentLookup[Slot];

        if( Index < 0 )
        {
            break;
        }

        const Resource* pResource = m_Resources[Index];

        if( pResource && !pResource->IsRetired && pResource->Desc.PersistentKey == Key )
        {
            return Index;
        }

        Slot = ( Slot + 1u ) & m_DescriptorLookupMask;
    }

    return -1;
}

//=========================================================================

xbool FrameResourcePool::HasPersistentResource( u64 Key ) const
{
    return FindPersistentResource( Key ) >= 0;
}

//=========================================================================

s32 FrameResourcePool::FindReusableDescriptor( const RenderResourceDesc& Desc ) const
{
    if( !m_nDescriptorLookups || !m_DescriptorLookup.GetCount() )
    {
        return -1;
    }

    u32 SlotIndex = HashResourceDesc( Desc ) & m_DescriptorLookupMask;

    for( u32 Probe = 0; Probe < static_cast<u32>( m_DescriptorLookup.GetCount() ); ++Probe )
    {
        const s32 ResourceIndex = m_DescriptorLookup[static_cast<s32>( SlotIndex )];

        if( ResourceIndex < 0 )
        {
            return -1;
        }

        Resource* pResource = m_Resources[ResourceIndex];

        if( pResource && !pResource->IsImported && !pResource->IsRetired &&
            ( pResource->LastUsedFrame != m_Generation ) && ( pResource->FrameSlot == m_ActiveFrameSlot ) &&
            ( pResource->LastUsedFrameSerial == 0 || eng_IsFrameComplete( pResource->LastUsedFrameSerial ) ) &&
            ResourceDescsMatch( pResource->Desc, Desc ) )
        {
            return ResourceIndex;
        }

        SlotIndex = ( SlotIndex + 1u ) & m_DescriptorLookupMask;
    }

    return -1;
}

//=========================================================================

FrameResourcePool::FrameResourcePool( void )
    : m_Resources(),
      m_FreeResources(),
      m_DescriptorLookup(),
      m_DescriptorLookupMask( 0 ),
      m_nDescriptorLookups( 0 ),
      m_Generation( 0 ),
      m_ResetEpoch( 1 ),
      m_ActiveFrameSlot( 0 ),
      m_nFrameSlots( 1 ),
      m_ActiveFrameSerial( 0 ),
      m_ActiveFrameTime( 0 ),
      m_isInitialized( FALSE )
{
    for( s32 i = 0; i < 3; ++i )
    {
        m_LastFrameSerialBySlot[i] = 0;
    }
}

//=========================================================================

FrameResourcePool::~FrameResourcePool( void )
{
    Kill();
}

//=========================================================================

void FrameResourcePool::Init( void )
{
    m_isInitialized = TRUE;
}

//=========================================================================

void FrameResourcePool::Kill( void )
{
    Reset();
    m_isInitialized = FALSE;
}

//=========================================================================

void FrameResourcePool::BeginEngineFrame( u32 FrameSlot, u64 FrameSerial, u32 FrameSlotCount )
{
    ASSERT( m_isInitialized );
    if( !m_isInitialized || ( FrameSlotCount < 1 ) || ( FrameSlotCount > 3 ) || ( FrameSlot >= FrameSlotCount ) ||
        ( FrameSerial == 0 ) )
    {
        RENDER_THROW( "FrameResourcePool", "invalid engine frame token slot=%u serial=%llu count=%u", FrameSlot,
                      static_cast<unsigned long long>( FrameSerial ), FrameSlotCount );
        return;
    }

    // Retain recent targets independently of frame rate; reclaim only after their fences complete.
    static const u64 kTransientCacheMaxAgeFrames    = 180;
    static const f32 kResourceCacheMaxIdleSeconds   = 10.0f;
    static const u64 kTransientCacheIdleBudgetBytes = u64{ 256 } * 1024 * 1024;
    const u64        ProtectedFrameAge              = static_cast<u64>( FrameSlotCount ) * 2;

    xbool TransientSlotComplete[3] = { TRUE, TRUE, TRUE };

    for( u32 SlotIndex = 0; SlotIndex < 3; ++SlotIndex )
    {
        const u64 LatestSerial = m_LastFrameSerialBySlot[SlotIndex];

        if( ( SlotIndex == FrameSlot ) && ( LatestSerial == FrameSerial ) )
        {
            RENDER_THROW( "FrameResourcePool", "frame serial %llu was reused for slot %u",
                          static_cast<unsigned long long>( FrameSerial ), SlotIndex );
            return;
        }

        TransientSlotComplete[SlotIndex] = ( LatestSerial == 0 ) || eng_IsFrameComplete( LatestSerial );

        if( ( SlotIndex == FrameSlot ) && !TransientSlotComplete[SlotIndex] )
        {
            RENDER_THROW( "FrameResourcePool", "frame slot %u is still in use by serial %llu", SlotIndex,
                          static_cast<unsigned long long>( LatestSerial ) );
            return;
        }
    }

    m_ActiveFrameSlot                  = FrameSlot;
    m_nFrameSlots                      = FrameSlotCount;
    m_ActiveFrameSerial                = FrameSerial;
    m_ActiveFrameTime                  = x_GetTime();
    m_LastFrameSerialBySlot[FrameSlot] = FrameSerial;

    xbool ResourcesChanged   = FALSE;
    u64   IdleTransientBytes = 0;

    for( s32 i = m_Resources.GetCount() - 1; i >= 0; --i )
    {
        Resource* pResource = m_Resources[i];

        if( !pResource )
        {
            continue;
        }

        pResource->IsCacheEvictable = FALSE;

        const xbool RetiredComplete = pResource->IsRetired && eng_IsFrameComplete( pResource->RetireFrameSerial );
        const u64   LastUsedSerial  = pResource->LastUsedFrameSerial;
        const u64   IdleAge         = ( FrameSerial > LastUsedSerial ) ? ( FrameSerial - LastUsedSerial ) : 0;

        xbool SafeTransient = FALSE;

        if( !pResource->IsImported && !pResource->IsRetired &&
            ( pResource->Desc.Lifetime == RENDER_RESOURCE_TRANSIENT ) && ( LastUsedSerial != 0 ) &&
            ( LastUsedSerial != FrameSerial ) )
        {
            SafeTransient = ( pResource->FrameSlot < 3 ) && TransientSlotComplete[pResource->FrameSlot];
        }

        const f32 IdleSeconds = m_ActiveFrameTime > pResource->LastUsedTime
                                    ? x_TicksToSec( m_ActiveFrameTime - pResource->LastUsedTime ) : 0.0f;
        const xbool Expired = ( IdleAge >= kTransientCacheMaxAgeFrames ) &&
                              ( IdleSeconds >= kResourceCacheMaxIdleSeconds );
        const xbool ExpiredTransient  = SafeTransient && Expired;
        const xbool ExpiredPersistent = !pResource->IsImported && !pResource->IsRetired &&
                                        pResource->Desc.Lifetime == RENDER_RESOURCE_PERSISTENT &&
                                        Expired && eng_IsFrameComplete( LastUsedSerial );

        if( RetiredComplete || ExpiredTransient || ExpiredPersistent )
        {
            LogResourceEvent( RetiredComplete ? "retired" : "expired", pResource->Desc,
                              FrameSerial, pResource->FrameSlot, IdleAge, 0.0f );
            rtarget_Destroy( pResource->Target );
            rbuffer_Destroy( pResource->Buffer );
            delete pResource;
            m_Resources[i]               = NULL;
            m_FreeResourceSlots.Append() = i;
            ResourcesChanged             = TRUE;
            continue;
        }

        if( SafeTransient && ( IdleAge > ProtectedFrameAge ) )
        {
            pResource->IsCacheEvictable = TRUE;

            const u64 ResourceBytes = EstimateResourceBytes( pResource->Desc );
            const u64 MaxValue      = ~static_cast<u64>( 0 );
            IdleTransientBytes =
                ResourceBytes > ( MaxValue - IdleTransientBytes ) ? MaxValue : IdleTransientBytes + ResourceBytes;
        }
    }

    if( IdleTransientBytes > kTransientCacheIdleBudgetBytes )
    {
        // Reclaim eligible targets until the idle cache fits its budget.
        for( s32 i = 0; i < m_Resources.GetCount() && IdleTransientBytes > kTransientCacheIdleBudgetBytes; )
        {
            Resource* pResource = m_Resources[i];

            if( pResource && pResource->IsCacheEvictable )
            {
                const u64 ResourceBytes = EstimateResourceBytes( pResource->Desc );
                LogResourceEvent( "budget", pResource->Desc, FrameSerial, pResource->FrameSlot,
                                  FrameSerial - pResource->LastUsedFrameSerial, 0.0f );
                rtarget_Destroy( pResource->Target );
                rbuffer_Destroy( pResource->Buffer );
                delete pResource;
                m_Resources[i]               = NULL;
                m_FreeResourceSlots.Append() = i;
                IdleTransientBytes = ( ResourceBytes >= IdleTransientBytes ) ? 0 : IdleTransientBytes - ResourceBytes;
                ResourcesChanged   = TRUE;
                continue;
            }

            ++i;
        }
    }

    if( ResourcesChanged )
    {
        RebuildDescriptorLookup();
    }
}

//=========================================================================

void FrameResourcePool::BeginScope( void )
{
    ASSERT( m_isInitialized );
    if( !m_isInitialized || ( m_ActiveFrameSerial == 0 ) )
    {
        RENDER_THROW( "FrameResourcePool", "scope began without an active engine frame" );
        return;
    }

    ++m_Generation;

    if( m_Generation == 0 )
    {
        ++m_Generation;
    }

    for( s32 i = m_Resources.GetCount() - 1; i >= 0; --i )
    {
        Resource* pResource = m_Resources[i];

        if( pResource && pResource->IsImported )
        {
            m_Resources[i]               = NULL;
            m_FreeResourceSlots.Append() = i;
            pResource->Reset();
            m_FreeResources.Append() = pResource;
        }
    }
}

//=========================================================================

void FrameResourcePool::Reset( void )
{
    ++m_ResetEpoch;

    if( ( m_Resources.GetCount() > 0 ) || ( m_FreeResources.GetCount() > 0 ) )
    {
        eng_Sync();
    }

    for( s32 i = 0; i < m_Resources.GetCount(); ++i )
    {
        Resource* pResource = m_Resources[i];

        if( pResource )
        {
            if( !pResource->IsImported )
            {
                rtarget_Destroy( pResource->Target );
                rbuffer_Destroy( pResource->Buffer );
            }

            delete pResource;
        }
    }

    m_Resources.Clear();
    m_FreeResourceSlots.Clear();
    m_DescriptorLookup.Clear();
    m_PersistentLookup.Clear();
    m_DescriptorLookupMask = 0;
    m_nDescriptorLookups   = 0;

    for( s32 i = 0; i < 3; ++i )
    {
        m_LastFrameSerialBySlot[i] = 0;
    }

    for( s32 i = 0; i < m_FreeResources.GetCount(); ++i )
    {
        delete m_FreeResources[i];
    }

    m_FreeResources.Clear();
}

//=========================================================================

RenderResourceHandle FrameResourcePool::Acquire( const RenderResourceDesc& Desc )
{
    if( !m_isInitialized || !m_ActiveFrameSerial || !IsResourceDescValid( Desc ) )
    {
        RENDER_THROW( "FrameResourcePool", "invalid or unavailable resource allocation request name='%s'",
                      Desc.pDebugName ? Desc.pDebugName : "unnamed" );
        return RenderResourceHandle();
    }

    if( Desc.Lifetime == RENDER_RESOURCE_PERSISTENT )
    {
        const s32 i = FindPersistentResource( Desc.PersistentKey );

        if( i >= 0 )
        {
            Resource* pResource = m_Resources[i];

            if( pResource && !pResource->IsImported && ( Desc.Lifetime == RENDER_RESOURCE_PERSISTENT ) &&
                ( pResource->Desc.Lifetime == RENDER_RESOURCE_PERSISTENT ) && !pResource->IsRetired &&
                ( pResource->Desc.PersistentKey == Desc.PersistentKey ) )
            {
                if( pResource->LastUsedFrame == m_Generation )
                {
                    RENDER_THROW( "FrameResourcePool", "persistent key 0x%llx acquired twice in one frame",
                                  static_cast<unsigned long long>( Desc.PersistentKey ) );
                    return RenderResourceHandle();
                }

                if( ResourceDescsMatch( pResource->Desc, Desc ) )
                {
                    pResource->Generation          = m_Generation;
                    pResource->LastUsedFrame       = m_Generation;
                    pResource->LastUsedFrameSerial = m_ActiveFrameSerial;
                    pResource->LastUsedTime        = m_ActiveFrameTime;
                    pResource->FrameSlot           = m_ActiveFrameSlot;

                    return RenderResourceHandle( static_cast<u32>( i ), pResource->Generation );
                }

                // A persistent descriptor change creates a new physical target. The
                // old target remains owned by its submission until its fence retires.
                pResource->IsRetired         = TRUE;
                pResource->RetireFrameSerial = pResource->LastUsedFrameSerial;
            }
        }
    }

    const s32 ReusableIndex = FindReusableDescriptor( Desc );

    if( ReusableIndex >= 0 )
    {
        Resource* pResource            = m_Resources[ReusableIndex];
        pResource->Generation          = m_Generation;
        pResource->LastUsedFrame       = m_Generation;
        pResource->LastUsedFrameSerial = m_ActiveFrameSerial;
        pResource->LastUsedTime        = m_ActiveFrameTime;
        pResource->FrameSlot           = m_ActiveFrameSlot;

        return RenderResourceHandle( static_cast<u32>( ReusableIndex ), pResource->Generation );
    }

    if( ResourceDiagnosticsEnabled() )
    {
        u32 Matches = 0;
        u32 OtherSlot = 0;
        u32 InScope = 0;
        u32 Pending = 0;

        for( s32 i = 0; i < m_Resources.GetCount(); ++i )
        {
            const Resource* pExisting = m_Resources[i];

            if( !pExisting || pExisting->IsImported || pExisting->IsRetired ||
                !ResourceDescsMatch( pExisting->Desc, Desc ) )
                continue;

            ++Matches;
            OtherSlot += pExisting->FrameSlot != m_ActiveFrameSlot;
            InScope += pExisting->LastUsedFrame == m_Generation;
            Pending += pExisting->LastUsedFrameSerial && !eng_IsFrameComplete( pExisting->LastUsedFrameSerial );
        }

        x_DebugMsg( "A51 resource miss: frame=%llu slot=%u name='%s' matches=%u "
                              "other-slot=%u in-scope=%u pending=%u\n",
                      static_cast<unsigned long long>( m_ActiveFrameSerial ), m_ActiveFrameSlot,
                      Desc.pDebugName ? Desc.pDebugName : "unnamed", Matches, OtherSlot, InScope, Pending );
    }

    // Keep exact-descriptor targets warm so scopes such as main view and PIP can alternate.
    Resource* pResource = new Resource;

    if( !pResource )
    {
        RENDER_THROW( "FrameResourcePool", "failed to allocate resource bookkeeping for '%s'",
                      Desc.pDebugName ? Desc.pDebugName : "unnamed" );
        return RenderResourceHandle();
    }

    rtarget_desc TargetDesc   = {};
    TargetDesc.Width          = Desc.Extent.Width;
    TargetDesc.Height         = Desc.Extent.Height;
    TargetDesc.Format         = Desc.Format;
    TargetDesc.MipCount       = Desc.MipCount;
    TargetDesc.SampleCount    = Desc.SampleCount;
    TargetDesc.SampleQuality  = 0;
    TargetDesc.bBindAsTexture = ( Desc.Usage & RENDER_RESOURCE_USAGE_SAMPLED ) != 0;
    TargetDesc.bRenderTarget  = ( Desc.Usage & ( RENDER_RESOURCE_USAGE_COLOR | RENDER_RESOURCE_USAGE_DEPTH ) ) != 0;
    TargetDesc.StorageUsage   = GetTextureStorageUsage( Desc.Usage );
    TargetDesc.ClearDepth     = Desc.ClearDepth;
    TargetDesc.ClearStencil   = Desc.ClearStencil;
    TargetDesc.pDebugName     = Desc.pDebugName;
    CopyClearColor( TargetDesc.ClearColor, Desc.ClearColor );

    rbuffer_desc BufferDesc = Desc.Buffer;
    BufferDesc.pDebugName = Desc.pDebugName;
    xtimer CreateTimer;
    if( ResourceDiagnosticsEnabled() )
        CreateTimer.Start();

    const xbool Created = Desc.IsBuffer() ? rbuffer_Create( pResource->Buffer, BufferDesc )
                                          : rtarget_Create( pResource->Target, TargetDesc );

    if( ResourceDiagnosticsEnabled() )
    {
        LogResourceEvent( Created ? "create" : "create-failed", Desc, m_ActiveFrameSerial,
                          m_ActiveFrameSlot, 0, CreateTimer.StopMs() );
    }

    if( !Created )
    {
        RENDER_THROW( "FrameResourcePool", "failed to create '%s' %ux%u format=%d samples=%u usage=0x%x",
                      Desc.pDebugName ? Desc.pDebugName : "unnamed", Desc.Extent.Width, Desc.Extent.Height,
                      static_cast<s32>( Desc.Format ), Desc.SampleCount, Desc.Usage );
        delete pResource;
        return RenderResourceHandle();
    }

    pResource->Desc                = Desc;
    pResource->Generation          = m_Generation;
    pResource->LastUsedFrame       = m_Generation;
    pResource->LastUsedFrameSerial = m_ActiveFrameSerial;
    pResource->LastUsedTime        = m_ActiveFrameTime;
    pResource->FrameSlot           = m_ActiveFrameSlot;
    pResource->IsRetired           = FALSE;
    pResource->RetireFrameSerial   = 0;
    pResource->IsImported          = FALSE;

    const u32 ResourceIndex = AddResource( pResource );

    if( Desc.Lifetime == RENDER_RESOURCE_PERSISTENT || !m_DescriptorLookup.GetCount() ||
        ( m_nDescriptorLookups + 1u ) * 2u > static_cast<u32>( m_DescriptorLookup.GetCount() ) )
    {
        RebuildDescriptorLookup();
    }
    else
    {
        u32 SlotIndex = HashResourceDesc( Desc ) & m_DescriptorLookupMask;

        while( m_DescriptorLookup[static_cast<s32>( SlotIndex )] >= 0 )
        {
            SlotIndex = ( SlotIndex + 1u ) & m_DescriptorLookupMask;
        }

        m_DescriptorLookup[static_cast<s32>( SlotIndex )] = static_cast<s32>( ResourceIndex );
        ++m_nDescriptorLookups;
    }

    return RenderResourceHandle( ResourceIndex, pResource->Generation );
}

//=========================================================================

RenderResourceHandle FrameResourcePool::ImportExternal( const rtarget& Target,
                                                        const char*    pDebugName,
                                                        u32            ExternalUsage )
{
    if( !m_isInitialized || ( !rtarget_HasRenderTarget( Target ) && !rtarget_HasDepthStencil( Target ) ) )
    {
        RENDER_THROW( "FrameResourcePool", "invalid external target '%s'", pDebugName ? pDebugName : "unnamed" );
        return RenderResourceHandle();
    }

    u32 UsageData = RENDER_RESOURCE_USAGE_NONE;

    if( !GetExternalUsage( Target, ExternalUsage, UsageData ) )
    {
        RENDER_THROW( "FrameResourcePool", "external target '%s' has incompatible capabilities usage=0x%x",
                      pDebugName ? pDebugName : "unnamed", ExternalUsage );
        return RenderResourceHandle();
    }

    Resource* pResource = NULL;

    if( m_FreeResources.GetCount() > 0 )
    {
        const s32 FreeIndex = m_FreeResources.GetCount() - 1;
        pResource           = m_FreeResources[FreeIndex];
        m_FreeResources.SetCount( FreeIndex );
        pResource->Reset();
    }
    else
    {
        pResource = new Resource;
    }

    if( !pResource )
    {
        RENDER_THROW( "FrameResourcePool", "failed to allocate external target bookkeeping for '%s'",
                      pDebugName ? pDebugName : "unnamed" );
        return RenderResourceHandle();
    }

    pResource->Desc.Extent         = RenderExtent( Target.Desc.Width, Target.Desc.Height );
    pResource->Desc.Format         = Target.Desc.Format;
    pResource->Desc.MipCount       = Target.Desc.MipCount;
    pResource->Desc.SampleCount    = Target.Desc.SampleCount;
    pResource->Desc.ArrayLayers    = 1;
    pResource->Desc.ViewMask       = 1;
    pResource->Desc.Usage          = UsageData;
    pResource->Desc.Lifetime       = RENDER_RESOURCE_EXTERNAL;
    pResource->Desc.pDebugName     = pDebugName;
    pResource->Generation          = m_Generation;
    pResource->LastUsedFrame       = m_Generation;
    pResource->LastUsedFrameSerial = m_ActiveFrameSerial;
    pResource->FrameSlot           = m_ActiveFrameSlot;
    pResource->IsRetired           = FALSE;
    pResource->RetireFrameSerial   = 0;
    pResource->IsImported          = TRUE;
    pResource->pExternal           = &Target;
    pResource->pExternalBackend    = Target.pBackend;

    const u32 ResourceIndex = AddResource( pResource );

    return RenderResourceHandle( ResourceIndex, pResource->Generation );
}

//=========================================================================

const rbuffer* FrameResourcePool::GetBuffer( RenderResourceHandle ResourceHandle ) const
{
    if( !ResourceHandle.IsValid() || ResourceHandle.Index >= static_cast<u32>( m_Resources.GetCount() ) )
    {
        return NULL;
    }

    const Resource* pResource = m_Resources[ResourceHandle.Index];
    return pResource && pResource->Desc.IsBuffer() && pResource->Generation == ResourceHandle.Generation &&
           pResource->LastUsedFrame == m_Generation && pResource->Buffer ? &pResource->Buffer : NULL;
}

//=========================================================================

const rtarget* FrameResourcePool::Get( RenderResourceHandle ResourceHandle ) const
{
    if( !ResourceHandle.IsValid() || ( ResourceHandle.Index >= static_cast<u32>( m_Resources.GetCount() ) ) )
    {
        return NULL;
    }

    const Resource* pResource = m_Resources[ResourceHandle.Index];

    if( !pResource || pResource->Desc.IsBuffer() || ( pResource->Generation != ResourceHandle.Generation ) ||
        ( pResource->LastUsedFrame != m_Generation ) )
    {
        return NULL;
    }

    if( !pResource->IsImported )
    {
        return rtarget_IsValid( pResource->Target ) ? &pResource->Target : NULL;
    }

    const rtarget* pTarget = pResource->pExternal;

    if( !pTarget || !rtarget_IsValid( *pTarget ) || ( pTarget->pBackend != pResource->pExternalBackend ) ||
        ( pTarget->Desc.Width != pResource->Desc.Extent.Width ) ||
        ( pTarget->Desc.Height != pResource->Desc.Extent.Height ) ||
        ( pTarget->Desc.Format != pResource->Desc.Format ) ||
        ( pTarget->Desc.SampleCount != pResource->Desc.SampleCount ) ||
        ( pTarget->Desc.MipCount != pResource->Desc.MipCount ) )
    {
        return NULL;
    }

    return pTarget;
}

//=========================================================================

rtarget* FrameResourcePool::GetMutable( RenderResourceHandle ResourceHandle )
{
    if( !ResourceHandle.IsValid() || ( ResourceHandle.Index >= static_cast<u32>( m_Resources.GetCount() ) ) )
    {
        return NULL;
    }

    Resource* pResource = m_Resources[ResourceHandle.Index];

    if( !pResource || pResource->Desc.IsBuffer() || ( pResource->Generation != ResourceHandle.Generation ) ||
        ( pResource->LastUsedFrame != m_Generation ) )
    {
        return NULL;
    }

    if( pResource->IsImported )
    {
        return Get( ResourceHandle ) && pResource->pExternalMutable ? pResource->pExternalMutable : NULL;
    }

    return rtarget_IsValid( pResource->Target ) ? &pResource->Target : NULL;
}

//=========================================================================

xbool FrameResourcePool::MarkExternalMutable( RenderResourceHandle ResourceHandle, rtarget& Target )
{
    if( !ResourceHandle.IsValid() || ResourceHandle.Index >= static_cast<u32>( m_Resources.GetCount() ) )
    {
        return FALSE;
    }

    Resource* pResource = m_Resources[ResourceHandle.Index];

    if( !pResource || !pResource->IsImported || pResource->Generation != ResourceHandle.Generation ||
        pResource->LastUsedFrame != m_Generation || pResource->pExternalBackend != Target.pBackend )
    {
        return FALSE;
    }

    pResource->pExternalMutable = &Target;

    return TRUE;
}

//=========================================================================
} // namespace render
