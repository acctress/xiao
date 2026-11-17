#pragma once

#include "sexp.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace isagen
{
    enum class Role : uint8_t
    {
        Read,
        Write,
        ReadWrite
    };

    enum class Kind : uint8_t
    {
        Gpr,
        GprMem,
        Mem,
        Imm,
        Cond
    };

    struct Operand
    {
        std::string name { };
        Role        role { };
        Kind        kind { };
        uint8_t     bits { };
        std::string fixed { };
        Location    loc { };

        [[nodiscard]] bool is_fixed( ) const noexcept { return !fixed.empty( ); }
    };

    inline constexpr int8_t no_slot = -1;

    struct Encoding
    {
        bool                   p66 { };
        uint8_t                rep { };
        bool                   rex_w { };
        std::vector< uint8_t > opcode { };
        bool                   plus_r { };
        bool                   plus_cc { };
        bool                   modrm_r { };
        int8_t                 digit { no_slot };
        uint8_t                imm_bytes { };

        int8_t rm { no_slot };
        int8_t reg { no_slot };
        int8_t opreg { no_slot };
        int8_t imm { no_slot };
        int8_t cond { no_slot };

        Location loc { };
    };

    struct Inst
    {
        std::string            name { };
        std::string            mnemonic { };
        std::vector< Operand > ops { };
        Encoding               enc { };
        Location               loc { };
    };

    struct Pseudo
    {
        std::string name { };
        Location    loc { };
    };

    struct Isa
    {
        std::vector< Inst >        insts { };
        std::vector< Pseudo >      pseudos { };
        std::vector< std::string > headers { };
    };
} // namespace isagen
