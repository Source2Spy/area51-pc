//==============================================================================
//
//  ShaderCombo.cpp
//
//  A bounded grammar prevents rule declarations from becoming runtime scripts.
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderCombo.hpp"
#include <cctype>
#include <algorithm>
#include <functional>
#include <limits>

//==============================================================================
//  PARSING
//==============================================================================

xbool shader_combo_expression::Parse( const std::string&           Text,
                                      const std::set<std::string>& Symbols, std::string& Error )
{
    m_Nodes.clear();
    m_Root = -1;
    Error.clear();

    if( Text.empty() || Text.size() > 4096 )
    {
        Error = "invalid combo expression length";
        return FALSE;
    }

    // Tokenize first so recursive parsing can validate complete operands.
    std::vector<std::string> Tokens;

    for( size_t i = 0; i < Text.size(); )
    {
        const unsigned char Value = static_cast<unsigned char>( Text[i] );

        if( std::isspace( Value ) )
        {
            ++i;
            continue;
        }

        const size_t Begin = i;

        if( std::isalpha( Value ) || Value == '_' )
        {
            while( i < Text.size() && ( std::isalnum( static_cast<unsigned char>( Text[i] ) ) || Text[i] == '_' ) )
                ++i;
        }
        else if( std::isdigit( Value ) )
        {
            while( i < Text.size() && std::isdigit( static_cast<unsigned char>( Text[i] ) ) )
                ++i;
        }
        else
        {
            const std::string Pair = Text.substr( i, 2 );

            if( Pair == "&&" || Pair == "||" || Pair == "==" || Pair == "!=" || Pair == "<=" || Pair == ">=" || Pair == "=>" )
                i += 2;
            else if( Value == '!' || Value == '-' || Value == '<' || Value == '>' || Value == '(' || Value == ')' || Value == '?' || Value == ':' || Value == '|' || Value == '&' )
                ++i;
            else
            {
                Error = "invalid token at byte " + std::to_string( i );
                return FALSE;
            }
        }

        Tokens.push_back( Text.substr( Begin, i - Begin ) );

        if( Tokens.size() > 512 )
        {
            Error = "combo expression exceeds token limit";
            return FALSE;
        }
    }

    // Parser helpers share the token cursor and the expression-node storage.
    size_t Position = 0;
    u32    Depth    = 0;

    const auto Peek = [&]() -> std::string
    {
        return Position < Tokens.size() ? Tokens[Position] : "";
    };

    const auto Add = [&]( operation Op, s32 Left, s32 Right ) -> s32
    {
        m_Nodes.push_back( { Op, 0, "", Left, Right, -1 } );
        return static_cast<s32>( m_Nodes.size() - 1 );
    };

    std::function<s32( u32 )> ParseLevel;
    std::function<s32()>      ParseConditional;
    std::function<s32()>      ParseUnary;

    // Parse literals, symbols, unary operators, and parenthesized expressions.
    ParseUnary = [&]() -> s32
    {
        if( ++Depth > 64 )
        {
            Error = "combo expression exceeds nesting limit";
            --Depth;
            return -1;
        }

        struct depth_guard
        {
            u32& Value;
            ~depth_guard()
            {
                --Value;
            }
        } Guard{ Depth };

        const std::string Token = Peek();

        if( Token.empty() )
        {
            Error = "missing combo operand";
            return -1;
        }

        ++Position;

        if( Token == "!" || Token == "-" )
        {
            const s32 Child = ParseUnary();
            return Child < 0 ? -1 : Add( Token == "!" ? NOT : NEGATE, Child, -1 );
        }

        if( Token == "(" )
        {
            const s32 Child = ParseConditional();

            if( Child < 0 || Peek() != ")" )
            {
                if( Error.empty() )
                    Error = "unclosed combo expression";
                return -1;
            }

            ++Position;
            return Child;
        }

        node Node;

        if( Token == "true" || Token == "false" )
            Node.Value = Token == "true";
        else if( std::isdigit( static_cast<unsigned char>( Token[0] ) ) )
        {
            for( char Digit : Token )
            {
                if( Node.Value > ( std::numeric_limits<s64>::max() - ( Digit - '0' ) ) / 10 )
                {
                    Error = "combo integer literal overflow";
                    return -1;
                }

                Node.Value = Node.Value * 10 + ( Digit - '0' );
            }
        }
        else if( Symbols.count( Token ) )
        {
            Node.Operation = SYMBOL;
            Node.Name      = Token;
        }
        else
        {
            Error = "unknown combo symbol '" + Token + "'";
            return -1;
        }

        m_Nodes.push_back( std::move( Node ) );

        return static_cast<s32>( m_Nodes.size() - 1 );
    };

    // Each level handles one precedence group. Implication is right-associative.
    ParseLevel = [&]( u32 Level ) -> s32
    {
        if( ++Depth > 64 )
        {
            Error = "combo expression exceeds nesting limit";
            --Depth;
            return -1;
        }

        struct depth_guard
        {
            u32& Value;
            ~depth_guard()
            {
                --Value;
            }
        } Guard{ Depth };

        if( Level == 6 )
            return ParseUnary();

        s32 Left = ParseLevel( Level + 1 );

        if( Left < 0 )
            return -1;

        while( Position < Tokens.size() )
        {
            const std::string Token = Peek();
            operation         Op    = LITERAL;

            if( Level == 0 && Token == "=>" )
                Op = IMPLIES;
            else if( Level == 1 && Token == "||" )
                Op = OR;
            else if( Level == 2 && Token == "&&" )
                Op = AND;
            else if( Level == 3 && Token == "|" )
                Op = BIT_OR;
            else if( Level == 4 && Token == "&" )
                Op = BIT_AND;
            else if( Level == 5 )
            {
                if( Token == "==" )
                    Op = EQUAL;
                else if( Token == "!=" )
                    Op = UNEQUAL;
                else if( Token == "<" )
                    Op = LESS;
                else if( Token == "<=" )
                    Op = LESS_EQUAL;
                else if( Token == ">" )
                    Op = GREATER;
                else if( Token == ">=" )
                    Op = GREATER_EQUAL;
            }

            if( Op == LITERAL )
                break;
            ++Position;
            const s32 Right = Op == IMPLIES ? ParseLevel( Level ) : ParseLevel( Level + 1 );

            if( Right < 0 )
                return -1;

            Left = Add( Op, Left, Right );

            if( Level == 5 )
                break;
        }

        return Left;
    };

    // Conditional expressions recurse into both alternatives.
    ParseConditional = [&]() -> s32
    {
        if( ++Depth > 64 )
        {
            Error = "combo expression exceeds nesting limit";
            --Depth;
            return -1;
        }

        struct depth_guard
        {
            u32& Value;
            ~depth_guard()
            {
                --Value;
            }
        } Guard{ Depth };
        const s32 Condition = ParseLevel( 0 );

        if( Condition < 0 || Peek() != "?" )
            return Condition;

        ++Position;
        const s32 Then = ParseConditional();

        if( Then < 0 || Peek() != ":" )
        {
            if( Error.empty() )
                Error = "missing conditional alternative";
            return -1;
        }

        ++Position;
        const s32 Else = ParseConditional();

        if( Else < 0 )
            return -1;

        const s32 Index      = Add( SELECT, Condition, Then );
        m_Nodes[Index].Third = Else;
        return Index;
    };

    // Reject partial parses before checking the depth of the completed tree.
    m_Root = ParseConditional();

    if( m_Root < 0 || Position != Tokens.size() )
    {
        if( Error.empty() )
            Error = "unexpected combo token '" + Peek() + "'";
        m_Nodes.clear();
        m_Root = -1;
        return FALSE;
    }

    std::vector<u32> Heights( m_Nodes.size() );

    for( size_t i = 0; i < m_Nodes.size(); ++i )
    {
        const node& Node   = m_Nodes[i];
        u32         Height = 1;

        for( s32 Child : { Node.Left, Node.Right, Node.Third } )
            if( Child >= 0 )
                Height = std::max( Height, Heights[Child] + 1 );
        if( Height > 64 )
        {
            Error = "combo expression exceeds tree depth limit";
            m_Nodes.clear();
            m_Root = -1;
            return FALSE;
        }

        Heights[i] = Height;
    }

    return TRUE;
}

