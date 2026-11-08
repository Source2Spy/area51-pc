//==============================================================================
//
//  ShaderComboSchema.cpp
//
//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderComboSchema.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>

namespace
{
xbool IsIdentifier( const std::string& Name )
{
    if( Name.empty() || Name.size() > 128 || !( std::isalpha( static_cast<unsigned char>( Name[0] ) ) || Name[0] == '_' ) )
        return FALSE;

    for( unsigned char Value : Name )
        if( !std::isalnum( Value ) && Value != '_' )
            return FALSE;

    return TRUE;
}

//==============================================================================

std::string DomainTest( const shader_combo_axis& Axis, const std::string& Value )
{
    std::string Text = "(";

    for( size_t i = 0; i < Axis.Values.size(); ++i )
    {
        if( i )
            Text += " || ";
        Text += Value + " == " + std::to_string( Axis.Values[i] ) + "ll";
    }

    return Text + ")";
}
}

//==============================================================================

xbool shader_combo_schema::Compile( const shader_combo_desc& Desc, std::string& Error )
{
    m_Compiled = FALSE;
    m_Mask     = 0;
    m_Rules.clear();
    m_Normalizations.clear();
    m_Vertex.clear();
    m_Pixel.clear();
    Error.clear();

    if( !IsIdentifier( Desc.Name ) || Desc.Axes.empty() || Desc.Axes.size() > 64 ||
        Desc.Rules.size() > 256 || Desc.Normalizations.size() > 256 || Desc.Vertex.size() > 128 || Desc.Pixel.size() > 128 )
    {
        Error = "invalid combo declaration limits";
        return FALSE;
    }

    // Validate axis domains and reserve their non-overlapping key bits.
    std::set<std::string> Symbols, Fields;
    u64                   Count = 1;

    for( const auto& Axis : Desc.Axes )
    {
        if( !IsIdentifier( Axis.Name ) || Axis.Name == "true" || Axis.Name == "false" || !IsIdentifier( Axis.Field ) ||
            !Symbols.insert( Axis.Name ).second || !Fields.insert( Axis.Field ).second ||
            Axis.Bits == 0 || Axis.Bits > 63 || Axis.Shift > 63 || Axis.Bits + Axis.Shift > 64 ||
            Axis.Values.empty() || Axis.Values.size() > 256 ||
            ( Axis.Scope != shader_combo_scope::Material && Axis.Scope != shader_combo_scope::Draw && Axis.Scope != shader_combo_scope::Pass ) )
        {
            Error = "invalid or duplicate combo axis '" + Axis.Name + "'";
            return FALSE;
        }

        const u64 Mask = ( ( 1ull << Axis.Bits ) - 1 ) << Axis.Shift;

        if( ( Mask & m_Mask ) || ( Mask & Desc.Base ) )
        {
            Error = "overlapping combo key fields";
            return FALSE;
        }

        m_Mask |= Mask;
        std::set<s64> Values;

        for( s64 Value : Axis.Values )
            if( Value < 0 || static_cast<u64>( Value ) >= ( 1ull << Axis.Bits ) || !Values.insert( Value ).second )
            {
                Error = "invalid combo axis domain '" + Axis.Name + "'";
                return FALSE;
            }

        if( Desc.MaterialProfiles && Axis.Scope == shader_combo_scope::Material )
            continue;
        if( Count > 4 * 1024 * 1024 / Axis.Values.size() )
        {
            Error = "combo candidate domain exceeds limit";
            return FALSE;
        }

        Count *= Axis.Values.size();
    }

    // Compile rules and normalizations against the complete axis symbol set.
    for( const auto& Text : Desc.Rules )
    {
        shader_combo_expression Rule;

        if( !Rule.Parse( Text, Symbols, Error ) )
            return FALSE;

        m_Rules.push_back( std::move( Rule ) );
    }

    for( const auto& Item : Desc.Normalizations )
    {
        normalization Rule;
        Rule.Axis = Item.Axis;

        if( !Symbols.count( Item.Axis ) )
        {
            Error = "unknown normalization axis '" + Item.Axis + "'";
            return FALSE;
        }

        if( !Rule.Condition.Parse( Item.Condition, Symbols, Error ) || !Rule.Value.Parse( Item.Value, Symbols, Error ) )
            return FALSE;

        m_Normalizations.push_back( std::move( Rule ) );
    }

    // Stage defines share the same expression language as the selection rules.
    const auto CompileDefines = [&]( const std::vector<shader_combo_define>& Items, std::vector<define>& Result ) -> xbool
    {
        std::set<std::string> Names;

        for( const auto& Item : Items )
        {
            define Define;
            Define.Name = Item.Name;

            if( !IsIdentifier( Item.Name ) || !Names.insert( Item.Name ).second )
            {
                Error = "invalid or duplicate stage define";
                return FALSE;
            }

            if( !Define.Value.Parse( Item.Value, Symbols, Error ) || !Define.When.Parse( Item.When, Symbols, Error ) )
                return FALSE;

            Result.push_back( std::move( Define ) );
        }

        return TRUE;
    };

    if( !CompileDefines( Desc.Vertex, m_Vertex ) || !CompileDefines( Desc.Pixel, m_Pixel ) )
        return FALSE;

    m_Desc     = Desc;
    m_Compiled = TRUE;
    return TRUE;
}

