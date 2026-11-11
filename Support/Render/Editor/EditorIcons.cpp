//=========================================================================
//
//  EditorIcons.cpp
//
//=========================================================================

#include "EditorIcons.hpp"

#include "Render/PrimitiveBatch.hpp"

//=========================================================================
//  GENERATED ICON DATA
//=========================================================================

struct vertex
{
    f32 X, Y, Z;
    u8  R, G, B, A;
    f32 U, V;
};

#include "Icons/icon_ai_nav_con.hpp"
#include "Icons/icon_alien_shield.hpp"
#include "Icons/icon_arrow.hpp"
#include "Icons/icon_bot_spawn_point.hpp"
#include "Icons/icon_bp_anchor.hpp"
#include "Icons/icon_bp_bag.hpp"
#include "Icons/icon_camera.hpp"
#include "Icons/icon_cinema_obj.hpp"
#include "Icons/icon_character_light.hpp"
#include "Icons/icon_closed_door.hpp"
#include "Icons/icon_controller.hpp"
#include "Icons/icon_coupler.hpp"
#include "Icons/icon_cover.hpp"
#include "Icons/icon_ctf_flag.hpp"
#include "Icons/icon_damage.hpp"
#include "Icons/icon_dynamic_light.hpp"
#include "Icons/icon_focus_obj.hpp"
#include "Icons/icon_gear.hpp"
#include "Icons/icon_group.hpp"
#include "Icons/icon_ai_nav_node.hpp"
#include "Icons/icon_input_settings.hpp"
#include "Icons/icon_jump_pad.hpp"
#include "Icons/icon_level_settings.hpp"
#include "Icons/icon_light.hpp"
#include "Icons/icon_loop.hpp"
#include "Icons/icon_marker.hpp"
#include "Icons/icon_mp_settings.hpp"
#include "Icons/icon_music_logic.hpp"
#include "Icons/icon_note.hpp"
#include "Icons/icon_open_door.hpp"
#include "Icons/icon_padlock.hpp"
#include "Icons/icon_particle_emitter.hpp"
#include "Icons/icon_path.hpp"
#include "Icons/icon_pip.hpp"
#include "Icons/icon_player_obj.hpp"
#include "Icons/icon_portal.hpp"
#include "Icons/icon_projector.hpp"
#include "Icons/icon_simple_trigger.hpp"
#include "Icons/icon_sound.hpp"
#include "Icons/icon_spacial_trigger.hpp"
#include "Icons/icon_spawner.hpp"
#include "Icons/icon_spawn_point.hpp"
#include "Icons/icon_task_obj.hpp"
#include "Icons/icon_tracker.hpp"
#include "Icons/icon_trigger.hpp"
#include "Icons/icon_viewable_trigger.hpp"
#include "Icons/icon_volumetric_light.hpp"
#include "Icons/icon_sphere.hpp"
#include "Icons/icon_hud_obj.hpp"

