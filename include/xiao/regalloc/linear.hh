#pragma once

#include <xiao/regalloc/interval.hh>
#include <xiao/codegen/reg.hh>
#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <print>
#include <unordered_map>
#include <vector>

namespace xiao::regalloc
{
    struct Active
    {
        uint32_t      vreg;
        codegen::PReg preg;
        Point         end;
        bool          fixed;
    };

    struct LinearScan
    {
        std::vector< LiveInterval >                   intervals;
        std::vector< codegen::PReg >                  ints_pool;
        std::vector< codegen::PReg >                  floats_pool;
        std::vector< Active >                         active;
        std::unordered_map< uint32_t, codegen::PReg > assignments;
        std::vector< uint32_t >                       spilled;

        std::vector< Point >                          call_points;
        std::array< std::vector< codegen::PReg >, 2 > callee_saved;

        std::unordered_map< uint8_t, std::vector< std::pair< Point, Point > > > fixed_ranges;

        LinearScan( std::vector< LiveInterval > intervals, std::vector< codegen::PReg > ints_pool, std::vector< codegen::PReg > floats_pool,
                    std::vector< Point > call_points, std::array< std::vector< codegen::PReg >, 2 > callee_saved ) :
            intervals { std::move( intervals ) },
            ints_pool { std::move( ints_pool ) },
            floats_pool { std::move( floats_pool ) },
            call_points { std::move( call_points ) },
            callee_saved { std::move( callee_saved ) }
        {
            for ( const auto& iv : this->intervals )
                if ( iv.fixed )
                    for ( const auto& r : iv.ranges ) fixed_ranges[ iv.fixed->index( ) ].push_back( r );
        }

        [[nodiscard]] std::vector< codegen::PReg >& pool( const codegen::RegClass cls )
        { return cls == codegen::RegClass::Int ? ints_pool : floats_pool; }

        [[nodiscard]] static constexpr bool same_pr( const codegen::PReg& a, const codegen::PReg& b )
        { return a.index( ) == b.index( ) && a.rc( ) == b.rc( ); }

        [[nodiscard]] bool crosses_call( const LiveInterval& iv ) const
        {
            const auto s = iv.start( ), e = iv.end( );
            return std::ranges::any_of( call_points,
                                        [ & ]( const Point p )
                                        {
                                            return s < p && e > p + 1;
                                        } );
        }

        [[nodiscard]] bool is_callee_saved( const codegen::PReg pr ) const
        {
            const auto& cs = callee_saved[ static_cast< uint8_t >( pr.rc( ) ) ];
            return std::ranges::any_of( cs,
                                        [ & ]( const codegen::PReg c )
                                        {
                                            return same_pr( c, pr );
                                        } );
        }

        [[nodiscard]] bool blocked_by_fixed( const codegen::PReg pr, const Point s, const Point e ) const
        {
            const auto it = fixed_ranges.find( pr.index( ) );
            if ( it == fixed_ranges.end( ) ) return false;
            return std::ranges::any_of( it->second,
                                        [ & ]( const auto& r )
                                        {
                                            return r.first < e && r.second > s;
                                        } );
        }

        [[nodiscard]] std::optional< codegen::PReg > pick( const LiveInterval& iv )
        {
            auto&      p       = pool( iv.cls );
            const bool crosses = crosses_call( iv );

            assert( ( !crosses || !callee_saved[ static_cast< uint8_t >( iv.cls ) ].empty( ) )
                    && "[regalloc::LinearScan::pick] value live across a call needs a callee-saved reg, and spilling is not implemented" );

            for ( auto it = p.rbegin( ); it != p.rend( ); ++it )
            {
                if ( crosses && !is_callee_saved( *it ) ) continue;
                if ( blocked_by_fixed( *it, iv.start( ), iv.end( ) ) ) continue;

                const auto pr = *it;
                p.erase( std::next( it ).base( ) );
                return pr;
            }

            return std::nullopt;
        }

        [[nodiscard]] std::pair< std::unordered_map< uint32_t, codegen::PReg >, std::vector< uint32_t > > run( )
        {
            std::ranges::stable_sort( intervals,
                                      []( const LiveInterval& a, const LiveInterval& b )
                                      {
                                          if ( a.start( ) != b.start( ) ) return a.start( ) < b.start( );
                                          return a.fixed.has_value( ) > b.fixed.has_value( );
                                      } );

            for ( const auto& interval : intervals )
            {
                std::vector< Active > current;
                for ( auto& a : active )
                {
                    if ( a.end <= interval.start( ) ) pool( a.preg.rc( ) ).push_back( a.preg );
                    else
                        current.push_back( a );
                }

                active = std::move( current );

                if ( interval.fixed )
                {
                    const auto pr = *interval.fixed;
                    auto&      p  = pool( pr.rc( ) );

                    auto iter = std::ranges::find_if( p.begin( ), p.end( ),
                                                      [ & ]( const codegen::PReg pp )
                                                      {
                                                          return same_pr( pp, pr );
                                                      } );

                    if ( iter != p.end( ) ) p.erase( iter );
                    else
                    {
                        auto victim = std::ranges::find_if( active.begin( ), active.end( ),
                                                            [ & ]( const Active& a )
                                                            {
                                                                return !a.fixed && same_pr( a.preg, pr );
                                                            } );

                        if ( victim == active.end( ) )
                        {
                            std::println( "\x1b[38;5;198m[error]\x1b[0m fixed reg requested but not free or held by active vreg (preg "
                                          "index: {}, class: {})",
                                          pr.index( ), static_cast< int >( pr.rc( ) ) );
                        }
                        assert( victim != active.end( ) && "[regalloc::LinearScan::run] fixed reg not free and not held by active vreg" );

                        if ( victim->end > interval.start( ) + 1 ) { spilled.push_back( victim->vreg ); }
                        active.erase( victim );
                    }

                    active.push_back( { interval.vreg, pr, interval.end( ), true } );
                    continue;
                }

                if ( const auto pr = pick( interval ) )
                {
                    assignments.emplace( interval.vreg, *pr );
                    active.push_back( { interval.vreg, *pr, interval.end( ), false } );
                }
                else
                {
                    auto victim = std::ranges::max_element( active,
                                                            [ & ]( const Active& a, const Active& b )
                                                            {
                                                                if ( a.fixed || a.preg.rc( ) != interval.cls ) return true;
                                                                if ( b.fixed || b.preg.rc( ) != interval.cls ) return false;
                                                                return a.end < b.end;
                                                            } );

                    if ( victim == active.end( ) || victim->fixed || victim->preg.rc( ) != interval.cls )
                    {
                        spilled.push_back( interval.vreg );
                    }
                    else if ( interval.end( ) > victim->end ) { spilled.push_back( interval.vreg ); }
                    else
                    {
                        const auto pr = victim->preg;
                        spilled.push_back( victim->vreg );
                        active.erase( victim );
                        assignments.emplace( interval.vreg, pr );
                        active.push_back( { interval.vreg, pr, interval.end( ), false } );
                    }
                }
            }

            return { std::move( assignments ), std::move( spilled ) };
        }
    };
} // namespace xiao::regalloc