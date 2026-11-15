#pragma once

#include <print>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace isagen
{
    template < typename... Forms >
    struct Overloaded : Forms...
    {
        using Forms::operator( )...;
    };

    struct Location
    {
        std::string_view file { };
        uint32_t         line { 1 };
        uint32_t         col { 1 };
    };

    struct Symbol
    {
        std::string name;
    };

    struct String
    {
        std::string value;
    };

    struct SExp
    {
        std::variant< Symbol, String, uint64_t, std::vector< SExp > > value;
        Location                                                      loc;
    };

    using List = std::vector< SExp >;

    [[noreturn]] inline void fail( const Location& loc, const std::string_view msg )
    {
        std::println( stderr, "[error @ {}:{}:{}] {}", loc.file, loc.line, loc.col, msg );
        std::exit( 1 );
    }
} // namespace isagen