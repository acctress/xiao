//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/codegen/reg.hh>
#include <xiao/codegen/operand.hh>
#include <xiao/ir/types.hh>

#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xiao::codegen
{

    //! @brief Describes how a block ends
    enum class MachTerminator : uint8_t
    {
        None,       //! Not a terminator
        Ret,        //! Return from function
        Branch,     //! Branch to one or more blocks
    };

    //! @brief CRTP base for a MachInst contract
    //! @tparam Derived The concrete instruction type
    //! @tparam LabelUse A type describing label reference fixup kinds
    template < typename Derived, typename LabelUse >
    class MachInst
    {
    public:
        //! @brief Collect all register operands into `ops`
        void get_operands( std::vector< MachOperand >& ops )
        { self( ).get_operands( ops ); }

        //! @brief How this instruction ends its block.
        [[nodiscard]] MachTerminator is_term( ) const noexcept
        { return self( ).is_term( ); }

        //! @brief Is this a pure reg to reg move, if so return dst and src
        [[nodiscard]] std::optional< std::pair< Reg, Reg > > as_move( ) const noexcept
        { return self( ).as_move( ); }

        //! @brief Does this instruction access memory?
        [[nodiscard]] bool is_mem_access( ) const noexcept
        { return self( ).is_mem_access( ); }

        //! @brief Generate a reg to reg move of the given type
        [[nodiscard]] static Derived gen_move( Reg dst, Reg src, ir::Type ty ) = delete;

        //! @brief Generate a dummy use of reg to keep it alive
        [[nodiscard]] static Derived gen_dummy_use( Reg reg ) = delete;

        //! @brief Generate an unconditional jump to `target`
        [[nodiscard]] static Derived gen_jump( uint32_t target ) = delete;

        //! @brief Generate a nop of at most `preferred_size`
        [[nodiscard]] static Derived gen_nop( std::size_t preferred_size ) = delete;

        //! @brief Worst case byte size of an instruction
        [[nodiscard]] static constexpr uint32_t worst_case_size( ) = delete;

        //! @brief Worse case additional is;land growth per instruction
        [[nodiscard]] static constexpr uint32_t worst_case_island_growth( ) = delete;

        //! @brief Emit this instruction into `buf`
        template < typename State, typename Info >
        void emit( std::vector< uint8_t >& buf, const Info& info, State& state ) const = delete;

        //! @brief Return a formatted, readable representation of this instruction
        template < typename State >
        [[nodiscard]] std::string fmt( State& state ) const = delete;

    private:
        [[nodiscard]] Derived& self( ) noexcept
        { return static_cast< Derived& >( *this ); }

        [[nodiscard]] const Derived& self( ) const noexcept
        { return static_cast< const Derived& >( *this ); }
    };

    template < typename T >
    concept MachInstC = requires ( T inst, std::vector< MachOperand >& ops ) {
        inst.get_operands( ops );
        { inst.is_term( ) } -> std::same_as< MachTerminator >;
        { inst.is_mem_access( ) } -> std::same_as< bool >;
        { inst.is_call( ) } -> std::same_as< bool >;
        { inst.as_move( ) } -> std::same_as< std::optional< std::pair< Reg, Reg > > >;
        { inst.clobbers( ) } -> std::convertible_to< std::span< const PReg > >;
    };

    template < typename T >
    concept LabelUseC = requires ( T use, uint32_t from, uint32_t to ) {
        { T::encode( use, from, to ) } -> std::same_as< int32_t >;
    };
} // namespace xiao::codegen