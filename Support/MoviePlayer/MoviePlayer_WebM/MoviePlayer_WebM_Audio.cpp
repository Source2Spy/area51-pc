//==============================================================================
//
//  MoviePlayer_WebM_Audio.cpp
//
//  Audio decoding for WebM playback using XAudio2.9.
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

#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>
#include <mmreg.h>

#include "x_files.hpp"
#include "x_threads.hpp"
#include "x_memory.hpp"
#include "x_math.hpp"
#include "MoviePlayer_WebM_Private.hpp"

using namespace movie_webm;

//==============================================================================
// CONSTANTS
//==============================================================================

namespace
{
    static const s32 MAX_OPUS_FRAME_MS       = 120;
    static const s32 AUDIO_SUBMIT_TIMEOUT_MS = 500;
}

//==============================================================================
// XAudio2 Voice Callback
//==============================================================================

class audio_decoder::voice_callback : public IXAudio2VoiceCallback
{
public:
                    voice_callback   (void) {}
                   ~voice_callback   (void) {}

    void __stdcall  OnVoiceProcessingPassStart(UINT32) override {}
    void __stdcall  OnVoiceProcessingPassEnd(void) override {}
    void __stdcall  OnStreamEnd(void) override {}
    void __stdcall  OnBufferStart(void*) override {}
    void __stdcall  OnBufferEnd(void* pBufferContext) override
    {
        if (pBufferContext)
        {
            x_free(pBufferContext);
        }
    }
    void __stdcall  OnLoopEnd(void*) override {}
    void __stdcall  OnVoiceError(void*, HRESULT) override {}
};

//==============================================================================
// CONSTRUCTION / DESTRUCTION
//==============================================================================

audio_decoder::audio_decoder(void)
{
    m_Volume               = 1.0f;
    m_CodecType            = CODEC_NONE;
    m_Channels             = 0;
    m_SampleRate           = 0;
    m_BitsPerSample        = 16;
    m_bInitialized         = FALSE;
    m_bWarned              = FALSE;
    m_bVoiceStarted        = FALSE;
    m_ComInitialized       = FALSE;

    m_pXAudio2             = NULL;
    m_pMasterVoice         = NULL;
    m_pSourceVoice         = NULL;
    m_pVoiceCallback       = NULL;

    m_pOpusDecoder         = NULL;
    m_pOpusMSDecoder       = NULL;
    m_OpusStreams          = 0;
    m_OpusCoupled          = 0;
    m_OpusPreSkip          = 0;
    m_OpusPreSkipRemaining = 0;
    m_MaxFrameSamples      = 0;
    m_OpusGain             = 0;

    m_pVorbisInfo          = NULL;
    m_pVorbisComment       = NULL;
    m_pVorbisDsp           = NULL;
    m_pVorbisBlock         = NULL;
    m_VorbisInitialized    = FALSE;
    m_VorbisPacketIndex    = 0;

    for (s32 i = 0; i < 3; ++i)
    {
        m_VorbisHeaders[i].Clear();
    }
}

//==============================================================================

audio_decoder::~audio_decoder(void)
{
    Shutdown();
}

//==============================================================================
// INITIALIZATION / SHUTDOWN
//==============================================================================

