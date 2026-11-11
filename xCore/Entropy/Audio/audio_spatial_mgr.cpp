//==============================================================================
//
//  audio_spatial_mgr.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_spatial_mgr.hpp"
#include "x_log.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

#define FALLOFF_TABLE_SIZE (9)

typedef f32 falloff_table[FALLOFF_TABLE_SIZE];

static falloff_table s_FalloffTables[NUM_ROLLOFFS] =
{
    { 1.00000f, 0.87500f, 0.75000f, 0.62500f, 0.50000f, 0.37500f, 0.25000f, 0.12500f, 0.00000f }, // LINEAR_ROLLOFF
    { 1.00000f, 0.76562f, 0.56250f, 0.39062f, 0.25000f, 0.14062f, 0.06250f, 0.01562f, 0.00000f }, // FAST_ROLLOFF
    { 1.00000f, 0.93541f, 0.86602f, 0.79056f, 0.70710f, 0.61237f, 0.50000f, 0.35355f, 0.00000f }  // SLOW_ROLLOFF
};

f32 s_EarVolume       = 0.0f;
f32 s_ZoneVolume      = 0.0f;
s32 s_ZoneID          = 0;
f32 s_ZoneFinalVolume = 0.0f;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

audio_spatial_mgr::audio_spatial_mgr( void )
{
    m_pUsedEars   = NULL;
    m_pFreeEars   = NULL;
    m_pCurrentEar = NULL;
}

//==============================================================================

audio_spatial_mgr::~audio_spatial_mgr( void )
{
}

//==============================================================================

void audio_spatial_mgr::Init( void )
{
    // FOr each ear...
    for( s32 i=0 ; i<MAX_EARS ; i++ )
    {
        // Initialize each ear.
        m_Ear[i].Volume   = 1.0f;
        m_Ear[i].Sequence = 0;

        // Link it.
        m_Ear[i].pNext = &m_Ear[i+1];
    }

    // Terminate list.
    m_Ear[MAX_EARS-1].pNext = NULL;

    // Init free and used ears.
    m_pUsedEars   = NULL;
    m_pFreeEars   = &m_Ear[0];
    m_pCurrentEar = NULL;
}

//==============================================================================

void audio_spatial_mgr::Kill( void )
{
    m_pUsedEars   = NULL;
    m_pFreeEars   = NULL;
    m_pCurrentEar = NULL;
}

//==============================================================================

void audio_spatial_mgr::GetEar( ear_id EarID, matrix4& W2V, vector3& Position, s32& ZoneID, f32& Volume )
{
    m_EarLock.Acquire();

    // Convert id.
    audio_spatial_ear* pEar = IdToEar( EarID );
    ASSERT( pEar );

    if( pEar )
    {
        W2V      = pEar->W2V;
        Position = pEar->Position;
        Volume   = pEar->Volume;
        ZoneID   = pEar->ZoneID;
    }

    m_EarLock.Release();
}

//==============================================================================

void audio_spatial_mgr::SetEar( ear_id EarID, const matrix4& W2V, const vector3& Position, s32 ZoneID, f32 Volume )
{
    m_EarLock.Acquire();

    ASSERT( (Volume >= 0.0f) && (Volume <= 1.0f) );

    // Convert id.
    audio_spatial_ear* pEar = IdToEar( EarID );
    ASSERT( pEar );

    if( pEar )
    {
        pEar->W2V      = W2V;
        pEar->Volume   = Volume;
        pEar->Position = Position;
        pEar->ZoneID   = ZoneID;
    }

    m_EarLock.Release();
}

//==============================================================================

