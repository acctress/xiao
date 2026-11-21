#include "gen_tests.hh"
#include "writer.hh"

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace isagen
{
    namespace
    {
        constexpr int samples = 10;

        [[nodiscard]] std::string join( const std::vector< std::string >& parts, const std::string_view sep )
        {
            std::string out;
            for ( std::size_t i = 0; i < parts.size( ); ++i )
            {
                if ( i ) out += sep;
                out += parts[ i ];
            }
            return out;
        }

        [[nodiscard]] std::string upper( const std::string_view text )
        {
            std::string out { text };
            for ( auto& c : out )
                if ( c >= 'a' && c <= 'z' ) c = static_cast< char >( c - 'a' + 'A' );
            return out;
        }

        [[nodiscard]] std::string sample_decl( const Operand& op, const std::size_t index )
        {
            if ( op.is_fixed( ) ) return std::format( "const Reg {} = Reg::from_preg( {} );", op.name, upper( op.fixed ) );

            switch ( op.kind )
            {
                case Kind::Gpr: return std::format( "const Reg {} = sample_gpr( k + {} );", op.name, index * 3 );
                case Kind::GprMem: return std::format( "const GprMem {} = sample_rm( k + {} );", op.name, index * 2 );
                case Kind::Mem: return std::format( "const AddressMode {} = sample_mem( k + {} );", op.name, index );
                case Kind::Imm:
                    return std::format( "const auto {} = static_cast< int{}_t >( sample_imm( {}, k ) );", op.name, op.bits, op.bits );
                case Kind::Cond: return std::format( "const CC {} = sample_cc( k );", op.name );
            }
            std::unreachable( );
        }

        [[nodiscard]] std::string expectation( const Operand& op )
        {
            switch ( op.kind )
            {
                case Kind::Gpr: return std::format( "expect_gpr( {}, {} )", op.name, op.bits );
                case Kind::GprMem: return std::format( "expect_rm( {}, {} )", op.name, op.bits );
                case Kind::Mem: return std::format( "expect_mem( {}, {} )", op.name, op.bits );
                case Kind::Imm: return std::format( "expect_imm( {} )", op.name );
                case Kind::Cond: break;
            }
            std::unreachable( );
        }

        [[nodiscard]] std::string check_call( const Inst& inst )
        {
            std::vector< std::string > fields;
            std::vector< std::string > expects;
            for ( const auto& op : inst.ops )
            {
                fields.push_back( op.name );
                if ( !op.is_fixed( ) && op.kind != Kind::Cond ) expects.push_back( expectation( op ) );
            }

            const auto cc = inst.enc.cond != no_slot ? "true" : "false";
            return "check( inst::" + inst.name + " { " + join( fields, ", " ) + " }, \"" + inst.mnemonic + "\", " + cc + ", { "
                 + join( expects, ", " ) + " } );";
        }

        void emit_case( Writer& w, const Inst& inst )
        {
            w.block( std::format( "TEST_CASE( \"isagen x86 {}\", \"[isagen][x86]\" )", inst.name ),
                     [ & ]
                     {
                         w.block( std::format( "for ( int k = 0; k < {}; ++k )", samples ),
                                  [ & ]
                                  {
                                      for ( std::size_t i = 0; i < inst.ops.size( ); ++i ) w.raw( sample_decl( inst.ops[ i ], i ) );
                                      w.raw( check_call( inst ) );
                                  } );
                     } );
        }
    } // namespace

    std::string gen_tests( const std::vector< Inst >& insts )
    {
        Writer w;
        w.raw( "#if defined( XIAO_HAS_ZYDIS )" );
        w.blank( );
        w.raw( "#include \"isa_check.hh\"" );
        w.blank( );
        w.raw( "#include <xiao/isa/x86/inst/gen/insts.hh>" );
        w.blank( );
        w.raw( "using namespace isa_check;" );
        w.blank( );

        for ( const auto& inst : insts )
        {
            emit_case( w, inst );
            w.blank( );
        }

        w.raw( "#endif" );
        return std::move( w ).take( );
    }
} // namespace isagen
