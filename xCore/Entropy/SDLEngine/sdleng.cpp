//==============================================================================
//
//  sdleng.cpp
//
//==============================================================================

#include "x_target.hpp"

#if (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"
#include "e_Engine.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    SDLENG_DEFAULT_BACKBUFFER_WIDTH  = 1024,
    SDLENG_DEFAULT_BACKBUFFER_HEIGHT = 768
};

//------------------------------------------------------------------------------

enum
{
    SDLENG_MIN_TRANSFER_BUFFER_SIZE = 64 * 1024,
    SDLENG_MAX_TRANSFER_BUFFER_SIZE = 4 * 1024 * 1024,
    SDLENG_MAX_TRANSFER_BYTES       = 256 * 1024 * 1024,
    SDLENG_TRANSIENT_BATCH_BYTES    = 4 * 1024 * 1024,
    SDLENG_TRANSIENT_BATCH_REQUESTS = 64,
    SDLENG_FRAMES_IN_FLIGHT         = 2
};

//==============================================================================
//  TYPES
//==============================================================================

struct sdleng_locals
{
    SDL_GPUCommandBuffer* pCommandBuffer;
    SDL_GPURenderPass*    pRenderPass;
    SDL_GPUComputePass*   pComputePass;
    SDL_GPUTexture*       pSwapchainTexture;
    SDL_GPUTexture*       pRenderAttachments[RTARGET_MAX_TARGETS + 1];
    u32                   RenderAttachmentCount;
    SDL_GPUTextureFormat  SwapchainFormat;
    u32                   BackBufferWidth;
    u32                   BackBufferHeight;
    u32                   AcquiredWidth;
    u32                   AcquiredHeight;
    sdleng_present_policy PresentPolicy;
    SDL_GPUPresentMode    ActivePresentMode;
    xtick                 RenderStartTime;
    f32                   FramePacingWaitMs;
    f32                   RenderSubmitMs;
    u64                   NextFrameSerial;
    u64                   CurrentFrameSerial;
    u64                   LastSubmittedFrameSerial;
    u32                   CurrentFrameSlot;
    u64                   FrameSerials[SDLENG_FRAMES_IN_FLIGHT];
    SDL_GPUFence*         pFrameFences[SDLENG_FRAMES_IN_FLIGHT];

    xbool bInitialized;
    xbool bWindowClaimed;
    xbool bWindowInitializedByDevice;
    xbool bSwapchainAcquired;
    xbool bBackBufferRendered;
    xbool bSwapchainAcquireLogged;

    sdleng_locals( void )
    {
        x_memset( this, 0, sizeof( sdleng_locals ) );
        PresentPolicy     = SDLENG_PRESENT_VSYNC;
        ActivePresentMode = SDL_GPU_PRESENTMODE_VSYNC;
    }
};

//------------------------------------------------------------------------------

struct sdleng_transfer_buffer
{
    SDL_GPUTransferBuffer* pBuffer;
    u32                    Capacity;
    u32                    Offset;
    xtick                  LastUsed;

