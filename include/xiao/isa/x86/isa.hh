#pragma once

#include <xiao/codegen/isa.hh>
#include <xiao/codegen/compiled.hh>
#include <xiao/ir/function.hh>
#include <xiao/isa/x86/abi.hh>
#include <xiao/isa/x86/emit.hh>
#include <xiao/isa/x86/frame.hh>
#include <xiao/isa/x86/lower.hh>
#include <xiao/isa/x86/inst/regs.hh>
#include <xiao/regalloc/regalloc.hh>
#include <expected>
#include <string_view>

namespace xiao::x86_64
{
    struct X86Isa final : TargetIsa
    {
        [[nodiscard]] std::string_view name( ) const noexcept override
        { return "x86_64"; }

        [[nodiscard]] CallConv default_cc( ) const noexcept override
        { return host_call_conv( ); }

        std::expected< CompiledCode, CodegenError > compile_function( ir::IRFunction& fn ) const override
        {
            const auto             env = env_for( fn.sig.cc );
            IRLowerer              lowerer { env, host_abi( ) };
            auto                   vcode  = lowerer.lower( fn );
            const auto             allocs = regalloc::allocate( vcode, env );
            const auto             frame  = compute_frame( vcode, allocs, env, fn.stack_slots );
            MachBuffer< LabelUse > buf;
            emit( vcode, allocs, frame, buf );
            return std::move( buf ).finish( );
        }
    };
} // namespace xiao::x86_64