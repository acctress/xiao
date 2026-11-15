#include "expand.hh"
#include "parser.hh"

#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
    void print( std::string& out, const isagen::SExp& e )
    {
        std::visit(
            [ & ]< typename T >( const T& v )
            {
                if constexpr ( std::is_same_v< T, isagen::Symbol > ) out += v.name;
                else if constexpr ( std::is_same_v< T, isagen::String > )
                    std::format_to( std::back_inserter( out ), "\"{}\"", v.value );
                else if constexpr ( std::is_same_v< T, uint64_t > )
                    std::format_to( std::back_inserter( out ), "0x{:X}", v );
                else
                {
                    out += '(';
                    for ( std::size_t i = 0; i < v.size( ); ++i )
                    {
                        if ( i ) out += ' ';
                        print( out, v[ i ] );
                    }
                    out += ')';
                }
            },
            e.value );
    }
} // namespace

int main( const int argc, char** argv )
{
    std::vector< std::string_view > inputs;
    for ( int i = 1; i < argc; ++i )
    {
        const std::string_view arg { argv[ i ] };
        if ( arg == "--header" || arg == "--tests" )
        {
            ++i;
            continue;
        }
        inputs.push_back( arg );
    }

    for ( const auto path : inputs )
    {
        std::ifstream file { std::string { path }, std::ios::binary };
        if ( !file )
        {
            std::println( stderr, "cannot open {}", path );
            return 1;
        }

        const std::string src( std::istreambuf_iterator< char > { file }, { } );
        for ( const auto& form : isagen::expand( isagen::read_source( src, path ) ) )
        {
            std::string line;
            print( line, form );
            std::println( "{}", line );
        }
    }
}
