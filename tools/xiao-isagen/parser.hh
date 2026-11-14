#pragma once

#include "sexp.hh"

#include <cstdint>
#include <string_view>
#include <vector>

namespace isagen
{
    enum class TokenType : uint8_t
    {
        LParen,
        RParen,
        Atom,
        String,
        END
    };

    struct Token
    {
        TokenType        type { };
        std::string_view lexeme { };
        Location         loc { };
    };

    class Lexer
    {
    public:
        Lexer( std::string_view src, std::string_view file ) noexcept;

        [[nodiscard]] Token next( ) const;

    private:
        [[nodiscard]] char peek( ) const noexcept;
        char               bump( ) noexcept;
        void               skip_trivia( ) noexcept;

        std::string_view m_src { };
        std::size_t      m_pos { };
        Location         m_loc { };
    };

    class Parser
    {
    public:
        Parser( std::string_view src, std::string_view file ) noexcept;

        [[nodiscard]] std::vector< SExp > parse( );

    private:
        [[nodiscard]] SExp parse_expr( const Token& tok );

        Lexer m_lexer;
    };

    [[nodiscard]] std::vector< SExp > read_source( std::string_view src, std::string_view file );
} // namespace isagen
