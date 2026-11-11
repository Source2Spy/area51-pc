//==============================================================================
//
//  hud_InfoBox.hpp
//
//  Copyright (c) 2002-2004 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#ifndef HUD_INFOBOX_HPP
#define HUD_INFOBOX_HPP

//==============================================================================
// INCLUDES
//==============================================================================

#include "Obj_mgr/obj_mgr.hpp"
#include "Render/Texture.hpp"
#include "Objects/Player/Player.hpp"

#include "hud_Renderable.hpp"
#include "UI/ui_text_data.hpp"

//==============================================================================
// CLASS
//==============================================================================

class hud_info_box : public hud_renderable
{
public:
    hud_info_box                            ( void );
    virtual        ~hud_info_box            ( void ) {};

    virtual void    OnRender                ( player*         pPlayer );
    virtual void    OnAdvanceSimulation          ( player*         pPlayer, f32 DeltaTime );
    virtual xbool   OnProperty              ( prop_query&     rPropQuery );
    virtual void    OnEnumProp              ( prop_enum&      List );

            void    OnRenderCTF_Flag        ( vector3& Pos );
            void    UpdateCFT_Flag          ( f32 DeltaTime, player* pPlayer );
            void    SetScoreInfo            ( const ui_text_data& Col1,
                                               const ui_text_data& Col2,
                                               const ui_text_data& Col3,
                                               s32 Slot );
            s32     GetWidth                ( void ) const;

    //------------------------------------------------------------------------------
    // Public Storage
public:

private:
            s32     MeasureScoreLayout      ( f32& Segment1Width,
                                               f32& Segment2Width,
                                               f32& Segment3Width,
                                               s32& NumBoxes ) const;

    ui_text_data   m_ScoreString_Col1[2];
    ui_text_data   m_ScoreString_Col2[2];
    ui_text_data   m_ScoreString_Col3[2];

    f32                 m_CTFFlagAlpha;
    rhandle<texture>    m_CTFFlag;
    rhandle<texture>    m_CTFFlagRing;
    xbool               m_RenderCTFFlag;
    xbool               m_HasCTFFlag;

};

#endif


