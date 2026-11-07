#pragma once

#include <xiao/regalloc/liveness.hh>
#include <xiao/codegen/vcode.hh>
#include <xiao/codegen/reg.hh>
#include <unordered_map>
#include <optional>
#include <ranges>
#include <vector>

namespace xiao::regalloc
{
    using Point = uint32_t;

    struct LiveInterval
    {
        uint32_t                                 vreg;
        codegen::RegClass                        cls;
        std::vector< std::pair< Point, Point > > ranges;
        std::optional< codegen::PReg >           fixed;

        [[nodiscard]] Point start( ) const
        {
            Point p { ~0u };
            for ( const auto& [ s, e ] : ranges ) p = std::min( p, s );
            return p;
        }

        [[nodiscard]] Point end( ) const
        {
            Point p { 0 };
            for ( const auto& [ s, e ] : ranges ) p = std::max( p, e );
            return p;
        }
    };

    struct LiveIntervals
    {
        std::vector< LiveInterval > intervals;

        [[nodiscard]] static constexpr bool same_pr( const codegen::PReg& a, const codegen::PReg& b ) noexcept
        { return a.index( ) == b.index( ) && a.rc( ) == b.rc( ); }

        template < codegen::MachInstC I >
        [[nodiscard]] static LiveIntervals build( const codegen::VCode< I >& vc, const Liveness& liveness )
        {
            std::unordered_map< uint32_t, std::vector< std::pair< Point, Point > > > ranges;
            std::vector< LiveInterval >                                              fixed_intervals;

            for ( uint32_t b { 0 }; b < vc.blocks.size( ); ++b )
            {
                const auto& block    = vc.blocks[ b ];
                const auto& live_in  = liveness.blocks[ b ].live_in;
                const auto& live_out = liveness.blocks[ b ].live_out;

                std::unordered_map< uint32_t, Point >                                first_def, last_use;
                std::vector< std::pair< codegen::PReg, std::pair< Point, Point > > > fix_ranges;

                for ( auto i { 0u }; i < block.insts_count; ++i )
                {
                    const Point abs                 = block.insts_offset + i;
                    const auto [ ops_off, ops_cnt ] = vc.operand_ranges[ block.insts_offset + i ];

                    for ( auto o { ops_off }; o < ops_off + ops_cnt; ++o )
                    {
                        const auto& op     = vc.operands[ o ];
                        const bool  is_use = op.kind( ) == codegen::OperandKind::Use || op.kind( ) == codegen::OperandKind::UseDef;
                        const bool  is_def = op.kind( ) == codegen::OperandKind::Def || op.kind( ) == codegen::OperandKind::UseDef;

                        const auto vr = op.vreg( );

                        if ( op.is_fixed( ) )
                        {
                            const auto pr = *op.fixed_preg( );
                            auto       it = std::find_if( fix_ranges.begin( ), fix_ranges.end( ),
                                                          [ & ]( const auto& p )
                                                          {
                                                        return same_pr( p.first, pr );
                                                    } );
                            if ( it != fix_ranges.end( ) )
                            {
                                it->second.first  = std::min( it->second.first, abs );
                                it->second.second = std::max( it->second.second, abs + 1 );
                            }
                            else
                            {
                                fix_ranges.push_back( {
                                    pr,
                                    { abs, abs + 1 }
                                } );
                            }
                        }

                        if ( vr.valid( ) && vr.index( ) >= codegen::Reg::pinned_count )
                        {
                            if ( is_use ) last_use[ vr.index( ) ] = abs;
                            if ( is_def ) first_def.try_emplace( vr.index( ), abs );
                        }
                    }
                }

                std::unordered_map< uint32_t, bool > touched;
                for ( const auto& v : first_def | std::views::keys ) touched[ v ] = true;
                for ( const auto& v : last_use | std::views::keys ) touched[ v ] = true;
                for ( const auto v : live_in ) touched[ v ] = true;
                for ( const auto v : live_out ) touched[ v ] = true;

                for ( const auto& vr : touched | std::views::keys )
                {
                    const Point s = live_in.contains( vr ) ? block.insts_offset : first_def.at( vr );
                    const Point e = live_out.contains( vr )   ? block.insts_offset + block.insts_count
                                  : last_use.contains( vr ) ? last_use.at( vr ) + 1
                                                            : first_def.at( vr ) + 1; // dead def
                    ranges[ vr ].emplace_back( s, e );
                }

                for ( const auto& [ pr, r ] : fix_ranges )
                {
                    fixed_intervals.push_back( {
                        .vreg   = ~0u,
                        .cls    = pr.rc( ),
                        .ranges = { r },
                        .fixed  = pr,
                    } );
                }
            }

            std::vector< LiveInterval > intervals;
            intervals.reserve( ranges.size( ) );
            for ( auto& [ vreg, rs ] : ranges )
            {
                intervals.push_back( {
                    .vreg   = vreg,
                    .ranges = std::move( rs ),
                    .fixed  = std::nullopt,
                } );
            }

            for ( auto& iv : intervals ) iv.cls = vc.vclasses[ iv.vreg ];

            intervals.insert( intervals.end( ), std::make_move_iterator( fixed_intervals.begin( ) ),
                              std::make_move_iterator( fixed_intervals.end( ) ) );

            return LiveIntervals { std::move( intervals ) };
        }
    };
} // namespace xiao::regalloc