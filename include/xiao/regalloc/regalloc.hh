#pragma once

#include <xiao/regalloc/liveness.hh>
#include <xiao/regalloc/ranges.hh>
#include <xiao/regalloc/coalesce.hh>
#include <xiao/regalloc/allocator.hh>
#include <xiao/regalloc/cfg.hh>
#include <xiao/regalloc/moves.hh>
#include <xiao/regalloc/checker.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace xiao::regalloc
{
    struct Allocations
    {
        std::vector< Allocation > ops;
        std::vector< Edit >       edits;
        uint32_t                  spill_slots { };
        uint32_t                  bundles { };
        uint32_t                  merged_copies { };
        uint32_t                  evictions { };
        uint32_t                  splits { };
        uint32_t                  redundant_stores { };
    };

    [[nodiscard]] inline std::array< std::vector< codegen::PReg >, 2 > allocation_order( const codegen::MachineEnv& env )
    {
        std::array< std::vector< codegen::PReg >, 2 > order;
        for ( std::size_t cls { 0 }; cls < 2; ++cls )
        {
            const auto scratch = std::span< const codegen::PReg > { env.scratch[ cls ] };
            const auto push    = [ & ]( const codegen::PReg pr )
            {
                if ( std::ranges::find( scratch, pr ) == scratch.end( ) && std::ranges::find( order[ cls ], pr ) == order[ cls ].end( ) )
                    order[ cls ].push_back( pr );
            };
            for ( const auto pr : env.preferred[ cls ] ) push( pr );
            for ( const auto pr : env.non_preferred[ cls ] ) push( pr );
        }
        return order;
    }

    class VRegLocations
    {
    public:
        VRegLocations( const std::size_t vregs, const std::vector< LiveBundle >& bundles ) : m_where( vregs ), m_bundles { bundles }
        {
            for ( uint32_t b { 0 }; b < bundles.size( ); ++b )
                for ( const auto& [ vreg, range ] : bundles[ b ].pieces )
                    for ( const auto& s : range.segments ) m_where[ vreg ].push_back( { s, b } );

            for ( auto& w : m_where )
                std::ranges::sort( w, { },
                                   []( const auto& e )
                                   {
                                       return e.first.from;
                                   } );
        }

        [[nodiscard]] const Allocation* at( const uint32_t vreg, const ProgPoint p ) const noexcept
        {
            if ( vreg >= m_where.size( ) ) return nullptr;
            const auto& w  = m_where[ vreg ];
            const auto  it = std::ranges::upper_bound( w, p, { },
                                                       []( const auto& e )
                                                       {
                                                          return e.first.from;
                                                      } );
            if ( it == w.begin( ) || std::prev( it )->first.to <= p ) return nullptr;
            return &m_bundles[ std::prev( it )->second ].alloc;
        }

        [[nodiscard]] const std::vector< std::pair< Segment, uint32_t > >& pieces( const uint32_t vreg ) const noexcept
        { return m_where[ vreg ]; }

        [[nodiscard]] std::size_t size( ) const noexcept
        { return m_where.size( ); }

    private:
        std::vector< std::vector< std::pair< Segment, uint32_t > > > m_where;
        const std::vector< LiveBundle >&                             m_bundles;
    };

    template < codegen::MachInstC I >
    [[nodiscard]] Allocations allocate( const codegen::VCode< I >& vc, const codegen::MachineEnv& env )
    {
        const auto liveness = Liveness::compute( vc );
        const auto ranges   = LiveRanges::build( vc, liveness );
        const auto bundles  = Bundles::build( ranges );
        const auto cfg      = Cfg::build( vc );

        BacktrackingAllocator allocator { bundles, ranges, allocation_order( env ) };
        allocator.run( );

        const auto&         live = allocator.bundles( );
        const VRegLocations where { vc.vclasses.size( ), live };

        Allocations result;
        result.spill_slots   = allocator.spill_slots( );
        result.bundles       = static_cast< uint32_t >( live.size( ) );
        result.merged_copies = bundles.merged_copies;
        result.evictions     = allocator.evictions( );
        result.splits        = allocator.splits( );

        result.ops.resize( vc.operands.size( ) );
        for ( uint32_t i { 0 }; i < vc.insts.size( ); ++i )
        {
            const auto [ off, cnt ] = vc.operand_ranges[ i ];
            for ( auto o { off }; o < off + cnt; ++o )
            {
                const auto& op = vc.operands[ o ];
                if ( op.is_fixed( ) )
                {
                    result.ops[ o ] = { .preg = *op.fixed_preg( ) };
                    continue;
                }
                if ( !op.vreg( ).valid( ) ) continue;

                const bool  early = op.kind( ) == codegen::OperandKind::Use || op.kind( ) == codegen::OperandKind::UseDef
                                      ? op.kind( ) == codegen::OperandKind::UseDef || op.pos( ) == codegen::OperandPos::Before
                                      : op.pos( ) == codegen::OperandPos::Before;
                const auto* a     = where.at( op.vreg( ).index( ), early ? before( i ) : after( i ) );
                assert( a && a->is_assigned( ) && "[xiao::regalloc::allocate] operand has no location" );
                if ( a ) result.ops[ o ] = *a;
            }
        }

        const auto block_start = [ & ]( const ProgPoint p )
        {
            return p % 2 == 0 && p / 2 < vc.insts.size( ) && vc.blocks[ cfg.block_of[ p / 2 ] ].insts_offset == p / 2;
        };

        for ( uint32_t v { codegen::Reg::pinned_count }; v < where.size( ); ++v )
        {
            const auto& ps = where.pieces( v );
            for ( std::size_t k { 1 }; k < ps.size( ); ++k )
            {
                const auto& [ a_seg, a_b ] = ps[ k - 1 ];
                const auto& [ b_seg, b_b ] = ps[ k ];
                if ( a_seg.to != b_seg.from || block_start( b_seg.from ) ) continue;
                if ( live[ a_b ].alloc == live[ b_b ].alloc ) continue;
                assert( b_seg.from % 2 == 0 && "[xiao::regalloc::allocate] split boundary inside an instruction" );
                result.edits.push_back( { b_seg.from, EditPrio::Split, v, live[ a_b ].alloc, live[ b_b ].alloc } );
            }
        }

        for ( uint32_t b { 0 }; b < vc.blocks.size( ); ++b )
        {
            const auto&     block = vc.blocks[ b ];
            const ProgPoint b_end = before( block.insts_offset + block.insts_count );
            for ( uint32_t k { 0 }; k < block.succs_count; ++k )
            {
                const auto s       = vc.succs[ block.succs_offset + k ].id;
                const auto s_start = before( vc.blocks[ s ].insts_offset );
                const bool at_pred = block.succs_count == 1;
                assert( ( at_pred || cfg.preds[ s ].size( ) == 1 ) && "[xiao::regalloc::allocate] critical edge reached the allocator" );

                liveness.blocks[ s ].live_in.for_each(
                    [ & ]( const uint32_t v )
                    {
                        const auto* from = where.at( v, b_end - 1 );
                        const auto* to   = where.at( v, s_start );
                        assert( from && to && "[xiao::regalloc::allocate] live-in vreg without a location at an edge" );
                        if ( !from || !to || *from == *to ) return;
                        if ( at_pred )
                            result.edits.push_back(
                                { before( block.insts_offset + block.insts_count - 1 ), EditPrio::EdgeOut, v, *from, *to } );
                        else
                            result.edits.push_back( { s_start, EditPrio::EdgeIn, v, *from, *to } );
                    } );
            }
        }

        const auto rewritten_next = [ & ]( const uint32_t i, const uint32_t v )
        {
            const auto [ off, cnt ] = vc.operand_ranges[ i + 1 ];
            for ( auto o { off }; o < off + cnt; ++o )
            {
                const auto& op = vc.operands[ o ];
                if ( !op.is_fixed( ) && op.vreg( ).index( ) == v && op.kind( ) != codegen::OperandKind::Use && result.ops[ o ].is_reg( ) )
                    return true;
            }
            return false;
        };

        for ( const auto& bundle : live )
        {
            const auto slot = allocator.spillset_slot( bundle.spillset );
            if ( slot < 0 ) continue;
            for ( const auto& m : bundle.mentions )
            {
                if ( !m.write ) continue;
                const auto i = m.inst( );
                if ( i + 1 >= vc.insts.size( ) || cfg.block_of[ i + 1 ] != cfg.block_of[ i ] ) continue;
                const auto* at_def = where.at( m.vreg, after( i ) );
                if ( !at_def || !at_def->is_reg( ) || !where.at( m.vreg, before( i + 1 ) ) || rewritten_next( i, m.vreg ) ) continue;
                result.edits.push_back( { before( i + 1 ), EditPrio::SpillAtDef, m.vreg, *at_def, Allocation { .slot = slot } } );
            }
        }

        result.redundant_stores = static_cast< uint32_t >( std::erase_if( result.edits,
                                                                          []( const Edit& e )
                                                                          {
                                                                              return e.prio != EditPrio::SpillAtDef && e.from.is_reg( )
                                                                                  && e.to.is_spill( );
                                                                          } ) );

        std::ranges::stable_sort( result.edits,
                                  []( const Edit& a, const Edit& b )
                                  {
                                      return a.point != b.point ? a.point < b.point : a.prio < b.prio;
                                  } );

#ifndef NDEBUG
        const auto errors = check_allocations( vc, cfg, result.ops, result.edits, result.spill_slots );
        assert( errors.empty( ) && "[xiao::regalloc::allocate] checker found a value in the wrong place" );
#endif

        return result;
    }
} // namespace xiao::regalloc
