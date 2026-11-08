//==============================================================================
//
//  e_InputActions.cpp
//
//==============================================================================

// GS: NOTE: Poor monkey Poopy Joe ( Monkey testing ) was deleted, 
//           Because it was tied to input logic from Support. 
//           In the future, it should be implemented at the Entropy level. So... TODO:

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_InputActions.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

static f32 const s_ActionActuationThreshold = 0.25f;

//==============================================================================
//  HELPERS
//==============================================================================

static
xbool IsActionActuated( f32 Value )
{
    return x_abs( Value ) > s_ActionActuationThreshold;
}

//==============================================================================

static
xbool IsInputDeviceValid( input_device Device )
{
    return( (Device > INPUT_DEVICE_NONE) && (Device < INPUT_DEVICE_COUNT) );
}

//==============================================================================

static
f32 TransformAnalogValue( f32 Value, f32 Scale, input_action_value_mode ValueMode )
{
    Value *= Scale;
    if( ValueMode == INPUT_ACTION_VALUE_POSITIVE_AXIS )
    {
        Value = MAX( Value, 0.0f );
    }
    return Value;
}

//==============================================================================

static
void RecordSampleValue( f32&                 SampledValue,
                        input_action_source& SampledSource,
                        f32                  Value,
                        input_gadget         GadgetID,
                        s32                  DeviceID )
{
    input_value_kind const ValueKind = input_system::GetGadgetValueKind( GadgetID );
    if( (ValueKind == INPUT_VALUE_RELATIVE_AXIS) || (ValueKind == INPUT_VALUE_PULSE) )
    {
        if( Value == 0.0f )
        {
            return;
        }

        if( SampledSource.IsValid() &&
            (SampledSource.GadgetID == GadgetID) &&
            (SampledSource.DeviceID == DeviceID) )
        {
            SampledValue += Value;
        }
        else
        {
            // Relative units are only accumulated with matching relative
            // sources. They are not comparable to absolute axis magnitudes.
            SampledValue = Value;
            SampledSource.Set( GadgetID, DeviceID );
        }
    }
    else if( x_abs( Value ) > x_abs( SampledValue ) )
    {
        SampledValue = Value;
        SampledSource.Set( GadgetID, DeviceID );
    }
}

//==============================================================================

static
void RecordPressedValue( f32&                 WasValue,
                         input_action_source& WasSource,
                         f32                  Value,
                         input_gadget         GadgetID,
                         s32                  DeviceID )
{
    f32 const PressedValue = x_abs( Value );
    if( PressedValue > x_abs( WasValue ) )
    {
        WasValue = PressedValue;
        WasSource.Set( GadgetID, DeviceID );
    }
}

//==============================================================================
//  INPUT ACTION SOURCE
//==============================================================================

input_action_source::input_action_source( void )
{
    Clear();
}

//==============================================================================

void input_action_source::Clear( void )
{
    GadgetID         = INPUT_UNDEFINED;
    DeviceID         = -1;
    Info.Platform    = INPUT_PLATFORM_NONE;
    Info.Device      = INPUT_DEVICE_NONE;
    Info.ValueKind   = INPUT_VALUE_NONE;
    Info.ControlKind = INPUT_CONTROL_NONE;
    Info.Flags       = INPUT_GADGET_FLAG_NONE;
}

//==============================================================================

void input_action_source::Set( input_gadget InGadgetID, s32 InDeviceID )
{
    GadgetID = InGadgetID;
    DeviceID = InDeviceID;
    Info     = input_system::GetGadgetInfo( InGadgetID );
}

//==============================================================================

xbool input_action_source::IsValid( void ) const
{
    return GadgetID != INPUT_UNDEFINED;
}

//==============================================================================

input_platform input_action_source::GetPlatform( void ) const
{
    return Info.Platform;
}

//==============================================================================

input_device input_action_source::GetDevice( void ) const
{
    return Info.Device;
}

//==============================================================================

input_value_kind input_action_source::GetValueKind( void ) const
{
    return Info.ValueKind;
}

//==============================================================================

input_control_kind input_action_source::GetControlKind( void ) const
{
    return Info.ControlKind;
}

//==============================================================================
//  INPUT ACTION STATE
//==============================================================================

