//==============================================================================
//
//  audio_descriptor_runtime.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_descriptor_runtime.hpp"
#include "Audio/audio_runtime.hpp"
#include "Audio/audio_package.hpp"
#include "Audio/audio_package_registry.hpp"
#include "Audio/audio_voice_mgr.hpp"

//==============================================================================
//  TYPES
//==============================================================================

#ifndef X_RETAIL
struct s_AudioData
{
    char   Desc[64];
    f32    fVolume;
    f32    fNearClip;
    f32    fFarClip;
};

extern s_AudioData *AudioTweakData;
extern xbool        AUDIO_TWEAK;
static const char*  s_pTweakDescriptor = NULL;
#endif

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_descriptor_runtime::audio_descriptor_runtime( void )
{
    m_pRuntime  = NULL;
    m_pPackages = NULL;
}

//==============================================================================

audio_descriptor_runtime::~audio_descriptor_runtime( void )
{
}

//==============================================================================

void audio_descriptor_runtime::Init( audio_runtime& Runtime, audio_package_registry& Packages )
{
    m_pRuntime  = &Runtime;
    m_pPackages = &Packages;
}

//==============================================================================

void audio_descriptor_runtime::Kill( void )
{
    m_pRuntime  = NULL;
    m_pPackages = NULL;
}

//==============================================================================

