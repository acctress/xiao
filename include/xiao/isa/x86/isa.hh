#pragma once

#include "abi.hh"
#include "emit.hh"
#include "lower.hh"
#include "machenv.hh"
#include "xiao/regalloc/regalloc.hh"

#include <xiao/codegen/isa.hh>
#include <xiao/codegen/compiled.hh>
#include <xiao/ir/function.hh>
#include <expected>
#include <string_view>

namespace xiao::x86_64
{
    struct X86Isa final : codegen::TargetIsa
    {
        [[nodiscard]] std::string_view name( ) const noexcept override
        { return "x86_64"; }

        [[nodiscard]] codegen::CallConv default_cc( ) const noexcept override
        { return codegen::host_call_conv( ); }

        std::expected< codegen::CompiledCode, codegen::CodegenError > compile_function( ir::IRFunction& fn ) const override
        {
            const auto             env = env_for( fn.sig.cc );
            IRLowerer              lowerer { env, host_abi( ) };
            auto                   vcode  = lowerer.lower( fn );
            const auto             allocs = regalloc::allocate( vcode, env );
            const auto             frame  = compute_frame( vcode, allocs, env, fn.sig.cc );
            MachBuffer< LabelUse > buf;
            emit( vcode, allocs, frame, buf );
            return std::move( buf ).finish( );
        }
    };
} // namespace xiao::x86_64