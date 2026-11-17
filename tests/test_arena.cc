#include "support.hh"

#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace xt;
using support::Arena;
using support::Slab;

namespace
{
    [[nodiscard]] bool aligned_to( const void* p, const std::size_t align )
    { return reinterpret_cast< std::uintptr_t >( p ) % align == 0; }

    struct Pair
    {
        int a { };
        int b { };

        Pair( ) = default;

        Pair( const int x, const int y ) : a { x }, b { y } { }
    };

    struct alignas( 64 ) Wide
    {
        uint8_t bytes[ 64 ] { };
    };
} // namespace

TEST_CASE( "arena default slab size is sixteen kilobytes", "[arena]" )
{ STATIC_REQUIRE( Arena::default_slab_size == 16384 ); }

TEST_CASE( "arena starts with no slabs and no usage", "[arena]" )
{
    const Arena a;
    CHECK( a.slab_count( ) == 0 );
    CHECK( a.bytes_allocated( ) == 0 );
    CHECK( a.bytes_used( ) == 0 );
}

TEST_CASE( "arena zero sized allocation returns null and allocates nothing", "[arena]" )
{
    Arena      a;
    const auto align = GENERATE( 1u, 8u, 64u, 4096u );
    CHECK( a.allocate( 0, align ) == nullptr );
    CHECK( a.slab_count( ) == 0 );
    CHECK( a.bytes_used( ) == 0 );
}

TEST_CASE( "arena first allocation creates one slab of the configured size", "[arena]" )
{
    const std::size_t size = GENERATE( 64uz, 128uz, 1000uz, 4096uz, Arena::default_slab_size );
    Arena             a { size };
    CHECK( a.allocate( 8, 8 ) != nullptr );
    CHECK( a.slab_count( ) == 1 );
    CHECK( a.bytes_allocated( ) == size );
}

TEST_CASE( "arena allocation honours every power of two alignment", "[arena]" )
{
    const std::size_t align = GENERATE( 1uz, 2uz, 4uz, 8uz, 16uz, 32uz, 64uz, 128uz, 256uz, 512uz, 1024uz, 2048uz, 4096uz );
    Arena             a;
    for ( std::size_t i { 0 }; i < 8; ++i )
    {
        CHECK( a.allocate( 1 + i * 3, 1 ) != nullptr );
        void* const p = a.allocate( 24 + i, align );
        REQUIRE( p != nullptr );
        CHECK( aligned_to( p, align ) );
    }
}

TEST_CASE( "arena alignment larger than the slab size still succeeds", "[arena]" )
{
    Arena       a { 64 };
    void* const p = a.allocate( 16, 1024 );
    REQUIRE( p != nullptr );
    CHECK( aligned_to( p, 1024 ) );
    CHECK( a.bytes_allocated( ) >= 16 + 1024 );
}

TEST_CASE( "arena allocations larger than the slab size get a dedicated slab", "[arena]" )
{
    Arena       a { 64 };
    void* const p = a.allocate( 1000, 8 );
    REQUIRE( p != nullptr );
    CHECK( aligned_to( p, 8 ) );
    CHECK( a.slab_count( ) == 1 );
    CHECK( a.bytes_allocated( ) == 1008 );
    CHECK( a.bytes_used( ) >= 1000 );
    std::memset( p, 0x5A, 1000 );
    CHECK( static_cast< const uint8_t* >( p )[ 999 ] == 0x5A );
}

TEST_CASE( "arena exact fill of a slab stays in that slab", "[arena]" )
{
    Arena a { 64 };
    CHECK( a.allocate( 32, 1 ) != nullptr );
    CHECK( a.allocate( 32, 1 ) != nullptr );
    CHECK( a.slab_count( ) == 1 );
    CHECK( a.bytes_used( ) == 64 );
    CHECK( a.allocate( 1, 1 ) != nullptr );
    CHECK( a.slab_count( ) == 2 );
    CHECK( a.bytes_allocated( ) == 128 );
    CHECK( a.bytes_used( ) == 65 );
}

TEST_CASE( "arena overflowing a slab starts a new one", "[arena]" )
{
    Arena a { 64 };
    CHECK( a.allocate( 40, 1 ) != nullptr );
    CHECK( a.allocate( 40, 1 ) != nullptr );
    CHECK( a.slab_count( ) == 2 );
    CHECK( a.bytes_allocated( ) == 128 );
    CHECK( a.bytes_used( ) == 80 );
}

