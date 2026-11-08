//==============================================================================
//
//  e_InputActions.hpp
//
//==============================================================================

#ifndef E_INPUT_ACTIONS_HPP
#define E_INPUT_ACTIONS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Input.hpp"
#include "x_array.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum input_action_value_mode
{
    INPUT_ACTION_VALUE_AUTO,
    INPUT_ACTION_VALUE_SIGNED_AXIS,
    INPUT_ACTION_VALUE_POSITIVE_AXIS
};

//------------------------------------------------------------------------------

struct input_action_source
{
                        input_action_source     ( void );

    void                Clear                   ( void );
    void                Set                     ( input_gadget GadgetID, s32 DeviceID );
    xbool               IsValid                 ( void ) const;
    input_platform      GetPlatform             ( void ) const;
    input_device        GetDevice               ( void ) const;
    input_value_kind    GetValueKind            ( void ) const;
    input_control_kind  GetControlKind          ( void ) const;

    input_gadget        GadgetID;
    s32                 DeviceID;
    input_gadget_info   Info;
};

//==============================================================================
//  INPUT ACTION STATE
//==============================================================================

class input_action_state
{
public:
                        input_action_state      ( void );

    char const*         GetActionName           ( void ) const;
    f32                 GetIsValue              ( void ) const;
    f32                 GetIsValue              ( input_device Device ) const;
    f32                 GetWasValue             ( void ) const;
    f32                 GetWasValue             ( input_device Device ) const;
    f32                 GetTimePressed          ( void ) const;
    input_action_source const& GetIsSource      ( void ) const;
    input_action_source const& GetIsSource      ( input_device Device ) const;
    input_action_source const& GetWasSource     ( void ) const;
    input_action_source const& GetWasSource     ( input_device Device ) const;

private:
    friend class input_action_map;

    void                Clear                   ( void );
    void                BeginSample             ( void );
    void                CommitSample            ( f32 DeltaTime );

private:

    char                m_ActionName[48];
    f32                 m_IsValue;
    f32                 m_WasValue;
    f32                 m_SampledValue;
    f32                 m_TimePressed;
    input_action_source m_IsSource;
    input_action_source m_WasSource;
    input_action_source m_SampledSource;

    f32                 m_DeviceIsValues[INPUT_DEVICE_COUNT];
    f32                 m_DeviceWasValues[INPUT_DEVICE_COUNT];
    f32                 m_DeviceSampledValues[INPUT_DEVICE_COUNT];
    input_action_source m_DeviceIsSources[INPUT_DEVICE_COUNT];
    input_action_source m_DeviceWasSources[INPUT_DEVICE_COUNT];
    input_action_source m_DeviceSampledSources[INPUT_DEVICE_COUNT];
};

//==============================================================================
//  INPUT ACTION MAP
//==============================================================================

class input_action_map
{
public:
                        input_action_map        ( void );

    input_action_state& GetActionState          ( s32 ActionID );
    input_action_state const& GetActionState    ( s32 ActionID ) const;
    s32                 GetActionCount          ( void ) const;

    void                SetActionCount          ( s32 Count );
    void                SetActionName           ( s32 ActionID, char const* pName );
    void                AddBinding              ( s32 ActionID,
                                                  input_gadget GadgetID,
                                                  f32 Scale,
                                                  u32 ContextMask,
                                                  input_action_value_mode ValueMode = INPUT_ACTION_VALUE_AUTO );

    void                RemoveBindings          ( s32 ActionID, input_device Device );

    void                SetActiveContext        ( u32 ContextMask );
    u32                 GetActiveContext        ( void ) const;
    void                SetDeviceID             ( s32 DeviceID );
    s32                 GetDeviceID             ( void ) const;
    xbool               HasDeviceID             ( void ) const;
    s32                 GetResolvedDeviceID     ( s32 FallbackDeviceID ) const;

    void                ClearAllActions         ( void );
    virtual void        Sample                  ( input_snapshot const& Snapshot, f32 DeltaTime );

protected:

    virtual xbool       OverrideActionValue     ( s32 ActionID, f32& Value );
    virtual void        OnActionValue           ( s32 ActionID, f32 Value );

private:

    struct binding
    {
                        binding                 ( void );

        input_gadget    GadgetID;
        f32             Scale;
        f32             LastAnalogValue;
        s32             ActionID;
        u32             ContextMask;
        input_action_value_mode ValueMode;
    };

    void                SampleDevice            ( input_snapshot const& Snapshot, f32 DeltaTime, s32 DeviceID );
    void                SampleBinding           ( input_snapshot const& Snapshot,
                                                  binding& Binding,
                                                  s32 DeviceID );
    void                SampleDigitalBinding    ( input_snapshot const& Snapshot,
                                                  binding const& Binding,
                                                  s32 DeviceID );
    void                SampleAnalogBinding     ( input_snapshot const& Snapshot,
                                                  binding& Binding,
                                                  s32 DeviceID );
    void                RecordSample            ( input_action_state& Action,
                                                  f32 Value,
                                                  input_gadget GadgetID,
                                                  s32 DeviceID );
    void                RecordPressed           ( input_action_state& Action,
                                                  f32 Value,
                                                  input_gadget GadgetID,
                                                  s32 DeviceID );

private:

    xarray<input_action_state> m_Actions;
    xarray<binding>            m_Bindings[INPUT_PLATFORM_COUNT];
    u32                        m_ActiveContext;
    s32                        m_DeviceID;
};

//==============================================================================
#endif // E_INPUT_ACTIONS_HPP
//==============================================================================
