//==============================================================================
//
//  MoviePlayer_WebM_Container.cpp
//
//  Container handling for WebM playback.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

// Let it be only for PC, for now...

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "x_memory.hpp"
#include "ResourceMgr/ResourceMgr.hpp"

#include "MoviePlayer_WebM_Private.hpp"

using namespace movie_webm;

//==============================================================================
// FILE READER IMPLEMENTATION
//==============================================================================

file_reader::file_reader(void)
{
    m_pFile  = NULL;
    m_Length = 0;
}

//==============================================================================

file_reader::~file_reader(void)
{
    Close();
}

//==============================================================================

xbool file_reader::Open(const char* pFilename)
{
    Close();

    if (!pFilename || !pFilename[0])
        return FALSE;

    xstring Path = pFilename;

    if (Path.Find('.') == -1)
    {
        Path += ".webm";
    }

    m_pFile = x_fopen(Path, "rb");

    if (!m_pFile)
        return FALSE;

    m_Length = x_flength(m_pFile);
    return TRUE;
}

//==============================================================================

void file_reader::Close(void)
{
    if (m_pFile)
    {
        x_fclose(m_pFile);
        m_pFile = NULL;
    }

    m_Length = 0;
}

//==============================================================================

int file_reader::Read(long long position, long length, unsigned char* buffer)
{
    if (!m_pFile || !buffer || (length <= 0))
        return -1;
    
    if (position < 0)
        return -1;
    
    if (position > 0x7FFFFFFF)
        return -1;
    
    if (length > 0x7FFFFFFF)
        return -1;
    
    if (x_fseek(m_pFile, (s32)position, X_SEEK_SET) != 0)
        return -1;
    
    const s32 len32 = (s32)length;
    const s32 bytesRead = x_fread(buffer, 1, len32, m_pFile);
    return (bytesRead == len32) ? 0 : -1;
}

//==============================================================================

int file_reader::Length(long long* total, long long* available)
{
    if (total)
        *total = m_Length;

    if (available)
        *available = m_Length;

    return 0;
}

//==============================================================================
// CONTAINER IMPLEMENTATION
//==============================================================================

container::container(void)
{
    m_pSegment   = NULL;
    m_pTracks    = NULL;
    m_pCluster   = NULL;
    m_pEntry     = NULL;
    m_TimeScale  = WEBM_NANOSEC;
    m_Duration   = 0.0;
    m_VideoTrack = -1;
    m_AudioTrack = -1;
    m_bEOF       = FALSE;
    m_bHasPendingSample = FALSE;
}

//==============================================================================

container::~container(void)
{
    Close();
}

//==============================================================================

xbool container::Open(const char* pFilename, player_config& Config)
{
    if (!m_FileReader.Open(pFilename))
        return FALSE;

    if (!InitializeSegment(Config))
    {
        Close();
        return FALSE;
    }

    if (!SelectTracks(Config))
    {
        Close();
        return FALSE;
    }

    m_bEOF              = FALSE;
    m_bHasPendingSample = FALSE;
    m_pCluster          = NULL;
    m_pEntry            = NULL;

    return TRUE;
}

//==============================================================================

void container::Close(void)
{
    if (m_pSegment)
    {
        delete m_pSegment;
        m_pSegment = NULL;
    }

    m_pTracks           = NULL;
    m_pCluster          = NULL;
    m_pEntry            = NULL;
    m_VideoTrack        = -1;
    m_AudioTrack        = -1;
    m_TimeScale         = WEBM_NANOSEC;
    m_Duration          = 0.0;
    m_bEOF              = FALSE;
    m_bHasPendingSample = FALSE;

    m_FileReader.Close();
}

//==============================================================================

void container::Rewind(void)
{
    if (!m_pSegment)
        return;

    m_pCluster          = m_pSegment->GetFirst();
    m_pEntry            = NULL;
    m_bEOF              = FALSE;
    m_bHasPendingSample = FALSE;
}

//==============================================================================

xbool container::PeekSample(sample& OutSample)
{
    if (m_bHasPendingSample)
    {
        OutSample = m_PendingSample;
        return TRUE;
    }

    if (!AcquireSample(m_PendingSample))
        return FALSE;

    m_bHasPendingSample = TRUE;
    OutSample = m_PendingSample;
    return TRUE;
}

//==============================================================================

xbool container::ReadSample(sample& OutSample)
{
    if (m_bHasPendingSample)
    {
        OutSample = m_PendingSample;
        m_bHasPendingSample = FALSE;
        return TRUE;
    }

    return AcquireSample(OutSample);
}

//==============================================================================

xbool container::AcquireSample(sample& OutSample)
{
    if (m_bEOF || !m_pSegment)
        return FALSE;

    s32 skippedBlocks = 0;

    while (AdvanceEntry())
    {
        const mkvparser::Block* pBlock = m_pEntry->GetBlock();
        if (!pBlock)
            continue;

        const s32 trackNumber = (s32)pBlock->GetTrackNumber();

        if ((trackNumber != m_VideoTrack) && (trackNumber != m_AudioTrack))
        {
            ++skippedBlocks;
            if (skippedBlocks > 1024) // 1024 Max skipped blocks
            {
                x_DebugMsg("MoviePlayer_WebM: Skipped too many blocks, stopping\n");
                m_bEOF = TRUE;
                return FALSE;
            }
            continue;
        }

        OutSample.pEntry      = m_pEntry;
        OutSample.pBlock      = pBlock;
        OutSample.pCluster    = m_pCluster;
        OutSample.TimeSeconds = (f64)pBlock->GetTime(m_pCluster) / WEBM_NANOSEC;
        OutSample.IsKeyFrame  = pBlock->IsKey();
        OutSample.Type        = (trackNumber == m_VideoTrack) ? STREAM_TYPE_VIDEO : STREAM_TYPE_AUDIO;
        return TRUE;
    }

    m_bEOF = TRUE;
    return FALSE;
}