TEST_CASE( "arena byte aligned allocations are contiguous", "[arena]" )
{
    Arena a;
    auto* p1 = static_cast< std::byte* >( a.allocate( 3, 1 ) );
    auto* p2 = static_cast< std::byte* >( a.allocate( 5, 1 ) );
    auto* p3 = static_cast< std::byte* >( a.allocate( 1, 1 ) );
    CHECK( p2 == p1 + 3 );
    CHECK( p3 == p2 + 5 );
    CHECK( a.bytes_used( ) == 9 );
}

TEST_CASE( "arena usage includes alignment padding", "[arena]" )
{
    Arena a;
    CHECK( a.allocate( 1, 1 ) != nullptr );
    CHECK( a.allocate( 8, 8 ) != nullptr );
    CHECK( a.bytes_used( ) == 16 );
    CHECK( a.allocate( 1, 1 ) != nullptr );
    CHECK( a.allocate( 4, 16 ) != nullptr );
    CHECK( a.bytes_used( ) == 36 );
}

TEST_CASE( "arena allocations never overlap", "[arena]" )
{
    Arena                                             a { 256 };
    std::vector< std::pair< uint8_t*, std::size_t > > blocks;
    for ( std::size_t i { 0 }; i < 200; ++i )
    {
        const auto size  = 1 + ( i * 37 ) % 97;
        const auto align = std::size_t { 1 } << ( i % 6 );
        auto*      p     = static_cast< uint8_t* >( a.allocate( size, align ) );
        REQUIRE( p != nullptr );
        CHECK( aligned_to( p, align ) );
        std::memset( p, static_cast< int >( i & 0xFF ), size );
        blocks.emplace_back( p, size );
    }
    for ( std::size_t i { 0 }; i < blocks.size( ); ++i )
        for ( std::size_t k { 0 }; k < blocks[ i ].second; ++k ) REQUIRE( blocks[ i ].first[ k ] == static_cast< uint8_t >( i & 0xFF ) );
    CHECK( a.slab_count( ) > 1 );
}

TEST_CASE( "arena alloc constructs objects in place", "[arena]" )
{
    Arena a;

    auto* pair = a.alloc< Pair >( 3, 4 );
    REQUIRE( pair != nullptr );
    CHECK( pair->a == 3 );
    CHECK( pair->b == 4 );
    CHECK( aligned_to( pair, alignof( Pair ) ) );

    auto* def = a.alloc< Pair >( );
    CHECK( def->a == 0 );
    CHECK( def->b == 0 );

    CHECK( *a.alloc< uint64_t >( ) == 0 );
    CHECK( *a.alloc< uint64_t >( uint64_t { 42 } ) == 42 );
    CHECK( *a.alloc< int8_t >( int8_t { -5 } ) == -5 );

    auto* wide = a.alloc< Wide >( );
    CHECK( aligned_to( wide, 64 ) );
    CHECK( wide->bytes[ 63 ] == 0 );
}

TEST_CASE( "arena alloc_array value initialises its elements", "[arena]" )
{
    Arena       a { 256 };
    void* const raw = a.allocate( 128, 16 );
    std::memset( raw, 0xAB, 128 );
    a.reset( );

    const auto arr = a.alloc_array< uint32_t >( 32 );
    REQUIRE( arr.size( ) == 32 );
    CHECK( static_cast< void* >( arr.data( ) ) == raw );
    for ( const auto v : arr ) CHECK( v == 0 );
}

TEST_CASE( "arena alloc_array_uninit leaves memory untouched", "[arena]" )
{
    Arena       a { 256 };
    void* const raw = a.allocate( 64, 16 );
    std::memset( raw, 0xAB, 64 );
    a.reset( );

    const auto arr = a.alloc_array_uninit< uint8_t >( 64 );
    REQUIRE( arr.size( ) == 64 );
    CHECK( static_cast< void* >( arr.data( ) ) == raw );
    for ( const auto v : arr ) CHECK( v == 0xAB );
}

TEST_CASE( "arena array allocations respect element alignment", "[arena]" )
{
    Arena a;
    CHECK( a.allocate( 1, 1 ) != nullptr );
    const auto u64 = a.alloc_array< uint64_t >( 3 );
    CHECK( aligned_to( u64.data( ), alignof( uint64_t ) ) );
    CHECK( a.allocate( 1, 1 ) != nullptr );
    const auto wide = a.alloc_array_uninit< Wide >( 2 );
    CHECK( aligned_to( wide.data( ), 64 ) );
    CHECK( wide.size( ) == 2 );
}