xbool audio_decoder::Initialize(const player_config& Config)
{
    Shutdown();

    if (!Config.HasAudio)
        return TRUE;

    m_Volume        = 1.0f;
    m_bWarned       = FALSE;
    m_bVoiceStarted = FALSE;
    m_CodecType     = CODEC_NONE;
    m_Channels      = (Config.AudioChannels > 0) ? Config.AudioChannels : 2;
    m_SampleRate    = (Config.AudioSampleRate > 0) ? Config.AudioSampleRate : 48000;
	m_BitsPerSample = 16; //m_BitsPerSample = (Config.AudioBitDepth > 0) ? Config.AudioBitDepth : 16;

    m_MaxFrameSamples = (m_SampleRate * MAX_OPUS_FRAME_MS) / 1000;
    if (m_MaxFrameSamples <= 0)
        m_MaxFrameSamples = 5760;

    xstring codecId = Config.AudioCodecId;
    codecId.MakeUpper();

    if (codecId.Find("OPUS") != -1)
    {
        m_CodecType = CODEC_OPUS;
    }
    else if (codecId.Find("VORBIS") != -1)
    {
        m_CodecType = CODEC_VORBIS;
    }
    else
    {
        x_DebugMsg("MoviePlayer_WebM: Unsupported audio codec '%s'\n", (const char*)codecId);
        return FALSE;
    }

    xbool codecInitialized = FALSE;
    if (m_CodecType == CODEC_OPUS)
    {
        codecInitialized = InitializeOpus(Config);
    }
    else if (m_CodecType == CODEC_VORBIS)
    {
        codecInitialized = InitializeVorbis(Config);
    }

    if (!codecInitialized)
    {
        Shutdown();
        return FALSE;
    }

    if (!InitializeXAudio())
    {
        Shutdown();
        return FALSE;
    }

    if (!CreateSourceVoice())
    {
        Shutdown();
        return FALSE;
    }

    m_bInitialized = TRUE;
    SetVolume(1.0f);
    return TRUE;
}

//==============================================================================

void audio_decoder::Shutdown(void)
{
    DestroyOpus();
    DestroyVorbis();
    DestroySourceVoice();
    ShutdownXAudio();

    m_FrameBuffer.Clear();
    m_PCMBuffer.Clear();

    m_CodecType      = CODEC_NONE;
    m_bInitialized   = FALSE;
    m_bWarned        = FALSE;
    m_bVoiceStarted  = FALSE;
    m_MaxFrameSamples= 0;
}

//==============================================================================

void audio_decoder::Flush(void)
{
    if (m_pSourceVoice)
    {
        m_pSourceVoice->Stop(0);
        m_pSourceVoice->FlushSourceBuffers();
        m_pSourceVoice->Discontinuity();
        m_bVoiceStarted = FALSE;
    }

    ResetDecoders();
}

//==============================================================================
// DECODING
//==============================================================================

xbool audio_decoder::DecodeSample(const sample& Sample, mkvparser::IMkvReader* pReader)
{
    if (!m_bInitialized)
    {
        if (!m_bWarned)
        {
            x_DebugMsg("MoviePlayer_WebM: Audio decoder not initialized.\n");
            m_bWarned = TRUE;
        }
        return TRUE;
    }

    if (!Sample.pBlock || !pReader)
        return FALSE;

    const mkvparser::Block* pBlock = Sample.pBlock;
    const s32 frameCount = (s32)pBlock->GetFrameCount();

    for (s32 i = 0; i < frameCount; ++i)
    {
        const mkvparser::Block::Frame& Frame = pBlock->GetFrame(i);
        if (!ReadFrameData(Frame, pReader))
            continue;

        if (m_CodecType == CODEC_OPUS)
        {
            DecodeOpusFrame(m_FrameBuffer.GetPtr(), m_FrameBuffer.GetCount());
        }
        else if (m_CodecType == CODEC_VORBIS)
        {
            DecodeVorbisPacket(m_FrameBuffer.GetPtr(), m_FrameBuffer.GetCount());
        }
    }

    return TRUE;
}

//==============================================================================

void audio_decoder::SetVolume(f32 Volume)
{
    if (Volume < 0.0f) Volume = 0.0f;
    if (Volume > 1.0f) Volume = 1.0f;

    m_Volume = Volume;

    if (m_pMasterVoice)
        m_pMasterVoice->SetVolume(m_Volume);
}

//==============================================================================
// INTERNAL HELPERS
//==============================================================================

