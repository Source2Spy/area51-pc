//==============================================================================
//  VolumetricLight.cpp
//
//  Copyright (c) 2004 Inevitable Entertainment Inc. All rights reserved.
//
//  Artist-authored volumetric emission, integrated through a textured 3D volume.
//==============================================================================

#include "VolumetricLight.hpp"
#include "../MiscUtils/SimpleUtils.hpp"
#include "Entropy.hpp"
#include "Render/PrimitiveBatch.hpp"
#include "Render/PipelinePrewarm.hpp"
#include "Render/Render.hpp"
#include "Render/Volumetric.hpp"

//==============================================================================
// DEFINES
//==============================================================================

//==============================================================================
// OBJECT DESCRIPTION
//==============================================================================

static render::primitive_draw_desc BuildVolumetricLightLineMaterial( const texture* pTexture )
{
    return render::primitive_draw_desc( pTexture,
                                       render::PRIMITIVE_TOPOLOGY_LINE_LIST,
                                       render::PRIMITIVE_BLEND_ALPHA,
                                       render::PRIMITIVE_DEPTH_READ_ONLY,
                                       render::PRIMITIVE_RASTER_SOLID_NO_CULL,
                                       render::PRIMITIVE_SAMPLER_LINEAR_CLAMP );
}

//------------------------------------------------------------------------------

static struct volumetric_light_obj_desc : public object_desc
{
    virtual xbool CollectRenderPipelineRequirements( render::PipelinePrewarmContext& Context ) const
    {
        return Context.QueuePrimitive( BuildVolumetricLightLineMaterial( NULL ) );
    }

    volumetric_light_obj_desc( void ) : object_desc( 
        object::TYPE_VOLUMETRIC_LIGHT, 
        "VolumetricLight", 
        "RENDER",

        object::ATTR_RENDERABLE    |
        object::ATTR_TRANSPARENT,

        FLAGS_GENERIC_EDITOR_CREATE | FLAGS_IS_DYNAMIC) {}

        //-------------------------------------------------------------------------

        virtual object* Create( void ) { return new volumetric_light_obj; }

        //-------------------------------------------------------------------------

#ifdef X_EDITOR
        virtual s32  OnEditorRender( object& Object ) const
        {
            object_desc::OnEditorRender( Object );
            return static_cast<s32>( EditorIcon::VolumetricLight );
        }
#endif // X_EDITOR

} s_VolumetricLightObj_Desc;

//------------------------------------------------------------------------------

const object_desc& volumetric_light_obj::GetTypeDesc( void ) const
{
    return s_VolumetricLightObj_Desc;
}

//------------------------------------------------------------------------------

const object_desc& volumetric_light_obj::GetObjectType( void )
{
    return s_VolumetricLightObj_Desc;
}

//==============================================================================
// FUNCTIONS
//==============================================================================

volumetric_light_obj::volumetric_light_obj( void ) :
    m_StartSize             ( 100.0f ),
    m_EndSize               ( 200.0f ),
    m_Length                ( 400.0f ),
    m_StartColor            ( 128, 128, 128, 255 ),
    m_EndColor              ( 128, 128, 128, 255 ),
    m_nSprites              ( 6 ),
    m_Intensity             ( 6.0f / 400.0f ),
    m_EdgeSoftness          ( 0.0f ),
    m_StartFade             ( 0.02f ),
    m_EndFade               ( 0.02f ),
    m_bCastShadows          ( TRUE ),
    m_bHasLegacySprites     ( FALSE ),
    m_bHasExplicitIntensity ( FALSE ),
    m_bRender               ( TRUE )
{
}

//==============================================================================

volumetric_light_obj::~volumetric_light_obj( void )
{
}

//==============================================================================

