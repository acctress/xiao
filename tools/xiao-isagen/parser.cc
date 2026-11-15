#include "parser.hh"

#include <string>
#include <utility>

namespace isagen
{
    namespace
    {
        [[nodiscard]] constexpr bool is_atom_char( const char c ) noexcept
        { return c > ' ' && c != '(' && c != ')' && c != '"' && c != ';'; }

    } // namespace

    Lexer::Lexer( const std::string_view src, const std::string_view file ) noexcept : m_src { src }, m_loc { .file = file } { }

    char Lexer::peek( ) const noexcept
    { return m_pos < m_src.length( ) ? m_src[ m_pos ] : '\0'; }

    char Lexer::bump( ) noexcept
    {
        const char c = m_src[ m_pos++ ];
        if ( c == '\n' )
        {
            ++m_loc.line;
            m_loc.col = 1;
        }
        else
            ++m_loc.col;
        return c;
    }

    void Lexer::skip_trivia( ) noexcept
    {
        for ( ;; )
        {
            if ( const char c = peek( ); c == ';' )
                while ( peek( ) != '\n' && peek( ) != '\0' ) bump( );
            else if ( c != '\0' && c <= ' ' )
                bump( );
            else
                return;
        }
    }

    Token Lexer::next( )
    {
        skip_trivia( );

        const auto begin = m_pos;
        const auto start = m_loc;

        if ( m_pos >= m_src.length( ) ) return { TokenType::END, { }, start };

        switch ( bump( ) )
        {
            case '(': return { TokenType::LParen, m_src.substr( begin, 1 ), start };
            case ')': return { TokenType::RParen, m_src.substr( begin, 1 ), start };
            case '"':
            {
                while ( peek( ) != '"' )
                {
                    if ( peek( ) == '\0' || peek( ) == '\n' ) fail( start, "unterminated string" );
                    bump( );
                }

                bump( );

                const auto lexeme = m_src.substr( begin + 1, m_pos - begin - 2 );
                return { TokenType::String, lexeme, start };
            }
            default:
            {
                while ( is_atom_char( peek( ) ) ) bump( );
                return { TokenType::Atom, m_src.substr( begin, m_pos - begin ), start };
            }
        }
    }

    Parser::Parser( const std::string_view src, const std::string_view file ) noexcept : m_lexer { src, file } { }

    std::vector< SExp > Parser::parse( )
    {
        std::vector< SExp > forms;
        for ( auto t = m_lexer.next( ); t.type != TokenType::END; t = m_lexer.next( ) ) forms.push_back( parse_expr( t ) );
        return forms;
    }

    SExp Parser::parse_expr( const Token& tok )
    {
        switch ( tok.type )
        {
            case TokenType::LParen:
            {
                std::vector< SExp > items;
                for ( auto t = m_lexer.next( ); t.type != TokenType::RParen; t = m_lexer.next( ) )
                {
                    if ( t.type == TokenType::END ) fail( tok.loc, "unclosed '('" );
                    items.push_back( parse_expr( t ) );
                }

                return SExp { std::move( items ), tok.loc };
            }

            case TokenType::RParen: fail( tok.loc, "unexpected ')'" );
            case TokenType::String: return SExp { String { std::string { tok.lexeme } }, tok.loc };
            case TokenType::Atom: return SExp { Symbol { std::string { tok.lexeme } }, tok.loc };

            case TokenType::END: std::unreachable( );
        }

        std::unreachable( );
    }

    std::vector< SExp > read_source( const std::string_view src, const std::string_view file )
    { return Parser { src, file }.parse( ); }
} // namespace isagen
