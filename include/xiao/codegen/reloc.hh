#pragma once

#include <xiao/ir/extname.hh>
#include <cstdint>

namespace xiao::codegen
{
    enum class RelocKind : uint8_t
    {
        X86CallPCRel14, //! call rel32
        Abs8            //! mov r64, imm64
    };

    struct Reloc
    {
        uint32_t         offset { };
        RelocKind        kind { };
        ir::ExternalName target { };
        int64_t          addend { };
    };
} // namespace xiao::codegen