#pragma once

#include <xiao/codegen/isa.hh>
#include <xiao/codegen/compiled.hh>
#include <xiao/codegen/flags.hh>
#include <xiao/ir/function.hh>
#include <xiao/isa/x86/abi.hh>
#include <xiao/isa/x86/emit.hh>
#include <xiao/isa/x86/frame.hh>
#include <xiao/isa/x86/lower.hh>
#include <xiao/isa/x86/inst/regs.hh>
#include <xiao/regalloc/regalloc.hh>
#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <expected>
#include <string_view>

namespace xiao::x86_64
{
    inline constexpr uint64_t k_max_stack_slot_bytes = uint64_t { 1 } << 30;

    [[nodiscard]] inline std::optional< std::string > exceeded_limit( const ir::IRFunction& fn )
    {
        uint64_t slots { };
        for ( const auto& s : fn.stack_slots )
        {
            const uint64_t align = uint64_t { 1 } << std::min< uint8_t >( s.align_shift, 32 );
            slots                = ( ( slots + align - 1 ) & ~( align - 1 ) ) + s.size;
        }
        if ( slots > k_max_stack_slot_bytes )
            return std::format( "stack slots need {} bytes, over the limit of {}", slots, k_max_stack_slot_bytes );

        const uint64_t vregs = uint64_t { Reg::pinned_count } + fn.values.size( ) + fn.operands.size( ) + fn.insts.size( );
        if ( vregs > VReg::max_index ) return std::format( "function may need {} virtual registers, over the limit of {}", vregs, VReg::max_index );

        return std::nullopt;
    }

    [[nodiscard]] inline bool uses_float( const ir::IRFunction& fn ) noexcept
    {
        const auto is_fl = []( const ir::Type ty )
        {
            return ty == ir::Type::f32 || ty == ir::Type::f64;
        };

        return is_fl( fn.sig.ret )
            || std::ranges::any_of( fn.values,
                                    [ & ]( const ir::Value& value )
                                    {
                                        return is_fl( value.type );
                                    } );
    }

    struct X86Isa final : TargetIsa
    {
        X86Isa( ) = default;

        explicit X86Isa( const Flags& fgs ) noexcept : m_flags { fgs } { }

        [[nodiscard]] std::string_view name( ) const noexcept override
        { return "x86_64"; }

        [[nodiscard]] const Flags& flags( ) const noexcept override
        { return m_flags; }

        [[nodiscard]] CallConv default_cc( ) const noexcept override
        { return host_call_conv( ); }

        std::expected< CompiledCode, CodegenError > compile_function( ir::IRFunction& fn ) const override
        {
            if ( uses_float( fn ) )
                return std::unexpected( CodegenError { .kind     = CodegenError::Kind::Unsupported,
                                                       .messages = { "floating point types are not supported yet" } } );

            if ( auto limit = exceeded_limit( fn ) )
                return std::unexpected( CodegenError { .kind = CodegenError::Kind::ImplLimitExceeded, .messages = { std::move( *limit ) } } );

            const auto             env = env_for( fn.sig.cc );
            IRLowerer              lowerer { env, host_abi( ) };
            auto                   vcode  = lowerer.lower( fn );
            const auto             allocs = regalloc::allocate( vcode, env );
            const auto             frame  = compute_frame( vcode, allocs, env, fn.stack_slots, flags( ) );
            MachBuffer< LabelUse > buf;
            emit( vcode, allocs, frame, buf );
            return std::move( buf ).finish( );
        }

    private:
        Flags m_flags { };
    };
} // namespace xiao::x86_64