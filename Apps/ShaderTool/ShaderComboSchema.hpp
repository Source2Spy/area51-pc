//==============================================================================
//
//  ShaderComboSchema.hpp
//
//  Combo domains describe structural state. Runtime uniforms have no key bits.
//==============================================================================

#ifndef SHADER_COMBO_SCHEMA_HPP
#define SHADER_COMBO_SCHEMA_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderCombo.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum class shader_combo_scope
{
    Material,
    Draw,
    Pass
};

//------------------------------------------------------------------------------

struct shader_combo_axis
{
    std::string        Name;
    std::string        Field;
    shader_combo_scope Scope = shader_combo_scope::Material;
    u32                Shift = 0;
    u32                Bits  = 0;
    std::vector<s64>   Values;
};

//------------------------------------------------------------------------------

struct shader_combo_normalization
{
    std::string Condition;
    std::string Axis;
    std::string Value;
};

//------------------------------------------------------------------------------

struct shader_combo_define
{
    std::string Name;
    std::string Value;
    std::string When = "true";
};

//------------------------------------------------------------------------------

struct shader_combo_desc
{
    std::string                             Name;
    u64                                     Base             = 0;
    xbool                                   MaterialProfiles = FALSE;
    std::vector<shader_combo_axis>          Axes;
    std::vector<std::string>                Rules;
    std::vector<shader_combo_normalization> Normalizations;
    std::vector<shader_combo_define>        Vertex;
    std::vector<shader_combo_define>        Pixel;
};

//------------------------------------------------------------------------------

struct shader_combo_variant
{
    u64                        Key = 0;
    std::map<std::string, s64> Values;
    std::string                VertexOptions;
    std::string                PixelOptions;
};

using shader_combo_profile = std::map<std::string, std::vector<s64>>;

//==============================================================================
//  CLASSES
//==============================================================================

class shader_combo_schema
{
public:

    xbool Compile( const shader_combo_desc& Desc, std::string& Error );
    xbool Resolve( const std::map<std::string, s64>& Input, shader_combo_variant& Variant,
                   xbool& Allowed, std::string& Error ) const;
    xbool Enumerate( std::vector<shader_combo_variant>& Variants, std::string& Error ) const;
    xbool Enumerate( const std::vector<shader_combo_profile>& Profiles,
                     std::vector<shader_combo_variant>& Variants, std::string& Error ) const;
    xbool EmitCpp( std::string& Text, std::string& Error ) const;

private:

    shader_combo_desc m_Desc;

    struct normalization
    {
        std::string             Axis;
        shader_combo_expression Condition;
        shader_combo_expression Value;
    };

    struct define
    {
        std::string             Name;
        shader_combo_expression Value;
        shader_combo_expression When;
    };

    std::vector<shader_combo_expression> m_Rules;
    std::vector<normalization>           m_Normalizations;
    std::vector<define>                  m_Vertex;
    std::vector<define>                  m_Pixel;
    u64                                  m_Mask     = 0;
    xbool                                m_Compiled = FALSE;
};

#endif
//==============================================================================
