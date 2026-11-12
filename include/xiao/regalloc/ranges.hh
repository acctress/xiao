#pragma once

#include <xiao/regalloc/liveness.hh>
#include <xiao/codegen/reg.hh>
#include <xiao/codegen/vcode.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <span>
#include <vector>

namespace xiao::regalloc
{
    using ProgPoint = uint32_t;

    [[nodiscard]] constexpr ProgPoint before( const uint32_t inst ) noexcept
    { return inst * 2; }

    [[nodiscard]] constexpr ProgPoint after( const uint32_t inst ) noexcept
    { return inst * 2 + 1; }

    struct Segment
    {
        ProgPoint from { };
        ProgPoint to { };

        [[nodiscard]] constexpr bool overlaps( const Segment& o ) const noexcept
        { return from < o.to && o.from < to; }
    };

    struct LiveRange
    {
        std::vector< Segment > segments { };

        [[nodiscard]] bool empty( ) const noexcept
        { return segments.empty( ); }

        [[nodiscard]] ProgPoint start( ) const noexcept
        { return segments.front( ).from; }

        [[nodiscard]] ProgPoint end( ) const noexcept
        { return segments.back( ).to; }

        [[nodiscard]] uint32_t length( ) const noexcept
        {
            uint32_t total { 0 };
            for ( const auto& s : segments ) total += s.to - s.from;
            return total;
        }

        void add( Segment seg )
        {
            auto iter = std::ranges::lower_bound( segments, seg.from, { }, &Segment::to );

            auto last = iter;
            while ( last != segments.end( ) && last->from <= seg.to )
            {
                seg.from = std::min( seg.from, last->from );
                seg.to   = std::max( seg.to, last->to );
                ++last;
            }

            iter = segments.erase( iter, last );
            segments.insert( iter, seg );
        }

        [[nodiscard]] bool covers( const ProgPoint p ) const noexcept
        {
            const auto iter = std::ranges::upper_bound( segments, p, { }, &Segment::from );
            return iter != segments.begin( ) && std::prev( iter )->to > p;
        }

        void start_at( const ProgPoint p )
        {
            if ( const auto iter = std::ranges::upper_bound( segments, p, { }, &Segment::from );
                 iter != segments.begin( ) && std::prev( iter )->to > p )
                std::prev( iter )->from = p;
            else
                add( { p, p + 1 } );
        }

        [[nodiscard]] bool overlaps( const LiveRange& o ) const noexcept
        {
            auto a = segments.begin( );
            auto b = o.segments.begin( );
            while ( a != segments.end( ) && b != o.segments.end( ) )
            {
                if ( a->overlaps( *b ) ) return true;
                if ( a->to <= b->to ) ++a;
                else
                    ++b;
            }
            return false;
        }

        void merge( const LiveRange& o )
        {
            for ( const auto& s : o.segments ) add( s );
        }
    };

    struct Mention
    {
        ProgPoint point { };
        float     weight { };
        uint32_t  vreg { };
        bool      read { };
        bool      write { };

        [[nodiscard]] constexpr uint32_t inst( ) const noexcept
        { return point / 2; }
    };

    struct VRegLiveness
    {
        LiveRange              range { };
        std::vector< Mention > mentions { };
        float                  use_weight { };
        codegen::RegClass      cls { };
    };

    struct CopyEdge
    {
        uint32_t dst { };
        uint32_t src { };
        float    weight { };
    };

    struct RegHint
    {
        uint32_t      vreg { };
        codegen::PReg preg { };
        float         weight { };
    };

    struct LiveRanges
    {
        std::vector< VRegLiveness >                         vregs { };
        std::array< LiveRange, codegen::Reg::pinned_count > fixed { };
        std::vector< CopyEdge >                             copies { };
        std::vector< RegHint >                              hints { };

        template < codegen::MachInstC I >
        [[nodiscard]] static std::vector< uint32_t > loop_depths( const codegen::VCode< I >& vc )
        {
            const auto n = static_cast< uint32_t >( vc.blocks.size( ) );

            std::vector< std::vector< uint32_t > > preds( n );
            const auto                             succs_of = [ & ]( const uint32_t b )
            {
                return std::span { vc.succs }.subspan( vc.blocks[ b ].succs_offset, vc.blocks[ b ].succs_count );
            };
            for ( uint32_t b { 0 }; b < n; ++b )
                for ( const auto s : succs_of( b ) ) preds[ s.id ].push_back( b );

            std::vector< std::vector< uint32_t > >         latches( n );
            std::vector< uint8_t >                         state( n, 0 );
            std::vector< std::pair< uint32_t, uint32_t > > stack;
            if ( n > 0 )
            {
                stack.push_back( { 0, 0 } );
                state[ 0 ] = 1;
            }
            while ( !stack.empty( ) )
            {
                auto& [ b, next ] = stack.back( );
                const auto succs  = succs_of( b );
                if ( next < succs.size( ) )
                {
                    const auto s = succs[ next++ ].id;
                    if ( state[ s ] == 1 ) latches[ s ].push_back( b );
                    else if ( state[ s ] == 0 )
                    {
                        state[ s ] = 1;
                        stack.push_back( { s, 0 } );
                    }
                }
                else
                {
                    state[ b ] = 2;
                    stack.pop_back( );
                }
            }

            std::vector< uint32_t > depth( n, 0 );
            for ( uint32_t header { 0 }; header < n; ++header )
            {
                if ( latches[ header ].empty( ) ) continue;

                std::vector             in_loop( n, false );
                std::vector< uint32_t > work;
                in_loop[ header ] = true;
                for ( const auto latch : latches[ header ] )
                    if ( !in_loop[ latch ] )
                    {
                        in_loop[ latch ] = true;
                        work.push_back( latch );
                    }
                while ( !work.empty( ) )
                {
                    const auto x = work.back( );
                    work.pop_back( );
                    for ( const auto p : preds[ x ] )
                        if ( !in_loop[ p ] )
                        {
                            in_loop[ p ] = true;
                            work.push_back( p );
                        }
                }

                for ( uint32_t b { 0 }; b < n; ++b )
                    if ( in_loop[ b ] ) ++depth[ b ];
            }

            return depth;
        }