//==============================================================================

xbool shader_combo_schema::Resolve( const std::map<std::string, s64>& Input, shader_combo_variant& Variant,
                                    xbool& Allowed, std::string& Error ) const
{
    Variant = {};
    Allowed = FALSE;
    Error.clear();

    if( !m_Compiled )
    {
        Error = "combo schema was not compiled";
        return FALSE;
    }

    if( Input.size() != m_Desc.Axes.size() )
    {
        Error = "incomplete combo axis values";
        return FALSE;
    }

    std::map<std::string, s64> Values = Input;

    for( const auto& Axis : m_Desc.Axes )
    {
        const auto Value = Values.find( Axis.Name );

        if( Value == Values.end() || std::find( Axis.Values.begin(), Axis.Values.end(), Value->second ) == Axis.Values.end() )
        {
            Error = "combo value outside domain '" + Axis.Name + "'";
            return FALSE;
        }
    }

    // Apply normalizations before packing the key and evaluating allow rules.
    for( const auto& Rule : m_Normalizations )
    {
        s64 Condition = 0, Value = 0;

        if( !Rule.Condition.Evaluate( Values, Condition, Error ) )
            return FALSE;

        if( Condition )
        {
            if( !Rule.Value.Evaluate( Values, Value, Error ) )
                return FALSE;

            Values[Rule.Axis] = Value;
        }
    }

    u64 Key = m_Desc.Base;

    for( const auto& Axis : m_Desc.Axes )
    {
        const s64 Value = Values[Axis.Name];

        if( std::find( Axis.Values.begin(), Axis.Values.end(), Value ) == Axis.Values.end() )
        {
            Error = "normalization value outside domain '" + Axis.Name + "'";
            return FALSE;
        }

        Key |= static_cast<u64>( Value ) << Axis.Shift;
    }

    for( const auto& Rule : m_Rules )
    {
        s64 Value = 0;

        if( !Rule.Evaluate( Values, Value, Error ) )
            return FALSE;

        if( !Value )
            return TRUE;
    }

    // Emit only stage defines enabled by the normalized axis values.
    const auto BuildOptions = [&]( const std::vector<define>& Defines, std::string& Text ) -> xbool
    {
        for( const auto& Define : Defines )
        {
            s64 When = 0, Value = 0;

            if( !Define.When.Evaluate( Values, When, Error ) )
                return FALSE;

            if( !When )
                continue;
            if( !Define.Value.Evaluate( Values, Value, Error ) )
                return FALSE;

            if( Value < std::numeric_limits<s32>::min() || Value > std::numeric_limits<s32>::max() )
            {
                Error = "stage define outside signed 32-bit range";
                return FALSE;
            }

            if( !Text.empty() )
                Text += ' ';
            Text += "-D" + Define.Name + "=" + std::to_string( Value );
        }

        return TRUE;
    };

    shader_combo_variant Result;
    Result.Key = Key;

    if( !BuildOptions( m_Vertex, Result.VertexOptions ) || !BuildOptions( m_Pixel, Result.PixelOptions ) )
        return FALSE;

    Result.Values = std::move( Values );
    Variant       = std::move( Result );
    Allowed       = TRUE;
    return TRUE;
}

//==============================================================================

xbool shader_combo_schema::Enumerate( std::vector<shader_combo_variant>& Variants, std::string& Error ) const
{
    if( m_Desc.MaterialProfiles )
    {
        Error = "material profile seeds are required";
        return FALSE;
    }

    return Enumerate( std::vector<shader_combo_profile>( 1 ), Variants, Error );
}

//==============================================================================