//==============================================================================

xbool container::InitializeSegment(player_config& Config)
{
    long long pos = 0;
    long long status = mkvparser::Segment::CreateInstance(&m_FileReader, pos, m_pSegment);
    if (status < 0 || !m_pSegment)
        return FALSE;

    status = m_pSegment->Load();
    if (status < 0)
        return FALSE;

    const mkvparser::SegmentInfo* pInfo = m_pSegment->GetInfo();
    if (!pInfo)
        return FALSE;

    m_TimeScale = (f64)pInfo->GetTimeCodeScale();
    if (m_TimeScale <= 0.0)
    {
        m_TimeScale = WEBM_NANOSEC;
    }

    m_Duration = pInfo->GetDuration();
    Config.TimecodeScale = m_TimeScale;
    Config.Duration      = (m_Duration > 0.0) ? (m_Duration / WEBM_NANOSEC) : 0.0;

    m_pTracks = m_pSegment->GetTracks();
    return (m_pTracks != NULL);
}

//==============================================================================

xbool container::SelectTracks(player_config& Config)
{
    if (!m_pTracks)
        return FALSE;

    const unsigned long trackCount = m_pTracks->GetTracksCount();

    for (unsigned long i = 0; i < trackCount; ++i)
    {
        const mkvparser::Track* pTrack = m_pTracks->GetTrackByIndex(i);
        if (!pTrack)
            continue;

        if ((pTrack->GetType() == mkvparser::Track::kVideo) && (m_VideoTrack < 0))
        {
            m_VideoTrack      = (s32)pTrack->GetNumber();
            Config.VideoTrack = m_VideoTrack;
            Config.VideoCodecId = pTrack->GetCodecId();

            const mkvparser::VideoTrack* pVideo = static_cast<const mkvparser::VideoTrack*>(pTrack);
            Config.Width  = (s32)pVideo->GetWidth();
            Config.Height = (s32)pVideo->GetHeight();
            Config.FrameRate = (f32)pVideo->GetFrameRate();
        }
        else if ((pTrack->GetType() == mkvparser::Track::kAudio) && (m_AudioTrack < 0))
        {
            m_AudioTrack      = (s32)pTrack->GetNumber();
            Config.AudioTrack = m_AudioTrack;
            Config.HasAudio   = TRUE;
            Config.AudioCodecId = pTrack->GetCodecId();

            const mkvparser::AudioTrack* pAudio = static_cast<const mkvparser::AudioTrack*>(pTrack);
            Config.AudioChannels   = (s32)pAudio->GetChannels();
            Config.AudioSampleRate = (s32)pAudio->GetSamplingRate();
            Config.AudioBitDepth   = (s32)pAudio->GetBitDepth();

            if (Config.AudioChannels <= 0)
                Config.AudioChannels = 2;

            if (Config.AudioSampleRate <= 0)
                Config.AudioSampleRate = 48000;

            if (Config.AudioBitDepth <= 0)
                Config.AudioBitDepth = 16;

            Config.AudioCodecPrivate.Clear();

            size_t privateSize = 0;
            const unsigned char* pPrivate = pTrack->GetCodecPrivate(privateSize);
            if (pPrivate && (privateSize > 0) && (privateSize <= INT_MAX))
            {
                Config.AudioCodecPrivate.SetCount((s32)privateSize);
                x_memcpy(Config.AudioCodecPrivate.GetPtr(), pPrivate, (s32)privateSize);
            }
        }
    }

    return (m_VideoTrack >= 0);
}

//==============================================================================

xbool container::AdvanceCluster(void)
{
    if (!m_pSegment)
        return FALSE;

    if (!m_pCluster)
    {
        m_pCluster = m_pSegment->GetFirst();
    }
    else
    {
        m_pCluster = m_pSegment->GetNext(m_pCluster);
    }

    if (!m_pCluster || m_pCluster->EOS())
    {
        m_bEOF    = TRUE;
        m_pCluster = NULL;
        return FALSE;
    }

    m_pEntry = NULL;
    return TRUE;
}

//==============================================================================

xbool container::AdvanceEntry(void)
{
    if (m_bEOF)
        return FALSE;

    s32 retries = 0;

    while (retries < 128) // 128 Max retries
    {
        if (!m_pCluster)
        {
            if (!AdvanceCluster())
                return FALSE;
        }

        if (!m_pEntry)
        {
            const mkvparser::BlockEntry* pEntry = NULL;
            const long status = m_pCluster->GetFirst(pEntry);

            if ((status < 0) || !pEntry || pEntry->EOS())
            {
                if (!AdvanceCluster())
                    return FALSE;
                ++retries;
                continue;
            }

            m_pEntry = pEntry;
            return TRUE;
        }

        const mkvparser::BlockEntry* pNext = NULL;
        const long status = m_pCluster->GetNext(m_pEntry, pNext);

        if ((status < 0) || !pNext || pNext->EOS())
        {
            if (!AdvanceCluster())
                return FALSE;
            ++retries;
            continue;
        }

        m_pEntry = pNext;
        return TRUE;
    }

    x_DebugMsg("MoviePlayer_WebM: Too many empty clusters\n");
    m_bEOF = TRUE;
    return FALSE;
}

//==============================================================================