        [[nodiscard]] static float depth_weight( const uint32_t depth ) noexcept
        {
            constexpr float table[] = { 1.f, 10.f, 100.f, 1000.f, 10000.f };
            return table[ std::min< uint32_t >( depth, 4 ) ];
        }

        template < codegen::MachInstC I >
        [[nodiscard]] static LiveRanges build( const codegen::VCode< I >& vc, const Liveness& liveness )
        {
            LiveRanges lr;
            lr.vregs.resize( vc.vclasses.size( ) );
            for ( std::size_t v { codegen::Reg::pinned_count }; v < vc.vclasses.size( ); ++v ) lr.vregs[ v ].cls = vc.vclasses[ v ];

            const auto depths = loop_depths( vc );

            const auto range_of = [ & ]( const uint32_t index ) -> LiveRange&
            {
                return index < codegen::Reg::pinned_count ? lr.fixed[ index ] : lr.vregs[ index ].range;
            };

            for ( auto b = static_cast< int64_t >( vc.blocks.size( ) ) - 1; b >= 0; --b )
            {
                const auto&     block  = vc.blocks[ b ];
                const ProgPoint b_from = before( block.insts_offset );
                const ProgPoint b_to   = before( block.insts_offset + block.insts_count );
                const float     weight = depth_weight( depths[ b ] );

                liveness.blocks[ b ].live_out.for_each(
                    [ & ]( const uint32_t value )
                    {
                        lr.vregs[ value ].range.add( { b_from, b_to } );
                    } );

                std::array< bool, codegen::Reg::pinned_count > pinned_open { };

                for ( auto i = static_cast< int64_t >( block.insts_offset + block.insts_count ) - 1; i >= block.insts_offset; --i )
                {
                    const auto inst         = static_cast< uint32_t >( i );
                    const auto [ off, cnt ] = vc.operand_ranges[ inst ];
                    const auto operands     = std::span { vc.operands }.subspan( off, cnt );

                    for ( const auto pr : vc.insts[ inst ].clobbers( ) )
                        lr.fixed[ pr.index( ) ].add( { after( inst ), after( inst ) + 1 } );

                    for ( const auto& op : operands )
                    {
                        const auto vr = op.vreg( );
                        if ( !vr.valid( ) ) continue;
                        if ( op.kind( ) == codegen::OperandKind::Def || op.kind( ) == codegen::OperandKind::UseDef )
                        {
                            const ProgPoint p = op.pos( ) == codegen::OperandPos::Before ? before( inst ) : after( inst );
                            range_of( vr.index( ) ).start_at( p );
                            if ( vr.index( ) < codegen::Reg::pinned_count ) pinned_open[ vr.index( ) ] = false;
                        }
                    }

                    for ( const auto& op : operands )
                    {
                        const auto vr = op.vreg( );
                        if ( !vr.valid( ) ) continue;
                        if ( op.kind( ) == codegen::OperandKind::Use || op.kind( ) == codegen::OperandKind::UseDef )
                        {
                            const ProgPoint p = op.kind( ) == codegen::OperandKind::UseDef || op.pos( ) == codegen::OperandPos::Before
                                                  ? before( inst )
                                                  : after( inst );
                            range_of( vr.index( ) ).add( { b_from, p + 1 } );
                            if ( vr.index( ) < codegen::Reg::pinned_count ) pinned_open[ vr.index( ) ] = true;
                        }
                    }

                    for ( const auto& op : operands )
                    {
                        const auto vr = op.vreg( );
                        if ( !vr.valid( ) || vr.index( ) < codegen::Reg::pinned_count ) continue;

                        const bool read  = op.kind( ) == codegen::OperandKind::Use || op.kind( ) == codegen::OperandKind::UseDef;
                        const bool write = op.kind( ) == codegen::OperandKind::Def || op.kind( ) == codegen::OperandKind::UseDef;
                        const bool early = op.kind( ) == codegen::OperandKind::UseDef || op.pos( ) == codegen::OperandPos::Before;

                        auto& v       = lr.vregs[ vr.index( ) ];
                        v.use_weight += weight;
                        v.mentions.push_back( { early ? before( inst ) : after( inst ), weight, vr.index( ), read, write } );
                    }

                    if ( const auto mv = vc.insts[ inst ].as_move( ) )
                    {
                        const auto [ dst, src ] = *mv;
                        if ( dst.is_physical( ) && src.is_physical( ) ) continue;
                        if ( !dst.is_physical( ) && !src.is_physical( ) )
                            lr.copies.push_back( { dst.as_vreg( ).index( ), src.as_vreg( ).index( ), weight } );
                        else if ( dst.is_physical( ) )
                            lr.hints.push_back( { src.as_vreg( ).index( ), dst.as_preg( ), weight } );
                        else
                            lr.hints.push_back( { dst.as_vreg( ).index( ), src.as_preg( ), weight } );
                    }
                }

                if ( b != 0 )
                    assert( std::ranges::none_of( pinned_open,
                                                  []( const bool open )
                                                  {
                                                      return open;
                                                  } )
                            && "[xiao::regalloc::LiveRanges::build] fixed register read before being written in a non entry block" );
            }

            for ( auto& v : lr.vregs ) std::ranges::stable_sort( v.mentions, { }, &Mention::point );

            return lr;
        }
    };
} // namespace xiao::regalloc