xbool shader_combo_schema::Enumerate( const std::vector<shader_combo_profile>& Profiles,
                                      std::vector<shader_combo_variant>& Variants, std::string& Error ) const
{
    Variants.clear();
    Error.clear();

    if( !m_Compiled )
    {
        Error = "combo schema was not compiled";
        return FALSE;
    }

    if( Profiles.empty() || Profiles.size() > 4096 )
    {
        Error = "invalid material profile count";
        return FALSE;
    }

    u64 Candidates = 0;

    for( const auto& Profile : Profiles )
    {
        for( const auto& Domain : Profile )
            if( std::none_of( m_Desc.Axes.begin(), m_Desc.Axes.end(), [&]( const auto& Axis )
                              { return Axis.Name == Domain.first; } ) )
            {
                Error = "unknown profile axis";
                return FALSE;
            }

        u64 Count = 1;

        for( const auto& Axis : m_Desc.Axes )
        {
            const auto Override = Profile.find( Axis.Name );

            if( m_Desc.MaterialProfiles && Axis.Scope == shader_combo_scope::Material &&
                ( Override == Profile.end() || Override->second.size() != 1 ) )
            {
                Error = "material profile must fix every material axis";
                return FALSE;
            }

            const auto& Domain = Override == Profile.end() ? Axis.Values : Override->second;

            if( Domain.empty() )
            {
                Error = "empty profile axis domain";
                return FALSE;
            }

            std::set<s64> Seen;

            for( s64 Value : Domain )
                if( !Seen.insert( Value ).second || std::find( Axis.Values.begin(), Axis.Values.end(), Value ) == Axis.Values.end() )
                {
                    Error = "invalid profile axis value";
                    return FALSE;
                }

            if( Count > 4 * 1024 * 1024 / Domain.size() )
            {
                Error = "profile candidate domain exceeds limit";
                return FALSE;
            }

            Count *= Domain.size();
        }

        Candidates += Count;

        if( Candidates > 4 * 1024 * 1024 )
        {
            Error = "profile candidate domain exceeds limit";
            return FALSE;
        }
    }

    std::map<u64, shader_combo_variant> Unique;
    const shader_combo_profile*         pProfile = nullptr;
    std::map<std::string, s64>          Values;
    std::function<xbool( size_t )>      Visit = [&]( size_t Index ) -> xbool
    {
        if( Index == m_Desc.Axes.size() )
        {
            shader_combo_variant Variant;
            xbool                Allowed = FALSE;

            if( !Resolve( Values, Variant, Allowed, Error ) )
                return FALSE;

            if( Allowed )
            {
                shader_combo_variant Repeated;
                xbool                StillAllowed = FALSE;

                if( !Resolve( Variant.Values, Repeated, StillAllowed, Error ) )
                    return FALSE;

                if( !StillAllowed || Repeated.Key != Variant.Key || Repeated.Values != Variant.Values )
                {
                    Error = "combo normalization is not idempotent";
                    return FALSE;
                }

                const auto Found = Unique.find( Variant.Key );

                if( Found != Unique.end() && ( Found->second.Values != Variant.Values ||
                                               Found->second.VertexOptions != Variant.VertexOptions || Found->second.PixelOptions != Variant.PixelOptions ) )
                {
                    Error = "ambiguous normalized combo key";
                    return FALSE;
                }

                Unique.emplace( Variant.Key, std::move( Variant ) );

                if( Unique.size() > 1024 * 1024 )
                {
                    Error = "combo table exceeds limit";
                    return FALSE;
                }
            }

            return TRUE;
        }

        const auto& Axis     = m_Desc.Axes[Index];
        const auto  Override = pProfile->find( Axis.Name );
        const auto& Domain   = Override == pProfile->end() ? Axis.Values : Override->second;

        for( s64 Value : Domain )
        {
            Values[Axis.Name] = Value;

            if( !Visit( Index + 1 ) )
                return FALSE;
        }

        return TRUE;
    };

    for( const auto& Profile : Profiles )
    {
        pProfile = &Profile;

        if( !Visit( 0 ) )
            return FALSE;
    }

    for( auto& Pair : Unique )
        Variants.push_back( std::move( Pair.second ) );
    return TRUE;
}

//==============================================================================

xbool shader_combo_schema::EmitCpp( std::string& Text, std::string& Error ) const
{
    Text.clear();
    Error.clear();

    if( !m_Compiled )
    {
        Error = "combo schema was not compiled";
        return FALSE;
    }

    std::map<std::string, std::string> Symbols;
    std::string                        Body = "inline xbool A51Normalize" + m_Desc.Name + "Combo( render::ShaderProgramKey& Key )\n{\n";
    Body += "    if( (Key.Hash() & ~" + std::to_string( m_Mask ) + "ull) != " + std::to_string( m_Desc.Base ) + "ull ) return FALSE;\n";

    for( const auto& Axis : m_Desc.Axes )
    {
        Symbols[Axis.Name] = "Combo_" + Axis.Name;
        Body += "    s64 " + Symbols[Axis.Name] + " = static_cast<s64>(Key." + Axis.Field + ");\n";
        Body += "    if( !" + DomainTest( Axis, Symbols[Axis.Name] ) + " ) return FALSE;\n";
    }

    for( const auto& Rule : m_Normalizations )
    {
        std::string Condition, Value;

        if( !Rule.Condition.EmitCpp( Symbols, Condition, Error ) || !Rule.Value.EmitCpp( Symbols, Value, Error ) )
            return FALSE;

        Body += "    if( " + Condition + " ) " + Symbols[Rule.Axis] + " = " + Value + ";\n";
    }

    for( const auto& Axis : m_Desc.Axes )
        Body += "    if( !" + DomainTest( Axis, Symbols[Axis.Name] ) + " ) return FALSE;\n";
    for( const auto& Rule : m_Rules )
    {
        std::string Condition;

        if( !Rule.EmitCpp( Symbols, Condition, Error ) )
            return FALSE;

        Body += "    if( !(" + Condition + ") ) return FALSE;\n";
    }

    // Commit only after every domain and rule succeeds, matching host resolution.
    for( const auto& Axis : m_Desc.Axes )
        Body += "    Key." + Axis.Field + " = static_cast<decltype(Key." + Axis.Field + ")>(" + Symbols[Axis.Name] + ");\n";
    Body += "    return TRUE;\n}\n";
    Text = std::move( Body );
    return TRUE;
}
