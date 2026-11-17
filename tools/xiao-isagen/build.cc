#include "build.hh"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>

namespace isagen
{
    namespace
    {
        enum class Slot : uint8_t
        {
            None,
            Rm,
            Reg,
            OpReg,
            Imm
        };

        struct OpEnForm
        {
            std::string_view      name { };
            std::array< Slot, 3 > slots { };
        };

        constexpr std::array op_en_forms {
            OpEnForm { "ZO",                                { }},
            OpEnForm {  "O",                    { Slot::OpReg }},
            OpEnForm { "OI",         { Slot::OpReg, Slot::Imm }},
            OpEnForm {  "I",                      { Slot::Imm }},
            OpEnForm {  "M",                       { Slot::Rm }},
            OpEnForm { "M1",                       { Slot::Rm }},
            OpEnForm { "MC",                       { Slot::Rm }},
            OpEnForm { "MI",            { Slot::Rm, Slot::Imm }},
            OpEnForm { "MR",            { Slot::Rm, Slot::Reg }},
            OpEnForm { "RM",            { Slot::Reg, Slot::Rm }},
            OpEnForm {"RMI", { Slot::Reg, Slot::Rm, Slot::Imm }},
        };

        const List& expect_list( const SExp& e, const std::string_view what )
        {
            if ( const auto* list = std::get_if< List >( &e.value ) ) return *list;
            fail( e.loc, std::format( "expected {}", what ) );
        }

        const std::string& expect_symbol( const SExp& e, const std::string_view what )
        {
            if ( const auto* sym = std::get_if< Symbol >( &e.value ) ) return sym->name;
            fail( e.loc, std::format( "expected {}", what ) );
        }

        const std::string& expect_string( const SExp& e, const std::string_view what )
        {
            if ( const auto* str = std::get_if< String >( &e.value ) ) return str->value;
            fail( e.loc, std::format( "expected {}", what ) );
        }

        const OpEnForm& find_op_en( const SExp& e )
        {
            const auto& name = expect_symbol( e, "an Op/En form such as MR or MI" );
            if ( const auto it = std::ranges::find( op_en_forms, name, &OpEnForm::name ); it != op_en_forms.end( ) ) return *it;
            fail( e.loc, std::format( "unknown Op/En form '{}'", name ) );
        }

        struct TypePrefix
        {
            std::string_view text { };
            Kind             kind { };
        };

        constexpr std::array type_prefixes {
            TypePrefix { "rm", Kind::GprMem},
            TypePrefix {"imm",    Kind::Imm},
            TypePrefix {  "m",    Kind::Mem},
            TypePrefix {  "r",    Kind::Gpr},
        };

        constexpr std::array widths { 8u, 16u, 32u, 64u };

        struct ImmSize
        {
            std::string_view text { };
            uint8_t          bytes { };
        };

        constexpr std::array imm_sizes {
            ImmSize {"ib", 1},
            ImmSize {"iw", 2},
            ImmSize {"id", 4},
            ImmSize {"io", 8},
        };

        constexpr std::array reg_suffixes { std::string_view { "rb" }, std::string_view { "rw" }, std::string_view { "rd" },
                                            std::string_view { "ro" } };

        [[nodiscard]] constexpr bool has( const int8_t slot ) noexcept
        { return slot != no_slot; }

        [[nodiscard]] std::optional< uint8_t > parse_byte( const std::string_view text ) noexcept
        {
            uint8_t           value { };
            const auto* const last = text.data( ) + text.size( );
            if ( const auto [ ptr, ec ] = std::from_chars( text.data( ), last, value, 16 );
                 text.size( ) != 2 || ec != std::errc { } || ptr != last )
                return std::nullopt;
            return value;
        }

        Role parse_role( const SExp& e )
        {
            const auto& name = expect_symbol( e, "a role: r, w or rw" );
            if ( name == "r" ) return Role::Read;
            if ( name == "w" ) return Role::Write;
            if ( name == "rw" ) return Role::ReadWrite;
            fail( e.loc, std::format( "unknown role '{}', expected r, w or rw", name ) );
        }

        void parse_type( const SExp& e, Operand& op )
        {
            const std::string_view name = expect_symbol( e, "an operand type such as r64, rm32 or imm8" );
            if ( name == "cc" )
            {
                op.kind = Kind::Cond;
                op.bits = 0;
                return;
            }

            const auto prefix = std::ranges::find_if( type_prefixes,
                                                      [ & ]( const TypePrefix& p )
                                                      {
                                                          return name.starts_with( p.text );
                                                      } );
            if ( prefix == type_prefixes.end( ) ) fail( e.loc, std::format( "unknown operand type '{}'", name ) );

            const auto digits = name.substr( prefix->text.size( ) );
            if ( prefix->kind == Kind::Mem && digits.empty( ) )
            {
                op.kind = Kind::Mem;
                op.bits = 0;
                return;
            }

            const auto* const last = digits.data( ) + digits.size( );
            unsigned          bits { };
            if ( const auto [ ptr, ec ] = std::from_chars( digits.data( ), last, bits );
                 ec != std::errc { } || ptr != last || !std::ranges::contains( widths, bits ) )
                fail( e.loc, std::format( "bad width in operand type '{}', expected 8, 16, 32 or 64", name ) );

            op.kind = prefix->kind;
            op.bits = static_cast< uint8_t >( bits );
        }