f32 audio_spatial_mgr::ComputeFalloff( f32 PercentToFarClip, s32 TableID )
{
    ASSERT( TableID >= 0 );
    ASSERT( TableID < NUM_ROLLOFFS );
    ASSERTS( PercentToFarClip >= 0.0f, xfs("%f", PercentToFarClip) );
    ASSERTS( PercentToFarClip <= 1.0f, xfs("%f", PercentToFarClip) );

    if( PercentToFarClip <= 0.0f )
    {
        return( s_FalloffTables[TableID][0] );
    }
    else if( PercentToFarClip >= 1.0f )
    {
        return( s_FalloffTables[TableID][FALLOFF_TABLE_SIZE-1] );
    }
    else
    {
        s32 i;
        f32 f;
        f32 d;

        // Convert from [0..1] to [0..FALLOFF_TABLE_SIZE-1]
        f = PercentToFarClip * (f32)(FALLOFF_TABLE_SIZE-1);

        // Snag integer portion.
        i = (s32)f;

        // Compute delta.
        d = f - (f32)i;

        // Tell the world...
        return (s_FalloffTables[TableID][i] * (1.0f - d)) + (s_FalloffTables[TableID][i+1] * d);
    }
}

//==============================================================================

void audio_spatial_mgr::Calculate3dVolume( f32               NearClipOrig,
                                           f32               FarClipOrig,
                                           s32               VolumeRolloff,
                                           const vector3&    WorldPosition,
                                           s32               ZoneID,
                                           f32&              Volume )
{
    audio_spatial_ear* pEar = m_pUsedEars;

    // Init.
    Volume = 0.0f;

    // Do it for all the ears...
    while( pEar )
    {
        f32 NearClip = NearClipOrig;
        f32 FarClip  = FarClipOrig;
        f32 EarVolume;

        // Put sound in ear-space, get distance to sound squared
        vector3 Position  = pEar->W2V * WorldPosition;
        f32     Distance2 = Position.LengthSquared();

        // Calculate the actual far clip plane.
        FarClip *= m_FarClip;

        // Is sound beyond the far clip?
        if( Distance2 >= FarClip*FarClip )
        {
            // Can't hear it...
            EarVolume = 0.0f;
        }
        else
        {
            // Calculate the actual near clip plane.
            NearClip *= m_NearClip;

            // Sound inside the near clip? Is the near clip too close to (or beyond) the far clip?
            if( (Distance2 <= NearClip*NearClip) || ((NearClip+1.0f) >= FarClip) )
            {
                // Full volume.
                EarVolume = 1.0f;
            }
            else
            {
                // Calculate the actual distance to the sound.
                f32 Distance = x_sqrt( Distance2 );

                // Calculate percentage to the far clip.
                f32 Percent = (Distance-NearClip) / (FarClip-NearClip);

                // Calculate the distance attenuation.
                EarVolume = ComputeFalloff( Percent, VolumeRolloff );
            }
        }

        // Include the ears volume in the calculation.
        EarVolume *= pEar->Volume;

        // Now take zone volume into account
        EarVolume *= pEar->ZoneVolumes[ ZoneID ];

        // Is this ear more dominant? (= is required, do NOT remove it!)
        if( EarVolume >= Volume )
        {
            Volume = EarVolume;
        }

        // Walk the list.
        pEar = pEar->pNext;
    }
}

//==============================================================================

void audio_spatial_mgr::Calculate2dPan( f32      Pan2d,
                                        vector4& Pan3d )
{
    s32     i;

    // Convert to [-90..90]
    Pan2d *= 90;

    i = (s32)Pan2d;

    if( i < -90 )
    {
        i = -90;
    }
    if( i > 90 )
    {
        i = 90;
    }

    // Get the stereo pan.
    Pan3d = m_StereoPan[ i+90 ];
}

//==============================================================================