xbool audio_decoder::InitializeXAudio(void)
{
    if (m_pXAudio2)
        return TRUE;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        m_ComInitialized = TRUE;
    }
    else if (hr == S_FALSE)
    {
        m_ComInitialized = FALSE;
    }
    else if (hr == RPC_E_CHANGED_MODE)
    {
        x_DebugMsg("MoviePlayer_WebM: CoInitializeEx failed (RPC_E_CHANGED_MODE).\n");
        m_ComInitialized = FALSE;
    }
    else
    {
        x_DebugMsg("MoviePlayer_WebM: CoInitializeEx failed (0x%08X).\n", hr);
        return FALSE;
    }

    hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        x_DebugMsg("MoviePlayer_WebM: XAudio2Create failed (0x%08X).\n", hr);
        ShutdownXAudio();
        return FALSE;
    }

    hr = m_pXAudio2->CreateMasteringVoice(&m_pMasterVoice);
    if (FAILED(hr))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to create mastering voice (0x%08X).\n", hr);
        ShutdownXAudio();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

void audio_decoder::ShutdownXAudio(void)
{
    if (m_pMasterVoice)
    {
        m_pMasterVoice->DestroyVoice();
        m_pMasterVoice = NULL;
    }

    if (m_pXAudio2)
    {
        m_pXAudio2->Release();
        m_pXAudio2 = NULL;
    }

    if (m_ComInitialized)
    {
        CoUninitialize();
        m_ComInitialized = FALSE;
    }
}

//==============================================================================

xbool audio_decoder::CreateSourceVoice(void)
{
    if (!m_pXAudio2)
        return FALSE;

    DestroySourceVoice();

    m_pVoiceCallback = new voice_callback();
    if (!m_pVoiceCallback)
        return FALSE;

    HRESULT hr = S_OK;

    if (m_Channels <= 2)
    {
        WAVEFORMATEX wfx;
        x_memset(&wfx, 0, sizeof(wfx));
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.nChannels       = (WORD)m_Channels;
        wfx.nSamplesPerSec  = (DWORD)m_SampleRate;
        wfx.wBitsPerSample  = (WORD)m_BitsPerSample;
        wfx.nBlockAlign     = (WORD)((wfx.nChannels * wfx.wBitsPerSample) / 8);
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        hr = m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, m_pVoiceCallback);
    }
    else
    {
        WAVEFORMATEXTENSIBLE wfx;
        x_memset(&wfx, 0, sizeof(wfx));
        wfx.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels       = (WORD)m_Channels;
        wfx.Format.nSamplesPerSec  = (DWORD)m_SampleRate;
        wfx.Format.wBitsPerSample  = (WORD)m_BitsPerSample;
        wfx.Format.nBlockAlign     = (WORD)((wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8);
        wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;
        wfx.dwChannelMask          = GetChannelMask(m_Channels);
        wfx.SubFormat              = KSDATAFORMAT_SUBTYPE_PCM;

        hr = m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, reinterpret_cast<WAVEFORMATEX*>(&wfx), 0, XAUDIO2_DEFAULT_FREQ_RATIO, m_pVoiceCallback);
    }

    if (FAILED(hr) || !m_pSourceVoice)
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to create source voice (0x%08X).\n", hr);
        DestroySourceVoice();
        return FALSE;
    }

    m_bVoiceStarted = FALSE;
    return TRUE;
}

//==============================================================================

void audio_decoder::DestroySourceVoice(void)
{
    if (m_pSourceVoice)
    {
        m_pSourceVoice->Stop(0);
        m_pSourceVoice->FlushSourceBuffers();
        m_pSourceVoice->DestroyVoice();
        m_pSourceVoice = NULL;
    }

    if (m_pVoiceCallback)
    {
        delete m_pVoiceCallback;
        m_pVoiceCallback = NULL;
    }

    m_bVoiceStarted = FALSE;
}

//==============================================================================