        Operand parse_operand( const SExp& e )
        {
            const auto& parts = expect_list( e, "an operand such as (r src r64)" );
            Operand     op { .loc = e.loc };

            if ( parts.size( ) == 2 )
            {
                op.name = expect_symbol( parts[ 0 ], "an operand name" );
                parse_type( parts[ 1 ], op );
                if ( op.kind == Kind::Gpr || op.kind == Kind::GprMem || op.kind == Kind::Mem )
                    fail( e.loc, std::format( "register or memory operand '{}' needs a role: r, w or rw", op.name ) );
                return op;
            }

            if ( parts.size( ) != 3 && parts.size( ) != 4 )
                fail( e.loc, "expected (role name type), (role name type (fixed reg)) or (name type)" );

            op.role = parse_role( parts[ 0 ] );
            op.name = expect_symbol( parts[ 1 ], "an operand name" );
            parse_type( parts[ 2 ], op );
            if ( op.kind == Kind::Imm || op.kind == Kind::Cond )
                fail( parts[ 0 ].loc, std::format( "operand '{}' takes no role", op.name ) );

            if ( parts.size( ) == 4 )
            {
                const auto& fixed = expect_list( parts[ 3 ], "(fixed reg)" );
                if ( fixed.size( ) != 2 || expect_symbol( fixed[ 0 ], "fixed" ) != "fixed" ) fail( parts[ 3 ].loc, "expected (fixed reg)" );
                if ( op.kind != Kind::Gpr ) fail( parts[ 3 ].loc, std::format( "only r operands can be fixed, '{}' is not", op.name ) );
                op.fixed = expect_symbol( fixed[ 1 ], "a register name" );
            }

            return op;
        }

        void parse_opcode( const SExp& e, Encoding& enc )
        {
            const std::string_view text = expect_string( e, "an SDM opcode string such as \"REX.W + 01 /r\"" );

            for ( const auto part : std::views::split( text, ' ' ) )
            {
                const std::string_view tok { part.begin( ), part.end( ) };
                if ( tok.empty( ) || tok == "+" ) continue;

                if ( tok == "REX.W" )
                {
                    if ( !enc.opcode.empty( ) ) fail( e.loc, "REX.W must come before the opcode bytes" );
                    enc.rex_w = true;
                    continue;
                }

                if ( tok == "66" && enc.opcode.empty( ) )
                {
                    if ( enc.p66 || enc.rex_w ) fail( e.loc, "66 must come first, before REX.W" );
                    enc.p66 = true;
                    continue;
                }

                if ( ( tok == "F2" || tok == "F3" ) && enc.opcode.empty( ) )
                {
                    if ( enc.rep || enc.rex_w ) fail( e.loc, std::format( "{} must come before REX.W, and only once", tok ) );
                    enc.rep = tok == "F2" ? 0xF2 : 0xF3;
                    continue;
                }

                if ( tok == "/r" || ( tok.size( ) == 2 && tok[ 0 ] == '/' && tok[ 1 ] >= '0' && tok[ 1 ] <= '7' ) )
                {
                    if ( enc.opcode.empty( ) ) fail( e.loc, std::format( "'{}' must follow the opcode bytes", tok ) );
                    if ( enc.modrm_r || has( enc.digit ) ) fail( e.loc, "more than one /r or /digit" );
                    if ( tok == "/r" ) enc.modrm_r = true;
                    else
                        enc.digit = static_cast< int8_t >( tok[ 1 ] - '0' );
                    continue;
                }

                if ( const auto imm = std::ranges::find( imm_sizes, tok, &ImmSize::text ); imm != imm_sizes.end( ) )
                {
                    if ( enc.imm_bytes ) fail( e.loc, "more than one immediate" );
                    enc.imm_bytes = imm->bytes;
                    continue;
                }

                const auto in_order = [ & ]
                {
                    if ( enc.modrm_r || has( enc.digit ) || enc.imm_bytes || enc.plus_r || enc.plus_cc )
                        fail( e.loc, std::format( "opcode byte '{}' after the ModRM, immediate or +r part", tok ) );
                };

                if ( const auto plus = tok.find( '+' ); plus != std::string_view::npos )
                {
                    const auto byte   = parse_byte( tok.substr( 0, plus ) );
                    const auto suffix = tok.substr( plus + 1 );
                    if ( !byte ) fail( e.loc, std::format( "bad opcode byte in '{}'", tok ) );
                    in_order( );

                    if ( std::ranges::contains( reg_suffixes, suffix ) ) enc.plus_r = true;
                    else if ( suffix == "cc" )
                        enc.plus_cc = true;
                    else
                        fail( e.loc, std::format( "unknown suffix '+{}', expected +rb, +rw, +rd, +ro or +cc", suffix ) );

                    enc.opcode.push_back( *byte );
                    continue;
                }

                if ( const auto byte = parse_byte( tok ) )
                {
                    in_order( );
                    enc.opcode.push_back( *byte );
                    continue;
                }

                fail( e.loc, std::format( "unknown opcode token '{}'", tok ) );
            }

            if ( enc.opcode.empty( ) ) fail( e.loc, "the opcode string has no opcode bytes" );
        }