void audio_descriptor_runtime::DecodeParameters( uncompressed_parameters* pParams, u16* pDescriptor )
{
    xbool bHasParams = GET_DESCRIPTOR_HAS_PARAMS( *pDescriptor );

    // Get the flags.
    pParams->Flags = *(pDescriptor+1);

    // Parameter defined?
    if( bHasParams )
    {
        u8* pDescriptorBytes;
        u32 Bits1;
        u32 Bits2;
        u32 Bits;
        u16 DataU16;

        // Skip past descriptor type, index, etc..
        pDescriptor++;

        // Skip past the flags
        pDescriptor++;

        // Skip past the size of the parameters.
        pDescriptor++;

        // Get the parameter bits.
        Bits1 = (u32)(*pDescriptor++);
        Bits2 = (u32)(*pDescriptor++);
        Bits  = Bits1 + (Bits2 << 16);

        // Set the bits
        pParams->Bits = Bits;

        //--------------------------------//
        // Process the 16-bit parameters. //
        //--------------------------------//

        // Pitch specified?
        if( GET_PITCH_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);

            // Is it a compressed 1.0f?
            if( DataU16 == FLOAT4_TO_U16BIT( 1.0f ) )
            {
                // Set it to exactly 1.0.
                pParams->Pitch = 1.0f;
            }
            else
            {
                // Decompress it to a float.
                pParams->Pitch = U16BIT_TO_FLOAT4( DataU16 );
            }
        }
        else
        {
            // Default is 1.0 since this is multiplied.
            pParams->Pitch = 1.0f;
        }

        // Pitch variance specified?
        if( GET_PITCH_VARIANCE_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);

            // Is it a compressed 0.0f?
            if( DataU16 == FLOAT1_TO_U16BIT( 0.0f ) )
            {
                pParams->PitchVariance = 0.0f;
            }
            else
            {
                pParams->PitchVariance = U16BIT_TO_FLOAT1( DataU16 );
            }
        }

        // Volume specified?
        if( GET_VOLUME_BIT( Bits )  )
        {
            DataU16 = (*pDescriptor++);

            // Is it a compressed 1.0f?
            if( DataU16 == FLOAT1_TO_U16BIT( 1.0f ) )
            {
                // Set it exactly to 1.0.
                pParams->Volume = 1.0f;
            }
            else
            {
                // Decompress it to a float.
                pParams->Volume = U16BIT_TO_FLOAT1( DataU16 );
            }
        }
        else
        {
            // Default is 1.0 since this is multiplied.
            pParams->Volume = 1.0f;
        }

        // Volume variance specified?
        if( GET_VOLUME_VARIANCE_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);

            // Is it a compressed 0.0f?
            if( DataU16 == FLOAT1_TO_U16BIT( 0.0f ) )
            {
                // Set it to exactly 0.0f.
                pParams->VolumeVariance = 0.0f;
            }
            else
            {
                // Decompress it to a float.
                pParams->VolumeVariance = U16BIT_TO_FLOAT1( DataU16 );
            }
        }

        // Center volume specified?
        if( GET_VOLUME_CENTER_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);

            // Is it a compressed 1.0f?
            if( DataU16 == FLOAT1_TO_U16BIT( 1.0f ) )
            {
                // Set it exactly to 1.0.
                pParams->VolumeCenter = 1.0f;
            }
            else
            {
                // Decompress it to a float.
                pParams->VolumeCenter = U16BIT_TO_FLOAT1( DataU16 );
            }
        }
        else
        {
            // Default is 1.0 since this is multiplied.
            pParams->VolumeCenter = 1.0f;
        }

        // LFE Volume specified?
        if( GET_VOLUME_LFE_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);

            // Is it a compressed 1.0f?
            if( DataU16 == FLOAT1_TO_U16BIT( 1.0f ) )
            {
                // Set it exactly to 1.0.
                pParams->VolumeLFE = 1.0f;
            }
            else
            {
                // Decompress it to a float.
                pParams->VolumeLFE = U16BIT_TO_FLOAT1( DataU16 );
            }
        }
        else
        {
            // Default is 1.0 since this is multiplied.
            pParams->VolumeLFE = 1.0f;
        }

        // Volume duck specified?
        if( GET_VOLUME_DUCK_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);

            // Is it a compressed 1.0f?
            if( DataU16 == FLOAT1_TO_U16BIT( 1.0f ) )
            {
                // Set it exactly to 1.0.
                pParams->VolumeDuck = 1.0f;
            }
            else
            {
                // Decompress it to a float.
                pParams->VolumeDuck = U16BIT_TO_FLOAT1( DataU16 );
            }
        }
        else
        {
            // Default is 1.0 since this is multiplied.
            pParams->VolumeDuck = 1.0f;
        }

        // User data specified?
        if( GET_USER_DATA_BIT( Bits ) )
        {
            pParams->UserData = (u32)*pDescriptor++;
        }

        // Replay delay specified?
        if( GET_REPLAY_DELAY_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);
            pParams->ReplayDelay = U16BIT_TO_FLOAT_TENTH( DataU16 );
        }

        // Last play specified?
        if( GET_LAST_PLAY_BIT( Bits ) )
        {
            DataU16 = (*pDescriptor++);
            pParams->LastPlay = U16BIT_TO_FLOAT_TENTH( DataU16 );
        }
        else
        {
            pParams->LastPlay = 0.0f;
        }

        //-------------------------------//
        // Process the 8-bit parameters. //
        //-------------------------------//

        // Coerce to 8-bit pointer.
        pDescriptorBytes = (u8*)pDescriptor;

        // Pan specified?
        if( GET_PAN_2D_BIT( Bits ) )
        {
            pParams->Pan2d = S8BIT_TO_FLOAT1( (*pDescriptorBytes++) );
        }

        // Priority specified?
        if( GET_PRIORITY_BIT( Bits ) )
        {
            pParams->Priority = (u32)(*pDescriptorBytes++);
        }

        // Effect send specified? Default is 1.0 since this is multiplied.
        if( GET_EFFECT_SEND_BIT( Bits ) )
        {
            pParams->EffectSend = U8BIT_TO_FLOAT1( (*pDescriptorBytes++) );
        }
        else
        {
            pParams->EffectSend = 1.0f;
        }

        // Near falloff specified? Default is 1.0 since this is multiplied.
        if( GET_NEAR_FALLOFF_BIT( Bits ) )
        {
            pParams->NearFalloff = U8BIT_TO_FLOAT10( (*pDescriptorBytes++) );
        }
        else
        {
            pParams->NearFalloff = 1.0f;
        }

        // Far falloff specified? Default is 1.0 since this is multiplied.
        if( GET_FAR_FALLOFF_BIT( Bits ) )
        {
            pParams->FarFalloff = U8BIT_TO_FLOAT10( (*pDescriptorBytes++) );
        }
        else
        {
            pParams->FarFalloff = 1.0f;
        }

        // Rolloff curve specified?
        if( GET_ROLLOFF_METHOD_BIT( Bits ) )
        {
            pParams->RolloffCurve = (u32)(*pDescriptorBytes++);
        }

        // Near diffuse specified? Default is 1.0 since this is multiplied.
        if( GET_NEAR_DIFFUSE_BIT( Bits ) )
        {
            pParams->NearDiffuse = U8BIT_TO_FLOAT10( (*pDescriptorBytes++) );
        }
        else
        {
            pParams->NearDiffuse = 1.0f;
        }

        // Far diffuse specified? Default is 1.0 since this is multiplied.
        if( GET_FAR_DIFFUSE_BIT( Bits ) )
        {
            pParams->FarDiffuse = U8BIT_TO_FLOAT10( (*pDescriptorBytes++) );
        }
        else
        {
            pParams->FarDiffuse = 1.0f;
        }

        // Play percent specified?
        if( GET_PLAY_PERCENT_BIT( Bits ) )
        {
            pParams->PlayPercent = (u32)(*pDescriptorBytes++);
        }
    }
    else
    {
        // None are defined...
        pParams->Bits = 0;

        // These are multiplied, so set to 1.0
        pParams->Pitch        =
        pParams->Volume       =
        pParams->VolumeCenter =
        pParams->VolumeLFE    =
        pParams->VolumeDuck   =
        pParams->EffectSend   =
        pParams->NearFalloff  =
        pParams->FarFalloff   =
        pParams->NearDiffuse  =
        pParams->FarDiffuse   = 1.0f;
   }