input_action_state::input_action_state( void )
{
    m_ActionName[0] = 0;
    Clear();
}

//==============================================================================

void input_action_state::Clear( void )
{
    m_IsValue      = 0.0f;
    m_WasValue     = 0.0f;
    m_SampledValue = 0.0f;
    m_TimePressed  = 0.0f;
    m_IsSource.Clear();
    m_WasSource.Clear();
    m_SampledSource.Clear();

    for( s32 i = 0; i < INPUT_DEVICE_COUNT; i++ )
    {
        m_DeviceIsValues[i]      = 0.0f;
        m_DeviceWasValues[i]     = 0.0f;
        m_DeviceSampledValues[i] = 0.0f;
        m_DeviceIsSources[i].Clear();
        m_DeviceWasSources[i].Clear();
        m_DeviceSampledSources[i].Clear();
    }
}

//==============================================================================

void input_action_state::BeginSample( void )
{
    m_WasValue     = 0.0f;
    m_SampledValue = 0.0f;
    m_WasSource.Clear();
    m_SampledSource.Clear();

    for( s32 i = 0; i < INPUT_DEVICE_COUNT; i++ )
    {
        m_DeviceWasValues[i]     = 0.0f;
        m_DeviceSampledValues[i] = 0.0f;
        m_DeviceWasSources[i].Clear();
        m_DeviceSampledSources[i].Clear();
    }
}

//==============================================================================

void input_action_state::CommitSample( f32 DeltaTime )
{
    m_IsValue  = m_SampledValue;
    m_IsSource = m_SampledSource;

    for( s32 i = 0; i < INPUT_DEVICE_COUNT; i++ )
    {
        m_DeviceIsValues[i] = m_DeviceSampledValues[i];
        m_DeviceIsSources[i] = m_DeviceSampledSources[i];
    }

    if( IsActionActuated( m_IsValue ) )
    {
        m_TimePressed += DeltaTime;
    }
    else
    {
        m_TimePressed = 0.0f;
    }
}

//==============================================================================

char const* input_action_state::GetActionName( void ) const
{
    return m_ActionName;
}

//==============================================================================

f32 input_action_state::GetIsValue( void ) const
{
    return m_IsValue;
}

//==============================================================================

f32 input_action_state::GetIsValue( input_device Device ) const
{
    ASSERT( IsInputDeviceValid( Device ) );
    return IsInputDeviceValid( Device ) ? m_DeviceIsValues[Device] : 0.0f;
}

//==============================================================================

f32 input_action_state::GetWasValue( void ) const
{
    return m_WasValue;
}

//==============================================================================

f32 input_action_state::GetWasValue( input_device Device ) const
{
    ASSERT( IsInputDeviceValid( Device ) );
    return IsInputDeviceValid( Device ) ? m_DeviceWasValues[Device] : 0.0f;
}

//==============================================================================

f32 input_action_state::GetTimePressed( void ) const
{
    return m_TimePressed;
}

//==============================================================================

input_action_source const& input_action_state::GetIsSource( void ) const
{
    return m_IsSource;
}

//==============================================================================

input_action_source const& input_action_state::GetIsSource( input_device Device ) const
{
    ASSERT( IsInputDeviceValid( Device ) );
    return IsInputDeviceValid( Device ) ? m_DeviceIsSources[Device]
                                        : m_DeviceIsSources[INPUT_DEVICE_NONE];
}

//==============================================================================

input_action_source const& input_action_state::GetWasSource( void ) const
{
    return m_WasSource;
}

//==============================================================================

input_action_source const& input_action_state::GetWasSource( input_device Device ) const
{
    ASSERT( IsInputDeviceValid( Device ) );
    return IsInputDeviceValid( Device ) ? m_DeviceWasSources[Device]
                                        : m_DeviceWasSources[INPUT_DEVICE_NONE];
}

//==============================================================================
//  INPUT ACTION MAP
//==============================================================================

input_action_map::binding::binding( void )
{
    GadgetID        = INPUT_UNDEFINED;
    Scale           = 1.0f;
    LastAnalogValue = 0.0f;
    ActionID        = -1;
    ContextMask     = 0;
    ValueMode       = INPUT_ACTION_VALUE_AUTO;
}