xbool audio_decoder::ReadFrameData(const mkvparser::Block::Frame& Frame, mkvparser::IMkvReader* pReader)
{
    if (!pReader)
        return FALSE;

    const s32 dataSize = (s32)Frame.len;
    if (dataSize <= 0)
        return FALSE;

    m_FrameBuffer.SetCount(dataSize);
    const long status = Frame.Read(pReader, m_FrameBuffer.GetPtr());
    if (status < 0)
    {
        if (!m_bWarned)
        {
            x_DebugMsg("MoviePlayer_WebM: Failed to read audio frame (status %ld).\n", status);
            m_bWarned = TRUE;
        }
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

u32 audio_decoder::GetChannelMask(s32 ChannelCount) const
{
    switch (ChannelCount)
    {
        case 1: return SPEAKER_FRONT_CENTER;
        case 2: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        case 3: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER;
        case 4: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
        case 5: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
        case 6: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
        default:
        {
            if (ChannelCount >= 2)
                return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            return SPEAKER_FRONT_CENTER;
        }
    }
}

//==============================================================================
// OPUS IMPLEMENTATION
//==============================================================================

xbool audio_decoder::InitializeOpus(const player_config& Config)
{
    DestroyOpus();

    const s32 privateSize = Config.AudioCodecPrivate.GetCount();
    const u8* pPrivate    = (privateSize > 0) ? Config.AudioCodecPrivate.GetPtr() : NULL;

    if (pPrivate && (privateSize >= 19) && (x_memcmp(pPrivate, "OpusHead", 8) == 0))
    {
        const u8 channelCount = pPrivate[9];
        if (channelCount > 0)
            m_Channels = channelCount;

        const s32 preSkip = (s32)(pPrivate[10] | (pPrivate[11] << 8));
        m_OpusPreSkip          = preSkip;
        m_OpusPreSkipRemaining = preSkip;

        const s32 inputRate = (s32)(pPrivate[12] | (pPrivate[13] << 8) | (pPrivate[14] << 16) | (pPrivate[15] << 24));
        if (inputRate > 0)
            m_SampleRate = inputRate;

        const s16 gain = (s16)(pPrivate[16] | (pPrivate[17] << 8));
        m_OpusGain = (s32)gain;

        const u8 mappingFamily = pPrivate[18];
        if (mappingFamily == 0)
        {
            int err = 0;
            m_pOpusDecoder = opus_decoder_create(m_SampleRate, m_Channels, &err);
            if ((err != OPUS_OK) || !m_pOpusDecoder)
            {
                x_DebugMsg("MoviePlayer_WebM: Failed to create Opus decoder (%d).\n", err);
                DestroyOpus();
                return FALSE;
            }

            if (gain != 0)
            {
                opus_decoder_ctl(m_pOpusDecoder, OPUS_SET_GAIN(gain));
            }

            m_OpusStreams = 1;
            m_OpusCoupled = (m_Channels > 1) ? 1 : 0;
        }
        else
        {
            if (privateSize < 21)
            {
                x_DebugMsg("MoviePlayer_WebM: Invalid Opus header (mapping).\n");
                return FALSE;
            }

            const u8 streamCount  = pPrivate[19];
            const u8 coupledCount = pPrivate[20];
            const s32 mappingSize = privateSize - 21;

            if (mappingSize < m_Channels)
            {
                x_DebugMsg("MoviePlayer_WebM: Invalid Opus mapping size.\n");
                return FALSE;
            }

            m_OpusMapping.SetCount(mappingSize);
            x_memcpy(m_OpusMapping.GetPtr(), pPrivate + 21, mappingSize);

            int err = 0;
            m_pOpusMSDecoder = opus_multistream_decoder_create(m_SampleRate, m_Channels, streamCount, coupledCount, m_OpusMapping.GetPtr(), &err);
            if ((err != OPUS_OK) || !m_pOpusMSDecoder)
            {
                x_DebugMsg("MoviePlayer_WebM: Failed to create Opus multistream decoder (%d).\n", err);
                DestroyOpus();
                return FALSE;
            }

            if (gain != 0)
            {
                opus_multistream_decoder_ctl(m_pOpusMSDecoder, OPUS_SET_GAIN(gain));
            }

            m_OpusStreams = streamCount;
            m_OpusCoupled = coupledCount;
        }
    }
    else
    {
        int err = 0;
        m_pOpusDecoder = opus_decoder_create(m_SampleRate, m_Channels, &err);
        if ((err != OPUS_OK) || !m_pOpusDecoder)
        {
            x_DebugMsg("MoviePlayer_WebM: Failed to create default Opus decoder (%d).\n", err);
            DestroyOpus();
            return FALSE;
        }

        m_OpusStreams = 1;
        m_OpusCoupled = (m_Channels > 1) ? 1 : 0;
        m_OpusPreSkip          = 0;
        m_OpusPreSkipRemaining = 0;
        m_OpusGain             = 0;
    }

    m_PCMBuffer.SetCount(m_MaxFrameSamples * m_Channels);
    return TRUE;
}

//==============================================================================

void audio_decoder::DestroyOpus(void)
{
    if (m_pOpusDecoder)
    {
        opus_decoder_destroy(m_pOpusDecoder);
        m_pOpusDecoder = NULL;
    }

    if (m_pOpusMSDecoder)
    {
        opus_multistream_decoder_destroy(m_pOpusMSDecoder);
        m_pOpusMSDecoder = NULL;
    }

    m_OpusMapping.Clear();
    m_OpusStreams          = 0;
    m_OpusCoupled          = 0;
    m_OpusPreSkip          = 0;
    m_OpusPreSkipRemaining = 0;
    m_OpusGain             = 0;
}

//==============================================================================

xbool audio_decoder::DecodeOpusFrame(const u8* pData, s32 DataSize)
{
    if ((!m_pOpusDecoder && !m_pOpusMSDecoder) || (DataSize <= 0))
        return TRUE;

    if (m_PCMBuffer.GetCount() < (m_MaxFrameSamples * m_Channels))
    {
        m_PCMBuffer.SetCount(m_MaxFrameSamples * m_Channels);
    }

    s16* pOutput = m_PCMBuffer.GetPtr();
    s32  samples = 0;

    if (m_pOpusMSDecoder)
    {
        samples = opus_multistream_decode(m_pOpusMSDecoder, pData, DataSize, pOutput, m_MaxFrameSamples, 0);
    }
    else
    {
        samples = opus_decode(m_pOpusDecoder, pData, DataSize, pOutput, m_MaxFrameSamples, 0);
    }

    if (samples < 0)
    {
        if (!m_bWarned)
        {
            x_DebugMsg("MoviePlayer_WebM: Opus decode error (%d).\n", samples);
            m_bWarned = TRUE;
        }
        return FALSE;
    }

    if (samples == 0)
        return TRUE;

    if (m_OpusPreSkipRemaining > 0)
    {
        const s32 skip = x_min(m_OpusPreSkipRemaining, samples);
        m_OpusPreSkipRemaining -= skip;
        
        const s32 remaining = samples - skip;
        if (remaining > 0)
        {
            x_memmove(pOutput, pOutput + (skip * m_Channels), remaining * m_Channels * (s32)sizeof(s16));
            samples = remaining;
        }
        else
        {
            return TRUE;
        }
    }

    return SubmitPCM(pOutput, samples);
}

//==============================================================================
// VORBIS IMPLEMENTATION
//==============================================================================

xbool audio_decoder::InitializeVorbis(const player_config& Config)
{
    DestroyVorbis();

    if (!ParseVorbisPrivate(Config))
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to parse Vorbis private data.\n");
        DestroyVorbis();
        return FALSE;
    }

    if (!InitializeVorbisState())
    {
        x_DebugMsg("MoviePlayer_WebM: Failed to initialize Vorbis decoder state.\n");
        DestroyVorbis();
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool audio_decoder::ParseVorbisPrivate(const player_config& Config)
{
    const s32 privateSize = Config.AudioCodecPrivate.GetCount();
    const u8* pPrivate    = (privateSize > 0) ? Config.AudioCodecPrivate.GetPtr() : NULL;

    if (!pPrivate || (privateSize <= 0))
        return FALSE;

    const s32 packetCount = (s32)pPrivate[0] + 1;
    if (packetCount != 3)
        return FALSE;

    const u8* pCurrent = pPrivate + 1;
    const u8* pEnd     = pPrivate + privateSize;

    s32 packetSizes[3] = {0, 0, 0};

    for (s32 i = 0; i < packetCount - 1; ++i)
    {
        s32 length = 0;
        while (pCurrent < pEnd)
        {
            const u8 value = *pCurrent++;
            length += value;
            if (value < 255)
                break;
        }
        packetSizes[i] = length;
    }

    for (s32 i = 0; i < packetCount; ++i)
    {
        s32 length = 0;
        if (i < packetCount - 1)
        {
            length = packetSizes[i];
        }
        else
        {
            length = (s32)(pEnd - pCurrent);
        }

        if ((length < 0) || ((pCurrent + length) > pEnd))
            return FALSE;

        m_VorbisHeaders[i].SetCount(length);
        if (length > 0)
        {
            x_memcpy(m_VorbisHeaders[i].GetPtr(), pCurrent, length);
        }
        pCurrent += length;
    }

    if (m_VorbisHeaders[0].GetCount() < 16)
        return FALSE;

    const u8* pHeader = m_VorbisHeaders[0].GetPtr();
    if ((pHeader[0] != 0x01) || (x_memcmp(pHeader + 1, "vorbis", 6) != 0))
        return FALSE;

    const u8 channelCount = pHeader[11];
    if (channelCount > 0)
        m_Channels = channelCount;

    const s32 sampleRate = (s32)(pHeader[12] | (pHeader[13] << 8) | (pHeader[14] << 16) | (pHeader[15] << 24));
    if (sampleRate > 0)
        m_SampleRate = sampleRate;

    m_VorbisPacketIndex = 0;
    return TRUE;
}

//==============================================================================

xbool audio_decoder::InitializeVorbisState(void)
{
    DestroyVorbisState(FALSE);

    if ((m_VorbisHeaders[0].GetCount() == 0) ||
        (m_VorbisHeaders[1].GetCount() == 0) ||
        (m_VorbisHeaders[2].GetCount() == 0))
    {
        return FALSE;
    }

    m_pVorbisInfo    = (vorbis_info*)x_malloc(sizeof(vorbis_info));
    m_pVorbisComment = (vorbis_comment*)x_malloc(sizeof(vorbis_comment));
    m_pVorbisDsp     = (vorbis_dsp_state*)x_malloc(sizeof(vorbis_dsp_state));
    m_pVorbisBlock   = (vorbis_block*)x_malloc(sizeof(vorbis_block));

    if (!m_pVorbisInfo || !m_pVorbisComment || !m_pVorbisDsp || !m_pVorbisBlock)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    x_memset(m_pVorbisInfo, 0, sizeof(vorbis_info));
    x_memset(m_pVorbisComment, 0, sizeof(vorbis_comment));
    x_memset(m_pVorbisDsp, 0, sizeof(vorbis_dsp_state));
    x_memset(m_pVorbisBlock, 0, sizeof(vorbis_block));

    vorbis_info_init(m_pVorbisInfo);
    vorbis_comment_init(m_pVorbisComment);

    ogg_packet packet;
    x_memset(&packet, 0, sizeof(packet));

    // Identification header
    packet.packet   = m_VorbisHeaders[0].GetPtr();
    packet.bytes    = m_VorbisHeaders[0].GetCount();
    packet.b_o_s    = 1;
    packet.packetno = 0;
    if (vorbis_synthesis_headerin(m_pVorbisInfo, m_pVorbisComment, &packet) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    // Comment header
    packet.packet   = m_VorbisHeaders[1].GetPtr();
    packet.bytes    = m_VorbisHeaders[1].GetCount();
    packet.b_o_s    = 0;
    packet.packetno = 1;
    if (vorbis_synthesis_headerin(m_pVorbisInfo, m_pVorbisComment, &packet) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    // Setup header
    packet.packet   = m_VorbisHeaders[2].GetPtr();
    packet.bytes    = m_VorbisHeaders[2].GetCount();
    packet.packetno = 2;
    if (vorbis_synthesis_headerin(m_pVorbisInfo, m_pVorbisComment, &packet) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    if (vorbis_synthesis_init(m_pVorbisDsp, m_pVorbisInfo) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    if (vorbis_block_init(m_pVorbisDsp, m_pVorbisBlock) != 0)
    {
        DestroyVorbisState(FALSE);
        return FALSE;
    }

    m_VorbisInitialized = TRUE;
    m_VorbisPacketIndex = 0;
    return TRUE;
}

//==============================================================================

void audio_decoder::DestroyVorbis(void)
{
    DestroyVorbisState(TRUE);
}

//==============================================================================

void audio_decoder::DestroyVorbisState(xbool ClearHeaders)
{
    if (m_pVorbisBlock)
    {
        vorbis_block_clear(m_pVorbisBlock);
        x_free(m_pVorbisBlock);
        m_pVorbisBlock = NULL;
    }

    if (m_pVorbisDsp)
    {
        vorbis_dsp_clear(m_pVorbisDsp);
        x_free(m_pVorbisDsp);
        m_pVorbisDsp = NULL;
    }

    if (m_pVorbisComment)
    {
        vorbis_comment_clear(m_pVorbisComment);
        x_free(m_pVorbisComment);
        m_pVorbisComment = NULL;
    }

    if (m_pVorbisInfo)
    {
        vorbis_info_clear(m_pVorbisInfo);
        x_free(m_pVorbisInfo);
        m_pVorbisInfo = NULL;
    }

    if (ClearHeaders)
    {
        for (s32 i = 0; i < 3; ++i)
            m_VorbisHeaders[i].Clear();
    }

    m_VorbisInitialized = FALSE;
    m_VorbisPacketIndex = 0;
}

//==============================================================================

xbool audio_decoder::DecodeVorbisPacket(const u8* pData, s32 DataSize)
{
    if (!m_VorbisInitialized || !m_pVorbisBlock || !m_pVorbisDsp || (DataSize <= 0))
        return TRUE;

    ogg_packet packet;
    x_memset(&packet, 0, sizeof(packet));

    packet.packet   = const_cast<unsigned char*>(pData);
    packet.bytes    = DataSize;
    packet.packetno = ++m_VorbisPacketIndex;

    const int result = vorbis_synthesis(m_pVorbisBlock, &packet);
    if (result != 0)
    {
        if (!m_bWarned)
        {
            x_DebugMsg("MoviePlayer_WebM: Vorbis synthesis failed (%d).\n", result);
            m_bWarned = TRUE;
        }
        return FALSE;
    }

    if (vorbis_synthesis_blockin(m_pVorbisDsp, m_pVorbisBlock) != 0)
        return FALSE;

    while (TRUE)
    {
        float** ppPcm = NULL;
        const long samples = vorbis_synthesis_pcmout(m_pVorbisDsp, &ppPcm);
        if (samples <= 0)
            break;

        const s32 sampleCount = (s32)samples;
        if (sampleCount > (INT_MAX / m_Channels))
        {
            x_DebugMsg("MoviePlayer_WebM: Sample count overflow detected.\n");
            vorbis_synthesis_read(m_pVorbisDsp, sampleCount);
            continue;
        }
        
        const s32 totalSamples = sampleCount * m_Channels;
        if (totalSamples <= 0)
        {
            vorbis_synthesis_read(m_pVorbisDsp, sampleCount);
            continue;
        }

        m_PCMBuffer.SetCount(totalSamples);
        s16* pOut = m_PCMBuffer.GetPtr();

        for (s32 i = 0; i < sampleCount; ++i)
        {
            for (s32 ch = 0; ch < m_Channels; ++ch)
            {
                f32 sample = (ppPcm[ch])[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;

                s32 value = (sample >= 0.0f) ? (s32)(sample * 32767.0f + 0.5f) : (s32)(sample * 32768.0f - 0.5f);
                if (value > 32767) value = 32767;
                if (value < -32768) value = -32768;

                *pOut++ = (s16)value;
            }
        }

        SubmitPCM(m_PCMBuffer.GetPtr(), sampleCount);
        vorbis_synthesis_read(m_pVorbisDsp, sampleCount);
    }

    return TRUE;
}

//==============================================================================

void audio_decoder::ResetDecoders(void)
{
    if (m_CodecType == CODEC_OPUS)
    {
        if (m_pOpusDecoder)
        {
            opus_decoder_ctl(m_pOpusDecoder, OPUS_RESET_STATE);
        }
        if (m_pOpusMSDecoder)
        {
            opus_multistream_decoder_ctl(m_pOpusMSDecoder, OPUS_RESET_STATE);
        }
        m_OpusPreSkipRemaining = m_OpusPreSkip;
    }
    else if (m_CodecType == CODEC_VORBIS)
    {
        if (!InitializeVorbisState())
        {
            x_DebugMsg("MoviePlayer_WebM: Failed to reset Vorbis decoder.\n");
        }
    }
}

//==============================================================================

xbool audio_decoder::SubmitPCM(const s16* pSamples, s32 SampleCount)
{
    if (!m_pSourceVoice || !pSamples || (SampleCount <= 0))
        return TRUE;

    XAUDIO2_VOICE_STATE state;
    m_pSourceVoice->GetState(&state);

    s32 waitMs = 0;
    while (state.BuffersQueued >= 6)
    {
        if (waitMs >= AUDIO_SUBMIT_TIMEOUT_MS)
        {
            if (!m_bWarned)
            {
                x_DebugMsg("MoviePlayer_WebM: Audio submit timeout, dropping buffer.\n");
                m_bWarned = TRUE;
            }
            return FALSE;
        }
        x_DelayThread(1);
        ++waitMs;
        m_pSourceVoice->GetState(&state);
    }

    const s32 bytes = SampleCount * m_Channels * (s32)sizeof(s16);
    u8* pBuffer = (u8*)x_malloc(bytes);
    if (!pBuffer)
        return FALSE;

    x_memcpy(pBuffer, pSamples, bytes);

    XAUDIO2_BUFFER Buffer;
    x_memset(&Buffer, 0, sizeof(Buffer));
    Buffer.AudioBytes = bytes;
    Buffer.pAudioData = pBuffer;
    Buffer.pContext   = pBuffer;

    HRESULT hr = m_pSourceVoice->SubmitSourceBuffer(&Buffer);
    if (FAILED(hr))
    {
        x_free(pBuffer);
        if (!m_bWarned)
        {
            x_DebugMsg("MoviePlayer_WebM: SubmitSourceBuffer failed (0x%08X).\n", hr);
            m_bWarned = TRUE;
        }
        return FALSE;
    }

    if (!m_bVoiceStarted)
    {
        hr = m_pSourceVoice->Start(0);
        if (FAILED(hr))
        {
            x_DebugMsg("MoviePlayer_WebM: Failed to start source voice (0x%08X).\n", hr);
            m_pSourceVoice->FlushSourceBuffers();
            return FALSE;
        }
        else
        {
            m_bVoiceStarted = TRUE;
        }
    }

    return TRUE;
}