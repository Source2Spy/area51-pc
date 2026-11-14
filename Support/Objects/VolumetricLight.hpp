//==============================================================================
//  VolumetricLight.hpp
//
//  Copyright (c) 2004 Inevitable Entertainment Inc. All rights reserved.
//
//  Artist-authored volumetric emission, integrated through a textured 3D volume.
//==============================================================================

#ifndef __VOLUMETRICLIGHT_HPP__
#define __VOLUMETRICLIGHT_HPP__

#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Texture.hpp"

namespace render
{
    struct volumetric_draw_desc;
}

//==============================================================================

class volumetric_light_obj : public object
{
public:

    CREATE_RTTI( volumetric_light_obj, object, object )

                            volumetric_light_obj    ( void );
    virtual                ~volumetric_light_obj    ( void );
    virtual bbox            GetLocalBBox            ( void ) const;
    virtual s32             GetMaterial             ( void ) const { return MAT_TYPE_FLESH; }
    virtual void            OnEnumProp              ( prop_enum& List );
    virtual xbool           OnProperty              ( prop_query& I );

    virtual const object_desc&  GetTypeDesc         ( void ) const;
    static  const object_desc&  GetObjectType       ( void );

    // Builds the same enabled, textured volume used by rendering, without submission.
    xbool                   GetShadowVolume         ( render::volumetric_draw_desc& Desc ) const;

#ifdef X_EDITOR
    virtual void            OnDebugRender   ( void );
#endif // X_EDITOR

protected:
    virtual void            OnRender                ( void );
    virtual void            OnRenderTransparent     ( void );

    f32                     GetEffectiveIntensity   ( void ) const;
    xbool                   GetVolumeDescription    ( render::volumetric_draw_desc& Desc ) const;

    //==========================================================================
    // DATA
    //==========================================================================

    f32                     m_StartSize;
    f32                     m_EndSize;
    f32                     m_Length;
    xcolor                  m_StartColor;
    xcolor                  m_EndColor;
    s32                     m_nSprites;     // Legacy authored intensity sample count.
    f32                     m_Intensity;
    f32                     m_EdgeSoftness;
    f32                     m_StartFade;
    f32                     m_EndFade;
    xbool                   m_bCastShadows;
    xbool                   m_bHasLegacySprites;
    xbool                   m_bHasExplicitIntensity;
    texture::handle         m_ProjTexHandle;
    xbool                   m_bRender;

};

#endif // __VOLUMETRICLIGHT_HPP__
