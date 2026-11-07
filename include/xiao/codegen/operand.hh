//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <optional>
#include <xiao/codegen/reg.hh>

namespace xiao::codegen {
    //! @brief The kind of operand an instruction will be using
    enum class OperandKind : uint8_t {
        Use     = 0,
        Def     = 1,
        UseDef  = 2,
    };

    //! @brief Whether the operand is live before or after the instruction
    enum class OperandPos : uint8_t {
        Before = 0,
        After  = 1,
    };

    //! @brief A packed register operand for a MachInst.
    //! Layout:
    //!   bits 63:43 - vreg index
    //!   bit  42    - vreg class
    //!   bits 41:40 - OperandKind
    //!   bit  39    - OperandPos
    //!   bit  38    - constraint kind
    //!   bits 37:8  - reserved
    //!   bits 7:0   - PReg bits if Fixed, else 0
    struct MachOperand {
        uint64_t bits {};

        //! @brief Construct an any constrained operand.
        [[nodiscard]] static constexpr MachOperand any(
            const VReg        vreg,
            const OperandKind kind,
            const OperandPos  pos
        ) noexcept {
            MachOperand op;
            op.bits = (static_cast<uint64_t>(vreg.index())                  << vreg_index_shift)
                    | (static_cast<uint64_t>(vreg.rc())                      << vreg_class_shift)
                    | (static_cast<uint64_t>(kind)                           << kind_shift)
                    | (static_cast<uint64_t>(pos)                            << pos_shift);
            return op;
        }

        //! @brief Construct a fixed constrained operand
        [[nodiscard]] static constexpr MachOperand fixed(
            const VReg        vreg,
            const OperandKind kind,
            const OperandPos  pos,
            const PReg        preg
        ) noexcept {
            assert(preg.valid());
            MachOperand op = any(vreg, kind, pos);
            op.bits |= (1ull                                << fixed_flag_shift)
                     | (static_cast<uint64_t>(preg.bits)   << preg_shift);
            return op;
        }

        [[nodiscard]] constexpr VReg vreg() const noexcept {
            VReg r;
            r.bits = static_cast<uint32_t>(
                ((bits & vreg_index_mask) >> vreg_index_shift) << 1 |
                ((bits & vreg_class_mask) >> vreg_class_shift)
            );
            return r;
        }

        [[nodiscard]] constexpr OperandKind kind() const noexcept {
            return static_cast<OperandKind>((bits & kind_mask) >> kind_shift);
        }

        [[nodiscard]] constexpr OperandPos pos() const noexcept {
            return static_cast<OperandPos>((bits & pos_mask) >> pos_shift);
        }

        [[nodiscard]] constexpr bool is_fixed() const noexcept {
            return (bits & fixed_flag_mask) != 0;
        }

        //! @brief Returns the fixed PReg if this is a fixed constrained operand.
        [[nodiscard]] constexpr std::optional< PReg > fixed_preg() const noexcept {
            if (!is_fixed()) return std::nullopt;
            PReg r;
            r.bits = static_cast<uint8_t>((bits & preg_mask) >> preg_shift);
            return r;
        }

        [[nodiscard]] constexpr bool operator==(const MachOperand &) const noexcept = default;
    private:
        static constexpr int vreg_index_shift = 43;
        static constexpr int vreg_class_shift = 42;
        static constexpr int kind_shift        = 40;
        static constexpr int pos_shift         = 39;
        static constexpr int fixed_flag_shift  = 38;
        static constexpr int preg_shift        = 0;
        static constexpr uint64_t vreg_index_mask = ((1ull << 21) - 1) << vreg_index_shift;
        static constexpr uint64_t vreg_class_mask = 1ull               << vreg_class_shift;
        static constexpr uint64_t kind_mask        = 0b11ull            << kind_shift;
        static constexpr uint64_t pos_mask         = 1ull               << pos_shift;
        static constexpr uint64_t fixed_flag_mask  = 1ull               << fixed_flag_shift;
        static constexpr uint64_t preg_mask        = 0xFFull            << preg_shift;
    };

    static_assert(sizeof(MachOperand) == 8, "MachOperand must be 8 bytes!!!");
}

template <>
struct std::formatter<xiao::codegen::MachOperand> {
    constexpr auto parse(auto& ctx) { return ctx.begin(); }
    auto format(const xiao::codegen::MachOperand& op, auto& ctx) const {
        const char* kind_str = op.kind() == xiao::codegen::OperandKind::Use    ? "use"
                             : op.kind() == xiao::codegen::OperandKind::Def    ? "def"
                             :                                                    "usedef";
        const char* pos_str  = op.pos() == xiao::codegen::OperandPos::Before   ? "before"
                                                                                : "after";
        if (op.is_fixed())
            return std::format_to(ctx.out(), "{}({}, fixed={}, {})",
                kind_str, op.vreg(), *op.fixed_preg(), pos_str);
        return std::format_to(ctx.out(), "{}({}, {})", kind_str, op.vreg(), pos_str);
    }
};