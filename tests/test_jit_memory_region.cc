#include "support.hh"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

using namespace xt;
using jit::Memory;
using jit::Protection;
using jit::Region;

namespace
{
    std::uintptr_t addr( const void* p )
    { return reinterpret_cast< std::uintptr_t >( p ); }

#if defined( _WIN32 )
    DWORD page_protection( const void* p )
    {
        MEMORY_BASIC_INFORMATION info { };
        REQUIRE( VirtualQuery( p, &info, sizeof info ) == sizeof info );
        return info.Protect;
    }
#endif

    std::byte* must( const std::expected< std::byte*, module::ModuleError >& r )
    {
        REQUIRE( r.has_value( ) );
        REQUIRE( *r != nullptr );
        return *r;
    }
} // namespace

TEST_CASE( "align_up rounds to the next multiple", "[memory]" )
{
    using jit::detail::align_up;
    CHECK( align_up( 0, 1 ) == 0 );
    CHECK( align_up( 5, 1 ) == 5 );
    CHECK( align_up( 0, 16 ) == 0 );
    CHECK( align_up( 1, 16 ) == 16 );
    CHECK( align_up( 15, 16 ) == 16 );
    CHECK( align_up( 16, 16 ) == 16 );
    CHECK( align_up( 17, 16 ) == 32 );
    CHECK( align_up( 17, 8 ) == 24 );
    CHECK( align_up( 4095, 4096 ) == 4096 );
    CHECK( align_up( 4097, 4096 ) == 8192 );
    CHECK( align_up( 65536, 65536 ) == 65536 );
    for ( std::uintptr_t v = 0; v < 300; v += 7 )
        for ( std::size_t a = 1; a <= 256; a <<= 1 )
        {
            const auto r = align_up( v, a );
            CHECK( r % a == 0 );
            CHECK( r >= v );
            CHECK( r - v < a );
        }
}

TEST_CASE( "os granularity is a power of two", "[memory]" )
{
    const auto g = jit::detail::os_granularity( );
    CHECK( g >= 4096 );
    CHECK( std::has_single_bit( g ) );
}

TEST_CASE( "region hands out consecutive committed ranges", "[memory]" )
{
    const auto  g = jit::detail::os_granularity( );
    Region      r { 8 * g };
    auto* const a = r.take( g );
    auto* const b = r.take( 2 * g );
    auto* const c = r.take( g );
    REQUIRE( a != nullptr );
    REQUIRE( b != nullptr );
    REQUIRE( c != nullptr );
    CHECK( b == a + g );
    CHECK( c == b + 2 * g );
    std::memset( a, 0x11, g );
    std::memset( b, 0x22, 2 * g );
    std::memset( c, 0x33, g );
    CHECK( a[ g - 1 ] == std::byte { 0x11 } );
    CHECK( b[ 0 ] == std::byte { 0x22 } );
    CHECK( c[ g - 1 ] == std::byte { 0x33 } );
}

TEST_CASE( "region size rounds up to the granularity", "[memory]" )
{
    const auto  g = jit::detail::os_granularity( );
    Region      r { 1 };
    auto* const p = r.take( g );
    REQUIRE( p != nullptr );
    CHECK( r.take( 1 ) == nullptr );
    CHECK( r.contains( p ) );
    CHECK( r.contains( p + g - 1 ) );
    CHECK_FALSE( r.contains( p + g ) );
}

TEST_CASE( "region refuses requests past its end", "[memory]" )
{
    const auto g = jit::detail::os_granularity( );
    Region     r { 2 * g };
    CHECK( r.take( 3 * g ) == nullptr );
    auto* const p = r.take( g );
    REQUIRE( p != nullptr );
    CHECK( r.take( 2 * g ) == nullptr );
    CHECK( r.take( g ) == p + g );
    CHECK( r.take( 1 ) == nullptr );
}

TEST_CASE( "region contains only its own range", "[memory]" )
{
    const auto  g = jit::detail::os_granularity( );
    Region      r { 4 * g };
    auto* const p = r.take( g );
    REQUIRE( p != nullptr );
    int on_stack = 0;
    CHECK( r.contains( p ) );
    CHECK( r.contains( p + 4 * g - 1 ) );
    CHECK_FALSE( r.contains( p + 4 * g ) );
    CHECK_FALSE( r.contains( p - 1 ) );
    CHECK_FALSE( r.contains( nullptr ) );
    CHECK_FALSE( r.contains( &on_stack ) );
}