//==============================================================================

input_action_map::input_action_map( void )
{
    for( s32 i = 0; i < INPUT_PLATFORM_COUNT; i++ )
    {
        m_Bindings[i].SetGrowAmount( 16 );
    }

    m_Actions.SetGrowAmount( 64 );
    m_ActiveContext = 0;
    m_DeviceID      = -1;
}

//==============================================================================

input_action_state& input_action_map::GetActionState( s32 ActionID )
{
    ASSERT( (ActionID >= 0) && (ActionID < m_Actions.GetCount()) );
    return m_Actions[ActionID];
}

//==============================================================================

input_action_state const& input_action_map::GetActionState( s32 ActionID ) const
{
    ASSERT( (ActionID >= 0) && (ActionID < m_Actions.GetCount()) );
    return m_Actions[ActionID];
}

//==============================================================================

s32 input_action_map::GetActionCount( void ) const
{
    return m_Actions.GetCount();
}

//==============================================================================

void input_action_map::SetActionCount( s32 Count )
{
    ASSERT( Count >= 0 );
    if( Count < 0 )
    {
        return;
    }

    m_Actions.SetCount( Count );
    ClearAllActions();
}

//==============================================================================

void input_action_map::SetActionName( s32 ActionID, char const* pName )
{
    ASSERT( (ActionID >= 0) && (ActionID < m_Actions.GetCount()) );
    ASSERT( pName );
    if( (ActionID < 0) || (ActionID >= m_Actions.GetCount()) || !pName )
    {
        return;
    }

    x_strncpy( m_Actions[ActionID].m_ActionName,
               pName,
               sizeof( m_Actions[ActionID].m_ActionName ) );
    m_Actions[ActionID].m_ActionName[sizeof( m_Actions[ActionID].m_ActionName ) - 1] = 0;
}

//==============================================================================

void input_action_map::AddBinding( s32                     ActionID,
                                   input_gadget            GadgetID,
                                   f32                     Scale,
                                   u32                     ContextMask,
                                   input_action_value_mode ValueMode )
{
    ASSERT( (ActionID >= 0) && (ActionID < m_Actions.GetCount()) );
    ASSERT( ContextMask != 0 );
    if( (ActionID < 0) || (ActionID >= m_Actions.GetCount()) ||
        (ContextMask == 0) || (GadgetID == INPUT_UNDEFINED) )
    {
        return;
    }

    input_platform const Platform = input_system::GetGadgetPlatform( GadgetID );
    ASSERT( (Platform >= 0) && (Platform < INPUT_PLATFORM_COUNT) );
    if( (Platform < 0) || (Platform >= INPUT_PLATFORM_COUNT) )
    {
        return;
    }

    input_gadget_info const& Info = input_system::GetGadgetInfo( GadgetID );
    if( ValueMode == INPUT_ACTION_VALUE_AUTO )
    {
        if( Info.ValueKind == INPUT_VALUE_RELATIVE_AXIS )
        {
            ValueMode = INPUT_ACTION_VALUE_SIGNED_AXIS;
        }
        else if( Info.ValueKind == INPUT_VALUE_PULSE )
        {
            ValueMode = INPUT_ACTION_VALUE_POSITIVE_AXIS;
        }
        else if( Info.ValueKind == INPUT_VALUE_ABSOLUTE_AXIS )
        {
            ASSERT( FALSE );
            ValueMode = INPUT_ACTION_VALUE_SIGNED_AXIS;
        }
    }

    binding& Binding      = m_Bindings[Platform].Append();
    Binding.GadgetID      = GadgetID;
    Binding.Scale         = Scale;
    Binding.ActionID      = ActionID;
    Binding.ContextMask   = ContextMask;
    Binding.ValueMode     = ValueMode;
}

//==============================================================================

void input_action_map::RemoveBindings( s32 ActionID, input_device Device )
{
    for( s32 Platform = 0; Platform < INPUT_PLATFORM_COUNT; Platform++ )
    {
        for( s32 i = m_Bindings[Platform].GetCount() - 1; i >= 0; i-- )
        {
            const binding& Binding = m_Bindings[Platform][i];
            if( (Binding.ActionID == ActionID) &&
                (input_system::GetGadgetDevice( Binding.GadgetID ) == Device) )
            {
                m_Bindings[Platform].Delete( i );
            }
        }
    }
}