void audio_spatial_mgr::Calculate3dVolumeAndPan( f32            NearClipOrig,
                                                 f32            FarClipOrig,
                                                 s32            VolumeRolloff,
                                                 f32            NearDiffusion,
                                                 f32            FarDiffusion,
                                                 const vector3& WorldPosition,
                                                 s32            ZoneID,
                                                 f32&           Volume,
                                                 vector4&       Pan,
                                                 s32&           DegreesToSound,
                                                 s32&           PrevDegreesToSound,
                                                 s32            EarID )
{
    xbool              bDoOnce = FALSE;
    audio_spatial_ear* pEar    = m_pUsedEars;
    audio_spatial_ear* pBest   = NULL;
    vector3            Position;

    // Init.
    Volume = 0.0f;

    // Do it for all the ears (if no ear specified!)
    if( EarID )
    {
        pEar    = IdToEar( EarID );
        bDoOnce = TRUE;
    }

    if( !pEar )
    {
        // No ear?  Very bad.  Oh, well.  Life goes on.
        LOG_ERROR( "audio_mgr::Calculate3dVolumeAndPan", "No ear for 3D sound." );
        return;
    }

    while( pEar )
    {
        f32 NearClip = NearClipOrig;
        f32 FarClip  = FarClipOrig;
        f32 EarVolume;

        // Put sound in ear-space.
        vector3 EarPosition = pEar->W2V * WorldPosition;

        // Get distance to sound squared.
        f32 Distance2 = EarPosition.LengthSquared();

        // Calculate the actual far clip plane.
        FarClip *= m_FarClip;

        // Is sound beyond the far clip?
        if( Distance2 >= FarClip*FarClip )
        {
            // Can't hear it...
            EarVolume = 0.0f;
        }
        else
        {
            f32 Distance;
            f32 Percent;

            // Calculate the actual near clip plane.
            NearClip *= m_NearClip;

            // Sound inside the near clip? Is the near clip too close too (or beyond) the far clip?
            if( (Distance2 <= NearClip*NearClip) || ((NearClip+1.0f) >= FarClip) )
            {
                // Full volume.
                EarVolume = 1.0f;
            }
            else
            {
                // Calculate the actual distance to the sound.
                Distance = x_sqrt( Distance2 );

                // Calculate percentage to the far clip.
                Percent = (Distance-NearClip) / (FarClip-NearClip);

                // Calculate the distance attenuation.
                EarVolume = ComputeFalloff( Percent, VolumeRolloff );
            }
        }

        // Inlude the ears volume in the calculation.
        EarVolume *= pEar->Volume;

        s_EarVolume  = EarVolume;
        s_ZoneVolume = pEar->ZoneVolumes[ ZoneID ];
        s_ZoneID     = ZoneID;

        // Now take zone volumes into consideration.
        EarVolume *= pEar->ZoneVolumes[ ZoneID ];

        s_ZoneFinalVolume = EarVolume;

        // More dominant? (= is required, do NOT remove it!)
        if( EarVolume >= Volume )
        {
            Volume   = EarVolume;
            pBest    = pEar;
            Position = EarPosition;
        }

        // Single ear or search entire list?
        if( bDoOnce )
        {
            // We are DONE!
            pEar = NULL;
        }
        else
        {
            // Next ear!
            pEar = pEar->pNext;
        }
    }

#ifndef X_EDITOR
    // Better have one...
    ASSERT( pBest );
#endif

    if( pBest )
    {
        // Now, re-calculate distance2 ignoring elevation.  This will make
        // a sound directly overhead sound like a 100 percent diffuse sound.
        // Given that the current systems don't support speakers on the ceiling,
        // this is the best approximation available.
        f32 Distance2 = Position.GetX()*Position.GetX() + Position.GetZ()*Position.GetZ();

        // Calculate the actual near and far diffusion plane.
        NearDiffusion *= m_NearClip;
        FarDiffusion  *= m_FarClip;

        // Inside the near diffusion plane?
        if( Distance2 <= (NearDiffusion*NearDiffusion) )
        {
            // 100 percent diffuse...
            Pan = m_Diffuse;

            // Initialize em...
            DegreesToSound     = -1;
            PrevDegreesToSound = 0;
        }
        else
        {
            // Calculate the actual distance in the XZ plane.
            f32 Distance = x_sqrt( Distance2 );

            // Calculate "angle from the ear to the sound" *IN SPEAKER SPACE*, where:
            //
            //   0 degrees is out of your nose.
            // -90 degrees is off your left shoulder.
            // +90 degrees is off your right shoulder.
            //
            f32 Angle = RAD_TO_DEG( x_atan2( -Position.GetX(), Position.GetZ() ) );

            while( Angle < 0.0f )
            {
                Angle += 360.0f;
            }
            while( Angle >= 360.0f )
            {
                Angle -= 360.0f;
            }
            s32 i = (s32)Angle;
            ASSERT( i>=0 );
            ASSERT( i<360 );

            // Set the degrees to the sound.
            DegreesToSound = i;

            // Get Delta to new sound position and and apply to old position % 720
            radian Delta = x_MinAngleDiff( DEG_TO_RAD(DegreesToSound), DEG_TO_RAD(PrevDegreesToSound % 360) );
            DegreesToSound = PrevDegreesToSound + (s32)(RAD_TO_DEG(Delta));

            while( DegreesToSound <    0 )
            {
                DegreesToSound += 720;
            }

            while( DegreesToSound >= 720 )
            {
                DegreesToSound -= 720;
            }

            // Store off position as previous position
            PrevDegreesToSound = DegreesToSound;

            // Is sound beyond the diffusion far plane? Is the near diffusion plane too close to (or beyond) the far diffusion plane?
            if( Distance >= FarDiffusion || ((NearDiffusion+1.0f) >= FarDiffusion) )
            {
                // Yes? Its 100 percent directional then...
                Pan = m_Pan[i];
            }
            else
            {
/*
                // Calculate the percentage of diffusion.
                f32 Percent = 1.0f - (Distance-NearDiffusion) / (FarDiffusion-NearDiffusion);

                // Scale it based on percent diffusion.
                Pan = m_Pan[i] + Percent * (m_Diffuse - m_Pan[i]);

                // Normalize it to maintain constant power.
                Pan.Normalize();
*/
                // Hack to test!
                Pan = m_Pan[i];
            }
        }
    }
}

