//==============================================================================
//
//  ShaderCombo.hpp
//
//  Combo expressions are compiled once. Host evaluation and generated C++ use
//  the same expression tree and short-circuit semantics.
//==============================================================================

#ifndef SHADER_COMBO_HPP
#define SHADER_COMBO_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

//==============================================================================
//  CLASSES
//==============================================================================

class shader_combo_expression
{
public:

    xbool                   Parse       ( const std::string& Text, const std::set<std::string>& Symbols, std::string& Error );
    xbool                   Evaluate    ( const std::map<std::string, s64>& Values, s64& Result, std::string& Error ) const;
    xbool                   EmitCpp     ( const std::map<std::string, std::string>& Symbols, std::string& Text, std::string& Error ) const;

private:

    //-------------------------------------------------------------------------
    //  TYPES
    //-------------------------------------------------------------------------

    enum operation
    {
        LITERAL,
        SYMBOL,
        NOT,
        NEGATE,
        EQUAL,
        UNEQUAL,
        LESS,
        LESS_EQUAL,
        GREATER,
        GREATER_EQUAL,
        AND,
        OR,
        IMPLIES,
        SELECT,
        BIT_AND,
        BIT_OR
    };

    struct node
    {
        operation   Operation = LITERAL;
        s64         Value     = 0;
        std::string Name;
        s32         Left      = -1;
        s32         Right     = -1;
        s32         Third     = -1;
    };

    //-------------------------------------------------------------------------
    //  DATA
    //-------------------------------------------------------------------------

    std::vector<node> m_Nodes;
    s32               m_Root = -1;

    //-------------------------------------------------------------------------
    //  HELPERS
    //-------------------------------------------------------------------------

    xbool                   EvaluateNode( s32 Index, const std::map<std::string, s64>& Values, s64& Result, std::string& Error ) const;
    xbool                   EmitNode    ( s32 Index, const std::map<std::string, std::string>& Symbols, std::string& Text, std::string& Error ) const;
};

#endif
//==============================================================================