//==============================================================================

void input_action_map::SetActiveContext( u32 ContextMask )
{
    ASSERT( ContextMask != 0 );
    if( ContextMask == 0 )
    {
        return;
    }

    if( m_ActiveContext != ContextMask )
    {
        m_ActiveContext = ContextMask;
        ClearAllActions();
    }
}

//==============================================================================

u32 input_action_map::GetActiveContext( void ) const
{
    return m_ActiveContext;
}

//==============================================================================

void input_action_map::SetDeviceID( s32 DeviceID )
{
    ASSERT( DeviceID >= -1 );
    if( DeviceID < -1 )
    {
        return;
    }

    if( m_DeviceID == DeviceID )
    {
        return;
    }

    m_DeviceID = DeviceID;
    ClearAllActions();

    for( s32 Platform = 0; Platform < INPUT_PLATFORM_COUNT; Platform++ )
    {
        for( s32 i = 0; i < m_Bindings[Platform].GetCount(); i++ )
        {
            m_Bindings[Platform][i].LastAnalogValue = 0.0f;
        }
    }
}

//==============================================================================

s32 input_action_map::GetDeviceID( void ) const
{
    return m_DeviceID;
}

//==============================================================================

xbool input_action_map::HasDeviceID( void ) const
{
    return m_DeviceID != -1;
}

//==============================================================================

s32 input_action_map::GetResolvedDeviceID( s32 FallbackDeviceID ) const
{
    ASSERT( FallbackDeviceID >= 0 );
    return HasDeviceID() ? m_DeviceID : MAX( FallbackDeviceID, 0 );
}

//==============================================================================

void input_action_map::ClearAllActions( void )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].Clear();
    }
}

//==============================================================================

void input_action_map::Sample( input_snapshot const& Snapshot, f32 DeltaTime )
{
    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].BeginSample();
    }

    SampleDevice( Snapshot, DeltaTime, GetResolvedDeviceID( 0 ) );

    for( s32 i = 0; i < m_Actions.GetCount(); i++ )
    {
        m_Actions[i].CommitSample( DeltaTime );
    }
}

//==============================================================================

void input_action_map::SampleDevice( input_snapshot const& Snapshot, f32 DeltaTime, s32 DeviceID )
{
    (void)DeltaTime;
    ASSERT( DeviceID >= 0 );
    DeviceID = MAX( DeviceID, 0 );

    for( s32 Platform = 0; Platform < INPUT_PLATFORM_COUNT; Platform++ )
    {
        for( s32 i = 0; i < m_Bindings[Platform].GetCount(); i++ )
        {
            SampleBinding( Snapshot, m_Bindings[Platform][i], DeviceID );
        }
    }
}

//==============================================================================

void input_action_map::SampleBinding( input_snapshot const& Snapshot,
                                      binding& Binding,
                                      s32 DeviceID )
{
    if( (Binding.GadgetID == INPUT_UNDEFINED) ||
        (Binding.ActionID < 0) || (Binding.ActionID >= m_Actions.GetCount()) )
    {
        return;
    }

    input_gadget_info const& Info = input_system::GetGadgetInfo( Binding.GadgetID );
    if( (Info.ValueKind == INPUT_VALUE_NONE) || (Info.ValueKind == INPUT_VALUE_QUERY) )
    {
        return;
    }

    if( (Binding.ContextMask & m_ActiveContext) == 0 )
    {
        if( Info.ValueKind == INPUT_VALUE_ABSOLUTE_AXIS )
        {
            Binding.LastAnalogValue = TransformAnalogValue(
                Snapshot.GetValue( Binding.GadgetID, DeviceID ),
                Binding.Scale,
                Binding.ValueMode );
        }
        return;
    }

    if( Info.ValueKind == INPUT_VALUE_DIGITAL )
    {
        SampleDigitalBinding( Snapshot, Binding, DeviceID );
    }
    else
    {
        SampleAnalogBinding( Snapshot, Binding, DeviceID );
    }
}

//==============================================================================