TEST_CASE( "default region reserves a large range", "[memory]" )
{
    Region      r;
    const auto  g = jit::detail::os_granularity( );
    auto* const p = r.take( g );
    REQUIRE( p != nullptr );
    CHECK( r.contains( p + Region::default_size - 1 ) );
    CHECK_FALSE( r.contains( p + Region::default_size ) );
}

TEST_CASE( "memory allocations honour every power of two alignment", "[memory]" )
{
    for ( const bool use_region : { false, true } )
    {
        const auto owned  = use_region ? std::make_unique< Region >( std::size_t { 64 } << 20 ) : nullptr;
        auto*      region = owned.get( );
        {
            Memory                                              m { Protection::Writable, region };
            std::vector< std::pair< std::byte*, std::size_t > > got;
            for ( int round = 0; round < 3; ++round )
                for ( std::size_t a = 1; a <= 4096; a <<= 1 )
                {
                    const std::size_t size = 1 + ( a * 3 + static_cast< std::size_t >( round ) ) % 37;
                    auto* const       p    = must( m.allocate( size, a ) );
                    CHECK( addr( p ) % a == 0 );
                    std::memset( p, 0xAB, size );
                    got.push_back( { p, size } );
                    if ( region ) CHECK( region->contains( p ) );
                }
            std::ranges::sort( got );
            for ( std::size_t k = 1; k < got.size( ); ++k ) CHECK( got[ k - 1 ].first + got[ k - 1 ].second <= got[ k ].first );
        }
    }
}

TEST_CASE( "zero sized memory allocations are distinct", "[memory]" )
{
    Memory      m { Protection::Writable };
    auto* const a = must( m.allocate( 0, 1 ) );
    auto* const b = must( m.allocate( 0, 1 ) );
    auto* const c = must( m.allocate( 0, 8 ) );
    CHECK( a != b );
    CHECK( b != c );
    CHECK( b == a + 1 );
    CHECK( addr( c ) % 8 == 0 );
}

TEST_CASE( "consecutive memory allocations are packed", "[memory]" )
{
    Memory      m { Protection::Writable };
    auto* const a = must( m.allocate( 10, 1 ) );
    auto* const b = must( m.allocate( 6, 1 ) );
    auto* const c = must( m.allocate( 1, 16 ) );
    CHECK( b == a + 10 );
    CHECK( addr( c ) % 16 == 0 );
    CHECK( c >= b + 6 );
    CHECK( c < b + 6 + 16 );
    CHECK( m.block_count( ) == 1 );
}

TEST_CASE( "memory grows into new blocks when full", "[memory]" )
{
    Memory      m { Protection::Writable };
    auto* const a = must( m.allocate( Memory::min_block_size - 8, 1 ) );
    CHECK( m.block_count( ) == 1 );
    auto* const b = must( m.allocate( 64, 1 ) );
    CHECK( m.block_count( ) == 2 );
    std::memset( a, 1, Memory::min_block_size - 8 );
    std::memset( b, 2, 64 );
    CHECK( a[ 0 ] == std::byte { 1 } );
    CHECK( b[ 63 ] == std::byte { 2 } );
}

TEST_CASE( "memory serves allocations larger than a block", "[memory]" )
{
    for ( const bool use_region : { false, true } )
    {
        Region      region;
        Memory      m { Protection::Writable, use_region ? &region : nullptr };
        const auto  big = std::size_t { 3 } << 20;
        auto* const p   = must( m.allocate( big, 4096 ) );
        CHECK( addr( p ) % 4096 == 0 );
        std::memset( p, 0x5A, big );
        CHECK( p[ big - 1 ] == std::byte { 0x5A } );
        CHECK( m.block_count( ) == 1 );
        CHECK( region.contains( p ) == use_region );
        auto* const q = must( m.allocate( 16, 16 ) );
        CHECK( q != nullptr );
    }
}

TEST_CASE( "memory falls back to its own pages when the region is exhausted", "[memory]" )
{
    const auto  g = jit::detail::os_granularity( );
    Region      small { 1 };
    Memory      m { Protection::Writable, &small };
    auto* const p = must( m.allocate( 4 * g, 16 ) );
    CHECK_FALSE( small.contains( p ) );
    std::memset( p, 7, 4 * g );
    CHECK( p[ 4 * g - 1 ] == std::byte { 7 } );
}

TEST_CASE( "memory uses its region while there is room", "[memory]" )
{
    Region      region;
    Memory      code { Protection::Executable, &region };
    Memory      data { Protection::ReadOnly, &region };
    auto* const a = must( code.allocate( 32, 16 ) );
    auto* const b = must( data.allocate( 32, 1 ) );
    CHECK( region.contains( a ) );
    CHECK( region.contains( b ) );
    CHECK( a != b );
}

