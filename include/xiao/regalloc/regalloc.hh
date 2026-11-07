#pragma once

#include <xiao/regalloc/liveness.hh>
#include <xiao/regalloc/interval.hh>
#include <xiao/regalloc/linear.hh>
#include <xiao/codegen/machenv.hh>
#include <xiao/codegen/vcode.hh>

namespace xiao::regalloc
{
    struct Allocations
    {
        std::vector< codegen::PReg > ops;
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

        // const auto excl = []( std::vector< codegen::PReg >& pool, const codegen::PReg scratch )
        // {
        //     pool.erase( std::ranges::remove( pool, scratch ).begin( ), pool.end( ) );
        // };
        //
        // excl( ints_pool, env.scratch[ 0 ] );
        // excl( floats_pool, env.scratch[ 1 ] );

        LinearScan scan { std::move( intervals.intervals ), std::move( ints_pool ), std::move( floats_pool ) };

        auto [ assignments, spilled ] = std::move( scan ).run( );

        Allocations result;

        result.ops.reserve( vc.operands.size( ) );
        for ( auto& op : vc.operands )
        {
            const auto vr = op.vreg( );
            if ( op.is_fixed( ) )
            {
                result.ops.push_back( *op.fixed_preg( ) );
                continue;
            }

            const auto iter = assignments.find( vr.index( ) );
            if ( iter != assignments.end( ) ) result.ops.push_back( iter->second );
            else
                result.ops.push_back( codegen::PReg::invalid( ) );
        }

        ( void ) spilled;
        return result;
    }
} // namespace xiao::regalloc