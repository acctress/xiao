//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <unordered_map>
#include <xiao/support/arena.hh>
#include <xiao/ir/function.hh>

namespace xiao::ir
{
    //! @brief A module which owns the arena and interned strings.
    class IRModule
    {
    public:
        explicit IRModule( const std::size_t arena_size = support::Arena::default_slab_size ) : m_arena { arena_size } { }

        IRModule( IRModule&& ) noexcept             = default;
        IRModule& operator= ( IRModule&& ) noexcept = default;

        IRModule( const IRModule& )             = delete;
        IRModule& operator= ( const IRModule& ) = delete;

        [[nodiscard]] FunctionRef declare( const std::string_view name, const Type ret, const std::span< const Type > params = { },
                                           const codegen::CallConv cc = codegen::host_call_conv( ) )
        {
            auto*      fn        = m_arena.alloc< IRFunction >( );
            const auto id        = static_cast< uint32_t >( m_functions.size( ) );
            fn->arena            = &m_arena;
            fn->self             = FunctionRef { id };
            fn->name             = intern( name ).first;
            fn->sig.ret          = ret;
            fn->sig.params_count = static_cast< uint16_t >( params.size( ) );
            fn->sig.cc           = cc;
            m_functions.push_back( fn );
            return FunctionRef { id };
        }

        [[nodiscard]] FunctionRef declare( const std::string_view name, const Type ret,
                                           const std::initializer_list< const Type > params = { },
                                           const codegen::CallConv                   cc     = codegen::host_call_conv( ) )
        { return declare( name, ret, std::span { params.begin( ), params.size( ) }, cc ); }

        //! @brief Intern a string; returns a StringRef + view pair.
        [[nodiscard]] std::pair< StringRef, std::string_view > intern( const std::string_view sv )
        {
            if ( const auto iter = m_lookup.find( sv ); iter != m_lookup.end( ) )
                return { StringRef { iter->second }, m_strings[ iter->second ] };

            auto       view = m_arena.alloc_str( sv );
            const auto id   = static_cast< uint32_t >( m_strings.size( ) );
            m_strings.push_back( view );
            m_lookup.emplace( view, id );
            return { StringRef { id }, view };
        }

        //! @brief Resolve a StringRef to its string_view.
        [[nodiscard]] std::string_view string( StringRef ref ) const noexcept
        { return m_strings[ ref.id ]; }

        [[nodiscard]] IRFunction* get( const FunctionRef ref ) noexcept
        { return m_functions[ ref.id ]; }

        [[nodiscard]] const IRFunction* get( const FunctionRef ref ) const noexcept
        { return m_functions[ ref.id ]; }

        [[nodiscard]] std::span< IRFunction* const > functions( ) const noexcept
        { return m_functions; }

        [[nodiscard]] support::Arena& arena( ) noexcept
        { return m_arena; }

        template < typename O >
        void fmt( O out ) const
        {
            for ( const auto* fn : m_functions )
            {
                std::format_to( out, "fn {}(", string( fn->name ) );
                for ( auto i { 0u }; i < fn->sig.params_count; ++i )
                {
                    if ( i ) std::format_to( out, ", " );
                    std::format_to( out, "v{}: {}", i, fn->values[ i ].type );
                }

                std::format_to( out, ") -> {} {{\n", fn->sig.ret );

                for ( auto bi { 0uz }; bi < fn->blocks.size( ); ++bi )
                {
                    const auto& b { fn->blocks[ bi ] };

                    std::format_to( out, "b{}(", bi );
                    for ( auto pi { 0u }; pi < b.params_count; ++pi )
                    {
                        if ( pi ) std::format_to( out, ", " );
                        const auto [ id ] = fn->block_params[ b.params_offset + pi ];
                        std::format_to( out, "v{}: {}", id, fn->values[ id ].type );
                    }
                    std::format_to( out, "):\n" );

                    for ( auto ii { 0u }; ii < b.insts_count; ++ii )
                    {
                        const auto& [ op, cc, result, ops_offset, ops_count, imm ] { fn->insts[ b.insts_offset + ii ] };
                        std::format_to( out, "    " );

                        switch ( op )
                        {
                            case IROp::iconst:
                                std::format_to( out, "v{} = iconst {} #{}\n", result.id, fn->values[ result.id ].type, imm );
                                break;
                            case IROp::iadd:
                            case IROp::isub:
                            case IROp::imul:
                            case IROp::idiv:
                                std::format_to( out, "v{} = {} v{}, v{}\n", result.id, op, fn->operands[ ops_offset ].id,
                                                fn->operands[ ops_offset + 1 ].id );
                                break;
                            case IROp::icmp:
                                std::format_to( out, "v{} = icmp {} v{}, v{}\n", result.id, cc, fn->operands[ ops_offset ].id,
                                                fn->operands[ ops_offset + 1 ].id );
                                break;
                            case IROp::br:
                            {
                                const auto target = fn->succs[ b.succs_offset ].id;
                                std::format_to( out, "br b{}\n", target );
                                break;
                            }
                            case IROp::brif:
                            {
                                const auto cond       = fn->operands[ ops_offset ].id;
                                const auto then_block = fn->succs[ b.succs_offset ].id;
                                const auto else_block = fn->succs[ b.succs_offset + 1 ].id;
                                std::format_to( out, "brif v{}, b{}, b{}\n", cond, then_block, else_block );
                                break;
                            }
                            case IROp::ret:
                                if ( ops_count > 0 ) std::format_to( out, "ret v{}\n", fn->operands[ ops_offset ].id );
                                else
                                    std::format_to( out, "ret\n" );
                                break;
                            case IROp::call:
                                std::format_to( out, "v{} = call .func{}", result.id, imm );
                                for ( uint16_t oi = 0; oi < ops_count; ++oi )
                                    std::format_to( out, " v{}", fn->operands[ ops_offset + oi ].id );
                                std::format_to( out, "\n" );
                                break;
                        }
                    }
                }

                std::format_to( out, "}}\n" );
            }
        }

    private:
        support::Arena                                   m_arena;
        std::vector< IRFunction* >                       m_functions;
        std::vector< std::string_view >                  m_strings;
        std::unordered_map< std::string_view, uint32_t > m_lookup;
    };
} // namespace xiao::ir
