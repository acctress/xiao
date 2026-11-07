//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <cassert>
#include <cstdint>
#include <format>

namespace xiao::codegen {
    //! @brief Register class
    enum class RegClass : uint8_t {
        Int   = 0,
        Float = 1,
    };

    //! @brief A physical register
    //! Hardware encoding and register class packed into a single byte (6 bits, 1 bit).
    //! Layout:
    //!   bit  0   - RegClass
    //!   bits 6:1 - hardware encoding
    //!   bit  7   - reserved
    struct PReg {
        static constexpr uint8_t max_hw_enc   = 63;
        static constexpr uint8_t invalid_bits = 0xFF;

        uint8_t bits{ invalid_bits };

        [[nodiscard]] static constexpr PReg make(const uint8_t hw, const RegClass rc) noexcept {
            assert(hw <= max_hw_enc);
            PReg reg;
            reg.bits = static_cast<uint8_t>((hw << 1) | static_cast<uint8_t>(rc));
            return reg;
        }

        [[nodiscard]] static constexpr PReg invalid() noexcept { return PReg{}; }

        //! @brief Hardware encoding index within the register class
        [[nodiscard]] constexpr uint8_t hw() const noexcept { return bits >> 1; }

        //! @brief Register class of this PReg
        [[nodiscard]] constexpr RegClass rc() const noexcept {
            return static_cast<RegClass>(bits & 1);
        }

        //! @brief Global index across both classes
        //! @note Int:   0..63
        //! @note Float: 64..127
        [[nodiscard]] constexpr uint8_t index() const noexcept {
            return static_cast<uint8_t>(hw() | (static_cast<uint8_t>(rc()) << 6));
        }

        [[nodiscard]] constexpr bool valid()                    const noexcept { return bits != invalid_bits; }
        [[nodiscard]] constexpr bool operator==(const PReg &)   const noexcept = default;
    };

    //! @brief A virtual register
    //! Layout:
    //!   bit  0    - RegClass
    //!   bits 31:1 - vreg index
    struct VReg {
        static constexpr uint32_t max_index    = (1u << 21) - 1;
        static constexpr uint32_t invalid_bits = ~0u;

        uint32_t bits{ invalid_bits };

        [[nodiscard]] static constexpr VReg make(const uint32_t index, const RegClass rc) noexcept {
            assert(index <= max_index);
            VReg reg;
            reg.bits = (index << 1) | static_cast<uint32_t>(rc);
            return reg;
        }

        [[nodiscard]] static constexpr VReg invalid() noexcept { return VReg{}; }

        [[nodiscard]] constexpr uint32_t index()  const noexcept { return bits >> 1; }
        [[nodiscard]] constexpr RegClass rc()      const noexcept { return static_cast<RegClass>(bits & 1); }

        [[nodiscard]] constexpr bool valid()                    const noexcept { return bits != invalid_bits; }
        [[nodiscard]] constexpr bool operator==(const VReg &)   const noexcept = default;
    };

    //! @brief A register, either physical or virtual.
    struct Reg {
        static constexpr uint32_t pinned_count = 128;

        uint32_t bits{ VReg::invalid_bits };

        [[nodiscard]] static constexpr Reg from_vreg(const VReg vreg) noexcept {
            Reg reg; reg.bits = vreg.bits; return reg;
        }

        [[nodiscard]] static constexpr Reg from_preg(const PReg preg) noexcept {
            return from_vreg(VReg::make(preg.index(), preg.rc()));
        }

        [[nodiscard]] static constexpr Reg invalid() noexcept { return Reg{}; }

        [[nodiscard]] constexpr bool is_physical() const noexcept { return (bits >> 1) < pinned_count; }

        [[nodiscard]] constexpr bool is_virtual()  const noexcept { return !is_physical(); }

        [[nodiscard]] constexpr VReg as_vreg() const noexcept {
            VReg reg; reg.bits = bits; return reg;
        }

        [[nodiscard]] constexpr PReg as_preg() const noexcept {
            assert(is_physical());
            return PReg::make(static_cast<uint8_t>((bits >> 1) & PReg::max_hw_enc), static_cast<RegClass>(bits & 1));
        }

        [[nodiscard]] constexpr RegClass rc()    const noexcept { return static_cast<RegClass>(bits & 1); }
        [[nodiscard]] constexpr bool valid()     const noexcept { return bits != VReg::invalid_bits; }

        [[nodiscard]] constexpr bool operator==(const Reg &) const noexcept = default;
    };

} // namespace xiao::codegen

template <>
struct std::formatter<xiao::codegen::PReg> {
    constexpr auto parse(auto& ctx) { return ctx.begin(); }
    auto format(const xiao::codegen::PReg r, auto& ctx) const {
        return std::format_to(ctx.out(), "{}{}",
            r.rc() == xiao::codegen::RegClass::Int ? "r" : "f", r.hw());
    }
};

template <>
struct std::formatter<xiao::codegen::VReg> {
    constexpr auto parse(auto& ctx) { return ctx.begin(); }
    auto format(const xiao::codegen::VReg r, auto& ctx) const {
        return std::format_to(ctx.out(), "v{}", r.index());
    }
};

template <>
struct std::formatter<xiao::codegen::Reg> {
    constexpr auto parse(auto& ctx) { return ctx.begin(); }
    auto format(const xiao::codegen::Reg r, auto& ctx) const {
        if (r.is_physical()) return std::format_to(ctx.out(), "{}", r.as_preg());
        return std::format_to(ctx.out(), "{}", r.as_vreg());
    }
};