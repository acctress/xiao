#pragma once

#include <xiao/regalloc/coalesce.hh>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

namespace xiao::regalloc
{
    struct Allocation
    {
        codegen::PReg preg { };
        int32_t       slot { -1 };

        [[nodiscard]] bool is_spill( ) const noexcept
        { return slot >= 0; }

        [[nodiscard]] bool is_reg( ) const noexcept
        { return slot < 0 && preg.valid( ); }

        [[nodiscard]] bool is_assigned( ) const noexcept
        { return is_spill( ) || is_reg( ); }

        [[nodiscard]] bool operator== ( const Allocation& o ) const noexcept
        { return is_spill( ) ? o.is_spill( ) && slot == o.slot : o.is_reg( ) && preg == o.preg; }
    };

    struct LiveBundle
    {
        std::vector< Piece >                             pieces { };
        LiveRange                                        range { };
        codegen::RegClass                                cls { };
        std::vector< Mention >                           mentions { };
        std::vector< std::pair< codegen::PReg, float > > hints { };
        uint32_t                                         spillset { };
        uint32_t                                         cascade { };
        Allocation                                       alloc { };
        float                                            weight { };
        bool                                             minimal { };
    };

    struct CutPoints
    {
        std::vector< bool > allowed { };

        [[nodiscard]] bool can_cut( const ProgPoint p ) const noexcept
        { return p % 2 == 0 && p / 2 < allowed.size( ) && allowed[ p / 2 ]; }

        [[nodiscard]] ProgPoint next( ProgPoint p ) const noexcept
        {
            if ( p % 2 ) ++p;
            while ( p / 2 < allowed.size( ) && !allowed[ p / 2 ] ) p += 2;
            return p;
        }

        [[nodiscard]] ProgPoint prev( ProgPoint p ) const noexcept
        {
            if ( p % 2 ) --p;
            while ( p > 0 && ( p / 2 >= allowed.size( ) || !allowed[ p / 2 ] ) ) p -= 2;
            return p;
        }
    };

    class BacktrackingAllocator
    {
    public:
        static constexpr uint32_t fixed_owner    = ~0u;
        static constexpr float    minimal_weight = 1e30f;

        BacktrackingAllocator( const Bundles& bundles, const LiveRanges& ranges, std::array< std::vector< codegen::PReg >, 2 > order,
                               CutPoints cuts ) :
            m_order { std::move( order ) },
            m_cuts { std::move( cuts ) }
        {
            for ( std::size_t pr { 0 }; pr < codegen::Reg::pinned_count; ++pr ) m_pregs[ pr ].insert( ranges.fixed[ pr ], fixed_owner );

            m_bundles.reserve( bundles.bundles.size( ) * 2 );
            for ( uint32_t i { 0 }; i < bundles.bundles.size( ); ++i )
            {
                const auto& [ pieces, range, cls, mentions, hints ] = bundles.bundles[ i ];
                LiveBundle b {
                    .pieces   = pieces,
                    .range    = range,
                    .cls      = cls,
                    .mentions = mentions,
                    .hints    = hints,
                    .spillset = i,
                };
                finish( b );
                m_bundles.push_back( std::move( b ) );
                m_members.push_back( { i } );
            }
            m_slot_of_spillset.assign( bundles.bundles.size( ), -1 );
        }

        void run( )
        {
            for ( uint32_t b { 0 }; b < m_bundles.size( ); ++b ) enqueue( b );

            while ( !m_queue.empty( ) )
            {
                const auto b = m_queue.top( ).second;
                m_queue.pop( );
                if ( !m_bundles[ b ].alloc.is_assigned( ) ) allocate( b );
            }
        }

        [[nodiscard]] const std::vector< LiveBundle >& bundles( ) const noexcept
        { return m_bundles; }

        [[nodiscard]] int32_t spillset_slot( const uint32_t spillset ) const noexcept
        { return m_slot_of_spillset[ spillset ]; }

        [[nodiscard]] uint32_t spill_slots( ) const noexcept
        { return m_next_slot; }

        [[nodiscard]] uint32_t evictions( ) const noexcept
        { return m_evictions; }

        [[nodiscard]] uint32_t splits( ) const noexcept
        { return m_splits; }

    private:
        struct Entry
        {
            Segment  seg { };
            uint32_t owner { };
        };

        struct PRegMap
        {
            std::vector< Entry > entries { };

            void insert( const LiveRange& range, const uint32_t owner )
            {
                for ( const auto& s : range.segments )
                {
                    const auto iter = std::ranges::lower_bound( entries, s.from, { },
                                                                []( const Entry& e )
                                                                {
                                                                    return e.seg.from;
                                                                } );
                    entries.insert( iter, Entry { s, owner } );
                }
            }

            void remove( const uint32_t owner )
            {
                std::erase_if( entries,
                               [ & ]( const Entry& e )
                               {
                                   return e.owner == owner;
                               } );
            }