#ifdef X_EDITOR
void volumetric_light_obj::OnDebugRender( void )
{
    f32 StartRadius = m_StartSize * 0.5f;
    f32 EndRadius   = m_EndSize * 0.5f;

    xcolor  Colors[8];
    vector3 Verts[8];
    Colors[0] = Colors[1] = Colors[2] = Colors[3] = m_StartColor;
    Colors[4] = Colors[5] = Colors[6] = Colors[7] = m_EndColor;
    Verts[0].Set( -StartRadius, -StartRadius, 0.0f );
    Verts[1].Set(  StartRadius, -StartRadius, 0.0f );
    Verts[2].Set(  StartRadius,  StartRadius, 0.0f );
    Verts[3].Set( -StartRadius,  StartRadius, 0.0f );
    Verts[4].Set( -EndRadius, -EndRadius, m_Length );
    Verts[5].Set(  EndRadius, -EndRadius, m_Length );
    Verts[6].Set(  EndRadius,  EndRadius, m_Length );
    Verts[7].Set( -EndRadius,  EndRadius, m_Length );

    static const u16 Indices[] =
    {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    render::primitive_vertex Vertices[8];
    for( s32 i = 0; i < 8; ++i )
        Vertices[i] = render::primitive_vertex( Verts[i], vector2( 0.0f, 0.0f ), Colors[i] );

    const render::primitive_draw_desc Material = BuildVolumetricLightLineMaterial( NULL );
    render::SubmitPrimitives( Material, GetL2W(), Vertices, 8, Indices, ARRAYSIZE( Indices ) );
}
#endif // X_EDITOR

//==============================================================================

void volumetric_light_obj::OnRender( void )
{
}

//==============================================================================

void volumetric_light_obj::OnRenderTransparent( void )
{
    render::volumetric_draw_desc Desc;
    if( GetVolumeDescription( Desc ) )
    {
        render::SubmitVolumetric( Desc );
    }
}

//==============================================================================

xbool volumetric_light_obj::GetShadowVolume( render::volumetric_draw_desc& Desc ) const
{
    return m_bCastShadows && GetVolumeDescription( Desc );
}

//==============================================================================

xbool volumetric_light_obj::GetVolumeDescription( render::volumetric_draw_desc& Desc ) const
{
    // Is the light on?
    if( !m_bRender )
    {
        return FALSE;
    }

    if( m_Length <= 0.0f )
    {
        return FALSE;
    }
    
    texture* pProjTex = m_ProjTexHandle.GetPointer();
    if( !pProjTex )
    {
        return FALSE;
    }

    Desc.OwnerGuid    = GetGuid();
    Desc.LocalToWorld = GetL2W();
    Desc.StartSize    = m_StartSize;
    Desc.EndSize      = m_EndSize;
    Desc.Length       = m_Length;
    Desc.StartColor.Set( m_StartColor.R / 255.0f, m_StartColor.G / 255.0f,
                         m_StartColor.B / 255.0f, m_StartColor.A / 255.0f );
    Desc.EndColor.Set( m_EndColor.R / 255.0f, m_EndColor.G / 255.0f,
                       m_EndColor.B / 255.0f, m_EndColor.A / 255.0f );
    Desc.Intensity    = GetEffectiveIntensity();
    Desc.EdgeSoftness = m_EdgeSoftness;
    Desc.StartFade    = m_StartFade;
    Desc.EndFade      = m_EndFade;
    Desc.CastShadows  = m_bCastShadows;
    Desc.pTexture     = pProjTex;
    return TRUE;
}

//==============================================================================

f32 volumetric_light_obj::GetEffectiveIntensity( void ) const
{
    // Defer legacy conversion until use: Length and Intensity may load in any order.
    if( m_bHasLegacySprites && !m_bHasExplicitIntensity )
    {
        return (f32)m_nSprites / MAX( m_Length, 1.0f );
    }

    return m_Intensity;
}

//==============================================================================

bbox volumetric_light_obj::GetLocalBBox( void ) const
{
    // Retain the conservative authored bounds used for visibility gathering.
    f32 MaxRadius = 0.5f * MAX( m_StartSize, m_EndSize );
    return bbox( vector3( -MaxRadius, -MaxRadius, -m_StartSize * 0.5f ),
                 vector3( MaxRadius, MaxRadius, m_Length + m_EndSize * 0.5f ) );
}

//==============================================================================

void volumetric_light_obj::OnEnumProp( prop_enum& List )
{
    object::OnEnumProp( List );

    List.PropEnumHeader  ( "VolumetricLight",               "Volumetric light properties", 0 );
    List.PropEnumBool    ( "VolumetricLight\\IsTurnedOn",   "Use this to turn the light on and off dynamically", PROP_TYPE_EXPOSE );
    List.PropEnumFloat   ( "VolumetricLight\\Intensity",    "Emission intensity per world unit (nonnegative). Changing Length does not change explicit intensity.", 0 );
    List.PropEnumFloat   ( "VolumetricLight\\EdgeSoftness", "Normalized side-edge fade width (0 to 1); 0 keeps hard edges.", 0 );
    List.PropEnumFloat   ( "VolumetricLight\\StartFade",    "Fade-in distance as a fraction of Length (0 to 1); 0 disables the start fade.", 0 );
    List.PropEnumFloat   ( "VolumetricLight\\EndFade",      "Fade-out distance as a fraction of Length (0 to 1); 0 disables the end fade.", 0 );
    List.PropEnumBool    ( "VolumetricLight\\CastShadows",  "Enable world-geometry occlusion along parallel local +Z rays (not a point-light cone). Uses the shared shadow atlas; disabled by default.", 0 );
    List.PropEnumFloat   ( "VolumetricLight\\StartSize",    "The starting size of this light", 0 );
    List.PropEnumFloat   ( "VolumetricLight\\EndSize",      "The ending size of this light", 0 );
    List.PropEnumFloat   ( "VolumetricLight\\Length",       "The length this light volume will expand across", 0 );
    List.PropEnumColor   ( "VolumetricLight\\StartColor",   "Starting color of this light", 0 );
    List.PropEnumColor   ( "VolumetricLight\\EndColor",     "Starting color of this light", 0 );
    List.PropEnumExternal( "VolumetricLight\\Texture",      "Resource\0xbmp\0", "Texture to project", 0 );
}

//==============================================================================

xbool volumetric_light_obj::OnProperty( prop_query& I )
{
    if( object::OnProperty( I ) )
    {
    }
    else if( I.VarBool( "VolumetricLight\\IsTurnedOn", m_bRender ) )
    {
    }
    else if( !I.IsRead() && I.VarInt( "VolumetricLight\\NumSprites", m_nSprites, 2, 64 ) )
    {
        // Load-only compatibility; never enumerate or save the old sample count.
        m_bHasLegacySprites = TRUE;
    }
    else if( I.IsVar( "VolumetricLight\\Intensity" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarFloat( GetEffectiveIntensity() );
        }
        else
        {
            m_Intensity             = I.GetVarFloat( 0.0f, F32_MAX );
            m_bHasExplicitIntensity = TRUE;
        }
    }
    else if( I.VarFloat( "VolumetricLight\\EdgeSoftness", m_EdgeSoftness, 0.0f, 1.0f ) )
    {
    }
    else if( I.VarFloat( "VolumetricLight\\StartFade", m_StartFade, 0.0f, 1.0f ) )
    {
    }
    else if( I.VarFloat( "VolumetricLight\\EndFade", m_EndFade, 0.0f, 1.0f ) )
    {
    }
    else if( I.VarBool( "VolumetricLight\\CastShadows", m_bCastShadows ) )
    {
    }
    else if( I.VarFloat( "VolumetricLight\\StartSize", m_StartSize, 1.0f, 1000.0f ) )
    {
        if( !I.IsRead() )
        {
            OnMove( GetPosition() );    // this forces the bbox to be rebuilt
        }
    }
    else if( I.VarFloat( "VolumetricLight\\EndSize", m_EndSize, 1.0f, 1000.0f ) )
    {
        if( !I.IsRead() )
        {
            OnMove( GetPosition() );    // this forces the bbox to be rebuilt
        }
    }
    else if( I.VarFloat( "VolumetricLight\\Length", m_Length, 1.0f, 3000.0f ) )
    {
        if( !I.IsRead() )
        {
            OnMove( GetPosition() );    // this forces the bbox to be rebuilt
        }
    }
    else if( I.VarColor( "VolumetricLight\\StartColor", m_StartColor ) )
    {
    }
    else if( I.VarColor( "VolumetricLight\\EndColor", m_EndColor ) )
    {
    }
    else if( I.IsVar( "VolumetricLight\\Texture" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarExternal( m_ProjTexHandle.GetName(), RESOURCE_NAME_SIZE );
        }
        else
        {
            m_ProjTexHandle.SetName( I.GetVarExternal() );
        }
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