#ifndef X_RETAIL
    if( AUDIO_TWEAK && s_pTweakDescriptor && AudioTweakData )
    {
        s32 i=0;

        while( AudioTweakData[i].Desc[0] )
        {
            if( x_stricmp( AudioTweakData[i].Desc, s_pTweakDescriptor ) == 0 )
            {
                if( AudioTweakData[i].fVolume )
                {
                    pParams->Volume = AudioTweakData[i].fVolume;
                }

                if( AudioTweakData[i].fNearClip )
                {
                    pParams->NearFalloff = AudioTweakData[i].fNearClip;
                }

                if( AudioTweakData[i].fFarClip)
                {
                    pParams->FarFalloff = AudioTweakData[i].fFarClip;
                }

                return;
            }

            i++;
        }
    }
#endif // X_RETAIL
}

//==============================================================================

s32 audio_descriptor_runtime::AppendHot( u32 Index, f32 DeltaTime, u16* pDescriptor, voice* pVoice, audio_package* pPackage )
{
    element* pElements[2];
    s32      nChannels;
    s32      i;

    // Calculate number of channels.
    ASSERT( pPackage->m_SampleIndices[ HOT ] );
    ASSERT( Index < (u32)pPackage->m_Header.nSampleIndices[ HOT ] );
    if( Index >= (u32)pPackage->m_Header.nSampleIndices[ HOT ] || !pPackage->m_SampleIndices[ HOT ] )
    {
        return 0;
    }
    nChannels = (s32)(pPackage->m_SampleIndices[ HOT ][ Index+1 ] - pPackage->m_SampleIndices[ HOT ][ Index ]);
    ASSERT( (nChannels > 0) && (nChannels <= 2 ) );
    if( (nChannels <=0) || (nChannels > 2) )
    {
        return 0;
    }

    // Acquire an element for each channel.
    for( i=0 ; i<nChannels ; i++ )
    {
        if( (pElements[ i ] = Runtime().Voices.AcquireElement()) == NULL)
        {
            for( s32 j=0 ; j<i ; j++ )
            {
                if( pElements[ j ] )
                {
                    Runtime().Voices.ReleaseElement( pElements[ j ], FALSE );
                }
            }

            return 0;
        }
    }

    switch( nChannels )
    {
        case 1:
        {
            pElements[ 0 ]->pStereoElement = NULL;
            break;
        }

        // Stereo
        case 2:
        {
            pElements[ 0 ]->pStereoElement = pElements[ 1 ];
            pElements[ 1 ]->pStereoElement = pElements[ 0 ];
            break;
        }
    }

    // For each channel...
    u32   HotIndex = (u32)pPackage->m_SampleIndices[ HOT ][ Index ];
    uaddr Base     = (uaddr)pPackage->m_HotSamples + (HotIndex * sizeof(sample_header));
    for( i=0 ; i<nChannels ; i++, Base+=sizeof(sample_header) )
    {
        element* pElement = pElements[ i ];

        // Put element at end of voices element list.
        Runtime().Voices.AppendElementToVoice( pElement, pVoice );

        // Get the elements parameters.
        GetElementParameters( &pElement->Params, pDescriptor, pVoice );

        switch( nChannels )
        {
            case 1:
            {
                // If pan was specified, then it cannot be changed.
                if( pElement->Params.Bits & PAN_2D )
                {
                    pElement->IsPanChangeable = FALSE;
                }
                else
                {
                    pElement->IsPanChangeable = TRUE;
                }
                break;
            }

            case 2:
            {
                // Can't modify pan on stereo sample.
                pElement->IsPanChangeable = FALSE;
                if( i == 0 )
                {
                    pElement->Params.Pan2d = -1.0f;
                }
                else
                {
                    pElement->Params.Pan2d =  1.0f;
                }
            }
        }

        pElement->DeltaTime         = DeltaTime;
        pElement->State             = ELEMENT_READY;
        pElement->Type              = HOT_SAMPLE;
        pElement->Sample.pHotSample = (hot_sample*)Base;

        // Initialize the elements parameters.
        Runtime().Voices.InitSingleElement( pElement );
    }

    return nChannels;
}