            template < typename F >
            void for_each_overlap( const LiveRange& range, F&& f ) const
            {
                auto a = entries.begin( );
                auto b = range.segments.begin( );
                while ( a != entries.end( ) && b != range.segments.end( ) )
                {
                    if ( a->seg.overlaps( *b ) ) f( a->owner, Segment { std::max( a->seg.from, b->from ), std::min( a->seg.to, b->to ) } );
                    if ( a->seg.to <= b->to ) ++a;
                    else
                        ++b;
                }
            }
        };

        static void finish( LiveBundle& b )
        {
            b.range.segments.clear( );
            for ( const auto& p : b.pieces ) b.range.merge( p.range );

            float total { 0.f };
            for ( const auto& m : b.mentions ) total += m.weight;

            b.minimal = !b.range.empty( ) && b.range.start( ) / 2 == ( b.range.end( ) - 1 ) / 2;
            if ( b.mentions.empty( ) ) b.weight = 0.f;
            else if ( b.minimal )
                b.weight = minimal_weight;
            else
                b.weight = total / static_cast< float >( b.range.length( ) + 50 );
        }

        void enqueue( const uint32_t b )
        { m_queue.push( { m_bundles[ b ].range.length( ), b } ); }

        [[nodiscard]] const std::vector< codegen::PReg >& order_of( const codegen::RegClass cls ) const
        { return m_order[ static_cast< uint8_t >( cls ) ]; }

        [[nodiscard]] bool allocatable( const codegen::RegClass cls, const codegen::PReg pr ) const
        { return std::ranges::find( order_of( cls ), pr ) != order_of( cls ).end( ); }

        void assign( const uint32_t b, const codegen::PReg pr )
        {
            m_pregs[ pr.index( ) ].insert( m_bundles[ b ].range, b );
            m_bundles[ b ].alloc = Allocation { .preg = pr };
        }

        void spill( const uint32_t b )
        {
            auto& slot = m_slot_of_spillset[ m_bundles[ b ].spillset ];
            if ( slot < 0 ) slot = static_cast< int32_t >( m_next_slot++ );
            m_bundles[ b ].alloc = Allocation { .slot = slot };
        }

        [[nodiscard]] std::vector< codegen::PReg > candidates( const uint32_t b ) const
        {
            const auto&                  bundle = m_bundles[ b ];
            std::vector< codegen::PReg > out;
            const auto                   add = [ & ]( const codegen::PReg pr )
            {
                if ( allocatable( bundle.cls, pr ) && std::ranges::find( out, pr ) == out.end( ) ) out.push_back( pr );
            };

            for ( const auto s : m_members[ bundle.spillset ] )
            {
                const auto& sib = m_bundles[ s ];
                if ( s == b || !sib.alloc.is_reg( ) || sib.range.empty( ) ) continue;
                if ( sib.range.end( ) == bundle.range.start( ) || bundle.range.end( ) == sib.range.start( ) ) add( sib.alloc.preg );
            }
            for ( const auto& [ pr, votes ] : bundle.hints ) add( pr );
            for ( const auto pr : order_of( bundle.cls ) ) add( pr );
            return out;
        }

        void allocate( const uint32_t b )
        {
            const auto cands = candidates( b );

            struct Option
            {
                codegen::PReg           preg { };
                float                   max_weight { };
                std::vector< uint32_t > victims { };
            };

            const float    weight     = m_bundles[ b ].weight;
            const uint32_t my_cascade = m_bundles[ b ].cascade ? m_bundles[ b ].cascade : m_next_cascade;

            std::optional< Option > best;

            for ( const auto pr : cands )
            {
                bool                    fixed_conflict { false };
                std::vector< uint32_t > victims;

                m_pregs[ pr.index( ) ].for_each_overlap( m_bundles[ b ].range,
                                                         [ & ]( const uint32_t owner, Segment )
                                                         {
                                                             if ( owner == fixed_owner ) fixed_conflict = true;
                                                             else if ( std::ranges::find( victims, owner ) == victims.end( ) )
                                                                 victims.push_back( owner );
                                                         } );

                if ( !fixed_conflict && victims.empty( ) )
                {
                    assign( b, pr );
                    return;
                }

                if ( fixed_conflict ) continue;

                float max_weight { 0.f };
                bool  evictable { true };
                for ( const auto v : victims )
                {
                    const float w = m_bundles[ v ].weight;
                    max_weight    = std::max( max_weight, w );
                    if ( w >= weight || m_bundles[ v ].cascade >= my_cascade ) evictable = false;
                }

                if ( evictable && ( !best || max_weight < best->max_weight ) ) best = Option { pr, max_weight, std::move( victims ) };
            }

            if ( best )
            {
                if ( m_bundles[ b ].cascade == 0 ) m_bundles[ b ].cascade = m_next_cascade++;

                for ( const auto v : best->victims )
                {
                    m_pregs[ best->preg.index( ) ].remove( v );
                    m_bundles[ v ].alloc   = Allocation { };
                    m_bundles[ v ].cascade = m_bundles[ b ].cascade;
                    enqueue( v );
                    ++m_evictions;
                }

                assign( b, best->preg );
                return;
            }

            const bool reads = std::ranges::any_of( m_bundles[ b ].mentions, &Mention::read );
            if ( m_bundles[ b ].minimal || !reads || !split( b, cands ) ) spill( b );
        }

