#include "support.hh"

#include <xiao-object/module.hh>
#include <xiao/support/arena.hh>

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

using namespace xt;

namespace
{
    using Errs = std::vector< std::string >;

    template < typename Body >
    [[nodiscard]] Errs errors_of( const Type ret, const std::vector< Type >& params, Body&& body )
    {
        ir::IRModule irm;
        auto&        fn = *irm.get( irm.declare( "f", ret, std::span< const Type > { params } ) );
        {
            ir::IRBuilder b { fn };
            body( b );
            b.finalise( );
        }
        return verify( fn );
    }

    template < typename Body >
    [[nodiscard]] Errs entry_errors_of( const Type ret, const std::vector< Type >& params, Body&& body )
    {
        return errors_of( ret, params,
                          [ & ]( ir::IRBuilder& b )
                          {
                              b.switch_to_block( b.add_block( std::span< const Type > { params } ) );
                              body( b );
                          } );
    }

    struct Built
    {
        Jit             jit;
        module::FuncId  id { };
        ir::IRFunction* fn { };
    };

    template < typename Body >
    void build_unverified( Built& out, const Type ret, const std::vector< Type >& params, Body&& body )
    {
        out.id         = out.jit.declare( "f", ret, params );
        const auto ref = out.jit.irm.declare( "f", ret, std::span< const Type > { params } );
        out.fn         = out.jit.irm.get( ref );

        ir::IRBuilder b { *out.fn };
        b.switch_to_block( b.add_block( std::span< const Type > { params } ) );
        body( b );
        b.finalise( );
    }
} // namespace

TEST_CASE( "arena reuses every slab after reset", "[known][arena][!shouldfail]" )
{
    support::Arena arena { 4096 };
    for ( int i { 0 }; i < 3; ++i ) ( void ) arena.allocate( 4000, 8 );
    const auto slabs = arena.slab_count( );

    arena.reset( );
    for ( int i { 0 }; i < 3; ++i ) ( void ) arena.allocate( 4000, 8 );
    CHECK( arena.slab_count( ) == slabs );
}