//==============================================================================

void audio_spatial_mgr::SetSpeakerAngles( s32 FrontLeft, s32 FrontRight, s32 BackRight, s32 BackLeft, s32 nSpeakers )
{
    s32 i;
    s32 j;
    f32 DeltaTheta;
    f32 Angle;
    f32 Q;
    f32 P0;
    f32 P1;

    // Set class variables
    m_FrontLeft  = FrontLeft;
    m_FrontRight = FrontRight;
    m_BackRight  = BackRight;
    m_BackLeft   = BackLeft;
    m_nSpeakers  = nSpeakers;

    // Default the stereo pan.
    DeltaTheta = 180.0f;

    for( i=0 ; i<180 ; i++ )
    {
        Angle = (f32)(i);
        Q = Angle / DeltaTheta;
        P0 = x_sqrt( 1.0f - Q );
        P1 = x_sqrt( Q );
        m_StereoPan[i].Set( P0, P1, 0.0f, 0.0f );
    }

    m_StereoPan[180].Set( 0.0f, 1.0f, 0.0f, 0.0f );

    switch( nSpeakers )
    {
        // Mono
        case 1:
        {
            // Set the diffuse vector for 1 speaker.
            m_Diffuse.Set( 1.0f, 1.0f, 0.0f, 0.0f );

            for( i=0 ; i<PAN_TABLE_ENTRIES ; i++ )
            {
                m_Pan[i].Set( 1.0f, 1.0f, 0.0f, 0.0f );
            }

            for( i=0 ; i<=180 ; i++ )
            {
                m_StereoPan[i].Set( 1.0f, 0.0f, 0.0f, 0.0f );
            }

            break;

        // Stereo
        }
        case 2:
        {
            ASSERT( FrontLeft  <= 0 );
            ASSERT( FrontRight >= 0 );

            // Set the diffuse vector for 2 speakers.
            P0 = 1.0f / x_sqrt( 2.0f );
            m_Diffuse.Set( P0, P0, 0.0f, 0.0f );

            DeltaTheta = (f32)x_abs( FrontRight - FrontLeft );

            for( i=FrontLeft ; (i<=FrontRight)  && (i<360) ; i++ )
            {
                j = i;

                if( j<0 )
                {
                    j += 360;
                }
                Angle = (f32)(i-FrontLeft);
                Q = Angle / DeltaTheta;
                P0 = x_sqrt( 1.0f - Q );
                P1 = x_sqrt( Q );
                m_Pan[j].Set( P0, P1, 0.0f, 0.0f );
            }

            DeltaTheta = (f32)x_abs( FrontLeft+360 - FrontRight );

            for( i=FrontRight ; (i<= FrontLeft+360) && (i<360) ; i++ )
            {
                Angle = (f32)(i-FrontRight);
                Q = Angle / DeltaTheta;
                P0 = x_sqrt( 1.0f - Q );
                P1 = x_sqrt( Q );
                m_Pan[i].Set( P1, P0, 0.0f, 0.0f );
            }

            break;
        }

        default:
        {
            ASSERT(0);
            break;
        }
    }
}

