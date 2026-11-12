//
// Created by griffith on 17/10/2026.
//

#pragma once

#include <cstdint>
#include <format>

namespace xiao::ir
{
    //! @brief A basic block in IR CFG
    //! @note Instructions and successor blocks are stored outside of IRBlock in flat pools
    //! on the owning IRFunction.
    struct IRBlock
    {
        uint32_t insts_offset { };  //! Offset into owning IRFunction's instruction pool.
        uint16_t insts_count { };   //! Count of instructions in this IRBlock.
        uint32_t succs_offset { };  //! Offset into owning IRFunction's successor pool.
        uint8_t  succs_count { };   //! Count of successors in this IRBlock.
        uint32_t params_offset { }; //! Offset into owning IRFunction's parameters pool.
        uint16_t params_count { };  //! Count of block parameters.
    };
} // namespace xiao::ir

template <>
struct std::formatter< xiao::ir::IRBlock >
{
    constexpr auto parse( auto& ctx )
    { return ctx.begin( ); }

    auto format( const xiao::ir::IRBlock& b, auto& ctx ) const
    {
        return std::format_to( ctx.out( ), "block(insts={}..{}, succs={}..{}, params={})", b.insts_offset, b.insts_offset + b.insts_count,
                               b.succs_offset, b.succs_offset + b.succs_count, b.params_count );
    }
};