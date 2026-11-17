#include "gen_header.hh"
#include "writer.hh"

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace isagen
{
    namespace
    {
        constexpr std::string_view ns = "xiao::x86_64::inst";

        [[nodiscard]] bool visible( const Operand& op ) noexcept
        { return !op.is_fixed( ) && op.kind != Kind::Cond; }

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

        [[nodiscard]] std::string escape_format( const std::string_view text )
        {
            std::string out;
            for ( const char c : text )
            {
                out += c;
                if ( c == '{' || c == '}' ) out += c;
            }
            return out;
        }

        [[nodiscard]] std::string upper( const std::string_view text )
        {
            std::string out { text };
            std::ranges::transform( out, out.begin( ),
                                    []( const char c )
                                    {
                                        return c >= 'a' && c <= 'z' ? static_cast< char >( c - 'a' + 'A' ) : c;
                                    } );
            return out;
        }

        [[nodiscard]] const Operand& at( const Inst& inst, const int8_t slot )
        { return inst.ops[ static_cast< std::size_t >( slot ) ]; }

        [[nodiscard]] std::string field_type( const Operand& op )
        {
            switch ( op.kind )
            {
                case Kind::Gpr: return "Reg";
                case Kind::GprMem: return "GprMem";
                case Kind::Mem: return "AddressMode";
                case Kind::Imm: return std::format( "int{}_t", op.bits );
                case Kind::Cond: return "CC";
            }
            std::unreachable( );
        }

        [[nodiscard]] std::string_view visitor_method( const Role role ) noexcept
        {
            switch ( role )
            {
                case Role::Read: return "use";
                case Role::Write: return "def";
                case Role::ReadWrite: return "usedef";
            }
            std::unreachable( );
        }

        [[nodiscard]] std::string_view operand_kind( const Role role ) noexcept
        {
            switch ( role )
            {
                case Role::Read: return "OperandKind::Use";
                case Role::Write: return "OperandKind::Def";
                case Role::ReadWrite: return "OperandKind::UseDef";
            }
            std::unreachable( );
        }

        [[nodiscard]] std::string visit_stmt( const Operand& op )
        {
            switch ( op.kind )
            {
                case Kind::Gpr:
                    if ( op.is_fixed( ) )
                        return std::format( "v.{}( self.{}, {} );", visitor_method( op.role ), op.name, upper( op.fixed ) );
                    return std::format( "v.{}( self.{} );", visitor_method( op.role ), op.name );
                case Kind::GprMem: return std::format( "visit_gpr_mem( self.{}, v, {} );", op.name, operand_kind( op.role ) );
                case Kind::Mem: return std::format( "v.use( self.{}.base );\nv.use( self.{}.index );", op.name, op.name );
                case Kind::Imm:
                case Kind::Cond: return { };
            }
            std::unreachable( );
        }

        [[nodiscard]] std::string opcode_literal( const Encoding& enc )
        {
            std::vector< std::string > fields;
            if ( enc.p66 ) fields.emplace_back( ".p66 = true" );
            if ( enc.rep ) fields.push_back( std::format( ".rep = 0x{:02X}", enc.rep ) );
            if ( enc.rex_w ) fields.emplace_back( ".rex_w = true" );

            std::vector< std::string > bytes;
            for ( const auto byte : enc.opcode ) bytes.push_back( std::format( "0x{:02X}", byte ) );
            fields.push_back( ".bytes = { " + join( bytes, ", " ) + " }" );
            fields.push_back( std::format( ".len = {}", enc.opcode.size( ) ) );

            return "Opcode { " + join( fields, ", " ) + " }";
        }

        [[nodiscard]] bool byte_regs( const Inst& inst ) noexcept
        {
            return std::ranges::any_of( inst.ops,
                                        []( const Operand& op )
                                        {
                                            return visible( op ) && ( op.kind == Kind::Gpr || op.kind == Kind::GprMem ) && op.bits == 8;
                                        } );
        }

        [[nodiscard]] std::string rm_arg( const Operand& op )
        { return op.kind == Kind::GprMem ? op.name : "GprMem { " + op.name + " }"; }

        [[nodiscard]] std::string encode_call( const Inst& inst )
        {
            const auto& enc  = inst.enc;
            const auto  byte = byte_regs( inst ) ? "true" : "false";

            if ( enc.modrm_r )
                return std::format( "emit_rm( buf, op, {}.as_preg( ), {}, {} );", at( inst, enc.reg ).name, rm_arg( at( inst, enc.rm ) ),
                                    byte );
            if ( enc.digit != no_slot )
                return std::format( "emit_rm_digit( buf, op, {}, {}, {} );", enc.digit, rm_arg( at( inst, enc.rm ) ), byte );
            if ( enc.plus_r ) return std::format( "emit_opreg( buf, op, {}.as_preg( ), {} );", at( inst, enc.opreg ).name, byte );
            return "emit_op( buf, op );";
        }

        [[nodiscard]] std::string imm_stmt( const Inst& inst )
        {
            const auto& enc = inst.enc;
            if ( enc.imm == no_slot ) return { };

            const auto& name = at( inst, enc.imm ).name;
            switch ( enc.imm_bytes )
            {
                case 1: return std::format( "buf.emit_u8( static_cast< uint8_t >( {} ) );", name );
                case 2: return std::format( "buf.emit_u16( static_cast< uint16_t >( {} ) );", name );
                case 4: return std::format( "buf.emit_i32( {} );", name );
                case 8: return std::format( "buf.emit_u64( static_cast< uint64_t >( {} ) );", name );
                default: std::unreachable( );
            }
        }

        [[nodiscard]] std::string to_string_stmt( const Inst& inst )
        {
            std::string                fmt { escape_format( inst.mnemonic ) };
            std::vector< std::string > args;

            if ( inst.enc.cond != no_slot )
            {
                fmt += "{}";
                args.push_back( std::format( "x86_64::to_string( {} )", at( inst, inst.enc.cond ).name ) );
            }

            std::vector< std::string > holes;
            for ( const auto& op : inst.ops | std::views::filter( visible ) )
            {
                holes.emplace_back( "{}" );
                switch ( op.kind )
                {
                    case Kind::Gpr: args.push_back( std::format( "fmt_gpr( {}, {} )", op.name, op.bits ) ); break;
                    case Kind::GprMem: args.push_back( std::format( "fmt_gpr_mem( {}, {} )", op.name, op.bits ) ); break;
                    case Kind::Mem: args.push_back( std::format( "fmt_mem( {}, {} )", op.name, op.bits ) ); break;
                    case Kind::Imm: args.push_back( op.name ); break;
                    case Kind::Cond: std::unreachable( );
                }
            }
            if ( !holes.empty( ) ) fmt += " " + join( holes, ", " );

            if ( args.empty( ) ) return "return std::string { \"" + inst.mnemonic + "\" };";
            return "return std::format( \"" + fmt + "\", " + join( args, ", " ) + " );";
        }

        void emit_fields( Writer& w, const Inst& inst )
        {
            std::size_t width { };
            for ( const auto& op : inst.ops ) width = std::max( width, field_type( op ).size( ) );

            for ( const auto& op : inst.ops ) w.line( "{:<{}} {} {{ }};", field_type( op ), width, op.name );
        }

        void emit_visit( Writer& w, const Inst& inst )
        {
            std::vector< std::string > stmts;
            for ( const auto& op : inst.ops )
                if ( auto stmt = visit_stmt( op ); !stmt.empty( ) ) stmts.push_back( std::move( stmt ) );

            if ( stmts.empty( ) )
            {
                w.raw( "void visit( this auto&, auto& ) { }" );
                return;
            }

            w.block( "void visit( this auto& self, auto& v )",
                     [ & ]
                     {
                         for ( const auto& stmt : stmts )
                             for ( const auto line : std::views::split( stmt, '\n' ) )
                                 w.raw( std::string_view { line.begin( ), line.end( ) } );
                     } );
        }

        void emit_emit( Writer& w, const Inst& inst )
        {
            w.block( "void emit( MachBuffer< LabelUse >& buf ) const",
                     [ & ]
                     {
                         const auto& enc = inst.enc;
                         if ( enc.plus_cc )
                         {
                             w.raw( "auto op = " + opcode_literal( enc ) + ";" );
                             w.line( "op.bytes[ {} ] |= cc_nibble( {} );", enc.opcode.size( ) - 1, at( inst, enc.cond ).name );
                         }
                         else
                             w.raw( "constexpr auto op = " + opcode_literal( enc ) + ";" );

                         w.raw( encode_call( inst ) );
                         if ( auto stmt = imm_stmt( inst ); !stmt.empty( ) ) w.raw( stmt );
                     } );
        }

        void emit_mem_access( Writer& w, const Inst& inst )
        {
            std::vector< std::string > terms;
            for ( const auto& op : inst.ops )
            {
                if ( op.kind == Kind::Mem ) terms.emplace_back( "true" );
                if ( op.kind == Kind::GprMem ) terms.push_back( "std::holds_alternative< AddressMode >( " + op.name + ".value )" );
            }
            if ( terms.empty( ) ) return;

            w.blank( );
            w.raw( "[[nodiscard]] bool mem_access( ) const noexcept { return " + join( terms, " || " ) + "; }" );
        }

        void emit_to_string( Writer& w, const Inst& inst )
        {
            w.block( "[[nodiscard]] std::string to_string( ) const",
                     [ & ]
                     {
                         w.raw( to_string_stmt( inst ) );
                     } );
        }

        void emit_struct( Writer& w, const Inst& inst )
        {
            w.block(
                "struct " + inst.name,
                [ & ]
                {
                    emit_fields( w, inst );
                    w.blank( );
                    emit_visit( w, inst );
                    w.blank( );
                    emit_emit( w, inst );
                    w.blank( );
                    emit_to_string( w, inst );
                    emit_mem_access( w, inst );
                },
                ";" );
        }

        [[nodiscard]] std::vector< std::string > kind_names( const Isa& isa )
        {
            std::vector< std::string > names;
            names.reserve( isa.insts.size( ) + isa.pseudos.size( ) );
            for ( const auto& inst : isa.insts ) names.push_back( inst.name );
            for ( const auto& pseudo : isa.pseudos ) names.push_back( pseudo.name );
            return names;
        }

        void emit_list( Writer& w, const std::string_view open, const std::vector< std::string >& items, const std::string_view close )
        {
            w.raw( open );
            w.indented(
                [ & ]
                {
                    for ( std::size_t i = 0; i < items.size( ); ++i ) w.raw( items[ i ] + ( i + 1 == items.size( ) ? "" : "," ) );
                } );
            w.raw( close );
        }

        void emit_kinds( Writer& w, const std::vector< std::string >& names )
        {
            w.block(
                "enum class Kind : uint16_t",
                [ & ]
                {
                    for ( const auto& name : names ) w.line( "{},", name );
                },
                ";" );
            w.blank( );
            w.line( "inline constexpr std::size_t kind_count = {};", names.size( ) );
            w.blank( );
            w.raw( "template < typename T >" );
            w.raw( "struct kind_of;" );
            w.blank( );
            for ( const auto& name : names )
                w.raw( "template <> struct kind_of< " + name + " > { static constexpr Kind value = Kind::" + name + "; };" );
            w.blank( );
            w.raw( "template < typename T >" );
            w.raw( "concept member = requires { kind_of< std::remove_cvref_t< T > >::value; };" );
            w.blank( );
            w.raw( "template < typename T >" );
            w.raw( "inline constexpr Kind kind_v = kind_of< std::remove_cvref_t< T > >::value;" );
            w.blank( );
            w.raw( "template < template < typename... > class List >" );
            emit_list( w, "using all_kinds = List<", names, ">;" );
        }

        void emit_minst( Writer& w, const std::vector< std::string >& names )
        {
            std::vector< std::string > aligns;
            std::vector< std::string > sizes;
            for ( const auto& name : names )
            {
                aligns.push_back( "alignof( " + name + " )" );
                sizes.push_back( "sizeof( " + name + " )" );
            }

            const auto rebuild = [ & ]( const std::string_view param, const std::string_view value )
            {
                w.raw( std::string { "other.dispatch( [ this ]< typename T >( " } + std::string { param }
                       + " inst ) { std::construct_at( slot< T >( ), " + std::string { value } + " ); } );" );
            };

            w.block(
                "class MInst",
                [ & ]
                {
                    w.label( "public:" );
                    w.raw( "template < typename F >" );
                    w.block( "decltype( auto ) dispatch( this auto& self, F&& f )",
                             [ & ]
                             {
                                 w.block( "switch ( self.m_kind )",
                                          [ & ]
                                          {
                                              for ( const auto& name : names )
                                                  w.raw( "case Kind::" + name + ": return std::forward< F >( f )( self.template get< "
                                                         + name + " >( ) );" );
                                          } );
                                 w.raw( "std::unreachable( );" );
                             } );
                    w.blank( );
                    w.raw( "template < member T >" );
                    w.block( "MInst( T&& inst ) : m_kind { kind_v< T > }",
                             [ & ]
                             {
                                 w.raw( "std::construct_at( slot< std::remove_cvref_t< T > >( ), std::forward< T >( inst ) );" );
                             } );
                    w.blank( );
                    w.block( "MInst( const MInst& other ) : m_kind { other.m_kind }",
                             [ & ]
                             {
                                 rebuild( "const T&", "inst" );
                             } );
                    w.blank( );
                    w.block( "MInst( MInst&& other ) noexcept : m_kind { other.m_kind }",
                             [ & ]
                             {
                                 rebuild( "T&", "std::move( inst )" );
                             } );
                    w.blank( );
                    w.block( "MInst& operator=( const MInst& other )",
                             [ & ]
                             {
                                 w.raw( "if ( this == &other ) return *this;" );
                                 w.raw( "destroy( );" );
                                 w.raw( "m_kind = other.m_kind;" );
                                 rebuild( "const T&", "inst" );
                                 w.raw( "return *this;" );
                             } );
                    w.blank( );
                    w.block( "MInst& operator=( MInst&& other ) noexcept",
                             [ & ]
                             {
                                 w.raw( "if ( this == &other ) return *this;" );
                                 w.raw( "destroy( );" );
                                 w.raw( "m_kind = other.m_kind;" );
                                 rebuild( "T&", "std::move( inst )" );
                                 w.raw( "return *this;" );
                             } );
                    w.blank( );
                    w.raw( "~MInst( ) { destroy( ); }" );
                    w.blank( );
                    w.raw( "[[nodiscard]] Kind kind( ) const noexcept { return m_kind; }" );
                    w.blank( );
                    w.raw( "template < member T >" );
                    w.raw( "[[nodiscard]] bool is( ) const noexcept { return m_kind == kind_v< T >; }" );
                    w.blank( );
                    w.raw( "template < member T >" );
                    w.raw( "[[nodiscard]] T* get_if( ) noexcept { return is< T >( ) ? std::launder( slot< T >( ) ) : nullptr; }" );
                    w.blank( );
                    w.raw( "template < member T >" );
                    w.raw(
                        "[[nodiscard]] const T* get_if( ) const noexcept { return is< T >( ) ? std::launder( slot< T >( ) ) : nullptr; }" );
                    w.blank( );
                    w.raw( "template < member T >" );
                    w.block( "[[nodiscard]] T& get( ) noexcept",
                             [ & ]
                             {
                                 w.raw( "assert( is< T >( ) && \"[xiao::x86_64::inst::MInst] get with the wrong kind\" );" );
                                 w.raw( "return *std::launder( slot< T >( ) );" );
                             } );
                    w.blank( );
                    w.raw( "template < member T >" );
                    w.block( "[[nodiscard]] const T& get( ) const noexcept",
                             [ & ]
                             {
                                 w.raw( "assert( is< T >( ) && \"[xiao::x86_64::inst::MInst] get with the wrong kind\" );" );
                                 w.raw( "return *std::launder( slot< T >( ) );" );
                             } );
                    w.blank( );
                    w.label( "private:" );
                    emit_list( w, "static constexpr std::size_t storage_align = std::max( {", aligns, "} );" );
                    w.blank( );
                    emit_list( w, "static constexpr std::size_t storage_size = std::max( {", sizes, "} );" );
                    w.blank( );
                    w.raw( "template < typename T >" );
                    w.raw( "[[nodiscard]] T* slot( ) noexcept { return reinterpret_cast< T* >( m_storage ); }" );
                    w.blank( );
                    w.raw( "template < typename T >" );
                    w.raw( "[[nodiscard]] const T* slot( ) const noexcept { return reinterpret_cast< const T* >( m_storage ); }" );
                    w.blank( );
                    w.block( "void destroy( ) noexcept",
                             [ & ]
                             {
                                 w.raw( "dispatch( []< typename T >( T& inst ) { std::destroy_at( &inst ); } );" );
                             } );
                    w.blank( );
                    w.raw( "alignas( storage_align ) std::byte m_storage[ storage_size ];" );
                    w.raw( "Kind                               m_kind;" );
                },
                ";" );
        }
    } // namespace

    std::string gen_header( const Isa& isa )
    {
        const auto& insts = isa.insts;
        const auto  names = kind_names( isa );

        Writer w;
        w.raw( "#pragma once" );
        w.blank( );
        w.raw( "#include <xiao/isa/x86/asm.hh>" );
        for ( const auto& header : isa.headers ) w.raw( "#include <" + header + ">" );
        w.blank( );
        for ( const auto* header :
              { "algorithm", "cassert", "cstddef", "cstdint", "format", "memory", "new", "string", "type_traits", "utility", "variant" } )
            w.raw( std::string { "#include <" } + header + ">" );
        w.blank( );

        w.block(
            std::format( "namespace {}", ns ),
            [ & ]
            {
                for ( const auto& inst : insts )
                {
                    emit_struct( w, inst );
                    w.blank( );
                }

                if ( names.empty( ) ) return;
                emit_kinds( w, names );
                w.blank( );
                emit_minst( w, names );
            },
            std::format( " // namespace {}", ns ) );

        return std::move( w ).take( );
    }
} // namespace isagen