TEST_CASE( "memory reports its protection", "[memory]" )
{
    CHECK( Memory { }.protection( ) == Protection::Executable );
    CHECK( Memory { Protection::ReadOnly }.protection( ) == Protection::ReadOnly );
    CHECK( Memory { Protection::Writable }.protection( ) == Protection::Writable );
    CHECK( Memory { }.block_count( ) == 0 );
}

TEST_CASE( "executable memory runs code after finalise", "[memory]" )
{
    Memory        m { Protection::Executable };
    const uint8_t code[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };
    auto* const   p      = must( m.allocate( sizeof code, 16 ) );
    std::memcpy( p, code, sizeof code );
    REQUIRE( m.finalise( ).has_value( ) );
    CHECK( reinterpret_cast< int ( * )( ) >( p )( ) == 42 );
#if defined( _WIN32 )
    CHECK( page_protection( p ) == PAGE_EXECUTE_READ );
#endif
}

TEST_CASE( "finalised memory starts a new block for later allocations", "[memory]" )
{
    for ( const auto prot : { Protection::Executable, Protection::ReadOnly } )
    {
        Memory      m { prot };
        auto* const a = must( m.allocate( 8, 8 ) );
        REQUIRE( m.finalise( ).has_value( ) );
        auto* const b = must( m.allocate( 8, 8 ) );
        CHECK( m.block_count( ) == 2 );
        CHECK( b != a + 8 );
        std::memset( b, 1, 8 );
        REQUIRE( m.finalise( ).has_value( ) );
        REQUIRE( m.finalise( ).has_value( ) );
        CHECK( m.block_count( ) == 2 );
    }
}

TEST_CASE( "read only memory is protected after finalise", "[memory]" )
{
    Memory      m { Protection::ReadOnly };
    auto* const p = must( m.allocate( 16, 1 ) );
    std::memset( p, 9, 16 );
#if defined( _WIN32 )
    CHECK( page_protection( p ) == PAGE_READWRITE );
#endif
    REQUIRE( m.finalise( ).has_value( ) );
    CHECK( p[ 15 ] == std::byte { 9 } );
#if defined( _WIN32 )
    CHECK( page_protection( p ) == PAGE_READONLY );
#endif
}

TEST_CASE( "writable memory stays writable and keeps its block after finalise", "[memory]" )
{
    Memory      m { Protection::Writable };
    auto* const a = must( m.allocate( 8, 8 ) );
    REQUIRE( m.finalise( ).has_value( ) );
    std::memset( a, 3, 8 );
    CHECK( a[ 7 ] == std::byte { 3 } );
#if defined( _WIN32 )
    CHECK( page_protection( a ) == PAGE_READWRITE );
#endif
    auto* const b = must( m.allocate( 8, 8 ) );
    CHECK( b == a + 8 );
    CHECK( m.block_count( ) == 1 );
}

TEST_CASE( "finalise on empty memory succeeds", "[memory]" )
{
    for ( const auto prot : { Protection::Executable, Protection::ReadOnly, Protection::Writable } )
    {
        Memory m { prot };
        CHECK( m.finalise( ).has_value( ) );
        CHECK( m.block_count( ) == 0 );
    }
}

TEST_CASE( "memory move construction transfers blocks", "[memory]" )
{
    Memory      a { Protection::ReadOnly };
    auto* const p = must( a.allocate( 100, 4 ) );
    std::memset( p, 4, 100 );
    Memory b { std::move( a ) };
    CHECK( b.block_count( ) == 1 );
    CHECK( b.protection( ) == Protection::ReadOnly );
    CHECK( a.block_count( ) == 0 );
    CHECK( p[ 99 ] == std::byte { 4 } );
    auto* const q = must( b.allocate( 4, 4 ) );
    CHECK( q == p + 100 );
}

TEST_CASE( "memory move assignment releases the old blocks and takes the new", "[memory]" )
{
    Memory      a { Protection::Writable };
    Memory      b { Protection::Executable };
    auto* const p = must( a.allocate( 16, 16 ) );
    ( void ) must( b.allocate( 16, 16 ) );
    ( void ) must( b.allocate( Memory::min_block_size, 16 ) );
    CHECK( b.block_count( ) == 2 );
    b = std::move( a );
    CHECK( b.block_count( ) == 1 );
    CHECK( b.protection( ) == Protection::Writable );
    CHECK( a.block_count( ) == 0 );
    std::memset( p, 1, 16 );
    auto* const q = must( b.allocate( 16, 16 ) );
    CHECK( q == p + 16 );
    auto& self = b;
    b          = std::move( self );
    CHECK( b.block_count( ) == 1 );
}
