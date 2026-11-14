#include "parser.hh"

#include <charconv>
#include <optional>
#include <string>
#include <utility>

namespace isagen
{
    namespace
    {
        [[nodiscard]] constexpr bool is_atom_char( const char c ) noexcept
        { return c > ' ' && c != '(' && c != ')' && c != '"' && c != ';'; }

        [[nodiscard]] std::optional< uint64_t > parse_int( std::string_view text ) noexcept
        {
            int base = 10;
            if ( text.starts_with( "0x" ) )
            {
                base = 16;
                text.remove_prefix( 2 );
            }

            uint64_t          value { };
            const auto* const last = text.data( ) + text.size( );
            if ( const auto [ ptr, ec ] = std::from_chars( text.data( ), last, value, base );
                 text.empty( ) || ec != std::errc { } || ptr != last )
                return std::nullopt;
            return value;
        }
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

    Token Lexer::next( ) const
    { return { TokenType::END, { }, m_loc }; }

    Parser::Parser( const std::string_view src, const std::string_view file ) noexcept : m_lexer { src, file } { }

    std::vector< SExp > Parser::parse( )
    { return { }; }

    SExp Parser::parse_expr( const Token& tok )
    { fail( tok.loc, "parse_expr is not implemented" ); }

    std::vector< SExp > read_source( const std::string_view src, const std::string_view file )
    { return Parser { src, file }.parse( ); }
} // namespace isagen
