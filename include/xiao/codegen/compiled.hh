#pragma once

#include <xiao/codegen/reloc.hh>
#include <cstdint>
#include <vector>

namespace xiao::codegen
{
    //! @brief The output of the compiling function
    struct CompiledCode
    {
        std::vector< uint8_t > code { };
        std::vector< Reloc >   relocs { };
        uint32_t               alignment { 16 };
    };
} // namespace xiao::codegen