//==============================================================================

void audio_spatial_mgr::GetSpeakerAngles( s32& FrontLeft, s32& FrontRight, s32& BackRight, s32& BackLeft, s32& nSpeakers )
{
    // Set class variables
    FrontLeft  = m_FrontLeft;
    FrontRight = m_FrontRight;
    BackRight  = m_BackRight;
    BackLeft   = m_BackLeft;
    nSpeakers  = m_nSpeakers;
}

//==============================================================================

audio_spatial_mgr::audio_spatial_ear* audio_spatial_mgr::IdToEar( ear_id EarID )
{
    audio_spatial_ear* pEar = &m_Ear[0];
    s32    Index    = ((EarID & 0xffff)-1);
    u32    Sequence = ((EarID >> 16) & 0x0000ffff);

    // Error check.
    if( (Index < 0) || (Index >= MAX_EARS) )
    {
        return NULL;
    }

    // Does the sequence match?
    if( ((pEar+Index)->Sequence & 0x0000ffff) == Sequence )
    {
        return pEar+Index;
    }
    else
    {
        return NULL;
    }
}

//==============================================================================

ear_id audio_spatial_mgr::EarToId( audio_spatial_ear* pEar )
{
    s32 Index = pEar-&m_Ear[0];

    // Encode index and sequence.
    return (((Index+1) & 0xffff) + (pEar->Sequence << 16));
}

//==============================================================================

xbool audio_spatial_mgr::IsEarInBox( const bbox& Box )
{
    xbool Result = FALSE;

    m_EarLock.Acquire();

    for( audio_spatial_ear* pEar = m_pUsedEars; pEar; pEar = pEar->pNext )
    {
        if( pEar->Volume && Box.Intersect( pEar->Position ) )
        {
            Result = TRUE;
            break;
        }
    }

    m_EarLock.Release();

    return Result;
}

//==============================================================================

ear_id audio_spatial_mgr::GetFirstEar( void )
{
    ASSERT( m_pCurrentEar == NULL );
    m_pCurrentEar = m_pUsedEars;

    if( m_pCurrentEar )
    {
        return EarToId(m_pCurrentEar);
    }
    else
    {
        return 0;
    }
}

//==============================================================================

ear_id audio_spatial_mgr::GetNextEar( void )
{
    if( m_pCurrentEar && m_pCurrentEar->pNext )
    {
        m_pCurrentEar = m_pCurrentEar->pNext;
        return EarToId(m_pCurrentEar);
    }
    else
    {
        return 0;
    }
}

//==============================================================================

void audio_spatial_mgr::ResetCurrentEar( void )
{
    m_pCurrentEar = NULL;
}

//==============================================================================