    sdleng_transfer_buffer( void ) :
        pBuffer ( NULL ),
        Capacity( 0 ),
        Offset  ( 0 ),
        LastUsed( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct sdleng_transfer_slot
{
    xarray<sdleng_transfer_buffer> Buffers;
    u32                            BufferCount;
    s32                            ActiveBuffer;
    u64                            ReservedBytes;
    u64                            UsedBytes;
    u64                            PeakUsedBytes;
    SDL_GPUFence*                  pFence;

    sdleng_transfer_slot( void ) :
        Buffers      (),
        BufferCount  ( 0 ),
        ActiveBuffer ( -1 ),
        ReservedBytes( 0 ),
        UsedBytes    ( 0 ),
        PeakUsedBytes( 0 ),
        pFence       ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct sdleng_transfer_state
{
    sdleng_transfer_slot  Frame[SDLENG_FRAMES_IN_FLIGHT];
    sdleng_transfer_slot  Transient[SDLENG_FRAMES_IN_FLIGHT];
    u64                   Creates;
    u64                   Trims;
    u32                   NextTransientSlot;
    SDL_GPUCommandBuffer* pCommandBuffer;
    SDL_GPUCopyPass*      pCopyPass;
    sdleng_transfer_slot* pSlot;
    s32                   RequestBuffer;
    u32                   RequestOffset;
    u32                   CommittedRequests;
    xbool                 bOwnCommandBuffer;
    xbool                 bMapped;

    sdleng_transfer_state( void ) :
        Creates          ( 0 ),
        Trims            ( 0 ),
        NextTransientSlot( 0 ),
        pCommandBuffer   ( NULL ),
        pCopyPass        ( NULL ),
        pSlot            ( NULL ),
        RequestBuffer    ( -1 ),
        RequestOffset    ( 0 ),
        CommittedRequests( 0 ),
        bOwnCommandBuffer( FALSE ),
        bMapped          ( FALSE )
    {
    }
};

//==============================================================================
//  GLOBAL BACKEND HANDLES
//==============================================================================

SDL_GPUDevice* g_pSDLGPUDevice = NULL;

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

static sdleng_locals         s;
static sdleng_transfer_state s_Transfer;

//==============================================================================
//  TRANSFER HELPERS
//==============================================================================

static
u32 sdleng_AlignTransferOffset( u32 Offset, u32 Alignment )
{
    if( Alignment < 1 )
    {
        Alignment = 1;
    }

    const u32 Remainder = Offset % Alignment;

    if( Remainder == 0 )
    {
        return Offset;
    }

    const u32 Padding = Alignment - Remainder;

    if( Offset > ( 0xffffffffu - Padding ) )
    {
        return 0xffffffffu;
    }

    return Offset + Padding;
}

//==============================================================================

static
void sdleng_ResetTransferContext( void )
{
    s_Transfer.pCommandBuffer    = NULL;
    s_Transfer.pCopyPass         = NULL;
    s_Transfer.pSlot             = NULL;
    s_Transfer.RequestBuffer     = -1;
    s_Transfer.RequestOffset     = 0;
    s_Transfer.CommittedRequests = 0;
    s_Transfer.bOwnCommandBuffer = FALSE;
    s_Transfer.bMapped           = FALSE;
}

//==============================================================================

static
void sdleng_ResetTransferSlot( sdleng_transfer_slot& Slot )
{
    for( u32 i = 0; i < Slot.BufferCount; ++i )
    {
        Slot.Buffers[i].Offset = 0;
    }

    Slot.UsedBytes    = 0;
    Slot.ActiveBuffer = -1;
}

//==============================================================================

static
void sdleng_UpdateTransferMetrics( void )
{
    static xprofile_gauge FrameBufferCount =
        x_GetProfiler().RegisterGauge( "SDLTransferFrameBufferCount", XPROFILE_UNIT_COUNT, "RendererMemory" );

    static xprofile_gauge FrameReservedBytes =
        x_GetProfiler().RegisterGauge( "SDLTransferFrameReservedBytes", XPROFILE_UNIT_BYTES, "RendererMemory" );

    static xprofile_gauge FrameUsedBytes =
        x_GetProfiler().RegisterGauge( "SDLTransferFrameUsedBytes", XPROFILE_UNIT_BYTES, "RendererMemory" );

    static xprofile_gauge FramePeakBytes =
        x_GetProfiler().RegisterGauge( "SDLTransferFramePeakBytes", XPROFILE_UNIT_BYTES, "RendererMemory" );

    static xprofile_gauge TransientBufferCount =
        x_GetProfiler().RegisterGauge( "SDLTransferTransientBufferCount", XPROFILE_UNIT_COUNT, "RendererMemory" );

    static xprofile_gauge TransientReservedBytes =
        x_GetProfiler().RegisterGauge( "SDLTransferTransientReservedBytes", XPROFILE_UNIT_BYTES, "RendererMemory" );

    static xprofile_gauge TransientUsedBytes =
        x_GetProfiler().RegisterGauge( "SDLTransferTransientUsedBytes", XPROFILE_UNIT_BYTES, "RendererMemory" );

    static xprofile_gauge TransientPeakBytes =
        x_GetProfiler().RegisterGauge( "SDLTransferTransientPeakBytes", XPROFILE_UNIT_BYTES, "RendererMemory" );

    u64 ReservedBytes[2] = { 0, 0 };
    u64 UsedBytes[2]     = { 0, 0 };
    u64 PeakBytes[2]     = { 0, 0 };
    s32 BufferCount[2]   = { 0, 0 };

    for( u32 iSlot = 0; iSlot < SDLENG_FRAMES_IN_FLIGHT; ++iSlot )
    {
        sdleng_transfer_slot& Slot = s_Transfer.Frame[iSlot];
        BufferCount[0] += (s32)Slot.BufferCount;

        ReservedBytes[0] += Slot.ReservedBytes;
        UsedBytes[0] += Slot.UsedBytes;
        PeakBytes[0] = MAX( PeakBytes[0], Slot.PeakUsedBytes );
    }

    for( u32 iSlot = 0; iSlot < SDLENG_FRAMES_IN_FLIGHT; ++iSlot )
    {
        const sdleng_transfer_slot& Slot = s_Transfer.Transient[iSlot];
        BufferCount[1] += Slot.BufferCount;
        ReservedBytes[1] += Slot.ReservedBytes;
        UsedBytes[1] += Slot.UsedBytes;
        PeakBytes[1] = MAX( PeakBytes[1], Slot.PeakUsedBytes );
    }

    FrameBufferCount.Set( (f64)BufferCount[0] );
    FrameReservedBytes.Set( (f64)ReservedBytes[0] );
    FrameUsedBytes.Set( (f64)UsedBytes[0] );
    FramePeakBytes.Set( (f64)PeakBytes[0] );
    TransientBufferCount.Set( (f64)BufferCount[1] );
    TransientReservedBytes.Set( (f64)ReservedBytes[1] );
    TransientUsedBytes.Set( (f64)UsedBytes[1] );
    TransientPeakBytes.Set( (f64)PeakBytes[1] );
}

//==============================================================================

static
void sdleng_RecordTransferBufferCreate( void )
{
    static xprofile_counter TransferBufferCreates =
        x_GetProfiler().RegisterCounter( "SDLTransferBufferCreates", "RendererMemory" );

    TransferBufferCreates.Add();
}

//==============================================================================

static
void sdleng_RecordTransferBudgetReject( u32 RequiredSize, u64 ReservedBytes, u32 BufferCount )
{
    static xprofile_counter TransferBudgetRejects =
        x_GetProfiler().RegisterCounter( "SDLTransferBudgetRejects", "RendererMemory" );

    TransferBudgetRejects.Add();
    x_DebugMsg( "SDLEngine: transfer allocation rejected request=%u reserved=%llu buffers=%u limit=%u\n",
                RequiredSize,
                static_cast<unsigned long long>( ReservedBytes ),
                BufferCount,
                (u32)SDLENG_MAX_TRANSFER_BYTES );
}

//==============================================================================

static
void sdleng_RecordTransferFenceWait( xtick WaitTicks )
{
    static xprofile_counter TransferFenceWaits =
        x_GetProfiler().RegisterCounter( "SDLTransferFenceWaits", "RendererMemory" );

    static xprofile_gauge TransferFenceWaitMs =
        x_GetProfiler().RegisterGauge( "SDLTransferFenceWaitMs", XPROFILE_UNIT_MILLISECONDS, "RendererMemory" );

    TransferFenceWaits.Add();
    TransferFenceWaitMs.Set( x_TicksToMs( WaitTicks ) );
}

//==============================================================================

static
void sdleng_RecordFrameFenceWait( xtick WaitTicks )
{
    static xprofile_counter FrameFenceWaits =
        x_GetProfiler().RegisterCounter( "SDLFrameFenceWaits", "RendererMemory" );

    static xprofile_gauge FrameFenceWaitMs =
        x_GetProfiler().RegisterGauge( "SDLFrameFenceWaitMs", XPROFILE_UNIT_MILLISECONDS, "RendererMemory" );

    FrameFenceWaits.Add();
    FrameFenceWaitMs.Set( x_TicksToMs( WaitTicks ) );
}

//==============================================================================

static
xbool sdleng_CollectFrameFence( u32 FrameSlot, xbool bWait, const char* pSubsystem )
{
    if( FrameSlot >= SDLENG_FRAMES_IN_FLIGHT )
    {
        return TRUE;
    }

    if( !s.pFrameFences[FrameSlot] )
    {
        return TRUE;
    }

    if( !SDL_QueryGPUFence( g_pSDLGPUDevice, s.pFrameFences[FrameSlot] ) )
    {
        if( !bWait )
        {
            return FALSE;
        }

        SDL_GPUFence* Fences[1] = { s.pFrameFences[FrameSlot] };
        const xtick   WaitStart = x_GetTime();

        if( !SDL_WaitForGPUFences( g_pSDLGPUDevice, true, Fences, 1 ) )
        {
            sdleng_LogError( pSubsystem, "SDL_WaitForGPUFences" );
            return FALSE;
        }

        const xtick WaitTicks = x_GetTime() - WaitStart;
        sdleng_RecordFrameFenceWait( WaitTicks );
        s.FramePacingWaitMs += x_TicksToMs( WaitTicks );
    }

    SDL_ReleaseGPUFence( g_pSDLGPUDevice, s.pFrameFences[FrameSlot] );
    s.pFrameFences[FrameSlot] = NULL;
    s.FrameSerials[FrameSlot] = 0;

    return TRUE;
}

//==============================================================================

static
xbool sdleng_CollectFrameFences( xbool bWait, const char* pSubsystem )
{
    for( u32 FrameSlot = 0; FrameSlot < SDLENG_FRAMES_IN_FLIGHT; ++FrameSlot )
    {
        if( !sdleng_CollectFrameFence( FrameSlot, bWait, pSubsystem ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool sdleng_TransferFits( const sdleng_transfer_buffer& Buffer,
                           u32                           RequiredSize,
                           u32                           Alignment )
{
    const u32 Offset = sdleng_AlignTransferOffset( Buffer.Offset, Alignment );

    if( Offset == 0xffffffffu || Offset > Buffer.Capacity )
    {
        return FALSE;
    }

    return RequiredSize <= ( Buffer.Capacity - Offset );
}

//==============================================================================

static
u64 sdleng_GetReservedTransferBytes( void )
{
    u64 Bytes = 0;

    for( u32 i = 0; i < SDLENG_FRAMES_IN_FLIGHT; ++i )
    {
        Bytes += s_Transfer.Frame[i].ReservedBytes + s_Transfer.Transient[i].ReservedBytes;
    }

    return Bytes;
}

//==============================================================================

static
xbool sdleng_IsTransferSlotAvailable( sdleng_transfer_slot& Slot )
{
    if( s_Transfer.pSlot == &Slot )
    {
        return FALSE;
    }

    for( u32 i = 0; i < SDLENG_FRAMES_IN_FLIGHT; ++i )
    {
        if( &s_Transfer.Frame[i] == &Slot )
        {
            if( s.pCommandBuffer && s.CurrentFrameSlot == i )
            {
                return FALSE;
            }

            return !s.pFrameFences[i] || SDL_QueryGPUFence( g_pSDLGPUDevice, s.pFrameFences[i] );
        }
    }

    return !Slot.pFence || SDL_QueryGPUFence( g_pSDLGPUDevice, Slot.pFence );
}

//==============================================================================

static
void sdleng_TrimTransferBuffers( u64 RequiredBytes, xtick Now )
{
    static xprofile_counter TransferBufferTrims =
        x_GetProfiler().RegisterCounter( "SDLTransferBufferTrims", "RendererMemory" );

    for( ;; )
    {
        sdleng_transfer_slot* pOldestSlot = NULL;
        s32                   OldestIndex = -1;
        xtick                 OldestUse   = Now;
        const xbool           Pressure    = sdleng_GetReservedTransferBytes() + RequiredBytes > SDLENG_MAX_TRANSFER_BYTES;

        for( u32 i = 0; i < SDLENG_FRAMES_IN_FLIGHT * 2; ++i )
        {
            sdleng_transfer_slot& Slot = i < SDLENG_FRAMES_IN_FLIGHT
                                             ? s_Transfer.Frame[i]
                                             : s_Transfer.Transient[i - SDLENG_FRAMES_IN_FLIGHT];

            if( !sdleng_IsTransferSlotAvailable( Slot ) )
            {
                continue;
            }

            for( u32 j = 0; j < Slot.BufferCount; ++j )
            {
                const sdleng_transfer_buffer& Buffer = Slot.Buffers[j];

                if( Buffer.LastUsed <= OldestUse &&
                    ( Pressure || x_TicksToMs( Now - Buffer.LastUsed ) >= 5000.0f ) )
                {
                    pOldestSlot = &Slot;
                    OldestIndex = j;
                    OldestUse   = Buffer.LastUsed;
                }
            }
        }

        if( !pOldestSlot )
        {
            return;
        }

        sdleng_transfer_buffer& Buffer = pOldestSlot->Buffers[OldestIndex];
        SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, Buffer.pBuffer );
        pOldestSlot->ReservedBytes -= Buffer.Capacity;
        pOldestSlot->UsedBytes -= Buffer.Offset;
        pOldestSlot->Buffers.Delete( OldestIndex );
        --pOldestSlot->BufferCount;

        if( pOldestSlot->ActiveBuffer == OldestIndex )
        {
            pOldestSlot->ActiveBuffer = -1;
        }
        else if( pOldestSlot->ActiveBuffer > OldestIndex )
        {
            --pOldestSlot->ActiveBuffer;
        }

        ++s_Transfer.Trims;
        TransferBufferTrims.Add();
    }
}

//==============================================================================

static
xbool sdleng_SelectTransferBuffer( sdleng_transfer_slot& Slot,
                                   u32                   RequiredSize,
                                   u32                   Alignment )
{
    if( Slot.ActiveBuffer >= 0 &&
        Slot.ActiveBuffer < (s32)Slot.BufferCount &&
        sdleng_TransferFits( Slot.Buffers[Slot.ActiveBuffer], RequiredSize, Alignment ) )
    {
        return TRUE;
    }

    for( u32 i = 0; i < Slot.BufferCount; ++i )
    {
        if( sdleng_TransferFits( Slot.Buffers[i], RequiredSize, Alignment ) )
        {
            Slot.ActiveBuffer = i;
            return TRUE;
        }
    }

    const u64 MaxTransferBytes = SDLENG_MAX_TRANSFER_BYTES;

    if( (u64)RequiredSize > MaxTransferBytes )
    {
        sdleng_RecordTransferBudgetReject( RequiredSize, 0, Slot.BufferCount );
        return FALSE;
    }

    if( sdleng_GetReservedTransferBytes() + RequiredSize > MaxTransferBytes )
    {
        sdleng_TrimTransferBuffers( RequiredSize, x_GetTime() );
    }

    const u64 ReservedBytes = sdleng_GetReservedTransferBytes();

    if( ReservedBytes > MaxTransferBytes ||
        (u64)RequiredSize > ( MaxTransferBytes - ReservedBytes ) )
    {
        sdleng_RecordTransferBudgetReject( RequiredSize, ReservedBytes, Slot.BufferCount );
        return FALSE;
    }

    u64 Capacity64 = MAX( (u64)RequiredSize, (u64)SDLENG_MIN_TRANSFER_BUFFER_SIZE );

    if( Slot.BufferCount > 0 )
    {
        const u64 PreviousCapacity = Slot.Buffers[Slot.BufferCount - 1].Capacity;
        Capacity64                 = MAX( Capacity64,
                          MIN( PreviousCapacity * 2,
                               (u64)SDLENG_MAX_TRANSFER_BUFFER_SIZE ) );
    }

    Capacity64 = MIN( Capacity64, MaxTransferBytes - ReservedBytes );

    if( Capacity64 < RequiredSize )
    {
        sdleng_RecordTransferBudgetReject( RequiredSize, ReservedBytes, Slot.BufferCount );
        return FALSE;
    }

    const u32 Capacity = (u32)Capacity64;

    SDL_GPUTransferBufferCreateInfo TransferDesc;
    x_memset( &TransferDesc, 0, sizeof( TransferDesc ) );
    TransferDesc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    TransferDesc.size  = Capacity;

    SDL_GPUTransferBuffer* pTransferBuffer =
        SDL_CreateGPUTransferBuffer( g_pSDLGPUDevice, &TransferDesc );

    if( !pTransferBuffer )
    {
        sdleng_LogError( "SDLEngine", "SDL_CreateGPUTransferBuffer" );
        return FALSE;
    }

    sdleng_transfer_buffer& Buffer = Slot.Buffers.Append();
    Buffer                         = sdleng_transfer_buffer();
    ++Slot.BufferCount;
    Buffer.pBuffer  = pTransferBuffer;
    Buffer.Capacity = Capacity;
    Buffer.Offset   = 0;
    Buffer.LastUsed = x_GetTime();
    Slot.ReservedBytes += Capacity;
    Slot.ActiveBuffer = (s32)Slot.BufferCount - 1;
    ++s_Transfer.Creates;
    sdleng_RecordTransferBufferCreate();

    return TRUE;
}

//==============================================================================

static
xbool sdleng_WaitForTransferSlot( sdleng_transfer_slot& Slot )
{
    if( !Slot.pFence )
    {
        return TRUE;
    }

    if( !SDL_QueryGPUFence( g_pSDLGPUDevice, Slot.pFence ) )
    {
        SDL_GPUFence* Fences[1] = { Slot.pFence };
        const xtick   WaitStart = x_GetTime();

        if( !SDL_WaitForGPUFences( g_pSDLGPUDevice, true, Fences, 1 ) )
        {
            sdleng_LogError( "SDLEngine", "SDL_WaitForGPUFences" );
            return FALSE;
        }

        sdleng_RecordTransferFenceWait( x_GetTime() - WaitStart );
    }

    SDL_ReleaseGPUFence( g_pSDLGPUDevice, Slot.pFence );
    Slot.pFence = NULL;

    return TRUE;
}

//==============================================================================

static
xbool sdleng_EnsureTransferCommandBuffer( const char* pSubsystem )
{
    if( s_Transfer.pCommandBuffer )
    {
        return TRUE;
    }

    if( s.pCommandBuffer )
    {
        s_Transfer.pCommandBuffer    = s.pCommandBuffer;
        s_Transfer.pSlot             = &s_Transfer.Frame[s.CurrentFrameSlot];
        s_Transfer.bOwnCommandBuffer = FALSE;
        return TRUE;
    }

    sdleng_transfer_slot& Slot = s_Transfer.Transient[s_Transfer.NextTransientSlot];

    if( !sdleng_WaitForTransferSlot( Slot ) )
    {
        return FALSE;
    }

    sdleng_ResetTransferSlot( Slot );
    sdleng_TrimTransferState( x_GetTime() );

    s_Transfer.pCommandBuffer = SDL_AcquireGPUCommandBuffer( g_pSDLGPUDevice );

    if( !s_Transfer.pCommandBuffer )
    {
        sdleng_LogError( pSubsystem, "SDL_AcquireGPUCommandBuffer" );
        return FALSE;
    }

    s_Transfer.pSlot             = &Slot;
    s_Transfer.bOwnCommandBuffer = TRUE;

    return TRUE;
}

//==============================================================================
//  TRANSFER COMMANDS
//==============================================================================

xbool sdleng_BeginTransferCommands( const char* pSubsystem )
{
    if( !g_pSDLGPUDevice || sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        return FALSE;
    }

    if( !sdleng_EnsureTransferCommandBuffer( pSubsystem ) )
    {
        return FALSE;
    }

    if( s_Transfer.pCopyPass )
    {
        sdleng_EndTransferPass();
    }

    return TRUE;
}

//==============================================================================

xbool sdleng_BeginCopyPass( const char* pSubsystem )
{
    if( !g_pSDLGPUDevice || sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        return FALSE;
    }

    if( !sdleng_EnsureTransferCommandBuffer( pSubsystem ) )
    {
        return FALSE;
    }

    if( s_Transfer.pCopyPass )
    {
        return TRUE;
    }

    s_Transfer.pCopyPass = SDL_BeginGPUCopyPass( s_Transfer.pCommandBuffer );

    if( !s_Transfer.pCopyPass )
    {
        sdleng_LogError( pSubsystem, "SDL_BeginGPUCopyPass" );
        sdleng_CancelTransfer();
        return FALSE;
    }

    static xprofile_counter TransferCopyPasses =
        x_GetProfiler().RegisterCounter( "SDLTransferCopyPasses", "RenderCounter" );

    TransferCopyPasses.Add();

    return TRUE;
}

//==============================================================================

xbool sdleng_BeginTransfer( u32 RequiredSize, u32 Alignment, const char* pSubsystem )
{
    ASSERT( s_Transfer.RequestBuffer == -1 );

    if( !g_pSDLGPUDevice || !RequiredSize || sdleng_InRenderPass() || sdleng_InComputePass() )
    {
        return FALSE;
    }

    if( !sdleng_EnsureTransferCommandBuffer( pSubsystem ) )
    {
        return FALSE;
    }

    sdleng_transfer_slot& Slot = *s_Transfer.pSlot;

    if( s_Transfer.pCopyPass &&
        Slot.ActiveBuffer >= 0 &&
        Slot.ActiveBuffer < (s32)Slot.BufferCount &&
        sdleng_TransferFits( Slot.Buffers[Slot.ActiveBuffer], RequiredSize, Alignment ) )
    {
        s_Transfer.RequestBuffer = Slot.ActiveBuffer;
        s_Transfer.RequestOffset = Slot.Buffers[Slot.ActiveBuffer].Offset;
        return TRUE;
    }

    if( s_Transfer.pCopyPass && ( Slot.ActiveBuffer < 0 ) )
    {
        if( !sdleng_SelectTransferBuffer( Slot, RequiredSize, Alignment ) )
        {
            sdleng_CancelTransfer();
            return FALSE;
        }

        s_Transfer.RequestBuffer = Slot.ActiveBuffer;
        s_Transfer.RequestOffset = Slot.Buffers[Slot.ActiveBuffer].Offset;
        return TRUE;
    }

    if( s_Transfer.pCopyPass )
    {
        sdleng_EndTransferPass();
    }

    if( !sdleng_SelectTransferBuffer( Slot, RequiredSize, Alignment ) )
    {
        sdleng_CancelTransfer();
        return FALSE;
    }

    s_Transfer.RequestBuffer = Slot.ActiveBuffer;
    s_Transfer.RequestOffset = Slot.Buffers[Slot.ActiveBuffer].Offset;

    return sdleng_BeginCopyPass( pSubsystem );
}

//==============================================================================

xbool sdleng_ReserveTransfer( u32 Size, u32 Alignment, u32& Offset )
{
    if( !s_Transfer.pSlot || !Size ||
        s_Transfer.pSlot->ActiveBuffer < 0 ||
        s_Transfer.pSlot->ActiveBuffer >= (s32)s_Transfer.pSlot->BufferCount )
    {
        return FALSE;
    }

    sdleng_transfer_buffer& Buffer =
        s_Transfer.pSlot->Buffers[s_Transfer.pSlot->ActiveBuffer];

    const u32 AlignedOffset = sdleng_AlignTransferOffset( Buffer.Offset, Alignment );

    if( AlignedOffset == 0xffffffffu || AlignedOffset > Buffer.Capacity ||
        Size > ( Buffer.Capacity - AlignedOffset ) )
    {
        return FALSE;
    }

    Offset = AlignedOffset;
    s_Transfer.pSlot->UsedBytes += AlignedOffset + Size - Buffer.Offset;
    Buffer.Offset                   = AlignedOffset + Size;
    Buffer.LastUsed                 = x_GetTime();
    s_Transfer.pSlot->PeakUsedBytes = MAX( s_Transfer.pSlot->PeakUsedBytes,
                                           s_Transfer.pSlot->UsedBytes );

    return TRUE;
}

//==============================================================================

xbool sdleng_CommitTransfer( void )
{
    s_Transfer.RequestBuffer = -1;
    s_Transfer.RequestOffset = 0;
    ++s_Transfer.CommittedRequests;

    if( s_Transfer.bOwnCommandBuffer )
    {
        const u64 UsedBytes = s_Transfer.pSlot->UsedBytes;

        if( ( UsedBytes >= SDLENG_TRANSIENT_BATCH_BYTES ) ||
            ( s_Transfer.CommittedRequests >= SDLENG_TRANSIENT_BATCH_REQUESTS ) )
        {
            return sdleng_FlushTransferCommands();
        }
    }

    return TRUE;
}

//==============================================================================

SDL_GPUTransferBuffer* sdleng_GetTransferBuffer( void )
{
    if( !s_Transfer.pSlot ||
        s_Transfer.pSlot->ActiveBuffer < 0 ||
        s_Transfer.pSlot->ActiveBuffer >= (s32)s_Transfer.pSlot->BufferCount )
    {
        return NULL;
    }

    return s_Transfer.pSlot->Buffers[s_Transfer.pSlot->ActiveBuffer].pBuffer;
}

//==============================================================================

SDL_GPUCopyPass* sdleng_GetTransferCopyPass( void )
{
    return s_Transfer.pCopyPass;
}

//==============================================================================

SDL_GPUCommandBuffer* sdleng_GetTransferCommandBuffer( void )
{
    return s_Transfer.pCommandBuffer;
}

//==============================================================================

xbool sdleng_MapTransferBuffer( void*& pMapped, const char* pSubsystem )
{
    pMapped                                = NULL;
    SDL_GPUTransferBuffer* pTransferBuffer = sdleng_GetTransferBuffer();

    if( !pTransferBuffer || s_Transfer.bMapped )
    {
        return FALSE;
    }

    pMapped = SDL_MapGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer, false );

    if( !pMapped )
    {
        sdleng_LogError( pSubsystem, "SDL_MapGPUTransferBuffer" );
        return FALSE;
    }

    s_Transfer.bMapped = TRUE;

    return TRUE;
}

//==============================================================================

void sdleng_UnmapTransferBuffer( void )
{
    SDL_GPUTransferBuffer* pTransferBuffer = sdleng_GetTransferBuffer();

    if( s_Transfer.bMapped && pTransferBuffer )
    {
        SDL_UnmapGPUTransferBuffer( g_pSDLGPUDevice, pTransferBuffer );
    }

    s_Transfer.bMapped = FALSE;
}

//==============================================================================

void sdleng_EndTransferPass( void )
{
    if( s_Transfer.bMapped )
    {
        sdleng_UnmapTransferBuffer();
    }

    if( s_Transfer.pCopyPass )
    {
        SDL_EndGPUCopyPass( s_Transfer.pCopyPass );
        s_Transfer.pCopyPass = NULL;
    }
}

//==============================================================================

static
xbool sdleng_SubmitTransferCommands( const char* pSubsystem )
{
    if( !s_Transfer.pCommandBuffer )
    {
        return TRUE;
    }

    sdleng_transfer_slot* pSlot = s_Transfer.pSlot;
    sdleng_EndTransferPass();

    if( !s_Transfer.bOwnCommandBuffer )
    {
        sdleng_ResetTransferContext();
        return TRUE;
    }

    SDL_GPUFence* pFence = SDL_SubmitGPUCommandBufferAndAcquireFence( s_Transfer.pCommandBuffer );

    if( !pFence )
    {
        sdleng_LogError( pSubsystem, "SDL_SubmitGPUCommandBufferAndAcquireFence" );

        if( pSlot )
        {
            sdleng_ResetTransferSlot( *pSlot );
        }

        sdleng_ResetTransferContext();
        return FALSE;
    }

    pSlot->pFence                = pFence;
    s_Transfer.NextTransientSlot = ( s_Transfer.NextTransientSlot + 1 ) % SDLENG_FRAMES_IN_FLIGHT;
    {
        static xprofile_counter TransferCommandBufferSubmissions =
            x_GetProfiler().RegisterCounter( "SDLTransferCommandBufferSubmissions", "RendererMemory" );

        TransferCommandBufferSubmissions.Add();
    }

    sdleng_ResetTransferContext();
    sdleng_UpdateTransferMetrics();

    return TRUE;
}

//==============================================================================

xbool sdleng_FlushTransferCommands( void )
{
    return sdleng_SubmitTransferCommands( "SDLEngine" );
}

//==============================================================================

void sdleng_CancelTransfer( void )
{
    if( !s_Transfer.pCommandBuffer )
    {
        return;
    }

    const xbool           bOwnCommandBuffer = s_Transfer.bOwnCommandBuffer;
    sdleng_transfer_slot* pSlot             = s_Transfer.pSlot;

    if( s_Transfer.pSlot &&
        s_Transfer.RequestBuffer >= 0 &&
        s_Transfer.RequestBuffer < (s32)s_Transfer.pSlot->BufferCount )
    {
        s_Transfer.pSlot->UsedBytes -=
            s_Transfer.pSlot->Buffers[s_Transfer.RequestBuffer].Offset - s_Transfer.RequestOffset;

        s_Transfer.pSlot->Buffers[s_Transfer.RequestBuffer].Offset = s_Transfer.RequestOffset;
        s_Transfer.pSlot->ActiveBuffer                             = s_Transfer.RequestBuffer;
    }

    sdleng_EndTransferPass();

    if( bOwnCommandBuffer )
    {
        if( s_Transfer.CommittedRequests )
        {
            sdleng_SubmitTransferCommands( "SDLEngine" );
            return;
        }

        SDL_CancelGPUCommandBuffer( s_Transfer.pCommandBuffer );

        if( pSlot )
        {
            sdleng_ResetTransferSlot( *pSlot );
        }
    }

    sdleng_ResetTransferContext();
    sdleng_UpdateTransferMetrics();
}

//==============================================================================

void sdleng_ReleaseTransferState( void )
{
    sdleng_CancelTransfer();

    for( u32 iSlot = 0; iSlot < SDLENG_FRAMES_IN_FLIGHT; ++iSlot )
    {
        sdleng_transfer_slot& Slot = s_Transfer.Frame[iSlot];

        for( u32 iBuffer = 0; iBuffer < Slot.BufferCount; ++iBuffer )
        {
            if( Slot.Buffers[iBuffer].pBuffer && g_pSDLGPUDevice )
            {
                SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, Slot.Buffers[iBuffer].pBuffer );
            }

            Slot.Buffers[iBuffer].pBuffer  = NULL;
            Slot.Buffers[iBuffer].Capacity = 0;
            Slot.Buffers[iBuffer].Offset   = 0;
        }

        Slot.Buffers.Clear();
        Slot.BufferCount   = 0;
        Slot.ActiveBuffer  = -1;
        Slot.PeakUsedBytes = 0;
        Slot.ReservedBytes = 0;
        Slot.UsedBytes     = 0;
    }

    for( u32 iSlot = 0; iSlot < SDLENG_FRAMES_IN_FLIGHT; ++iSlot )
    {
        sdleng_transfer_slot& Slot = s_Transfer.Transient[iSlot];

        if( Slot.pFence && g_pSDLGPUDevice )
        {
            SDL_ReleaseGPUFence( g_pSDLGPUDevice, Slot.pFence );
        }

        Slot.pFence = NULL;

        for( u32 iBuffer = 0; iBuffer < Slot.BufferCount; ++iBuffer )
        {
            if( Slot.Buffers[iBuffer].pBuffer && g_pSDLGPUDevice )
            {
                SDL_ReleaseGPUTransferBuffer( g_pSDLGPUDevice, Slot.Buffers[iBuffer].pBuffer );
            }

            Slot.Buffers[iBuffer].pBuffer  = NULL;
            Slot.Buffers[iBuffer].Capacity = 0;
            Slot.Buffers[iBuffer].Offset   = 0;
        }

        Slot.Buffers.Clear();
        Slot.BufferCount   = 0;
        Slot.ActiveBuffer  = -1;
        Slot.PeakUsedBytes = 0;
        Slot.ReservedBytes = 0;
        Slot.UsedBytes     = 0;
    }

    s_Transfer.NextTransientSlot = 0;

    sdleng_UpdateTransferMetrics();
}

//==============================================================================
//  TRANSFER STATISTICS
//==============================================================================

void sdleng_TrimTransferState( xtick Now )
{
    sdleng_TrimTransferBuffers( 0, Now );
}

//==============================================================================

sdleng_transfer_stats sdleng_GetTransferStats( void )
{
    sdleng_transfer_stats Stats = {};
    Stats.Creates               = s_Transfer.Creates;
    Stats.Trims                 = s_Transfer.Trims;

    for( u32 i = 0; i < SDLENG_FRAMES_IN_FLIGHT; ++i )
    {
        Stats.ReservedBytes += s_Transfer.Frame[i].ReservedBytes + s_Transfer.Transient[i].ReservedBytes;
        Stats.UsedBytes += s_Transfer.Frame[i].UsedBytes + s_Transfer.Transient[i].UsedBytes;
        Stats.BufferCount += s_Transfer.Frame[i].BufferCount + s_Transfer.Transient[i].BufferCount;
    }

    return Stats;
}

//==============================================================================
//  FRAME AND SWAPCHAIN HELPERS
//==============================================================================

static
u32 sdleng_NormalizeExtent( s32 Value, u32 Fallback )
{
    return ( Value > 0 ) ? (u32)Value : Fallback;
}

//==============================================================================

static
void sdleng_ResetFrameState( void )
{
    sdleng_ResetDynamicState();
    s.pCommandBuffer        = NULL;
    s.pRenderPass           = NULL;
    s.pComputePass          = NULL;
    s.pSwapchainTexture     = NULL;
    s.AcquiredWidth         = 0;
    s.AcquiredHeight        = 0;
    s.bSwapchainAcquired    = FALSE;
    s.bBackBufferRendered   = FALSE;
    s.RenderAttachmentCount = 0;
    s.CurrentFrameSerial    = 0;
}

//==============================================================================

static
xbool sdleng_SubmitFrameCommandBuffer( SDL_GPUCommandBuffer* pCommandBuffer, const char* pSubsystem )
{
    if( s.CurrentFrameSlot >= SDLENG_FRAMES_IN_FLIGHT || s.pFrameFences[s.CurrentFrameSlot] )
    {
        sdleng_LogError( pSubsystem, "frame slot is still in flight" );
        return FALSE;
    }

    SDL_GPUFence* pFrameFence = SDL_SubmitGPUCommandBufferAndAcquireFence( pCommandBuffer );

    if( !pFrameFence )
    {
        sdleng_LogError( pSubsystem, "SDL_SubmitGPUCommandBufferAndAcquireFence" );
        return FALSE;
    }

    s.pFrameFences[s.CurrentFrameSlot] = pFrameFence;
    s.FrameSerials[s.CurrentFrameSlot] = s.CurrentFrameSerial;
    s.LastSubmittedFrameSerial         = s.CurrentFrameSerial;
    {
        static xprofile_counter FrameCommandBufferSubmissions =
            x_GetProfiler().RegisterCounter( "SDLFrameCommandBufferSubmissions", "Renderer" );

        FrameCommandBufferSubmissions.Add();
    }

    return TRUE;
}

//==============================================================================

static
void sdleng_ResetDeviceState( void )
{
    sdleng_ResetFrameState();

    s.SwapchainFormat          = SDL_GPU_TEXTUREFORMAT_INVALID;
    s.ActivePresentMode        = SDL_GPU_PRESENTMODE_VSYNC;
    s.BackBufferWidth          = 0;
    s.BackBufferHeight         = 0;
    s.FramePacingWaitMs        = 0.0f;
    s.RenderSubmitMs           = 0.0f;
    s.NextFrameSerial          = 0;
    s.CurrentFrameSerial       = 0;
    s.LastSubmittedFrameSerial = 0;
    s.CurrentFrameSlot         = 0;

    for( u32 FrameSlot = 0; FrameSlot < SDLENG_FRAMES_IN_FLIGHT; ++FrameSlot )
    {
        s.FrameSerials[FrameSlot] = 0;
        s.pFrameFences[FrameSlot] = NULL;
    }

    s.bInitialized               = FALSE;
    s.bWindowClaimed             = FALSE;
    s.bWindowInitializedByDevice = FALSE;
    s.bSwapchainAcquireLogged    = FALSE;
}

//==============================================================================

static
void sdleng_UpdateBackBufferSizeFromWindow( u32 FallbackWidth, u32 FallbackHeight )
{
    s32 ClientWidth  = 0;
    s32 ClientHeight = 0;
    sdleng_WindowGetClientSize( ClientWidth, ClientHeight );

    s.BackBufferWidth  = ( ClientWidth > 0 ) ? (u32)ClientWidth : FallbackWidth;
    s.BackBufferHeight = ( ClientHeight > 0 ) ? (u32)ClientHeight : FallbackHeight;
}

//==============================================================================

static
xbool sdleng_EnsureWindow( sdleng_native_window_handle hWindow, u32 Width, u32 Height )
{
    SDL_Window* pExistingWindow = sdleng_WindowGetSDLWindow();

    if( pExistingWindow )
    {
        if( hWindow && sdleng_WindowGetHandle() && ( sdleng_WindowGetHandle() != hWindow ) )
        {
            x_DebugMsg( "SDLEngine: existing SDL window does not match requested native window\n" );
            return FALSE;
        }

        g_pSDLWindow = pExistingWindow;
        sdleng_UpdateBackBufferSizeFromWindow( Width, Height );
        return TRUE;
    }

    sdleng_window_desc WindowDesc;
    x_memset( &WindowDesc, 0, sizeof( WindowDesc ) );
    WindowDesc.hWindow     = hWindow;
    WindowDesc.Width       = Width;
    WindowDesc.Height      = Height;
    WindowDesc.pTitle      = "Dreamlnd 51";
    WindowDesc.DisplayMode = sdleng_WindowGetDisplayMode();

    if( !sdleng_WindowInit( WindowDesc ) )
    {
        return FALSE;
    }

    g_pSDLWindow                 = sdleng_WindowGetSDLWindow();
    s.bWindowInitializedByDevice = TRUE;

    sdleng_UpdateBackBufferSizeFromWindow( Width, Height );

    return g_pSDLWindow != NULL;
}

//==============================================================================

static
xbool sdleng_ToPresentMode( sdleng_present_policy Policy, SDL_GPUPresentMode& PresentMode )
{
    switch( Policy )
    {
        case SDLENG_PRESENT_IMMEDIATE:
        {
            PresentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
            return TRUE;
        }

        case SDLENG_PRESENT_MAILBOX:
        {
            PresentMode = SDL_GPU_PRESENTMODE_MAILBOX;
            return TRUE;
        }

        case SDLENG_PRESENT_VSYNC:
        {
            PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
            return TRUE;
        }

        default:
        {
            x_DebugMsg( "SDLEngine: invalid present policy %d\n", (s32)Policy );
            return FALSE;
        }
    }
}

//==============================================================================

static
const char* sdleng_GetPresentModeName( SDL_GPUPresentMode Mode )
{
    switch( Mode )
    {
        case SDL_GPU_PRESENTMODE_IMMEDIATE: return "immediate"; break;
        case SDL_GPU_PRESENTMODE_MAILBOX:   return "mailbox";   break;
        case SDL_GPU_PRESENTMODE_VSYNC:     return "vsync";     break;
        default:                            return "unknown";   break;
    }
}

//==============================================================================

static
xbool sdleng_ConfigureSwapchain( void )
{
    if( !g_pSDLGPUDevice || !g_pSDLWindow )
    {
        return FALSE;
    }

    if( !SDL_WindowSupportsGPUSwapchainComposition( g_pSDLGPUDevice,
                                                    g_pSDLWindow,
                                                    SDLENG_GPU_SWAPCHAIN_COMPOSITION ) )
    {
        x_DebugMsg( "SDLEngine: requested swapchain composition is unsupported\n" );
        return FALSE;
    }

    SDL_GPUPresentMode PresentMode;

    if( !sdleng_ToPresentMode( s.PresentPolicy, PresentMode ) )
    {
        return FALSE;
    }

    if( !SDL_WindowSupportsGPUPresentMode( g_pSDLGPUDevice,
                                           g_pSDLWindow,
                                           PresentMode ) )
    {
        x_DebugMsg( "SDLEngine: requested present mode '%s' is unsupported\n",
                    sdleng_GetPresentModeName( PresentMode ) );

        return FALSE;
    }

    if( !SDL_SetGPUSwapchainParameters( g_pSDLGPUDevice,
                                        g_pSDLWindow,
                                        SDLENG_GPU_SWAPCHAIN_COMPOSITION,
                                        PresentMode ) )
    {
        sdleng_LogError( "SDLEngine", "SDL_SetGPUSwapchainParameters" );
        return FALSE;
    }

    s.ActivePresentMode       = PresentMode;
    s.bSwapchainAcquireLogged = FALSE;

    s.SwapchainFormat = SDL_GetGPUSwapchainTextureFormat( g_pSDLGPUDevice, g_pSDLWindow );

    if( s.SwapchainFormat == SDL_GPU_TEXTUREFORMAT_INVALID )
    {
        x_DebugMsg( "SDLEngine: invalid swapchain texture format\n" );
        return FALSE;
    }

    sdleng_UpdateBackBufferSizeFromWindow( s.BackBufferWidth ? s.BackBufferWidth : SDLENG_DEFAULT_BACKBUFFER_WIDTH,
                                           s.BackBufferHeight ? s.BackBufferHeight : SDLENG_DEFAULT_BACKBUFFER_HEIGHT );

    x_DebugMsg( "SDLEngine: swapchain present=%s frames-in-flight=%u size=%ux%u\n",
                sdleng_GetPresentModeName( PresentMode ),
                (u32)SDLENG_FRAMES_IN_FLIGHT,
                s.BackBufferWidth,
                s.BackBufferHeight );

    return TRUE;
}

//==============================================================================
//  PRESENTATION POLICY
//==============================================================================

xbool sdleng_SetPresentPolicy( sdleng_present_policy Policy )
{
    if( ( Policy < SDLENG_PRESENT_VSYNC ) || ( Policy > SDLENG_PRESENT_IMMEDIATE ) )
    {
        return FALSE;
    }

    if( s.PresentPolicy == Policy )
    {
        return TRUE;
    }

    if( s.pCommandBuffer || s.pRenderPass || s.pComputePass || s.bSwapchainAcquired )
    {
        x_DebugMsg( "SDLEngine: present mode cannot change during an active frame\n" );
        return FALSE;
    }

    const sdleng_present_policy PreviousPolicy = s.PresentPolicy;
    s.PresentPolicy                            = Policy;

    if( s.bWindowClaimed && !sdleng_ConfigureSwapchain() )
    {
        s.PresentPolicy = PreviousPolicy;
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool sdleng_IsPresentPolicySupported( sdleng_present_policy Policy )
{
    if( !g_pSDLGPUDevice || !g_pSDLWindow || !s.bWindowClaimed )
    {
        return FALSE;
    }

    SDL_GPUPresentMode PresentMode;

    if( !sdleng_ToPresentMode( Policy, PresentMode ) )
    {
        return FALSE;
    }

    return SDL_WindowSupportsGPUPresentMode( g_pSDLGPUDevice,
                                             g_pSDLWindow,
                                             PresentMode );
}

//==============================================================================

sdleng_present_policy sdleng_GetPresentPolicy( void )
{
    if( s.bWindowClaimed )
    {
        switch( s.ActivePresentMode )
        {
            case SDL_GPU_PRESENTMODE_IMMEDIATE:
            {
                return SDLENG_PRESENT_IMMEDIATE;
            }

            break;

            case SDL_GPU_PRESENTMODE_MAILBOX:
            {
                return SDLENG_PRESENT_MAILBOX;
            }

            break;

            case SDL_GPU_PRESENTMODE_VSYNC:
            default:
            {
                return SDLENG_PRESENT_VSYNC;
            }

            break;
        }
    }

    return s.PresentPolicy;
}

//==============================================================================

static
xbool sdleng_SubmitCurrentCommandBuffer( void )
{
    if( !s.pCommandBuffer )
    {
        return TRUE;
    }

    if( s.pRenderPass || s.pComputePass )
    {
        x_DebugMsg( "SDLEngine: cannot submit with an active GPU pass\n" );
        return FALSE;
    }

    if( !s.bSwapchainAcquired || !s.bBackBufferRendered )
    {
        x_DebugMsg( "SDLEngine: frame submit requires an acquired and rendered backbuffer\n" );
        return FALSE;
    }

    if( !sdleng_FlushTransferCommands() )
    {
        return FALSE;
    }

    SDL_GPUCommandBuffer* pCommandBuffer = s.pCommandBuffer;
    const xtick           SubmitStart    = x_GetTime();
    static xprofile_gauge RenderEncodingMetric =
        x_GetProfiler().RegisterGauge( "RenderEncodingMs", XPROFILE_UNIT_MILLISECONDS, "Frame" );

    RenderEncodingMetric.Set( s.RenderStartTime ? x_TicksToMs( SubmitStart - s.RenderStartTime ) : 0.0f );

    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "Submit" );

        if( !sdleng_SubmitFrameCommandBuffer( pCommandBuffer, "SDLEngine" ) )
        {
            sdleng_ResetFrameState();
            return FALSE;
        }
    }

    sdleng_ResetFrameState();

    s.RenderSubmitMs = x_TicksToMs( x_GetTime() - SubmitStart );

    return TRUE;
}

//==============================================================================
//  BACKEND LIFETIME
//==============================================================================

xbool sdleng_CreateDeviceForWindow( sdleng_native_window_handle hWindow, s32 Width, s32 Height )
{
    if( s.bInitialized )
    {
        if( !hWindow || ( sdleng_WindowGetHandle() == hWindow ) )
        {
            return TRUE;
        }

        x_DebugMsg( "SDLEngine: device already initialized for another window\n" );
        return FALSE;
    }

    const u32 RequestedWidth  = sdleng_NormalizeExtent( Width, SDLENG_DEFAULT_BACKBUFFER_WIDTH );
    const u32 RequestedHeight = sdleng_NormalizeExtent( Height, SDLENG_DEFAULT_BACKBUFFER_HEIGHT );

    if( !sdleng_EnsureWindow( hWindow, RequestedWidth, RequestedHeight ) )
    {
        sdleng_DestroyDevice();
        return FALSE;
    }

#if defined(CONFIG_DEBUG) || defined(X_DEBUG)
    const bool DebugMode = true;
#else
    const bool DebugMode = false;
#endif

    SDL_PropertiesID const DeviceProperties = SDL_CreateProperties();

    if( !DeviceProperties )
    {
        return FALSE;
    }

    SDL_SetBooleanProperty( DeviceProperties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, ( SDLENG_GPU_SHADER_FORMATS & SDL_GPU_SHADERFORMAT_SPIRV ) != 0 );

    SDL_SetBooleanProperty( DeviceProperties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, ( SDLENG_GPU_SHADER_FORMATS & SDL_GPU_SHADERFORMAT_DXIL ) != 0 );

    SDL_SetBooleanProperty( DeviceProperties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, DebugMode );
    SDL_SetBooleanProperty( DeviceProperties, SDL_PROP_GPU_DEVICE_CREATE_PREFER_FLOAT16_BOOLEAN, true );
    SDL_SetStringProperty( DeviceProperties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, SDLENG_GPU_DRIVER_NAME );
    usize PipelineCacheSize = 0;
    void* pPipelineCache    = sdleng_LoadPipelineCache( PipelineCacheSize );

    if( pPipelineCache )
    {
        SDL_SetPointerProperty( DeviceProperties, SDL_PROP_GPU_DEVICE_CREATE_PIPELINE_CACHE_DATA_POINTER, pPipelineCache );
        SDL_SetNumberProperty( DeviceProperties, SDL_PROP_GPU_DEVICE_CREATE_PIPELINE_CACHE_SIZE_NUMBER, static_cast<Sint64>( PipelineCacheSize ) );
    }

    g_pSDLGPUDevice = SDL_CreateGPUDeviceWithProperties( DeviceProperties );
    x_free( pPipelineCache );
    SDL_DestroyProperties( DeviceProperties );

    if( !g_pSDLGPUDevice )
    {
        sdleng_LogError( "SDLEngine", "SDL_CreateGPUDevice" );
        sdleng_DestroyDevice();
        return FALSE;
    }

    if( !sdleng_InitializeFormatPolicy() )
    {
        sdleng_DestroyDevice();
        return FALSE;
    }

    if( !SDL_ClaimWindowForGPUDevice( g_pSDLGPUDevice, g_pSDLWindow ) )
    {
        sdleng_LogError( "SDLEngine", "SDL_ClaimWindowForGPUDevice" );
        sdleng_DestroyDevice();
        return FALSE;
    }

    s.bWindowClaimed = TRUE;

    if( !SDL_SetGPUAllowedFramesInFlight( g_pSDLGPUDevice, SDLENG_FRAMES_IN_FLIGHT ) )
    {
        sdleng_LogError( "SDLEngine", "SDL_SetGPUAllowedFramesInFlight" );
        sdleng_DestroyDevice();
        return FALSE;
    }

    if( !sdleng_ConfigureSwapchain() )
    {
        sdleng_DestroyDevice();
        return FALSE;
    }

    s.bInitialized = TRUE;

    x_GetProfiler().SetProperty( "Renderer.Driver", SDL_GetGPUDeviceDriver( g_pSDLGPUDevice ) );
    x_DebugMsg( "SDLEngine: GPU device initialized using driver '%s' shader formats 0x%08X\n",
                SDL_GetGPUDeviceDriver( g_pSDLGPUDevice ),
                SDL_GetGPUShaderFormats( g_pSDLGPUDevice ) );

    return TRUE;
}

//==============================================================================

void sdleng_DestroyDevice( void )
{
    const xbool bDestroyWindow = s.bWindowInitializedByDevice;

    sdleng_EndComputePass();

    if( s.pRenderPass )
    {
        x_DebugMsg( "SDLEngine: closing render pass during device teardown\n" );
        sdleng_EndRenderPass();
    }

    sdleng_CancelFrame();
    const xbool bGPUIdle = sdleng_WaitForIdle();

    if( !bGPUIdle && g_pSDLGPUDevice )
    {
        for( u32 FrameSlot = 0; FrameSlot < SDLENG_FRAMES_IN_FLIGHT; ++FrameSlot )
        {
            if( s.pFrameFences[FrameSlot] )
            {
                SDL_ReleaseGPUFence( g_pSDLGPUDevice, s.pFrameFences[FrameSlot] );
            }

            s.pFrameFences[FrameSlot] = NULL;
            s.FrameSerials[FrameSlot] = 0;
        }
    }

    sdleng_ReleaseTransferState();

    if( s.bWindowClaimed && g_pSDLGPUDevice && g_pSDLWindow )
    {
        SDL_ReleaseWindowFromGPUDevice( g_pSDLGPUDevice, g_pSDLWindow );
        s.bWindowClaimed = FALSE;
    }

    if( g_pSDLGPUDevice )
    {
        sdleng_ResetFormatPolicy();
        eng_SavePipelineCache();
        SDL_DestroyGPUDevice( g_pSDLGPUDevice );
        g_pSDLGPUDevice = NULL;
    }

    if( bDestroyWindow )
    {
        sdleng_WindowKill();
    }

    g_pSDLWindow = sdleng_WindowGetSDLWindow();
    sdleng_ResetDeviceState();
}

//==============================================================================

xbool sdleng_WaitForIdle( void )
{
    if( !g_pSDLGPUDevice )
    {
        return TRUE;
    }

    if( s.pCommandBuffer || s.pRenderPass || s.pComputePass )
    {
        x_DebugMsg( "SDLEngine: GPU idle wait is not valid during an active frame\n" );
        return FALSE;
    }

    if( !sdleng_FlushTransferCommands() )
    {
        return FALSE;
    }

    if( !SDL_WaitForGPUIdle( g_pSDLGPUDevice ) )
    {
        sdleng_LogError( "SDLEngine", "SDL_WaitForGPUIdle" );
        return FALSE;
    }

    for( u32 iSlot = 0; iSlot < SDLENG_FRAMES_IN_FLIGHT; ++iSlot )
    {
        if( !sdleng_WaitForTransferSlot( s_Transfer.Transient[iSlot] ) )
        {
            return FALSE;
        }
    }

    return sdleng_CollectFrameFences( TRUE, "SDLEngine" );
}

//==============================================================================

u32 sdleng_GetCurrentFrameSlot( void )
{
    return s.CurrentFrameSlot;
}

//==============================================================================

u64 sdleng_GetCurrentFrameSerial( void )
{
    return s.CurrentFrameSerial;
}

//==============================================================================

u64 sdleng_GetFrameLifetimeSerial( void )
{
    return s.CurrentFrameSerial ? s.CurrentFrameSerial : s.LastSubmittedFrameSerial;
}

//==============================================================================

u32 sdleng_GetFramesInFlight( void )
{
    return SDLENG_FRAMES_IN_FLIGHT;
}

//==============================================================================

xbool sdleng_IsFrameComplete( u64 FrameSerial )
{
    if( FrameSerial == 0 )
    {
        return TRUE;
    }

    if( FrameSerial == s.CurrentFrameSerial && s.CurrentFrameSerial != 0 )
    {
        return FALSE;
    }

    for( u32 FrameSlot = 0; FrameSlot < SDLENG_FRAMES_IN_FLIGHT; ++FrameSlot )
    {
        if( s.FrameSerials[FrameSlot] == FrameSerial )
        {
            return sdleng_CollectFrameFence( FrameSlot, FALSE, "SDLEngine" );
        }
    }

    return TRUE;
}

//==============================================================================
//  FRAME AND PASS LIFETIME
//==============================================================================

xbool sdleng_AcquireCommandBuffer( void )
{
    if( !s.bInitialized || !g_pSDLGPUDevice )
    {
        return FALSE;
    }

    if( s.pCommandBuffer )
    {
        return TRUE;
    }

    u64 FrameSerial = s.NextFrameSerial + 1;

    if( FrameSerial == 0 )
    {
        FrameSerial = 1;
    }

    const u32 FrameSlot = (u32)( ( FrameSerial - 1 ) % SDLENG_FRAMES_IN_FLIGHT );
    s.FramePacingWaitMs = 0.0f;

    if( !sdleng_CollectFrameFence( FrameSlot, TRUE, "SDLEngine" ) )
    {
        return FALSE;
    }

    sdleng_ResetTransferSlot( s_Transfer.Frame[FrameSlot] );
    sdleng_TrimTransferState( x_GetTime() );
    sdleng_CollectPipelineWork();

    if( !sdleng_FlushTransferCommands() )
    {
        return FALSE;
    }

    s.NextFrameSerial    = FrameSerial;
    s.CurrentFrameSerial = s.NextFrameSerial;
    s.CurrentFrameSlot   = FrameSlot;
    {
        X_PROFILE_SCOPE_CATEGORY( "Renderer", "AcquireCommandBuffer" );
        s.pCommandBuffer = SDL_AcquireGPUCommandBuffer( g_pSDLGPUDevice );
    }

    if( !s.pCommandBuffer )
    {
        sdleng_LogError( "SDLEngine", "SDL_AcquireGPUCommandBuffer" );
        sdleng_ResetFrameState();
        return FALSE;
    }

    sdleng_UpdateTransferMetrics();
    sdleng_ResetShaderUniforms();
    s.RenderStartTime = 0;

    return TRUE;
}

//==============================================================================

xbool sdleng_AcquireSwapchainTexture( void )
{
    if( !s.bInitialized || !g_pSDLGPUDevice || !g_pSDLWindow )
    {
        return FALSE;
    }

    if( s.pRenderPass || s.pComputePass )
    {
        return FALSE;
    }

    if( !sdleng_FlushTransferCommands() )
    {
        return FALSE;
    }

    if( s.bSwapchainAcquired )
    {
        return TRUE;
    }

    if( !s.pCommandBuffer )
    {
        x_DebugMsg( "SDLEngine: swapchain acquire requires an active command buffer\n" );
        return FALSE;
    }

    const xtick AcquireStart = x_GetTime();
    xbool       bAcquired;
    {
        X_PROFILE_SCOPE_CATEGORY( "Frame", "Frame.WaitForSwapchain" );
        bAcquired = SDL_WaitAndAcquireGPUSwapchainTexture( s.pCommandBuffer,
                                                           g_pSDLWindow,
                                                           &s.pSwapchainTexture,
                                                           &s.AcquiredWidth,
                                                           &s.AcquiredHeight );
    }

    s.FramePacingWaitMs += x_TicksToMs( x_GetTime() - AcquireStart );

    if( !bAcquired )
    {
        sdleng_LogError( "SDLEngine", "SDL_WaitAndAcquireGPUSwapchainTexture" );
        return FALSE;
    }

    if( !s.pSwapchainTexture )
    {
        return TRUE;
    }

    s.bSwapchainAcquired = TRUE;

    if( s.AcquiredWidth && s.AcquiredHeight )
    {
        s.BackBufferWidth  = s.AcquiredWidth;
        s.BackBufferHeight = s.AcquiredHeight;
    }

    if( !s.bSwapchainAcquireLogged )
    {
        x_DebugMsg( "SDLEngine: swapchain acquire=%1.3fms size=%ux%u\n",
                    s.FramePacingWaitMs,
                    s.BackBufferWidth,
                    s.BackBufferHeight );

        s.bSwapchainAcquireLogged = TRUE;
    }

    return TRUE;
}

//==============================================================================

xbool sdleng_BeginRenderPass( const SDL_GPUColorTargetInfo*        pColorTargets,
                              u32                                  ColorTargetCount,
                              const SDL_GPUDepthStencilTargetInfo* pDepthStencilTarget )
{
    if( !s.pCommandBuffer )
    {
        x_DebugMsg( "SDLEngine: render pass requires an active frame lifecycle\n" );
        return FALSE;
    }

    if( s.pRenderPass || s.pComputePass )
    {
        return FALSE;
    }

    if( ColorTargetCount && !pColorTargets )
    {
        return FALSE;
    }

    if( ColorTargetCount > RTARGET_MAX_TARGETS )
    {
        return FALSE;
    }

    if( !ColorTargetCount && !pDepthStencilTarget )
    {
        return FALSE;
    }

    if( !sdleng_FlushTransferCommands() )
    {
        return FALSE;
    }

    static xprofile_counter RenderPassCountMetric =
        x_GetProfiler().RegisterCounter( "RenderPassCalls", "Renderer" );

    RenderPassCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick RenderStart = x_GetTime();
    const xtick BeginStart  = bFineTiming ? RenderStart : 0;
    s.pRenderPass           = SDL_BeginGPURenderPass( s.pCommandBuffer,
                                            pColorTargets,
                                            ColorTargetCount,
                                            pDepthStencilTarget );

    const xtick BeginEnd = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone RenderPassMetric =
            x_GetProfiler().RegisterZone( "RenderPassAPI", "RendererAPI" );

        RenderPassMetric.Record( BeginEnd - BeginStart );
    }

    if( !s.pRenderPass )
    {
        sdleng_LogError( "SDLEngine", "SDL_BeginGPURenderPass" );
        return FALSE;
    }

    s.RenderAttachmentCount = 0;

    for( u32 iTarget = 0; iTarget < ColorTargetCount; ++iTarget )
    {
        if( pColorTargets[iTarget].texture )
        {
            s.pRenderAttachments[s.RenderAttachmentCount++] = pColorTargets[iTarget].texture;
        }
    }

    if( pDepthStencilTarget && pDepthStencilTarget->texture )
    {
        s.pRenderAttachments[s.RenderAttachmentCount++] = pDepthStencilTarget->texture;
    }

    if( s.RenderStartTime == 0 )
    {
        s.RenderStartTime = RenderStart;
    }

    for( u32 iTarget = 0; iTarget < ColorTargetCount; iTarget++ )
    {
        if( pColorTargets[iTarget].texture == s.pSwapchainTexture )
        {
            s.bBackBufferRendered = TRUE;
            break;
        }
    }

    sdleng_ResetDynamicState();
    sdleng_ResetPipelineBinding();
    sdleng_ResetBufferBindings();
    sdleng_ResetShaderBindings();
    sdleng_ResetGraphicsBindingDebug();

    return TRUE;
}

//==============================================================================

void sdleng_EndRenderPass( void )
{
    if( !s.pRenderPass )
    {
        return;
    }

    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick EndStart    = bFineTiming ? x_GetTime() : 0;
    SDL_EndGPURenderPass( s.pRenderPass );
    const xtick EndEnd = bFineTiming ? x_GetTime() : 0;

    if( bFineTiming )
    {
        static xprofile_zone RenderPassMetric =
            x_GetProfiler().RegisterZone( "RenderPassAPI", "RendererAPI" );

        RenderPassMetric.Record( EndEnd - EndStart );
    }

    s.pRenderPass           = NULL;
    s.RenderAttachmentCount = 0;
    sdleng_ResetDynamicState();
    sdleng_ResetPipelineBinding();
    sdleng_ResetBufferBindings();
    sdleng_ResetShaderBindings();
    sdleng_ResetGraphicsBindingDebug();
}

//==============================================================================

xbool sdleng_BeginComputePass( const SDL_GPUStorageTextureReadWriteBinding* pTextures,
                               u32                                          TextureCount,
                               const SDL_GPUStorageBufferReadWriteBinding*  pBuffers,
                               u32                                          BufferCount )
{
    if( !s.pCommandBuffer || s.pRenderPass || s.pComputePass ||
        ( TextureCount && !pTextures ) || ( BufferCount && !pBuffers ) )
    {
        return FALSE;
    }

    if( !sdleng_FlushTransferCommands() )
    {
        return FALSE;
    }

    s.pComputePass = SDL_BeginGPUComputePass( s.pCommandBuffer, pTextures, TextureCount, pBuffers, BufferCount );

    if( !s.pComputePass )
    {
        sdleng_LogError( "SDLShader", "SDL_BeginGPUComputePass" );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void sdleng_EndComputePass( void )
{
    if( s.pComputePass )
    {
        SDL_EndGPUComputePass( s.pComputePass );
        s.pComputePass = NULL;
    }
}

//==============================================================================

xbool sdleng_InComputePass( void )
{
    return s.pComputePass != NULL;
}

//==============================================================================

SDL_GPUComputePass* sdleng_GetComputePass( void )
{
    return s.pComputePass;
}

//==============================================================================

xbool sdleng_EndFrame( void )
{
    sdleng_UpdateTransferMetrics();

    return sdleng_SubmitCurrentCommandBuffer();
}

//==============================================================================

void sdleng_CancelFrame( void )
{
    if( !s.pCommandBuffer )
    {
        return;
    }

    sdleng_EndComputePass();

    if( s.pRenderPass )
    {
        x_DebugMsg( "SDLEngine: cannot cancel a command buffer with an active render pass\n" );
        ASSERT( FALSE );
        return;
    }

    SDL_GPUCommandBuffer* pCommandBuffer     = s.pCommandBuffer;
    const xbool           bSwapchainAcquired = s.bSwapchainAcquired;

    if( bSwapchainAcquired )
    {
        if( !sdleng_FlushTransferCommands() )
        {
            sdleng_CancelTransfer();
        }

        if( !sdleng_SubmitFrameCommandBuffer( pCommandBuffer, "SDLEngine" ) )
        {
            sdleng_ResetFrameState();
            return;
        }

        sdleng_ResetFrameState();
    }
    else
    {
        sdleng_CancelTransfer();
        sdleng_ResetFrameState();

        if( !SDL_CancelGPUCommandBuffer( pCommandBuffer ) )
        {
            sdleng_LogError( "SDLEngine", "SDL_CancelGPUCommandBuffer" );
        }
    }
}

//==============================================================================

xbool sdleng_InFrame( void )
{
    return s.pCommandBuffer != NULL;
}

//==============================================================================

xbool sdleng_InRenderPass( void )
{
    return s.pRenderPass != NULL;
}

//==============================================================================

xbool sdleng_WasBackBufferRendered( void )
{
    return s.bBackBufferRendered;
}

//==============================================================================
//  ACCESSORS
//==============================================================================

xbool sdleng_IsCurrentRenderAttachment( SDL_GPUTexture* pTexture )
{
    if( !pTexture )
    {
        return FALSE;
    }

    for( u32 iAttachment = 0; iAttachment < s.RenderAttachmentCount; ++iAttachment )
    {
        if( s.pRenderAttachments[iAttachment] == pTexture )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

SDL_GPUDevice* sdleng_GetDevice( void )
{
    return g_pSDLGPUDevice;
}

//==============================================================================

SDL_Window* sdleng_GetWindow( void )
{
    return g_pSDLWindow;
}

//==============================================================================

SDL_GPUCommandBuffer* sdleng_GetCommandBuffer( void )
{
    return s.pCommandBuffer;
}

//==============================================================================

SDL_GPURenderPass* sdleng_GetRenderPass( void )
{
    return s.pRenderPass;
}

//==============================================================================

SDL_GPUTexture* sdleng_GetSwapchainTexture( void )
{
    return s.pSwapchainTexture;
}

//==============================================================================

SDL_GPUTextureFormat sdleng_GetSwapchainFormat( void )
{
    return s.SwapchainFormat;
}

//==============================================================================

f32 sdleng_GetFramePacingWaitMs( void )
{
    return s.FramePacingWaitMs;
}

//==============================================================================

f32 sdleng_GetRenderSubmitMs( void )
{
    return s.RenderSubmitMs;
}

//==============================================================================

void sdleng_GetBackBufferSize( s32& Width, s32& Height )
{
    if( !s.bSwapchainAcquired )
    {
        sdleng_UpdateBackBufferSizeFromWindow( s.BackBufferWidth ? s.BackBufferWidth : SDLENG_DEFAULT_BACKBUFFER_WIDTH,
                                               s.BackBufferHeight ? s.BackBufferHeight : SDLENG_DEFAULT_BACKBUFFER_HEIGHT );
    }

    Width  = (s32)s.BackBufferWidth;
    Height = (s32)s.BackBufferHeight;
}

//==============================================================================
#endif // (defined(TARGET_DESKTOP) || defined(TARGET_MOBILE)) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