//==============================================================================

s32 audio_descriptor_runtime::AppendCold( u32 Index, f32 DeltaTime, u16* pDescriptor, voice* pVoice, audio_package* pPackage )
{
    element* pElements[2];
    s32      nChannels;
    s32      i;

    // Calculate number of channels.
    ASSERT( pPackage->m_SampleIndices[ COLD ] );
    ASSERT( Index < (u32)pPackage->m_Header.nSampleIndices[ COLD ] );
    if( Index >= (u32)pPackage->m_Header.nSampleIndices[ COLD ] || !pPackage->m_SampleIndices[ COLD ] )
    {
        return 0;
    }
    nChannels = (s32)(pPackage->m_SampleIndices[ COLD ][ Index+1 ] - pPackage->m_SampleIndices[ COLD ][ Index ]);
    ASSERT( (nChannels > 0) && (nChannels <= 2 ) );
    if( (nChannels <=0) || (nChannels > 2) )
    {
        return 0;
    }

    // Acquire an element for each channel.
    for( i=0 ; i<nChannels ; i++ )
    {
        if( (pElements[ i ] = Runtime().Voices.AcquireElement()) == NULL)
        {
            for( s32 j=0 ; j<i ; j++ )
            {
                if( pElements[ j ] )
                {
                    Runtime().Voices.ReleaseElement( pElements[ j ], FALSE );
                }
            }

            return 0;
        }
    }

    switch( nChannels )
    {
        case 1:
        {
            pElements[ 0 ]->pStereoElement = NULL;
            break;
        }

        // Stereo
        case 2:
        {
            pElements[ 0 ]->pStereoElement = pElements[ 1 ];
            pElements[ 1 ]->pStereoElement = pElements[ 0 ];
            break;
        }
    }

    // For each channel...
    u32   ColdIndex = (u32)pPackage->m_SampleIndices[ COLD ][ Index ];
    uaddr Base      = (uaddr)pPackage->m_ColdSamples + (ColdIndex * sizeof(sample_header));
    for( i=0 ; i<nChannels ; i++, Base+=sizeof(sample_header) )
    {
        element* pElement = pElements[ i ];

        // Put element at end of voices element list.
        Runtime().Voices.AppendElementToVoice( pElement, pVoice );

        // Get the elements parameters.
        GetElementParameters( &pElement->Params, pDescriptor, pVoice );

        switch( nChannels )
        {
            case 1:
            {
                // If pan was specified, then it cannot be changed.
                if( pElement->Params.Bits & PAN_2D )
                {
                    pElement->IsPanChangeable = FALSE;
                }
                else
                {
                    pElement->IsPanChangeable = TRUE;
                }
                break;
            }

            case 2:
            {
                // Can't modify pan on stereo sample.
                pElement->IsPanChangeable = FALSE;
                if( i == 0 )
                {
                    pElement->Params.Pan2d = -1.0f;
                }
                else
                {
                    pElement->Params.Pan2d =  1.0f;
                }
            }
        }

        pElement->DeltaTime          = DeltaTime;
        pElement->State              = ELEMENT_NEEDS_TO_LOAD;
        pElement->Type               = COLD_SAMPLE;
        pElement->Sample.pColdSample = (cold_sample*)Base;

        // Nuke the aram (shouldn't mattter).
        pElement->Sample.pColdSample->AudioRam = 0;

        // Initialize the elements parameters.
        Runtime().Voices.InitSingleElement( pElement );
    }

    return nChannels;
}

//==============================================================================

class audio_descriptor_runtime::descriptor_visitor
{
public:
    virtual s32     VisitSample             ( index_type Type, u32 Index, f32 BaseTime, u16* pDescriptor ) = 0;
    virtual xbool   VisitAllListElements    ( void ) { return FALSE; }
    virtual xbool   ShouldStop              ( s32 Result ) { (void)Result; return FALSE; }
};

//==============================================================================

class audio_descriptor_runtime::append_descriptor_visitor : public audio_descriptor_runtime::descriptor_visitor
{
public:
                    append_descriptor_visitor  ( audio_descriptor_runtime& Runtime, voice* pVoice, audio_package* pPackage ) :
                        m_Runtime   ( Runtime ),
                        m_pVoice    ( pVoice ),
                        m_pPackage  ( pPackage )
                    {
                    }

