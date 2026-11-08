//==============================================================================
//
//  audio_spatial_mgr.hpp
//
//==============================================================================

#ifndef AUDIO_SPATIAL_MGR_HPP
#define AUDIO_SPATIAL_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"
#include "Audio/audio_spatial_types.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define PAN_TABLE_ENTRIES (360)
#define MAX_EARS          (4)

//==============================================================================
//  TYPES
//==============================================================================

class audio_spatial_mgr
{
private:

struct audio_spatial_ear
{
            audio_spatial_ear* pNext;
            matrix4            W2V;
            vector3            Position;
            f32                Volume;
            u32                Sequence;
            s32                ZoneID;
            f32                ZoneVolumes[MAX_ZONES];
};

public:

                            audio_spatial_mgr       ( void );
                           ~audio_spatial_mgr       ( void );

            void            Init                    ( void );
            void            Kill                    ( void );

            void            SetClip                 ( f32               NearClip,
                                                      f32               FarClip );
            void            GetClip                 ( f32&              NearClip,
                                                      f32&              FarClip );

            void            GetEar                  ( ear_id            EarID,
                                                      matrix4&          W2V,
                                                      vector3&          Position,
                                                      s32&              ZoneID,
                                                      f32&              Volume );
            void            SetEar                  ( ear_id            EarID,
                                                      const matrix4&    W2V,
                                                      const vector3&    Position,
                                                      s32               ZoneID,
                                                      f32               Volume );

            void            UpdateEarZoneVolume     ( ear_id            EarID,
                                                      s32               ZoneID,
                                                      f32               Volume );
            void            UpdateEarZoneVolumes    ( ear_id            EarID,
                                                      f32*              pVolumes );

            xbool           IsEarInBox( const bbox& Box );
            ear_id          GetFirstEar             ( void );
            ear_id          GetNextEar              ( void );
            void            ResetCurrentEar         ( void );
            ear_id          CreateEar               ( void );
            void            DestroyEar              ( ear_id            EarID );

            void            SetSpeakerConfig        ( s32               SpeakerConfig );
            s32             GetSpeakerConfig        ( void ) const      { return m_SpeakerConfig; }
            void            SetSpeakerAngles        ( s32               FrontLeft,
                                                      s32               FrontRight,
                                                      s32               BackRight,
                                                      s32               BackLeft,
                                                      s32               nSpeakers );
            void            GetSpeakerAngles        ( s32&              FrontLeft,
                                                      s32&              FrontRight,
                                                      s32&              BackRight,
                                                      s32&              BackLeft,
                                                      s32&              nSpeakers );

            void            Calculate3dVolume       ( f32               NearClip,
                                                      f32               FarClip,
                                                      s32               VolumeRolloff,
                                                      const vector3&    WorldPosition,
                                                      s32               ZoneID,
                                                      f32&              Volume );
            void            Calculate3dVolumeAndPan ( f32               NearClip,
                                                      f32               FarClip,
                                                      s32               VolumeRolloff,
                                                      f32               NearDiffusion,
                                                      f32               FarDiffusion,
                                                      const vector3&    WorldPosition,
                                                      s32               ZoneID,
                                                      f32&              Volume,
                                                      vector4&          Pan,
                                                      s32&              DegreesToSound,
                                                      s32&              PrevDegreesToSound,
                                                      s32               EarID );
            void            Calculate2dPan          ( f32               Pan2d,
                                                      vector4&          Pan3d );
            f32             ComputeFalloff          ( f32               PercentToFarClip,
                                                      s32               TableID );

private:

            audio_spatial_ear* IdToEar              ( ear_id            EarID );
            ear_id          EarToId                 ( audio_spatial_ear* pEar );

private:

            vector4             m_Pan[PAN_TABLE_ENTRIES];
            vector4             m_StereoPan[181];
            vector4             m_Diffuse;
            s32                 m_FrontLeft;
            s32                 m_FrontRight;
            s32                 m_BackRight;
            s32                 m_BackLeft;
            s32                 m_nSpeakers;
            s32                 m_SpeakerConfig;
            f32                 m_NearClip;
            f32                 m_FarClip;
            xmutex              m_EarLock;
            audio_spatial_ear   m_Ear[MAX_EARS];
            audio_spatial_ear*  m_pFreeEars;
            audio_spatial_ear*  m_pUsedEars;
            audio_spatial_ear*  m_pCurrentEar;
};

//==============================================================================
#endif // AUDIO_SPATIAL_MGR_HPP
//==============================================================================
