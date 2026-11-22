#pragma once

#include <xiao/codegen/reloc.hh>
#include <xiao/ir/inst.hh>
#include <cstdint>
#include <vector>

namespace xiao::codegen
{
    struct TrapSite
    {
        uint32_t     offset { };
        ir::TrapCode code { };
    };

    //! @brief The output of the compiling function
    struct CompiledCode
    {
        std::vector< uint8_t >  code { };
        std::vector< Reloc >    relocs { };
        std::vector< TrapSite > traps { };
        uint32_t                alignment { 16 };
    };
} // namespace xiao::codegen