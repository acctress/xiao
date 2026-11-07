//
// Created by griffith on 18/10/2026.
//

#pragma once

#include <xiao/codegen/operand.hh>
#include <xiao/codegen/machinst.hh>

#include <cstdint>
#include <span>
#include <vector>
#include <utility>

namespace xiao::codegen
{

    //! @brief An opaque index into a VCode block table
    struct BlockId
    {
        uint32_t id { };
    };

    //! @brief Metadata for a basic block in VCode
    struct VCodeBlock
    {
        uint32_t insts_offset { };
        uint16_t insts_count { };
        uint32_t succs_offset { };
        uint8_t  succs_count { };
        uint32_t params_offset { };
        uint16_t params_count { };
    };

    //! @brief Immutable virtual register code for one function
    template < MachInstC I >
    struct VCode
    {
        std::vector< I >                               insts;
        std::vector< MachOperand >                     operands;
        std::vector< std::pair< uint32_t, uint32_t > > operand_ranges;
        std::vector< VCodeBlock >                      blocks;
        std::vector< VReg >                            block_params;
        std::vector< VReg >                            branch_block_args;
        std::vector< BlockId >                         succs;
        std::vector< RegClass >                        vclasses;
    };

    //! @brief Builder for VCode<I>
    template < MachInstC I >
    class VCodeBuilder
    {
    public:
        //! @brief Begin a new VCode block with the given param vregs
        [[nodiscard]] BlockId start_block( const std::span< const VReg > params = { } )
        {
            const auto id = static_cast< uint32_t >( m_blocks.size( ) );

            VCodeBlock b { };
            b.params_offset = static_cast< uint32_t >( m_block_params.size( ) );
            b.params_count  = static_cast< uint16_t >( params.size( ) );
            b.succs_offset  = static_cast< uint32_t >( m_succs.size( ) );
            b.insts_offset  = static_cast< uint32_t >( m_insts.size( ) );
            for ( const auto vr : params ) m_block_params.push_back( vr );
            m_blocks.push_back( b );

            return BlockId { id };
        }

        //! @brief Close the current block with its successors and branch args
        void end_block( const std::span< const BlockId > succs, const std::span< const VReg > branch_args = { } )
        {
            auto& b       = m_blocks[ m_cur.id ];
            b.insts_count = static_cast< uint16_t >( m_insts.size( ) - b.insts_offset );
            b.succs_count = static_cast< uint8_t >( succs.size( ) );

            for ( const auto s : succs ) m_succs.push_back( s );
            for ( const auto vr : branch_args ) m_branch_block_args.push_back( vr );
        }

        //! @brief Make `block` current and record where its instructions and successors start
        void begin_block( const BlockId block )
        {
            m_cur          = block;
            auto& b        = m_blocks[ block.id ];
            b.insts_offset = static_cast< uint32_t >( m_insts.size( ) );
            b.succs_offset = static_cast< uint32_t >( m_succs.size( ) );
        }

        //! @brief Set the active block for next push() calls
        void switch_to_block( const BlockId block )
        { m_cur = block; }

        //! @brief Push an instruction into the current block
        void push( I inst )
        {
            const auto ops_offset = static_cast< uint32_t >( m_operands.size( ) );
            inst.get_operands( m_operands );
            const auto ops_count = static_cast< uint32_t >( m_operands.size( ) ) - ops_offset;

            m_operand_ranges.emplace_back( ops_offset, ops_count );
            m_insts.push_back( std::move( inst ) );
        }

        //! @brief Finalise into an immutable VCode<I>
        [[nodiscard]] VCode< I > finalise( ) &&
        {
            return VCode< I > {
                .insts             = std::move( m_insts ),
                .operands          = std::move( m_operands ),
                .operand_ranges    = std::move( m_operand_ranges ),
                .blocks            = std::move( m_blocks ),
                .block_params      = std::move( m_block_params ),
                .branch_block_args = std::move( m_branch_block_args ),
                .succs             = std::move( m_succs ),
                .vclasses          = std::move( m_vclasses ),
            };
        }

        //! @brief Register a VReg's class
        void register_vreg( const VReg vr )
        {
            const auto idx = vr.index( );
            if ( idx >= m_vclasses.size( ) ) m_vclasses.resize( idx + 1 );
            m_vclasses[ idx ] = vr.rc( );
        }

    private:
        BlockId m_cur { };

        std::vector< I >                               m_insts;
        std::vector< MachOperand >                     m_operands;
        std::vector< std::pair< uint32_t, uint32_t > > m_operand_ranges;
        std::vector< VCodeBlock >                      m_blocks;
        std::vector< VReg >                            m_block_params;
        std::vector< VReg >                            m_branch_block_args;
        std::vector< BlockId >                         m_succs;
        std::vector< RegClass >                        m_vclasses;
    };
} // namespace xiao::codegen