#pragma once

#include <xiao/regalloc/liveness.hh>
#include <xiao/regalloc/interval.hh>
#include <xiao/regalloc/linear.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace xiao::regalloc
{
    struct Allocation
    {
        codegen::PReg preg { };
        int32_t       slot { -1 };

        [[nodiscard]] bool is_spill( ) const noexcept
        { return slot >= 0; }
    };

    struct Allocations
    {
        std::vector< Allocation > ops;
        uint32_t                  spill_slots { };
    };

    template < codegen::MachInstC I >
    [[nodiscard]] Allocations allocate( const codegen::VCode< I >& vc, const codegen::MachineEnv& env )
    {
        const auto liveness  = Liveness::compute( vc );
        const auto intervals = LiveIntervals::build( vc, liveness );

        std::vector< codegen::PReg > ints_pool, floats_pool;
        for ( const auto pr : env.non_preferred[ 0 ] ) ints_pool.push_back( pr );
        for ( const auto pr : env.preferred[ 0 ] ) ints_pool.push_back( pr );
        for ( const auto pr : env.non_preferred[ 1 ] ) floats_pool.push_back( pr );
        for ( const auto pr : env.preferred[ 1 ] ) floats_pool.push_back( pr );

        const auto exclude = [ ]( std::vector< codegen::PReg >& pool, const std::span< const codegen::PReg > scratch )
        {
            for ( const auto pr : scratch ) pool.erase( std::ranges::remove( pool, pr ).begin( ), pool.end( ) );
        };

        exclude( ints_pool, env.scratch[ 0 ] );
        exclude( floats_pool, env.scratch[ 1 ] );

        std::array< std::vector< codegen::PReg >, 2 > callee_saved;
        for ( std::size_t cls = 0; cls < 2; ++cls )
            callee_saved[ cls ].assign( env.non_preferred[ cls ].begin( ), env.non_preferred[ cls ].end( ) );

        LinearScan scan { intervals.intervals, std::move( ints_pool ), std::move( floats_pool ), intervals.call_points,
                          std::move( callee_saved ) };

        auto [ assignments, spilled ] = std::move( scan ).run( );

        std::unordered_map< uint32_t, int32_t > slot_of;
        int32_t                                 next_slot { 0 };
        for ( const auto vr : spilled )
            if ( slot_of.try_emplace( vr, next_slot ).second ) ++next_slot;

        Allocations result;
        result.spill_slots = static_cast< uint32_t >( next_slot );

        result.ops.reserve( vc.operands.size( ) );
        for ( auto& op : vc.operands )
        {
            if ( op.is_fixed( ) )
            {
                result.ops.push_back( { .preg = *op.fixed_preg( ) } );
                continue;
            }

            const auto vr = op.vreg( ).index( );
            if ( const auto it = assignments.find( vr ); it != assignments.end( ) )
            {
                result.ops.push_back( { .preg = it->second } );
                continue;
            }

            if ( const auto it = slot_of.find( vr ); it != slot_of.end( ) )
            {
                result.ops.push_back( { .slot = it->second } );
                continue;
            }

            assert( false && "[regalloc::allocate] operand vreg has neither a register nor a slot" );
            result.ops.push_back( { } );
        }

        return result;
    }
} // namespace xiao::regalloc