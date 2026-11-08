//
// Created by griffith on 17/10/2026.
//

#pragma once

#include <cstdint>

namespace xiao::ir {
    //! @brief A typed opaque index into an arena allocated IR table.
    template <typename Tag>
    struct Ref { uint32_t id; };

    using ValueRef      = Ref<struct ValueTag>;
    using BlockRef      = Ref<struct BlockTag>;
    using InstRef       = Ref<struct InstTag>;
    using StringRef     = Ref<struct StringTag>;
    using FunctionRef   = Ref<struct FunctionTag>;
    using ImmRef        = Ref<struct ImmTag>;
    using FuncRef       = Ref<struct FuncRefTag>; //! Different to `FunctionRef`, this is an index into `IRFunction::ext_funcs`
    using GlobalValueRef = Ref<struct GlobalValueTag>;
}


template <typename Tag>
struct std::formatter<xiao::ir::Ref<Tag>> {
    constexpr auto parse(auto& ctx) { return ctx.begin(); }
    auto format(xiao::ir::Ref<Tag> r, auto& ctx) const {
        return std::format_to(ctx.out(), "{}", r.id);
    }
};