TEST_CASE( "arena zero length arrays are empty and allocate nothing", "[arena]" )
{
    Arena a;
    CHECK( a.alloc_array< uint64_t >( 0 ).empty( ) );
    CHECK( a.alloc_array_uninit< uint64_t >( 0 ).empty( ) );
    CHECK( a.alloc_array< uint64_t >( 0 ).data( ) == nullptr );
    CHECK( a.slab_count( ) == 0 );
}

TEST_CASE( "arena alloc_str copies the contents", "[arena]" )
{
    Arena       a;
    std::string src { "hello, world" };
    const auto  view = a.alloc_str( src );
    CHECK( view == "hello, world" );
    CHECK( view.data( ) != src.data( ) );
    src[ 0 ] = 'J';
    CHECK( view == "hello, world" );
    CHECK( a.bytes_used( ) == src.size( ) );
}

TEST_CASE( "arena alloc_str of an empty string allocates nothing", "[arena]" )
{
    Arena      a;
    const auto view = a.alloc_str( "" );
    CHECK( view.empty( ) );
    CHECK( view.data( ) == nullptr );
    CHECK( a.slab_count( ) == 0 );
}

TEST_CASE( "arena alloc_str handles strings longer than a slab", "[arena]" )
{
    Arena             a { 16 };
    const std::string big( 1000, 'x' );
    const auto        view = a.alloc_str( big );
    CHECK( view == big );
    const auto small = a.alloc_str( "abc" );
    CHECK( small == "abc" );
    CHECK( view == big );
}

TEST_CASE( "arena alloc_str copies embedded nul bytes", "[arena]" )
{
    Arena                  a;
    const std::string_view src { "a\0b\0c", 5 };
    const auto             view = a.alloc_str( src );
    CHECK( view.size( ) == 5 );
    CHECK( view == src );
}

TEST_CASE( "arena mark on an empty arena points at nothing", "[arena]" )
{
    const Arena a;
    const auto  c = a.mark( );
    CHECK( c.slab == 0 );
    CHECK( c.cursor == nullptr );
}

TEST_CASE( "arena mark records the last slab and its cursor", "[arena]" )
{
    Arena a { 64 };
    CHECK( a.allocate( 40, 1 ) != nullptr );
    CHECK( a.allocate( 40, 1 ) != nullptr );
    const auto c = a.mark( );
    CHECK( c.slab == 1 );
    CHECK( c.cursor != nullptr );
}

TEST_CASE( "arena rewind discards allocations within one slab", "[arena]" )
{
    Arena a;
    CHECK( a.allocate( 16, 1 ) != nullptr );
    const auto c    = a.mark( );
    const auto used = a.bytes_used( );
    void*      p    = a.allocate( 100, 1 );
    CHECK( a.allocate( 7, 1 ) != nullptr );
    a.rewind( c );
    CHECK( a.bytes_used( ) == used );
    CHECK( a.allocate( 100, 1 ) == p );
}

TEST_CASE( "arena rewind across slabs resets later slabs but keeps them", "[arena]" )
{
    Arena a { 64 };
    CHECK( a.allocate( 32, 1 ) != nullptr );
    const auto c = a.mark( );
    CHECK( a.allocate( 48, 1 ) != nullptr );
    CHECK( a.allocate( 48, 1 ) != nullptr );
    REQUIRE( a.slab_count( ) == 3 );
    const auto allocated = a.bytes_allocated( );

    a.rewind( c );
    CHECK( a.bytes_used( ) == 32 );
    CHECK( a.slab_count( ) == 3 );
    CHECK( a.bytes_allocated( ) == allocated );
}

TEST_CASE( "arena nested marks rewind independently", "[arena]" )
{
    Arena a { 128 };
    CHECK( a.allocate( 10, 1 ) != nullptr );
    const auto outer = a.mark( );
    CHECK( a.allocate( 20, 1 ) != nullptr );
    const auto inner = a.mark( );
    CHECK( a.allocate( 30, 1 ) != nullptr );
    CHECK( a.bytes_used( ) == 60 );
    a.rewind( inner );
    CHECK( a.bytes_used( ) == 30 );
    a.rewind( outer );
    CHECK( a.bytes_used( ) == 10 );
}

TEST_CASE( "arena rewind on an arena with no slabs is a no-op", "[arena]" )
{
    Arena      a;
    const auto c = a.mark( );
    a.rewind( c );
    CHECK( a.slab_count( ) == 0 );
    CHECK( a.bytes_used( ) == 0 );
}

