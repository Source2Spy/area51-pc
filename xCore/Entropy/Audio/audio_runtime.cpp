//==============================================================================
//
//  audio_runtime.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Audio.hpp"
#include "Audio/audio_runtime.hpp"
#include "IOManager/io_filesystem.hpp"
#include "IOManager/io_mgr.hpp"

//==============================================================================

audio_runtime::audio_runtime( audio_mgr& AudioRef ) :
    Audio( AudioRef ),
    Backend(),
    Channels(),
    Voices(),
    Streams(),
    Decoders( g_IOFSMgr ),
    StreamRuntime( g_IOFSMgr, g_IoMgr ),
    Packages(),
    Descriptors(),
    Spatial(),
    Commands(),
    Service( HNULL )
{
    Time            = 0.0f;
    AudioDuckLevel  = 0;
    ServiceRunning  = FALSE;
    ServiceThreadId = -1;
}

//==============================================================================

audio_runtime::~audio_runtime( void )
{
}