namespace
{
struct icon_mesh
{
    s32           FacetCount;
    s32           VertexCount;
    const vertex* pVertices;
    const s16*    pIndices;
};

//-------------------------------------------------------------------------

enum
{
    MAX_ICON_VERTICES = 512,
    MAX_ICON_INDICES  = 1536
};

//=========================================================================

static
icon_mesh GetIconMesh( EditorIcon icon )
{
    switch( icon )
    {
        case EditorIcon::Anchor:               return { NUM_FACETS_ICON_BP_ANCHOR,         NUM_VERTICES_ICON_BP_ANCHOR,         s_vicon_bp_anchor,         s_iicon_bp_anchor };
        case EditorIcon::Trigger:              return { NUM_FACETS_ICON_TRIGGER,           NUM_VERTICES_ICON_TRIGGER,           s_vicon_trigger,           s_iicon_trigger };
        case EditorIcon::Speaker:              return { NUM_FACETS_ICON_SOUND,             NUM_VERTICES_ICON_SOUND,             s_vicon_sound,             s_iicon_sound };
        case EditorIcon::Portal:               return { NUM_FACETS_ICON_PORTAL,            NUM_VERTICES_ICON_PORTAL,            s_vicon_portal,            s_iicon_portal };
        case EditorIcon::NavigationNode:       return { NUM_FACETS_ICON_AI_NAV_NODE,       NUM_VERTICES_ICON_AI_NAV_NODE,       s_vicon_ai_nav_node,       s_iicon_ai_nav_node };
        case EditorIcon::Light:                return { NUM_FACETS_ICON_LIGHT,             NUM_VERTICES_ICON_LIGHT,             s_vicon_light,             s_iicon_light };
        case EditorIcon::CharacterLight:       return { NUM_FACETS_ICON_CHARACTER_LIGHT,   NUM_VERTICES_ICON_CHARACTER_LIGHT,   s_vicon_character_light,   s_iicon_character_light };
        case EditorIcon::DynamicLight:         return { NUM_FACETS_ICON_DYNAMIC_LIGHT,     NUM_VERTICES_ICON_DYNAMIC_LIGHT,     s_vicon_dynamic_light,     s_iicon_dynamic_light };
        case EditorIcon::ParticleEmitter:      return { NUM_FACETS_ICON_PARTICLE_EMITTER,  NUM_VERTICES_ICON_PARTICLE_EMITTER,  s_vicon_particle_emitter,  s_iicon_particle_emitter };
        case EditorIcon::Note:                 return { NUM_FACETS_ICON_NOTE,              NUM_VERTICES_ICON_NOTE,              s_vicon_note,              s_iicon_note };
        case EditorIcon::CoverNode:            return { NUM_FACETS_ICON_COVER,             NUM_VERTICES_ICON_COVER,             s_vicon_cover,             s_iicon_cover };
        case EditorIcon::Projector:            return { NUM_FACETS_ICON_PROJECTOR,         NUM_VERTICES_ICON_PROJECTOR,         s_vicon_projector,         s_iicon_projector };
        case EditorIcon::CharacterTask:        return { NUM_FACETS_ICON_TASK_OBJ,          NUM_VERTICES_ICON_TASK_OBJ,          s_vicon_task_obj,          s_iicon_task_obj };
        case EditorIcon::Loop:                 return { NUM_FACETS_ICON_LOOP,              NUM_VERTICES_ICON_LOOP,              s_vicon_loop,              s_iicon_loop };
        case EditorIcon::Gear:                 return { NUM_FACETS_ICON_GEAR,              NUM_VERTICES_ICON_GEAR,              s_vicon_gear,              s_iicon_gear };
        case EditorIcon::Marker:               return { NUM_FACETS_ICON_MARKER,            NUM_VERTICES_ICON_MARKER,            s_vicon_marker,            s_iicon_marker };
        case EditorIcon::SimpleTrigger:        return { NUM_FACETS_ICON_SIMPLE_TRIGGER,    NUM_VERTICES_ICON_SIMPLE_TRIGGER,    s_vicon_simple_trigger,    s_iicon_simple_trigger };
        case EditorIcon::ViewableTrigger:      return { NUM_FACETS_ICON_VIEWABLE_TRIGGER,  NUM_VERTICES_ICON_VIEWABLE_TRIGGER,  s_vicon_viewable_trigger,  s_iicon_viewable_trigger };
        case EditorIcon::SpatialTrigger:       return { NUM_FACETS_ICON_SPACIAL_TRIGGER,   NUM_VERTICES_ICON_SPACIAL_TRIGGER,   s_vicon_spacial_trigger,   s_iicon_spacial_trigger };
        case EditorIcon::Damage:               return { NUM_FACETS_ICON_DAMAGE,            NUM_VERTICES_ICON_DAMAGE,            s_vicon_damage,            s_iicon_damage };
        case EditorIcon::AlienShield:          return { NUM_FACETS_ICON_ALIEN_SHIELD,      NUM_VERTICES_ICON_ALIEN_SHIELD,      s_vicon_alien_shield,      s_iicon_alien_shield };
        case EditorIcon::BlueprintBag:         return { NUM_FACETS_ICON_BP_BAG,            NUM_VERTICES_ICON_BP_BAG,            s_vicon_bp_bag,            s_iicon_bp_bag };
        case EditorIcon::BotSpawnPoint:        return { NUM_FACETS_ICON_BOT_SPAWN_POINT,   NUM_VERTICES_ICON_BOT_SPAWN_POINT,   s_vicon_bot_spawn_point,   s_iicon_bot_spawn_point };
        case EditorIcon::ClosedDoor:           return { NUM_FACETS_ICON_CLOSED_DOOR,       NUM_VERTICES_ICON_CLOSED_DOOR,       s_vicon_closed_door,       s_iicon_closed_door };
        case EditorIcon::Coupler:              return { NUM_FACETS_ICON_COUPLER,           NUM_VERTICES_ICON_COUPLER,           s_vicon_coupler,           s_iicon_coupler };
        case EditorIcon::CaptureTheFlag:       return { NUM_FACETS_ICON_CTF_FLAG,          NUM_VERTICES_ICON_CTF_FLAG,          s_vicon_ctf_flag,          s_iicon_ctf_flag };
        case EditorIcon::FocusObject:          return { NUM_FACETS_ICON_FOCUS_OBJ,         NUM_VERTICES_ICON_FOCUS_OBJ,         s_vicon_focus_obj,         s_iicon_focus_obj };
        case EditorIcon::Group:                return { NUM_FACETS_ICON_GROUP,             NUM_VERTICES_ICON_GROUP,             s_vicon_group,             s_iicon_group };
        case EditorIcon::JumpPad:              return { NUM_FACETS_ICON_JUMP_PAD,          NUM_VERTICES_ICON_JUMP_PAD,          s_vicon_jump_pad,          s_iicon_jump_pad };
        case EditorIcon::MultiplayerSettings:  return { NUM_FACETS_ICON_MP_SETTINGS,       NUM_VERTICES_ICON_MP_SETTINGS,       s_vicon_mp_settings,       s_iicon_mp_settings };
        case EditorIcon::OpenDoor:             return { NUM_FACETS_ICON_OPEN_DOOR,         NUM_VERTICES_ICON_OPEN_DOOR,         s_vicon_open_door,         s_iicon_open_door };
        case EditorIcon::Padlock:              return { NUM_FACETS_ICON_PADLOCK,           NUM_VERTICES_ICON_PADLOCK,           s_vicon_padlock,           s_iicon_padlock };
        case EditorIcon::PlayerObject:         return { NUM_FACETS_ICON_PLAYER_OBJ,        NUM_VERTICES_ICON_PLAYER_OBJ,        s_vicon_player_obj,        s_iicon_player_obj };
        case EditorIcon::SpawnPoint:           return { NUM_FACETS_ICON_SPAWN_POINT,       NUM_VERTICES_ICON_SPAWN_POINT,       s_vicon_spawn_point,       s_iicon_spawn_point };
        case EditorIcon::TwoWayArrow:          return { NUM_FACETS_ICON_ARROW,             NUM_VERTICES_ICON_ARROW,             s_vicon_arrow,             s_iicon_arrow };
        case EditorIcon::VolumetricLight:      return { NUM_FACETS_ICON_VOLUMETRIC_LIGHT,  NUM_VERTICES_ICON_VOLUMETRIC_LIGHT,  s_vicon_volumetric_light,  s_iicon_volumetric_light };
        case EditorIcon::NavigationConnection: return { NUM_FACETS_ICON_AI_NAV_CON,        NUM_VERTICES_ICON_AI_NAV_CON,        s_vicon_ai_nav_con,        s_iicon_ai_nav_con };
        case EditorIcon::Camera:               return { NUM_FACETS_ICON_CAMERA,            NUM_VERTICES_ICON_CAMERA,            s_vicon_camera,            s_iicon_camera };
        case EditorIcon::Controller:           return { NUM_FACETS_ICON_CONTROLLER,        NUM_VERTICES_ICON_CONTROLLER,        s_vicon_controller,        s_iicon_controller };
        case EditorIcon::InputSettings:        return { NUM_FACETS_ICON_INPUT_SETTINGS,    NUM_VERTICES_ICON_INPUT_SETTINGS,    s_vicon_input_settings,    s_iicon_input_settings };
        case EditorIcon::LevelSettings:        return { NUM_FACETS_ICON_LEVEL_SETTINGS,    NUM_VERTICES_ICON_LEVEL_SETTINGS,    s_vicon_level_settings,    s_iicon_level_settings };
        case EditorIcon::MusicLogic:           return { NUM_FACETS_ICON_MUSIC_LOGIC,       NUM_VERTICES_ICON_MUSIC_LOGIC,       s_vicon_music_logic,       s_iicon_music_logic };
        case EditorIcon::Path:                 return { NUM_FACETS_ICON_PATH,              NUM_VERTICES_ICON_PATH,              s_vicon_path,              s_iicon_path };
        case EditorIcon::PictureInPicture:     return { NUM_FACETS_ICON_PIP,               NUM_VERTICES_ICON_PIP,               s_vicon_pip,               s_iicon_pip };
        case EditorIcon::Spawner:              return { NUM_FACETS_ICON_SPAWNER,           NUM_VERTICES_ICON_SPAWNER,           s_vicon_spawner,           s_iicon_spawner };
        case EditorIcon::Tracker:              return { NUM_FACETS_ICON_TRACKER,           NUM_VERTICES_ICON_TRACKER,           s_vicon_tracker,           s_iicon_tracker };
        case EditorIcon::Sphere:               return { NUM_FACETS_ICON_SPHERE,            NUM_VERTICES_ICON_SPHERE,            s_vicon_sphere,            s_iicon_sphere };
        case EditorIcon::HudObject:            return { NUM_FACETS_ICON_HUD_OBJ,           NUM_VERTICES_ICON_HUD_OBJ,           s_vicon_hud_obj,           s_iicon_hud_obj };
        case EditorIcon::CinemaObject:         return { NUM_FACETS_ICON_CINEMA_OBJ,        NUM_VERTICES_ICON_CINEMA_OBJ,        s_vicon_cinema_obj,        s_iicon_cinema_obj };
        default:                               return { 0, 0, NULL, NULL };
    }
}

//=========================================================================

static
xcolor TintColor( const vertex& source, xcolor tint )
{
    return xcolor( (u8)( ( source.R * tint.R + 127 ) / 255 ),
                   (u8)( ( source.G * tint.G + 127 ) / 255 ),
                   (u8)( ( source.B * tint.B + 127 ) / 255 ),
                   (u8)( ( source.A * tint.A + 127 ) / 255 ) );
}

//=========================================================================

static
xbool SubmitIconMesh( const icon_mesh& mesh, const matrix4& LocalToWorld, xcolor tint,
                      const render::primitive_draw_desc& material )
{
    if( !mesh.pVertices || !mesh.pIndices || mesh.VertexCount <= 0 || mesh.FacetCount <= 0 ||
        mesh.VertexCount > MAX_ICON_VERTICES || mesh.FacetCount * 3 > MAX_ICON_INDICES )
    {
        return FALSE;
    }

    render::primitive_vertex vertices[MAX_ICON_VERTICES];
    u16 indices[MAX_ICON_INDICES];
    for( s32 i = 0; i < mesh.VertexCount; ++i )
    {
        const vertex& source = mesh.pVertices[i];
        vertices[i] = render::primitive_vertex( vector3( source.X, source.Y, source.Z ),
                                                vector2( source.U, source.V ),
                                                TintColor( source, tint ) );
    }

    const s32 IndexCount = mesh.FacetCount * 3;
    for( s32 i = 0; i < IndexCount; ++i )
    {
        ASSERT( mesh.pIndices[i] >= 0 && mesh.pIndices[i] < mesh.VertexCount );
        indices[i] = (u16)mesh.pIndices[i];
    }

    return render::SubmitPrimitives( material, LocalToWorld, vertices, mesh.VertexCount, indices, IndexCount );
}
}