void input_action_map::RecordSample( input_action_state& Action,
                                     f32 Value,
                                     input_gadget GadgetID,
                                     s32 DeviceID )
{
    RecordSampleValue( Action.m_SampledValue,
                       Action.m_SampledSource,
                       Value,
                       GadgetID,
                       DeviceID );

    input_device const Device = input_system::GetGadgetDevice( GadgetID );
    if( IsInputDeviceValid( Device ) )
    {
        RecordSampleValue( Action.m_DeviceSampledValues[Device],
                           Action.m_DeviceSampledSources[Device],
                           Value,
                           GadgetID,
                           DeviceID );
    }
}

//==============================================================================

void input_action_map::RecordPressed( input_action_state& Action,
                                      f32 Value,
                                      input_gadget GadgetID,
                                      s32 DeviceID )
{
    RecordPressedValue( Action.m_WasValue,
                        Action.m_WasSource,
                        Value,
                        GadgetID,
                        DeviceID );

    input_device const Device = input_system::GetGadgetDevice( GadgetID );
    if( IsInputDeviceValid( Device ) )
    {
        RecordPressedValue( Action.m_DeviceWasValues[Device],
                            Action.m_DeviceWasSources[Device],
                            Value,
                            GadgetID,
                            DeviceID );
    }
}

//==============================================================================

void input_action_map::SampleDigitalBinding( input_snapshot const& Snapshot,
                                             binding const& Binding,
                                             s32 DeviceID )
{
    input_action_state& Action = m_Actions[Binding.ActionID];
    xbool const WasPressed = Snapshot.WasPressed( Binding.GadgetID, DeviceID );
    f32 Value = Snapshot.IsPressed( Binding.GadgetID, DeviceID ) ? Binding.Scale : 0.0f;

    xbool const WasOverridden = OverrideActionValue( Binding.ActionID, Value );
    OnActionValue( Binding.ActionID, Value );
    RecordSample( Action, Value, Binding.GadgetID, DeviceID );

    if( WasPressed || (WasOverridden && IsActionActuated( Value ) && !IsActionActuated( Action.m_IsValue )) )
    {
        RecordPressed( Action,
                       IsActionActuated( Value ) ? Value : Binding.Scale,
                       Binding.GadgetID,
                       DeviceID );
    }
}

//==============================================================================

void input_action_map::SampleAnalogBinding( input_snapshot const& Snapshot,
                                            binding& Binding,
                                            s32 DeviceID )
{
    input_action_state& Action = m_Actions[Binding.ActionID];
    input_gadget_info const& Info = input_system::GetGadgetInfo( Binding.GadgetID );
    f32 Value = Snapshot.GetValue( Binding.GadgetID, DeviceID );

    (void)OverrideActionValue( Binding.ActionID, Value );
    Value = TransformAnalogValue( Value, Binding.Scale, Binding.ValueMode );
    OnActionValue( Binding.ActionID, Value );
    RecordSample( Action, Value, Binding.GadgetID, DeviceID );

    if( Info.ValueKind == INPUT_VALUE_PULSE )
    {
        if( Value > 0.0f )
        {
            RecordPressed( Action, Value, Binding.GadgetID, DeviceID );
        }
        return;
    }

    if( Info.ValueKind != INPUT_VALUE_ABSOLUTE_AXIS )
    {
        return;
    }

    if( Binding.ValueMode == INPUT_ACTION_VALUE_POSITIVE_AXIS )
    {
        xbool const WasActuated = Binding.LastAnalogValue > s_ActionActuationThreshold;
        xbool const IsActuated  = Value > s_ActionActuationThreshold;
        Binding.LastAnalogValue = Value;

        if( Snapshot.WasPressed( Binding.GadgetID, DeviceID ) || (IsActuated && !WasActuated) )
        {
            RecordPressed( Action, Value, Binding.GadgetID, DeviceID );
        }
    }
}

//==============================================================================

xbool input_action_map::OverrideActionValue( s32 ActionID, f32& Value )
{
    (void)ActionID;
    (void)Value;
    return FALSE;
}

//==============================================================================

void input_action_map::OnActionValue( s32 ActionID, f32 Value )
{
    (void)ActionID;
    (void)Value;
}

//==============================================================================
