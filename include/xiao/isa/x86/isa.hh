#pragma once

#include <xiao/codegen/isa.hh>
#include <xiao/isa/x86/lower.hh>
#include <xiao/isa/x86/abi.hh>
#include <xiao/isa/x86/emit.hh>
#include <xiao/regalloc/regalloc.hh>
#include <memory>
#include <print>

namespace xiao::x86_64
{
    struct X86Target final
    {
        explicit X86Target( const MachineEnv& env, const ABISpec& abi ) : m_env { env }, m_abi { abi } { }

        void compile( ir::IRFunction& fn, CallConv cc )
        {
            IRLowerer lowerer { m_env, m_abi };

            const auto vcode  = lowerer.lower( fn, cc );
            const auto allocs = regalloc::allocate( vcode, m_env );

            for (uint32_t bi = 0; bi < vcode.blocks.size(); ++bi)
            {
                std::println("block{}: insts {}..{}", bi,
                    vcode.blocks[bi].insts_offset,
                    vcode.blocks[bi].insts_offset + vcode.blocks[bi].insts_count);
                for (uint32_t ii = 0; ii < vcode.blocks[bi].insts_count; ++ii)
                    std::println("  {}", vcode.insts[vcode.blocks[bi].insts_offset + ii].to_string());
            }

            m_buf = MachBuffer< LabelUse > { };
            emit( vcode, allocs, m_buf );
        }

        [[nodiscard]] std::span< const uint8_t > bytes( ) const noexcept
        { return m_buf.data( ); }

    private:
        MachineEnv             m_env;
        const ABISpec&         m_abi;
        MachBuffer< LabelUse > m_buf;
    };

    [[nodiscard]] inline std::unique_ptr< X86Target > make_x86_target( )
    { return std::make_unique< X86Target >( host_machine_env( ), host_abi( ) ); }
} // namespace xiao::x86_64