//=========================================================================

void DrawEditorIcon( EditorIcon icon, matrix4 const& L2W, xbool isSelected, xcolor TintColor )
{
    const icon_mesh mesh = GetIconMesh( icon );
    if( !mesh.pVertices )
        return;

    matrix4 LocalToWorld = L2W;
    const f32 ScaleSquared = L2W( 0, 0 ) * L2W( 0, 0 ) + L2W( 1, 0 ) * L2W( 1, 0 ) + L2W( 2, 0 ) * L2W( 2, 0 );
    if( ScaleSquared >= ( 1.01f * 1.01f ) || ScaleSquared <= ( 0.98f * 0.98f ) )
        LocalToWorld.Setup( vector3( 1.0f, 1.0f, 1.0f ), L2W.GetRotation(), L2W.GetTranslation() );

    const render::primitive_draw_desc IconMaterial( NULL,
                                                    render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                    render::PRIMITIVE_BLEND_OPAQUE,
                                                    render::PRIMITIVE_DEPTH_READ_WRITE,
                                                    render::PRIMITIVE_RASTER_SOLID_NO_CULL,
                                                    render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);
    SubmitIconMesh( mesh, LocalToWorld, TintColor, IconMaterial );

    const render::primitive_draw_desc SelectionMaterial( NULL,
                                                         render::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                         render::PRIMITIVE_BLEND_ALPHA,
                                                         render::PRIMITIVE_DEPTH_READ_ONLY,
                                                         render::PRIMITIVE_RASTER_SOLID_NO_CULL,
                                                         render::PRIMITIVE_SAMPLER_LINEAR_CLAMP);
    const xcolor SelectionColor = isSelected ? xcolor( 255, 0, 0, 128 ) : xcolor( 255, 255, 255, 64 );
    SubmitIconMesh( GetIconMesh( EditorIcon::Sphere ), LocalToWorld, SelectionColor, SelectionMaterial );
}
