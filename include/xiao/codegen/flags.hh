#pragma once

#include <cstdint>

namespace xiao::codegen
{
    enum class ProbeStackStrat : uint8_t
    {
        Inline
    };

    struct Flags
    {
        bool            enable_verifier { true };
        bool            enable_probe_stack { true };
        uint8_t         probe_stack_size_log2 { 12 };
        ProbeStackStrat probe_stack_strat { ProbeStackStrat::Inline };
        bool            has_popcnt { false };
        bool            has_lzcnt { false };
        bool            has_bmi1 { false };
    };
} // namespace xiao::codegen