        [[nodiscard]] bool split( const uint32_t b, const std::vector< codegen::PReg >& cands )
        {
            struct Region
            {
                codegen::PReg preg { };
                ProgPoint     from { };
                ProgPoint     to { };
                float         score { -1.f };
            };

            const auto&     bundle = m_bundles[ b ];
            const ProgPoint start  = bundle.range.start( );
            const ProgPoint end    = bundle.range.end( );

            Region best;

            for ( const auto pr : cands )
            {
                std::vector< Segment > busy;
                m_pregs[ pr.index( ) ].for_each_overlap( bundle.range,
                                                         [ & ]( uint32_t, const Segment s )
                                                         {
                                                             busy.push_back( s );
                                                         } );

                ProgPoint  cursor   = start;
                const auto consider = [ & ]( const ProgPoint g0, const ProgPoint g1 )
                {
                    if ( g0 >= g1 ) return;
                    const ProgPoint s0 = g0 == start ? start : m_cuts.next( g0 );
                    const ProgPoint s1 = g1 == end ? end : m_cuts.prev( g1 );
                    if ( s0 >= s1 || ( s0 == start && s1 == end ) ) return;

                    float score { 0.f };
                    bool  reads { false };
                    for ( const auto& m : bundle.mentions )
                        if ( m.point >= s0 && m.point < s1 )
                        {
                            score += m.weight;
                            reads |= m.read;
                        }

                    if ( reads && score > best.score ) best = Region { pr, s0, s1, score };
                };

                for ( const auto& [ from, to ] : busy )
                {
                    consider( cursor, from );
                    cursor = std::max( cursor, to );
                }
                consider( cursor, end );
            }

            if ( best.score < 0.f ) return false;

            uint32_t mid = b;
            if ( best.from > start )
            {
                mid = cut( b, best.from );
                enqueue( b );
            }
            if ( best.to < m_bundles[ mid ].range.end( ) ) enqueue( cut( mid, best.to ) );

            assign( mid, best.preg );
            return true;
        }

        [[nodiscard]] uint32_t cut( const uint32_t b, const ProgPoint p )
        {
            assert( m_cuts.can_cut( p ) && "[xiao::regalloc::BacktrackingAllocator::cut] split point is not a legal cut" );

            LiveBundle hi {
                .cls      = m_bundles[ b ].cls,
                .hints    = m_bundles[ b ].hints,
                .spillset = m_bundles[ b ].spillset,
                .cascade  = m_bundles[ b ].cascade,
            };

            auto&                lo = m_bundles[ b ];
            std::vector< Piece > lo_pieces;
            for ( auto& [ vreg, range ] : lo.pieces )
            {
                Piece low { vreg }, high { vreg };
                for ( const auto& s : range.segments )
                {
                    if ( s.to <= p ) low.range.segments.push_back( s );
                    else if ( s.from >= p )
                        high.range.segments.push_back( s );
                    else
                    {
                        low.range.segments.push_back( { s.from, p } );
                        high.range.segments.push_back( { p, s.to } );
                    }
                }
                if ( !low.range.empty( ) ) lo_pieces.push_back( std::move( low ) );
                if ( !high.range.empty( ) ) hi.pieces.push_back( std::move( high ) );
            }
            lo.pieces = std::move( lo_pieces );

            const auto split_at = std::ranges::lower_bound( lo.mentions, p, { }, &Mention::point );
            hi.mentions.assign( split_at, lo.mentions.end( ) );
            lo.mentions.erase( split_at, lo.mentions.end( ) );

            finish( lo );
            finish( hi );

            const auto id = static_cast< uint32_t >( m_bundles.size( ) );
            m_members[ hi.spillset ].push_back( id );
            m_bundles.push_back( std::move( hi ) );
            ++m_splits;
            return id;
        }

        std::array< std::vector< codegen::PReg >, 2 >          m_order;
        CutPoints                                              m_cuts;
        std::array< PRegMap, codegen::Reg::pinned_count >      m_pregs { };
        std::vector< LiveBundle >                              m_bundles { };
        std::vector< std::vector< uint32_t > >                 m_members { };
        std::vector< int32_t >                                 m_slot_of_spillset { };
        std::priority_queue< std::pair< uint32_t, uint32_t > > m_queue { };
        uint32_t                                               m_next_cascade { 1 };
        uint32_t                                               m_next_slot { 0 };
        uint32_t                                               m_evictions { 0 };
        uint32_t                                               m_splits { 0 };
    };
} // namespace xiao::regalloc