ear_id audio_spatial_mgr::CreateEar( void )
{
    m_EarLock.Acquire();
    // Nuke the current ear.
    m_pCurrentEar = NULL;

    // Only if an ear is available...
    if( m_pFreeEars )
    {
        audio_spatial_ear* pEar;

        // Take it out of free list.
        pEar        = m_pFreeEars;
        m_pFreeEars = m_pFreeEars->pNext;

        // Put it in the used list
        pEar->pNext = m_pUsedEars;
        m_pUsedEars = pEar;

        // Initialize it.
        pEar->W2V.Identity();
        pEar->Position.Zero();
        pEar->Volume   = 1.0f;
        pEar->ZoneID   = ZONELESS;

        // Set volumes.
        for( s32 i=0 ; i<MAX_ZONES ; i++ )
        {
            pEar->ZoneVolumes[i] = 1.0f;
        }

        // its all good
        ear_id EarID = EarToId( pEar );
        m_EarLock.Release();
        return EarID;
    }
    else
    {
        m_EarLock.Release();

        // Out of ears!
        x_DebugMsg( "Out of audio ears.\n" );
        return 0;
    }
}

//==============================================================================

void audio_spatial_mgr::DestroyEar( ear_id EarID )
{
    m_EarLock.Acquire();

    audio_spatial_ear* pEar = IdToEar( EarID );

    if( pEar )
    {
        // Nuke the current ear.
        m_pCurrentEar = NULL;

        audio_spatial_ear* pCurr = m_pUsedEars;
        audio_spatial_ear* pPrev = NULL;

        // Bump sequence
        if( ++pEar->Sequence >= 32768 )
        {
            pEar->Sequence = 1;
        }

        // Walk the entire list (all 4 of em!!!)
        while( pCurr )
        {
            // Found it?
            if( pCurr == pEar )
            {
                break;
            }

            // Next!
            pPrev = pCurr;
            pCurr = pCurr->pNext;
        }

        // Find it?
        if( pCurr )
        {
            // Make sure its not first in the list...
            if( pPrev )
            {
                // Remove it from the list.
                pPrev->pNext = pCurr->pNext;
            }
            else
            {
                // Its first in the list...
                m_pUsedEars  = m_pUsedEars->pNext;
            }

            // Put it in the free list.
            pCurr->pNext = m_pFreeEars;
            m_pFreeEars  = pCurr;
        }
    }
    else
    {
        // Trying to destroy an invalid ear...
        x_DebugMsg( "Trying to destroy an invalid ear.\n" );
    }

    m_EarLock.Release();
}

//==============================================================================

void audio_spatial_mgr::SetSpeakerConfig( s32 SpeakerConfig )
{
    m_SpeakerConfig = SpeakerConfig;

    switch( SpeakerConfig )
    {
        case SPEAKERS_MONO:
        {
            SetSpeakerAngles( 0, 0, 0, 0, 1 );
            break;
        }

        case SPEAKERS_STEREO:
        {
            SetSpeakerAngles( -90, 90, 0, 0, 2 );
            break;
        }

        default:
        {
            ASSERT( 0 );
            break;
        }
    }
}

//==============================================================================

void audio_spatial_mgr::SetClip( f32 NearClip, f32 FarClip )
{
    m_NearClip = NearClip;
    m_FarClip  = FarClip;
}

//==============================================================================

void audio_spatial_mgr::GetClip( f32& NearClip, f32& FarClip )
{
    NearClip = m_NearClip;
    FarClip  = m_FarClip;
}

//==============================================================================

void audio_spatial_mgr::UpdateEarZoneVolume( ear_id EarID, s32 ZoneID, f32 Volume )
{
    // Convert id.
    audio_spatial_ear* pEar = IdToEar( EarID );
    ASSERT( pEar );

    if( pEar )
    {
        ASSERT( (ZoneID >= 0) && (ZoneID <MAX_ZONES) );

        if( (ZoneID >= 0) && (ZoneID <MAX_ZONES) )
        {
            pEar->ZoneVolumes[ ZoneID ] = Volume;
        }
    }
}

//==============================================================================

void audio_spatial_mgr::UpdateEarZoneVolumes( ear_id EarID, f32* pVolumes )
{
    // Convert id.
    audio_spatial_ear* pEar = IdToEar( EarID );
    ASSERT( pEar );

    if( pEar )
    {
        for( s32 i=0 ; i<MAX_ZONES ; i++ )
        {
            pEar->ZoneVolumes[i] = pVolumes[i];
        }
    }
}