    virtual s32     VisitSample             ( index_type Type, u32 Index, f32 BaseTime, u16* pDescriptor )
                    {
                        switch( Type )
                        {
                            case HOT_INDEX:
                            {
                                return m_Runtime.AppendHot( Index, BaseTime, pDescriptor, m_pVoice, m_pPackage );
                            }

                            case COLD_INDEX:
                            {
                                return m_Runtime.AppendCold( Index, BaseTime, pDescriptor, m_pVoice, m_pPackage );
                            }

                            default:
                            {
                                ASSERT( 0 );
                                return 0;
                            }
                        }
                    }

private:
    audio_descriptor_runtime& m_Runtime;
    voice*                    m_pVoice;
    audio_package*            m_pPackage;
};

//==============================================================================

class audio_descriptor_runtime::cold_descriptor_visitor : public audio_descriptor_runtime::descriptor_visitor
{
public:
    virtual s32     VisitSample             ( index_type Type, u32 Index, f32 BaseTime, u16* pDescriptor )
                    {
                        (void)Index;
                        (void)BaseTime;
                        (void)pDescriptor;
                        return( Type == COLD_INDEX );
                    }

    virtual xbool   VisitAllListElements    ( void ) { return TRUE; }
    virtual xbool   ShouldStop              ( s32 Result ) { return( Result != 0 ); }
};

//==============================================================================

class audio_descriptor_runtime::length_descriptor_visitor : public audio_descriptor_runtime::descriptor_visitor
{
public:
                    length_descriptor_visitor  ( audio_descriptor_runtime& Runtime, audio_package* pPackage ) :
                        m_Runtime   ( Runtime ),
                        m_pPackage  ( pPackage ),
                        m_Length    ( 0.0f )
                    {
                    }

    virtual s32     VisitSample             ( index_type Type, u32 Index, f32 BaseTime, u16* pDescriptor )
                    {
                        (void)pDescriptor;

                        f32 EndTime = BaseTime + m_Runtime.GetSampleLengthSeconds( Type, Index, m_pPackage );

                        if( EndTime > m_Length )
                        {
                            m_Length = EndTime;
                        }

                        return 0;
                    }

    virtual xbool   VisitAllListElements    ( void ) { return TRUE; }
            f32     GetLengthSeconds        ( void ) const { return m_Length; }

private:
    audio_descriptor_runtime& m_Runtime;
    audio_package*            m_pPackage;
    f32                       m_Length;
};

//==============================================================================

static
u32 GetElementParameterWords( u16* pDescriptor )
{
    u32 ElementIndex = (u32)(*pDescriptor);

    if( GET_INDEX_HAS_PARAMS( ElementIndex ) )
    {
        return (*(pDescriptor+2)) >> 1;
    }
    else
    {
        return 0;
    }
}

//==============================================================================

static
u16* SkipElementDescriptor( u16* pDescriptor )
{
    return pDescriptor + 2 + GetElementParameterWords( pDescriptor );
}

//==============================================================================

s32 audio_descriptor_runtime::WalkElement( f32 BaseTime, u16* pDescriptor, audio_package* pPackage, descriptor_visitor& Visitor, u32& RecursionDepth )
{
    u32        ElementIndex = (u32)(*pDescriptor);
    index_type ElementType  = GET_INDEX_TYPE( ElementIndex );
    u32        Index        = GET_INDEX( ElementIndex );

    if( ElementType == DESCRIPTOR_INDEX )
    {
        u16* pNewDescriptor = (u16*)pPackage->m_DescriptorTable[ Index ];
        return WalkDescriptor( BaseTime, pNewDescriptor, pPackage, Visitor, RecursionDepth );
    }
    else
    {
        return Visitor.VisitSample( ElementType, Index, BaseTime, pDescriptor );
    }
}

//==============================================================================