        void bind_slots( const OpEnForm& form, Inst& inst )
        {
            auto&       enc = inst.enc;
            std::size_t next { };

            for ( std::size_t i = 0; i < inst.ops.size( ); ++i )
            {
                const auto& op    = inst.ops[ i ];
                const auto  index = static_cast< int8_t >( i );
                if ( op.is_fixed( ) ) continue;

                if ( op.kind == Kind::Cond )
                {
                    if ( has( enc.cond ) ) fail( op.loc, "more than one cc operand" );
                    enc.cond = index;
                    continue;
                }

                if ( next == form.slots.size( ) || form.slots[ next ] == Slot::None )
                    fail( op.loc, std::format( "too many operands for Op/En {}", form.name ) );

                switch ( form.slots[ next++ ] )
                {
                    case Slot::Rm: enc.rm = index; break;
                    case Slot::Reg: enc.reg = index; break;
                    case Slot::OpReg: enc.opreg = index; break;
                    case Slot::Imm: enc.imm = index; break;
                    case Slot::None: std::unreachable( );
                }
            }

            if ( next < form.slots.size( ) && form.slots[ next ] != Slot::None )
            {
                const auto needed = std::ranges::count_if( form.slots,
                                                           []( const Slot s )
                                                           {
                                                               return s != Slot::None;
                                                           } );
                fail( enc.loc, std::format( "Op/En {} needs {} operands besides fixed and cc ones, got {}", form.name, needed, next ) );
            }
        }

        void check_encoding( const Inst& inst )
        {
            const auto& enc = inst.enc;
            const auto  bad = [ & ]( const std::string_view msg )
            {
                fail( enc.loc, std::format( "{}: {}", inst.name, msg ) );
            };
            const auto operand = [ & ]( const int8_t slot ) -> const Operand&
            {
                return inst.ops[ static_cast< std::size_t >( slot ) ];
            };

            if ( has( enc.reg ) != enc.modrm_r )
                bad( enc.modrm_r ? "/r needs an Op/En with a reg operand" : "the Op/En has a reg operand but the opcode has no /r" );
            if ( has( enc.rm ) && !enc.modrm_r && !has( enc.digit ) )
                bad( "the Op/En has an r/m operand but the opcode has no /r or /digit" );
            if ( !has( enc.rm ) && has( enc.digit ) ) bad( "/digit needs an Op/En with an r/m operand" );
            if ( has( enc.opreg ) != enc.plus_r )
                bad( enc.plus_r ? "+r needs an Op/En with an opcode register (O or OI)"
                                : "the Op/En adds a register to the opcode but the opcode has no +rd" );
            if ( has( enc.cond ) != enc.plus_cc ) bad( enc.plus_cc ? "+cc needs a cc operand" : "a cc operand needs +cc in the opcode" );
            if ( has( enc.imm ) != ( enc.imm_bytes != 0 ) )
                bad( enc.imm_bytes ? "the opcode has an immediate but the Op/En has no immediate operand"
                                   : "the Op/En has an immediate operand but the opcode has no ib, iw, id or io" );

            if ( has( enc.reg ) && operand( enc.reg ).kind != Kind::Gpr ) bad( "the reg operand must be an r type" );
            if ( has( enc.opreg ) && operand( enc.opreg ).kind != Kind::Gpr ) bad( "the opcode register operand must be an r type" );
            if ( has( enc.rm ) && operand( enc.rm ).kind != Kind::Gpr && operand( enc.rm ).kind != Kind::GprMem
                 && operand( enc.rm ).kind != Kind::Mem )
                bad( "the r/m operand must be an r or rm type" );
            if ( has( enc.imm ) && operand( enc.imm ).kind != Kind::Imm ) bad( "the immediate operand must be an imm type" );

            if ( has( enc.imm ) && operand( enc.imm ).bits / 8 != enc.imm_bytes )
                bad( std::format( "operand '{}' is imm{} but the opcode immediate is {} byte(s)", operand( enc.imm ).name,
                                  operand( enc.imm ).bits, enc.imm_bytes ) );
        }

