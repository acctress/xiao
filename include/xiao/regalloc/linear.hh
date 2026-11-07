#pragma once

#include <xiao/regalloc/interval.hh>
#include <xiao/codegen/reg.hh>
#include <algorithm>
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

        LinearScan( std::vector< LiveInterval > intervals, std::vector< codegen::PReg > ints_pool,
                    std::vector< codegen::PReg > floats_pool ) :
            intervals { std::move( intervals ) },
            ints_pool { std::move( ints_pool ) },
            floats_pool { std::move( floats_pool ) }
        {
        }

        [[nodiscard]] std::vector< codegen::PReg >& pool( const codegen::RegClass cls )
        { return cls == codegen::RegClass::Int ? ints_pool : floats_pool; }

        [[nodiscard]] static constexpr bool same_pr( const codegen::PReg& a, const codegen::PReg& b )
        { return a.index( ) == b.index( ) && a.rc( ) == b.rc( ); }

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

                auto& p = pool( interval.cls );
                if ( !p.empty( ) )
                {
                    const auto pr = p.back( );
                    p.pop_back( );

                    assignments.emplace( interval.vreg, pr );
                    active.push_back( { interval.vreg, pr, interval.end( ), false } );
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