//==============================================================================
//  EVALUATION
//==============================================================================

xbool shader_combo_expression::EvaluateNode( s32 Index, const std::map<std::string, s64>& Values,
                                             s64& Result, std::string& Error ) const
{
    const auto& Node = m_Nodes[Index];

    if( Node.Operation == LITERAL )
    {
        Result = Node.Value;
        return TRUE;
    }

    if( Node.Operation == SYMBOL )
    {
        const auto Found = Values.find( Node.Name );

        if( Found == Values.end() )
        {
            Error = "missing value for combo symbol '" + Node.Name + "'";
            return FALSE;
        }

        Result = Found->second;
        return TRUE;
    }

    // Resolve unary and short-circuit operations before evaluating the right side.
    s64 Left = 0, Right = 0;

    if( !EvaluateNode( Node.Left, Values, Left, Error ) )
        return FALSE;

    if( Node.Operation == SELECT )
        return EvaluateNode( Left ? Node.Right : Node.Third, Values, Result, Error );

    if( Node.Operation == NOT )
    {
        Result = !Left;
        return TRUE;
    }

    if( Node.Operation == NEGATE )
    {
        if( Left == std::numeric_limits<s64>::min() )
        {
            Error = "combo negation overflow";
            return FALSE;
        }

        Result = -Left;
        return TRUE;
    }

    if( ( Node.Operation == AND && !Left ) || ( ( Node.Operation == OR || Node.Operation == IMPLIES ) &&
                                                ( Node.Operation == OR ? Left != 0 : Left == 0 ) ) )
    {
        Result = Node.Operation != AND;
        return TRUE;
    }

    if( !EvaluateNode( Node.Right, Values, Right, Error ) )
        return FALSE;

    switch( Node.Operation )
    {
        case BIT_AND:
            Result = Left & Right;
            break;

        case BIT_OR:
            Result = Left | Right;
            break;

        case EQUAL:
            Result = Left == Right;
            break;

        case UNEQUAL:
            Result = Left != Right;
            break;

        case LESS:
            Result = Left < Right;
            break;

        case LESS_EQUAL:
            Result = Left <= Right;
            break;

        case GREATER:
            Result = Left > Right;
            break;

        case GREATER_EQUAL:
            Result = Left >= Right;
            break;

        case AND:
            Result = Left && Right;
            break;

        case OR:
            Result = Left || Right;
            break;

        case IMPLIES:
            Result = !Left || Right;
            break;

        default:
            Error = "invalid combo operation";
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

xbool shader_combo_expression::Evaluate( const std::map<std::string, s64>& Values,
                                         s64& Result, std::string& Error ) const
{
    Result = 0;
    Error.clear();

    if( m_Root < 0 )
    {
        Error = "combo expression was not parsed";
        return FALSE;
    }

    return EvaluateNode( m_Root, Values, Result, Error );
}

//==============================================================================

//  C++ GENERATION
//==============================================================================

xbool shader_combo_expression::EmitNode( s32 Index, const std::map<std::string, std::string>& Symbols,
                                         std::string& Text, std::string& Error ) const
{
    const auto& Node = m_Nodes[Index];

    if( Node.Operation == LITERAL )
    {
        Text = std::to_string( Node.Value ) + "ll";
        return TRUE;
    }

    if( Node.Operation == SYMBOL )
    {
        const auto Found = Symbols.find( Node.Name );

        if( Found == Symbols.end() )
        {
            Error = "missing C++ mapping for combo symbol '" + Node.Name + "'";
            return FALSE;
        }

        Text = "static_cast<s64>(" + Found->second + ")";
        return TRUE;
    }

    std::string Left, Right;

    if( !EmitNode( Node.Left, Symbols, Left, Error ) )
        return FALSE;

    if( Node.Operation == SELECT )
    {
        std::string Else;

        if( !EmitNode( Node.Right, Symbols, Right, Error ) || !EmitNode( Node.Third, Symbols, Else, Error ) )
            return FALSE;

        Text = "(" + Left + " ? " + Right + " : " + Else + ")";
        return TRUE;
    }

    if( Node.Operation == NOT || Node.Operation == NEGATE )
    {
        Text = "(" + std::string( Node.Operation == NOT ? "!" : "-" ) + Left + ")";
        return TRUE;
    }

    if( !EmitNode( Node.Right, Symbols, Right, Error ) )
        return FALSE;

    if( Node.Operation == IMPLIES )
        Text = "(!(" + Left + ") || (" + Right + "))";
    else
    {
        const char* Operators[] = { "", "", "", "", "==", "!=", "<", "<=", ">", ">=", "&&", "||", "", "", "&", "|" };
        Text                    = "(" + Left + " " + Operators[Node.Operation] + " " + Right + ")";
    }

    return TRUE;
}

//==============================================================================

xbool shader_combo_expression::EmitCpp( const std::map<std::string, std::string>& Symbols,
                                        std::string& Text, std::string& Error ) const
{
    Text.clear();
    Error.clear();

    if( m_Root < 0 )
    {
        Error = "combo expression was not parsed";
        return FALSE;
    }

    return EmitNode( m_Root, Symbols, Text, Error );
}