        void parse_asm( const SExp& clause, Inst& inst )
        {
            const auto& parts = std::get< List >( clause.value );
            if ( parts.size( ) != 2 ) fail( clause.loc, "expected (asm \"mnemonic\")" );
            inst.mnemonic = expect_string( parts[ 1 ], "a mnemonic string" );
        }

        void parse_ops( const SExp& clause, Inst& inst )
        {
            for ( const auto& part : std::get< List >( clause.value ) | std::views::drop( 1 ) )
            {
                auto op = parse_operand( part );
                if ( std::ranges::contains( inst.ops, op.name, &Operand::name ) )
                    fail( part.loc, std::format( "duplicate operand '{}'", op.name ) );
                inst.ops.push_back( std::move( op ) );
            }
        }

        const OpEnForm& parse_enc( const SExp& clause, Inst& inst )
        {
            const auto& parts = std::get< List >( clause.value );
            if ( parts.size( ) != 3 ) fail( clause.loc, "expected (enc FORM \"opcode\")" );

            inst.enc.loc     = clause.loc;
            const auto& form = find_op_en( parts[ 1 ] );
            parse_opcode( parts[ 2 ], inst.enc );
            return form;
        }

        Inst build_inst( const SExp& form )
        {
            const auto& items = expect_list( form, "an inst form" );
            if ( items.size( ) < 2 ) fail( form.loc, "inst needs a name" );

            Inst inst { .name = expect_symbol( items[ 1 ], "an instruction name" ), .loc = form.loc };

            const OpEnForm* op_en { };
            bool            seen_asm { }, seen_ops { }, seen_enc { };
            for ( const auto& clause : items | std::views::drop( 2 ) )
            {
                const auto& parts = expect_list( clause, "a clause such as (asm ...)" );
                if ( parts.empty( ) ) fail( clause.loc, "empty clause" );

                const auto& head = expect_symbol( parts.front( ), "a clause name" );
                const auto  once = [ & ]( bool& seen )
                {
                    if ( seen ) fail( clause.loc, std::format( "duplicate ({} ...) clause", head ) );
                    seen = true;
                };

                if ( head == "asm" )
                {
                    once( seen_asm );
                    parse_asm( clause, inst );
                }
                else if ( head == "ops" )
                {
                    once( seen_ops );
                    parse_ops( clause, inst );
                }
                else if ( head == "enc" )
                {
                    once( seen_enc );
                    op_en = &parse_enc( clause, inst );
                }
                else
                    fail( parts.front( ).loc, std::format( "unknown clause '{}'", head ) );
            }

            if ( !seen_asm ) fail( form.loc, std::format( "{} has no (asm ...) clause", inst.name ) );
            if ( !seen_ops ) fail( form.loc, std::format( "{} has no (ops ...) clause", inst.name ) );
            if ( !seen_enc ) fail( form.loc, std::format( "{} has no (enc ...) clause", inst.name ) );

            bind_slots( *op_en, inst );
            check_encoding( inst );
            return inst;
        }
    } // namespace

    Isa build( const List& forms )
    {
        Isa                               isa;
        std::unordered_set< std::string > names;

        const auto claim = [ & ]( const std::string& name, const Location& loc )
        {
            if ( !names.insert( name ).second ) fail( loc, std::format( "duplicate instruction '{}'", name ) );
        };

        for ( const auto& form : forms )
        {
            const auto& items = expect_list( form, "a top-level form such as (inst ...)" );
            if ( items.empty( ) ) fail( form.loc, "empty top-level form" );

            const auto& head = expect_symbol( items.front( ), "a form name" );
            if ( head == "inst" )
            {
                const auto& inst = isa.insts.emplace_back( build_inst( form ) );
                claim( inst.name, form.loc );
            }
            else if ( head == "pseudo" )
            {
                if ( items.size( ) < 2 ) fail( form.loc, "pseudo needs at least one hand-written instruction name" );
                for ( const auto& item : items | std::views::drop( 1 ) )
                {
                    const auto& pseudo
                        = isa.pseudos.emplace_back( Pseudo { expect_symbol( item, "a pseudo instruction name" ), item.loc } );
                    claim( pseudo.name, item.loc );
                }
            }
            else if ( head == "header" )
            {
                if ( items.size( ) != 2 ) fail( form.loc, "expected (header \"path/to/header.hh\")" );
                isa.headers.push_back( expect_string( items[ 1 ], "a header path string" ) );
            }
            else
                fail( items.front( ).loc, std::format( "unknown top-level form '{}'", head ) );
        }

        return isa;
    }
} // namespace isagen
