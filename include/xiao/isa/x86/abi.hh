#pragma once

#include <xiao/codegen/abi.hh>
#include <xiao/isa/x86/reg.hh>
#include <utility>

namespace xiao::x86_64 {

    using namespace codegen;

    struct X86ABISpec final : ABISpec {
        ABISig compute(
            CallConv cc, 
            std::span<const ir::Type> params, 
            std::span<const ir::Type> rets
        ) const override {
            switch (cc) {
                case CallConv::SystemV:         return compute_sysv(params, rets);
                case CallConv::WindowsFastcall: return compute_fastcall(params, rets);
            }

            std::unreachable();
        }

    private:
        static constexpr PReg k_sysv_int_args[]   = { RDI, RSI, RDX, RCX, R8, R9 };
        static constexpr PReg k_sysv_float_args[]  = { XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7 };
        static constexpr PReg k_sysv_int_rets[]   = { RAX, RDX };
        static constexpr PReg k_sysv_float_rets[] = { XMM0, XMM1 };
        static constexpr PReg k_win64_int_args[]   = { RCX, RDX, R8, R9 };
        static constexpr PReg k_win64_float_args[] = { XMM0, XMM1, XMM2, XMM3 };
        static constexpr PReg k_win64_int_rets[]   = { RAX };
        static constexpr PReg k_win64_float_rets[] = { XMM0 };

        [[nodiscard]] static ABISig compute_sysv(
            std::span<const ir::Type> params,
            std::span<const ir::Type> rets
        ) {
            ABISig sig;
            sig.stack_align = 16;

            uint32_t next_int   = 0;
            uint32_t next_float = 0;
            int32_t  stack_off  = 0;

            for (const auto ty : params) {
                if (is_float(ty)) {
                    if (next_float < std::size(k_sysv_float_args)) {
                        sig.params.push_back({ k_sysv_float_args[next_float++], ty });
                    } else {
                        sig.params.push_back({ stack_off, ty });
                        stack_off += 8;
                    }
                } else {
                    if (next_int < std::size(k_sysv_int_args)) {
                        sig.params.push_back({ k_sysv_int_args[next_int++], ty });
                    } else {
                        sig.params.push_back({ stack_off, ty });
                        stack_off += 8;
                    }
                }
            }

            sig.stack_arg_bytes = static_cast<uint32_t>(stack_off);

            for (const auto ty : rets) {
                if (is_float(ty)) {
                    sig.rets.push_back({ k_sysv_float_rets[0], ty });
                } else {
                    sig.rets.push_back({ k_sysv_int_rets[0], ty });
                }
            }

            return sig;
        }

        [[nodiscard]] static ABISig compute_fastcall(
            std::span<const ir::Type> params,
            std::span<const ir::Type> rets
        ) {
            ABISig sig;
            sig.stack_align = 16;

            int32_t stack_off = 32;
            for (uint32_t i = 0; i < params.size(); ++i) {
                const auto ty = params[i];
                if (i < 4) {
                    const auto preg = is_float(ty) ? k_win64_float_args[i]
                                                   : k_win64_int_args[i];
                    sig.params.push_back({ preg, ty });
                } else {
                    sig.params.push_back({ stack_off, ty });
                    stack_off += 8;
                }
            }

            sig.stack_arg_bytes = static_cast<uint32_t>(stack_off);

            for (const auto ty : rets) {
                if (is_float(ty)) {
                    sig.rets.push_back({ k_win64_float_rets[0], ty });
                } else {
                    sig.rets.push_back({ k_win64_int_rets[0], ty });
                }
            }

            return sig;
        }

        [[nodiscard]] static constexpr bool is_float(const ir::Type ty) noexcept {
            return ty == ir::Type::f64;
        }
    };

    [[nodiscard]] inline MachineEnv host_machine_env() noexcept
    {
#if defined(_WIN64)
        return win64_env();
#else
        return sysv_env();
#endif
    }

    [[nodiscard]] inline const X86ABISpec& host_abi() noexcept {
        static constexpr X86ABISpec k_spec;
        return k_spec;
    }

    [[nodiscard]] inline const ABISpec& host_abi_spec() noexcept
    {
        return host_abi();
    }

}