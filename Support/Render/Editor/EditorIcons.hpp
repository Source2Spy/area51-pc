//=========================================================================
//
//  EditorIcons.hpp
//
//=========================================================================

#ifndef EDITOR_ICONS_HPP
#define EDITOR_ICONS_HPP

#include "x_files.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum class EditorIcon
{
    Invalid = -1,
    Anchor,
    NavigationNode,
    Portal,
    Speaker,
    Light,
    CharacterLight,
    DynamicLight,
    Note,
    ParticleEmitter,
    Trigger,
    CoverNode,
    Projector,
    CharacterTask,
    Loop,
    Gear,
    Marker,
    SimpleTrigger,
    ViewableTrigger,
    SpatialTrigger,
    Damage,
    AlienShield,
    BlueprintBag,
    BotSpawnPoint,
    ClosedDoor,
    Coupler,
    CaptureTheFlag,
    FocusObject,
    Group,
    JumpPad,
    MultiplayerSettings,
    OpenDoor,
    Padlock,
    PlayerObject,
    SpawnPoint,
    TwoWayArrow,
    VolumetricLight,
    NavigationConnection,
    Camera,
    Controller,
    InputSettings,
    LevelSettings,
    MusicLogic,
    Path,
    PictureInPicture,
    Spawner,
    Tracker,
    Sphere,
    HudObject,
    CinemaObject,
};

//=========================================================================
//  FUNCTIONS
//=========================================================================

// Draws an editor visualization icon using the supplied world transform.
void DrawEditorIcon( EditorIcon icon, matrix4 const& L2W, xbool isSelected, xcolor TintColor );

//=========================================================================
#endif // EDITOR_ICONS_HPP
//=========================================================================
