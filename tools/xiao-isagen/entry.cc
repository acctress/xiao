#include "build.hh"
#include "expand.hh"
#include "gen_header.hh"
#include "gen_tests.hh"
#include "parser.hh"

#include <cstddef>
#include <filesystem>
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

    [[nodiscard]] constexpr std::string_view role_name( const isagen::Role role ) noexcept
    {
        switch ( role )
        {
            case isagen::Role::Read: return "r";
            case isagen::Role::Write: return "w";
            case isagen::Role::ReadWrite: return "rw";
        }
        return "?";
    }

    [[nodiscard]] constexpr std::string_view kind_name( const isagen::Kind kind ) noexcept
    {
        switch ( kind )
        {
            case isagen::Kind::Gpr: return "r";
            case isagen::Kind::GprMem: return "rm";
            case isagen::Kind::Mem: return "m";
            case isagen::Kind::Imm: return "imm";
            case isagen::Kind::Cond: return "cc";
        }
        return "?";
    }

    void dump( const isagen::Inst& inst )
    {
        std::println( "{} \"{}\"", inst.name, inst.mnemonic );

        for ( std::size_t i = 0; i < inst.ops.size( ); ++i )
        {
            const auto& op = inst.ops[ i ];
            std::print( "  op{} {} {} {}{}", i, op.name, role_name( op.role ), kind_name( op.kind ),
                        op.bits ? std::to_string( op.bits ) : "" );
            if ( op.is_fixed( ) ) std::print( " fixed {}", op.fixed );
            std::println( );
        }

        const auto& enc = inst.enc;
        std::string line { "  enc" };

        if ( enc.p66 ) line += " 66";
        if ( enc.rep ) std::format_to( std::back_inserter( line ), " {:02X}", enc.rep );
        if ( enc.rex_w ) line += " REX.W";

        for ( const auto byte : enc.opcode ) std::format_to( std::back_inserter( line ), " {:02X}", byte );

        if ( enc.plus_r ) line += " +r";
        if ( enc.plus_cc ) line += " +cc";
        if ( enc.modrm_r ) line += " /r";
        if ( enc.digit != isagen::no_slot ) std::format_to( std::back_inserter( line ), " /{}", enc.digit );
        if ( enc.imm_bytes ) std::format_to( std::back_inserter( line ), " imm{}", enc.imm_bytes * 8 );

        const auto slot = [ & ]( const std::string_view name, const int8_t index )
        {
            if ( index != isagen::no_slot ) std::format_to( std::back_inserter( line ), " {}=op{}", name, index );
        };
        slot( "rm", enc.rm );
        slot( "reg", enc.reg );
        slot( "opreg", enc.opreg );
        slot( "imm", enc.imm );
        slot( "cond", enc.cond );

        std::println( "{}", line );
    }

    [[nodiscard]] bool write_file( const std::string_view path, const std::string_view content )
    {
        const std::string name { path };
        if ( std::ifstream in { name, std::ios::binary } )
        {
            const std::string current( std::istreambuf_iterator< char > { in }, { } );
            if ( current == content ) return true;
        }

        if ( const auto parent = std::filesystem::path { name }.parent_path( ); !parent.empty( ) )
            std::filesystem::create_directories( parent );

        if ( std::ofstream out { name, std::ios::binary | std::ios::trunc };
             !out.write( content.data( ), static_cast< std::streamsize >( content.size( ) ) ) )
        {
            std::println( stderr, "cannot write {}", path );
            return false;
        }
        return true;
    }
} // namespace

int main( const int argc, char** argv )
{
    bool                            dump_sexp { };
    std::string_view                header_path { };
    std::string_view                tests_path { };
    std::vector< std::string_view > inputs;
    for ( int i = 1; i < argc; ++i )
    {
        const std::string_view arg { argv[ i ] };
        if ( arg == "--header" || arg == "--tests" )
        {
            if ( i + 1 == argc || std::string_view { argv[ i + 1 ] }.starts_with( "--" ) )
            {
                std::println( stderr, "{} needs a path", arg );
                return 1;
            }
            ( arg == "--header" ? header_path : tests_path ) = argv[ ++i ];
            continue;
        }
        if ( arg == "--sexp" )
        {
            dump_sexp = true;
            continue;
        }
        if ( arg.starts_with( "--" ) )
        {
            std::println( stderr, "unknown option {}", arg );
            return 1;
        }
        inputs.push_back( arg );
    }

    if ( inputs.empty( ) )
    {
        std::println( stderr, "usage: xiao-isagen [--sexp] [--header <path>] [--tests <path>] <input.isa>..." );
        return 1;
    }

    isagen::List forms;
    for ( const auto path : inputs )
    {
        std::ifstream file { std::string { path }, std::ios::binary };
        if ( !file )
        {
            std::println( stderr, "cannot open {}", path );
            return 1;
        }

        const std::string src( std::istreambuf_iterator< char > { file }, { } );
        for ( auto& form : isagen::expand( isagen::read_source( src, path ) ) ) forms.push_back( std::move( form ) );
    }

    if ( dump_sexp )
    {
        for ( const auto& form : forms )
        {
            std::string line;
            print( line, form );
            std::println( "{}", line );
        }
        return 0;
    }

    const auto isa = isagen::build( forms );

    if ( header_path.empty( ) && tests_path.empty( ) )
    {
        for ( const auto& inst : isa.insts ) dump( inst );
        for ( const auto& [ name, loc ] : isa.pseudos ) std::println( "{} (pseudo)",  name );
        return 0;
    }

    if ( !header_path.empty( ) && !write_file( header_path, isagen::gen_header( isa ) ) ) return 1;
    if ( !tests_path.empty( ) && !write_file( tests_path, isagen::gen_tests( isa.insts ) ) ) return 1;
}