s32 audio_descriptor_runtime::WalkSimple( f32 BaseTime, u16* pDescriptor, audio_package* pPackage, descriptor_visitor& Visitor, u32& RecursionDepth )
{
    return WalkElement( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
}

//==============================================================================

s32 audio_descriptor_runtime::WalkComplex( f32 BaseTime, u16* pDescriptor, audio_package* pPackage, descriptor_visitor& Visitor, u32& RecursionDepth )
{
    s32 Result       = 0;
    s32 ElementCount = (s32)(*pDescriptor++);

    for( s32 i=0 ; i<ElementCount ; i++ )
    {
        u16 U16DeltaTime = *pDescriptor++;
        f32 DeltaTime    = U16BIT_TO_FLOAT100( U16DeltaTime );
        s32 ChildResult  = WalkElement( BaseTime+DeltaTime, pDescriptor, pPackage, Visitor, RecursionDepth );

        Result += ChildResult;
        if( Visitor.ShouldStop( Result ) )
        {
            return Result;
        }

        pDescriptor = SkipElementDescriptor( pDescriptor );
    }

    return Result;
}

//==============================================================================

s32 audio_descriptor_runtime::WalkRandomList( f32 BaseTime, u16* pDescriptor, audio_package* pPackage, descriptor_visitor& Visitor, u32& RecursionDepth )
{
    s32  ElementCount    = (s32)(*pDescriptor++);
    u16* pUsedWordBuffer = pDescriptor;

    ASSERT( ElementCount < 64 );

    if( Visitor.VisitAllListElements() )
    {
        s32 Result = 0;

        pDescriptor += 4;
        for( s32 i=0 ; i<ElementCount ; i++ )
        {
            Result += WalkElement( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
            if( Visitor.ShouldStop( Result ) )
            {
                return Result;
            }

            pDescriptor = SkipElementDescriptor( pDescriptor );
        }

        return Result;
    }
    else
    {
        u16* pUsedWords;
        u64  UsedBits;
        s32  i;
        s32  nToPickFrom;
        s32  Choice;
        s32  Shift;
        u8   RandomList[64];

        // Construct the used bitfield (data is not 64-bit aligned).
        pUsedWords = pUsedWordBuffer;
        for( i=0, UsedBits=0, Shift=0 ; i<4 ; i++, Shift+=16 )
        {
            UsedBits |= (((u64)(*pUsedWords++)) << Shift);
        }

        // Have we used em all? If so, start fresh.
        if( UsedBits == (((u64)1<<ElementCount)-1) )
        {
            UsedBits = 0;
        }

        // Now construct the random list.
        for( i=0, nToPickFrom=0 ; i<ElementCount ; i++ )
        {
            if( (UsedBits & ((u64)1<<i)) == 0 )
            {
                RandomList[ nToPickFrom++ ] = i;
            }
        }

        // Pick one.
        Choice = RandomList[ x_irand( 0, nToPickFrom-1 ) ];

        // Mark it used.
        UsedBits |= ((u64)1<<Choice);

        // Update the used bits.
        pUsedWords = pUsedWordBuffer;
        for( i=0, Shift=0 ; i<4 ; i++, Shift+=16 )
        {
            *pUsedWords++ = (u16)((UsedBits >> Shift) & 0xffff);
        }

        pDescriptor += 4;
        while( Choice-- )
        {
            pDescriptor = SkipElementDescriptor( pDescriptor );
        }

        return WalkElement( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
    }
}

//==============================================================================

s32 audio_descriptor_runtime::WalkWeightedList( f32 BaseTime, u16* pDescriptor, audio_package* pPackage, descriptor_visitor& Visitor, u32& RecursionDepth )
{
    s32  ElementCount = (s32)(*pDescriptor++);
    u16* pWeights     = pDescriptor;

    if( Visitor.VisitAllListElements() )
    {
        s32 Result = 0;

        pDescriptor += ElementCount;
        for( s32 i=0 ; i<ElementCount ; i++ )
        {
            Result += WalkElement( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
            if( Visitor.ShouldStop( Result ) )
            {
                return Result;
            }

            pDescriptor = SkipElementDescriptor( pDescriptor );
        }

        return Result;
    }
    else
    {
        s32 Choice = 0;
        u16 Weight = (u16)((x_irand( 0, 0x7fff )*2) & 0xffff);

        while( Choice < ElementCount )
        {
            if( Weight <= *pWeights++ )
            {
                break;
            }
            Choice++;
        }

        ASSERT( Choice < ElementCount );
        if( Choice >= ElementCount )
        {
            return 0;
        }

        pDescriptor += ElementCount;
        while( Choice-- )
        {
            pDescriptor = SkipElementDescriptor( pDescriptor );
        }

        return WalkElement( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
    }
}

//==============================================================================

s32 audio_descriptor_runtime::WalkDescriptor( f32 BaseTime, u16* pDescriptor, audio_package* pPackage, descriptor_visitor& Visitor, u32& RecursionDepth )
{
    u32 DescriptorIndex = (u32)(*pDescriptor++);
    u32 DescriptorType  = GET_DESCRIPTOR_TYPE( DescriptorIndex );
    u32 ParameterSize   = 0;
    s32 Result          = 0;

    if( GET_DESCRIPTOR_HAS_PARAMS( DescriptorIndex ) )
    {
        ParameterSize = *(pDescriptor+1) >> 1;
    }

    if( ++RecursionDepth > MAX_DESCRIPTOR_RECURSION_DEPTH )
    {
        ASSERT( 0 );
    }
    else
    {
        pDescriptor += 1+ParameterSize;

        switch( DescriptorType )
        {
            case SIMPLE:
            {
                Result = WalkSimple( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
                break;
            }

            case COMPLEX:
            {
                Result = WalkComplex( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
                break;
            }

            case RANDOM_LIST:
            {
                Result = WalkRandomList( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
                break;
            }

            case WEIGHTED_LIST:
            {
                Result = WalkWeightedList( BaseTime, pDescriptor, pPackage, Visitor, RecursionDepth );
                break;
            }

            default:
            {
                ASSERT( 0 );
                Result = 0;
                break;
            }
        }
    }

    --RecursionDepth;
    return Result;
}

//==============================================================================

s32 audio_descriptor_runtime::AppendDescriptor( f32 BaseTime, u16* pDescriptor, voice* pVoice, audio_package* pPackage )
{
    append_descriptor_visitor Visitor( *this, pVoice, pPackage );
    return WalkDescriptor( BaseTime, pDescriptor, pPackage, Visitor, pVoice->RecursionDepth );
}

//==============================================================================

s32 audio_descriptor_runtime::IsCold( char* pIdentifier )
{
    audio_package* pPackage;
    u16*           pDescriptor;
    char*          pString;

    // Find the descriptor.
    pDescriptor = Packages().FindDescriptorByName( pIdentifier, &pPackage, pString );

    if( pDescriptor )
    {
        return IsDescriptorCold( pDescriptor, pPackage );
    }
    else
    {
        return 0;
    }
}

//==============================================================================

s32 audio_descriptor_runtime::IsDescriptorCold( u16* pDescriptor, audio_package* pPackage )
{
    u32 RecursionDepth = 0;
    cold_descriptor_visitor Visitor;
    return WalkDescriptor( 0.0f, pDescriptor, pPackage, Visitor, RecursionDepth );
}

//==============================================================================

f32 audio_descriptor_runtime::GetSampleLengthSeconds( index_type Type, u32 Index, audio_package* pPackage )
{
    sample_temperature Temperature;
    void*              pSamples;

    ASSERT( pPackage );
    if( !pPackage )
    {
        return 0.0f;
    }

    switch( Type )
    {
        case HOT_INDEX:
        {
            Temperature = HOT;
            pSamples    = pPackage->m_HotSamples;
            break;
        }

        case COLD_INDEX:
        {
            Temperature = COLD;
            pSamples    = pPackage->m_ColdSamples;
            break;
        }

        default:
        {
            ASSERT( 0 );
            return 0.0f;
        }
    }

    ASSERT( pPackage->m_SampleIndices[ Temperature ] );
    ASSERT( Index < (u32)pPackage->m_Header.nSampleIndices[ Temperature ] );
    if( (Index >= (u32)pPackage->m_Header.nSampleIndices[ Temperature ]) || !pPackage->m_SampleIndices[ Temperature ] || !pSamples )
    {
        return 0.0f;
    }

    s32 nChannels = (s32)(pPackage->m_SampleIndices[ Temperature ][ Index+1 ] - pPackage->m_SampleIndices[ Temperature ][ Index ]);
    ASSERT( (nChannels > 0) && (nChannels <= 2 ) );
    if( (nChannels <= 0) || (nChannels > 2) )
    {
        return 0.0f;
    }

    u32            SampleIndex = (u32)pPackage->m_SampleIndices[ Temperature ][ Index ];
    byte*          pBase       = (byte*)pSamples + (SampleIndex * sizeof(sample_header));
    sample_header* pSample     = (sample_header*)pBase;

    ASSERT( pSample->SampleRate > 0 );
    if( pSample->SampleRate <= 0 )
    {
        return 0.0f;
    }

    return (f32)pSample->nSamples / (f32)pSample->SampleRate;
}

//==============================================================================

f32 audio_descriptor_runtime::GetDescriptorLengthSeconds( u16* pDescriptor, audio_package* pPackage )
{
    u32 RecursionDepth = 0;
    length_descriptor_visitor Visitor( *this, pPackage );

    WalkDescriptor( 0.0f, pDescriptor, pPackage, Visitor, RecursionDepth );
    return Visitor.GetLengthSeconds();
}

//==============================================================================

f32 audio_descriptor_runtime::GetLengthSeconds( const char* pIdentifier )
{
    f32            Result = 0.0f;
    audio_package* pPackage;
    u16*           pDescriptor;
    char*          DescriptorName   = NULL;

    // Find the decriptor by name.
    pDescriptor = Packages().FindDescriptorByName( pIdentifier, &pPackage, DescriptorName );

    // Only if it could be found...
    if( pDescriptor )
    {
        Result = GetDescriptorLengthSeconds( pDescriptor, pPackage );
    }

    return Result;
}

//==============================================================================

void audio_descriptor_runtime::GetVoiceParameters( uncompressed_parameters* pParams, u16* pDescriptor, audio_package* pPackage, char* pDescriptorName )
{
    (void)pDescriptorName;
    // Inherit some parameters from parent.
    pParams->PitchVariance  = pPackage->m_Header.Params.PitchVariance;
    pParams->VolumeVariance = pPackage->m_Header.Params.VolumeVariance;
    pParams->Pan2d          = pPackage->m_Header.Params.Pan2d;
    pParams->Pan3d          = pPackage->m_Header.Params.Pan3d;
    pParams->Priority       = pPackage->m_Header.Params.Priority;
    pParams->UserData       = pPackage->m_Header.Params.UserData;
    pParams->ReplayDelay    = pPackage->m_Header.Params.ReplayDelay;
    pParams->RolloffCurve   = pPackage->m_Header.Params.RolloffCurve;
    pParams->PlayPercent    = pPackage->m_Header.Params.PlayPercent;

#ifndef X_RETAIL
    if( AUDIO_TWEAK )
    {
        s_pTweakDescriptor = pDescriptorName;
    }
#endif // X_RETAIL

    // Decode the parameters.
    DecodeParameters( pParams, pDescriptor );

#ifndef X_RETAIL
    if( AUDIO_TWEAK )
    {
        s_pTweakDescriptor = NULL;
    }
#endif // X_RETAIL
}

//==============================================================================

void audio_descriptor_runtime::GetElementParameters( uncompressed_parameters* pParams, u16* pDescriptor, voice* pVoice )
{
    // Inherit some parameters from parent.
    pParams->PitchVariance  = pVoice->Params.PitchVariance;
    pParams->VolumeVariance = pVoice->Params.VolumeVariance;
    pParams->Pan2d          = pVoice->Params.Pan2d;
    pParams->Pan3d          = pVoice->Params.Pan3d;
    pParams->Priority       = pVoice->Params.Priority;
    pParams->UserData       = pVoice->Params.UserData;
    pParams->ReplayDelay    = pVoice->Params.ReplayDelay;
    pParams->RolloffCurve   = pVoice->Params.RolloffCurve;
    pParams->PlayPercent    = pVoice->Params.PlayPercent;

    // Decode the parameters.
    DecodeParameters( pParams, pDescriptor );
}

//==============================================================================

u32 audio_descriptor_runtime::GetUserData( const char* pIdentifier )
{
    audio_package* pPackage;
    u16*           pDescriptor;
    char*          pString;
    u32            Result = 0;

    // Find the descriptor.
    pDescriptor = Packages().FindDescriptorByName( pIdentifier, &pPackage, pString );

    // Find it?
    if( pDescriptor )
    {
        uncompressed_parameters Params;

        // Decode the parameters.
        GetVoiceParameters( &Params, pDescriptor, pPackage, (char*)pIdentifier );

        // Get the user data.
        Result = Params.UserData;
    }

    // Tell the world!
    return Result;
}

//==============================================================================

xbool audio_descriptor_runtime::GetVoiceParameters( const char* pIdentifier, uncompressed_parameters& Params )
{
    audio_package* pPackage;
    u16*           pDescriptor;
    char*          pString;

    // Find the descriptor.
    pDescriptor = Packages().FindDescriptorByName( pIdentifier, &pPackage, pString );

    // Find it?
    if( pDescriptor )
    {
        // Decode the parameters.
        GetVoiceParameters( &Params, pDescriptor, pPackage, (char*)pIdentifier );
        return TRUE;
    }
    return FALSE;
}

//==============================================================================

s32 audio_descriptor_runtime::GetPriority( const char* pIdentifier )
{
    uncompressed_parameters Params;

    // Decode the parameters.
    if( GetVoiceParameters( pIdentifier, Params ) )
    {
        return Params.Priority;
    }
    else
    {
        return 0;
    }
}

//==============================================================================

f32 audio_descriptor_runtime::GetFarFalloff( const char* pIdentifier )
{
    uncompressed_parameters Params;

    // Decode the parameters.
    if( GetVoiceParameters( pIdentifier, Params ) )
    {
        return Params.FarFalloff;
    }
    else
    {
        return 0.0f;
    }
}

//==============================================================================

f32 audio_descriptor_runtime::GetNearFalloff( const char* pIdentifier )
{
    uncompressed_parameters Params;

    // Decode the parameters.
    if( GetVoiceParameters( pIdentifier, Params ) )
    {
        return Params.NearFalloff;
    }
    else
    {
        return 0.0f;
    }
}

//==============================================================================
