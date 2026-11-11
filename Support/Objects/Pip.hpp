#ifndef __PIP_HPP__
#define __PIP_HPP__

//=========================================================================
// INCLUDES
//=========================================================================

#include "Obj_mgr/obj_mgr.hpp"
#include "Objects/Render/SkinInst.hpp"
#include "Animation/AnimData.hpp"
#include "Animation/AnimPlayer.hpp"

#include "Render/RenderScope.hpp"

struct shader_resource;

//=========================================================================
// CLASS
//=========================================================================

struct audio_request;

class pip : public object
{
//=====================================================================
// FRIEND CLASSES
//=====================================================================
    friend struct pip_desc ;
    friend class obj_mgr;
    friend class collision_mgr;

//=====================================================================
// DEFINES
//=====================================================================
public:

    // Types of pip
    enum type
    {
        TYPE_HUD,   // Hud overlay type
        TYPE_WORLD  // In world pip
    } ;

    // List of available states
    enum state
    {
        STATE_INACTIVE, // Not active
        STATE_ENTER,    // Entering screen
        STATE_ACTIVE,   // Active on screen
        STATE_EXIT      // Exiting screen
    } ;

    // Miscallenous
    enum misc
    {
        MAX_WORLD_OBJECTS = 4,   // Maximum number of objects to control
        MAX_TEXTURE_REFS  = 4    // Maximum number of textures to override
    } ;

//=====================================================================
// PRIVATE STRUCTURES
//=====================================================================

    struct texture_ref
    {
        // Data
        texture::handle        m_hTexture ;    // Handle of texture to override
        const shader_resource* m_pOriginalResourceOverride;

        // Constructor
        texture_ref()
        {
            //m_hTexture = HNULL ;
            m_pOriginalResourceOverride = NULL;
        }
    } ;

//=====================================================================
// PUBLIC BASE CLASS FUNCTIONS
//=====================================================================
public:

    CREATE_RTTI( pip, object, object )
    
                            pip         ( void );
    virtual bbox            GetLocalBBox    ( void ) const { return bbox( vector3(0,0,0), 100 ); }
    virtual s32             GetMaterial     ( void ) const { return MAT_TYPE_FLESH; }
    virtual void            OnEnumProp      ( prop_enum& List );
    virtual xbool           OnProperty      ( prop_query& I );

    virtual const object_desc&  GetTypeDesc     ( void ) const;
    static  const object_desc&  GetObjectType   ( void );

//=====================================================================
// PRIVATE BASE CLASS FUNCTIONS
//=====================================================================

protected:
    virtual void            OnInit              ( void );     
    virtual void            OnKill              ( void );     
    virtual void            OnRender            ( void );
    virtual void            OnAdvanceSimulation      ( f32 DeltaTime ) ;
    virtual void            OnActivate          ( xbool bFlag ) ;    
    
//=====================================================================
// PRIVATE FUNCTIONS
//=====================================================================
private:
            void                RenderView          ( void ) ;
            void                AddTextureRefs      ( object* pObject ) ;
            void                InitTextureRefs     ( void ) ;
            void                ActivatePipTexture  ( xbool bEnable ) ;
            void                SetupState          ( state State ) ;
            void                AdvanceState        ( f32   DeltaTime ) ;
            void                UpdateEar           ( void ) ;

//=====================================================================
// PUBLIC FUNCTIONS
//=====================================================================
public:
            type            GetType             ( void ) const { return m_Type ;  }
            state           GetState            ( void ) const { return m_State ; }
            guid            GetCameraGuid       ( void ) const { return m_CameraGuid; }
            s32             GetWidth            ( void ) const { return m_Width; }
            s32             GetHeight           ( void ) const { return m_Height; }

    virtual render_inst*        GetRenderInstPtr      ( void );
    virtual anim_group::handle* GetAnimGroupHandlePtr ( void );


//=====================================================================
// DATA
//=====================================================================

protected:

    // Rendering components
    anim_group::handle      m_hAnimGroup ;      // Animation group handle
    skin_inst               m_RenderInst ;      // Render instance
    simple_anim_player      m_AnimPlayer ;      // Animation player

    // Texture
    s32                     m_Width, m_Height ; // Size of texture

    // Camera
    guid                    m_CameraGuid ;      // Camera guid

    // Camera ear
    guid                    m_EarGuid ;         // Current ear guid
    audio_request*          m_pEarRequest = NULL;
    s32                     m_EarID ;           // Currnet ear id

    // Misc
    type                    m_Type ;                                // Type of pip
    state                   m_State ;                               // Current state
    guid                    m_WorldObjectGuids[MAX_WORLD_OBJECTS];  // Guid of objects to control
    texture_ref             m_TextureRefs[MAX_TEXTURE_REFS] ;       // List of texture refs
    s32                     m_nTextureRefs ;                        // # of texture refs
    render::RenderTargetSurface m_RenderTarget;
    u64                     m_LastRenderFrameSerial;
};

//=========================================================================
// END
//=========================================================================
#endif
