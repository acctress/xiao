#pragma once

#include <xiao/codegen/machbuffer.hh>
#include <xiao/codegen/machinst.hh>
#include <xiao/codegen/operand.hh>
#include <xiao/codegen/reg.hh>
#include <xiao/isa/x86/gen/insts.hh>
#include <xiao/ir/types.hh>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace xiao::x86_64
{
    struct X86Inst : MachInst< X86Inst, LabelUse >
    {
        inst::MInst inner;

        template < inst::member I >
        X86Inst( I&& i ) : inner { std::forward< I >( i ) }
        {
        }

        void visit( this auto& self, auto& v )
        {
            self.inner.dispatch(
                [ & ]( auto& i )
                {
                    i.visit( v );
                } );
        }

        [[nodiscard]] MachTerminator is_term( ) const noexcept
        {
            if ( inner.is< inst::ret >( ) ) return MachTerminator::Ret;
            if ( inner.is< inst::Jmp >( ) || inner.is< inst::Jcc >( ) ) return MachTerminator::Branch;
            return MachTerminator::None;
        }

        [[nodiscard]] bool is_call( ) const noexcept
        { return inner.is< inst::Call >( ) || inner.is< inst::CallInd >( ); }

        [[nodiscard]] std::optional< std::pair< Reg, Reg > > as_move( ) const noexcept
        {
            if ( const auto* mov = inner.get_if< inst::movq_mr >( ) )
                if ( const auto* dst = std::get_if< Reg >( &mov->dst.value ) ) return std::pair { *dst, mov->src };
            return std::nullopt;
        }

        [[nodiscard]] std::span< const PReg > clobbers( ) const noexcept
        {
            if ( const auto* call = inner.get_if< inst::Call >( ) ) return call->info->clobbers;
            if ( const auto* call = inner.get_if< inst::CallInd >( ) ) return call->info->clobbers;
            return { };
        }

        [[nodiscard]] bool is_mem_access( ) const noexcept
        {
            return inner.dispatch(
                []( const auto& i ) -> bool
                {
                    if constexpr ( requires { i.mem_access( ); } ) return i.mem_access( );
                    else
                        return false;
                } );
        }

        [[nodiscard]] static X86Inst gen_move( Reg dst, Reg src, ir::Type ty ) = delete;

        [[nodiscard]] static X86Inst gen_dummy_use( Reg reg ) = delete;

        [[nodiscard]] static X86Inst gen_jump( uint32_t target ) = delete;

        [[nodiscard]] static X86Inst gen_nop( std::size_t preferred_size ) = delete;

        [[nodiscard]] static constexpr uint32_t worst_case_size( ) = delete;

        [[nodiscard]] static constexpr uint32_t worst_case_island_growth( ) = delete;

        void emit( MachBuffer< LabelUse >& buf, const std::span< const int32_t > slot_offsets = { } ) const
        {
            inner.dispatch(
                [ & ]( const auto& i )
                {
                    if constexpr ( requires { i.emit( buf, slot_offsets ); } ) i.emit( buf, slot_offsets );
                    else
                        i.emit( buf );
                } );
        }

        [[nodiscard]] std::string to_string( ) const
        {
            return inner.dispatch(
                []( const auto& i ) -> std::string
                {
                    return i.to_string( );
                } );
        }
    };
} // namespace xiao::x86_64