TEST_CASE( "arena rewind to a mark taken before any allocation discards everything", "[arena]" )
{
    Arena      a;
    const auto c = a.mark( );
    CHECK( a.allocate( 64, 8 ) != nullptr );
    a.rewind( c );
    CHECK( a.bytes_used( ) == 0 );
}

TEST_CASE( "arena reset keeps slabs but frees their space", "[arena]" )
{
    Arena a { 64 };
    CHECK( a.allocate( 48, 1 ) != nullptr );
    CHECK( a.allocate( 48, 1 ) != nullptr );
    CHECK( a.allocate( 200, 8 ) != nullptr );
    const auto slabs     = a.slab_count( );
    const auto allocated = a.bytes_allocated( );

    a.reset( );
    CHECK( a.slab_count( ) == slabs );
    CHECK( a.bytes_allocated( ) == allocated );
    CHECK( a.bytes_used( ) == 0 );
    CHECK( a.allocate( 8, 8 ) != nullptr );
    CHECK( a.slab_count( ) == slabs );
}

TEST_CASE( "arena release drops every slab and can allocate again", "[arena]" )
{
    Arena a { 64 };
    CHECK( a.allocate( 48, 1 ) != nullptr );
    CHECK( a.allocate( 48, 1 ) != nullptr );
    a.release( );
    CHECK( a.slab_count( ) == 0 );
    CHECK( a.bytes_allocated( ) == 0 );
    CHECK( a.bytes_used( ) == 0 );
    CHECK( a.allocate( 8, 8 ) != nullptr );
    CHECK( a.slab_count( ) == 1 );
    CHECK( a.bytes_allocated( ) == 64 );
}

TEST_CASE( "arena move construction transfers slabs and slab size", "[arena]" )
{
    Arena a { 128 };
    auto* p = static_cast< uint8_t* >( a.allocate( 16, 8 ) );
    std::memset( p, 0x11, 16 );

    Arena b { std::move( a ) };
    CHECK( a.slab_count( ) == 0 );
    CHECK( b.slab_count( ) == 1 );
    CHECK( b.bytes_used( ) >= 16 );
    CHECK( p[ 15 ] == 0x11 );

    CHECK( b.allocate( 100, 1 ) != nullptr );
    CHECK( b.slab_count( ) == 1 );
    CHECK( b.allocate( 100, 1 ) != nullptr );
    CHECK( b.slab_count( ) == 2 );
    CHECK( b.bytes_allocated( ) == 256 );
}

TEST_CASE( "arena move assignment transfers slabs", "[arena]" )
{
    Arena a { 128 };
    auto* p = static_cast< uint8_t* >( a.allocate( 32, 8 ) );
    std::memset( p, 0x22, 32 );

    Arena b { 64 };
    CHECK( b.allocate( 8, 8 ) != nullptr );
    b = std::move( a );
    CHECK( b.slab_count( ) == 1 );
    CHECK( b.bytes_allocated( ) == 128 );
    CHECK( p[ 31 ] == 0x22 );
}

TEST_CASE( "arena slab tracks its remaining space", "[arena]" )
{
    Slab s { 100 };
    CHECK( s.remaining( ) == 100 );
    CHECK( s.cursor == s.data.get( ) );
    CHECK( s.end - s.data.get( ) == 100 );
    s.cursor += 30;
    CHECK( s.remaining( ) == 70 );
    s.cursor = s.end;
    CHECK( s.remaining( ) == 0 );
}

TEST_CASE( "arena tiny slab size still serves every request", "[arena]" )
{
    Arena a { 1 };
    for ( std::size_t i { 1 }; i <= 32; ++i )
    {
        auto* p = static_cast< uint8_t* >( a.allocate( i, 8 ) );
        REQUIRE( p != nullptr );
        CHECK( aligned_to( p, 8 ) );
        std::memset( p, 0x7F, i );
    }
    CHECK( a.slab_count( ) > 1 );
}

TEST_CASE( "arena usage never exceeds allocation", "[arena]" )
{
    Arena a { 512 };
    for ( std::size_t i { 0 }; i < 500; ++i )
    {
        CHECK( a.allocate( 1 + i % 64, std::size_t { 1 } << ( i % 5 ) ) != nullptr );
        REQUIRE( a.bytes_used( ) <= a.bytes_allocated( ) );
    }
}
