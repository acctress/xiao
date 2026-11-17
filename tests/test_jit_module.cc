#include "support.hh"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

using namespace xt;
using module::Linkage;
using module::ModuleError;

namespace
{
    int64_t h_first( const int64_t x )
    { return x + 1; }

    int64_t h_second( const int64_t x )
    { return x + 2; }

    int64_t h_third( const int64_t x )
    { return x + 3; }

    module::Signature sig( const std::vector< Type >& params, const Type ret )
    { return module::Signature { .params = params, .ret = ret, .cc = codegen::host_call_conv( ) }; }

    ir::IRFunction& simple_fn( ir::IRModule& irm, const std::string_view name, const int64_t value )
    {
        const auto    ref = irm.declare( name, Type::i64, { } );
        auto&         fn  = *irm.get( ref );
        ir::IRBuilder b { fn };
        b.switch_to_block( b.add_block( { } ) );
        b.ret( b.iconst( Type::i64, static_cast< uint64_t >( value ) ) );
        b.finalise( );
        return fn;
    }

    module::FuncId caller_of( Jit& j, const std::string_view name, const module::FuncId callee )
    {
        return j.define( name, Type::i64, { Type::i64 },
                         [ & ]( ir::IRBuilder& b, const Args& a )
                         {
                             b.ret( b.call( j.mod.declare_func_in_func( callee, b ), Args { a[ 0 ] } ) );
                         } );
    }
}

TEST_CASE( "declarations assign sequential ids and record names", "[module]" )
{
    module::ModuleDeclarations d;
    const auto                 f0 = d.declare_function( "f0", Linkage::Export, sig( { Type::i64 }, Type::i64 ) );
    const auto                 f1 = d.declare_function( "f1", Linkage::Local, sig( { }, Type::unit ) );
    const auto                 d0 = d.declare_data( "d0", Linkage::Import, false );
    const auto                 d1 = d.declare_data( "d1", Linkage::Export, true );
    REQUIRE( f0.has_value( ) );
    REQUIRE( f1.has_value( ) );
    REQUIRE( d0.has_value( ) );
    REQUIRE( d1.has_value( ) );
    CHECK( f0->id == 0 );
    CHECK( f1->id == 1 );
    CHECK( d0->id == 0 );
    CHECK( d1->id == 1 );
    CHECK( d.size( ) == 2 );
    CHECK( d.data_count( ) == 2 );

    CHECK( d.get( *f0 ).name == "f0" );
    CHECK( d.get( *f0 ).linkage == Linkage::Export );
    CHECK( d.get( *f0 ).sig == sig( { Type::i64 }, Type::i64 ) );
    CHECK( d.get( *f1 ).linkage == Linkage::Local );
    CHECK( d.get( *d0 ).name == "d0" );
    CHECK_FALSE( d.get( *d0 ).writable );
    CHECK( d.get( *d1 ).writable );
    CHECK( d.get( *d1 ).linkage == Linkage::Export );
}

TEST_CASE( "get_name finds functions and data", "[module]" )
{
    module::ModuleDeclarations d;
    const auto                 f = *d.declare_function( "fn", Linkage::Local, sig( { }, Type::i64 ) );
    const auto                 x = *d.declare_data( "dat", Linkage::Local, true );

    const auto nf = d.get_name( "fn" );
    REQUIRE( nf.has_value( ) );
    REQUIRE( std::holds_alternative< module::FuncId >( *nf ) );
    CHECK( std::get< module::FuncId >( *nf ) == f );

    const auto nd = d.get_name( "dat" );
    REQUIRE( nd.has_value( ) );
    REQUIRE( std::holds_alternative< module::DataId >( *nd ) );
    CHECK( std::get< module::DataId >( *nd ) == x );

    CHECK_FALSE( d.get_name( "missing" ).has_value( ) );
    CHECK_FALSE( d.get_name( "" ).has_value( ) );
    CHECK_FALSE( d.get_name( "FN" ).has_value( ) );
}

TEST_CASE( "redeclaring a function with the same signature returns the same id", "[module]" )
{
    module::ModuleDeclarations d;
    const auto                 a = d.declare_function( "f", Linkage::Import, sig( { Type::i32, Type::ptr }, Type::i8 ) );
    const auto                 b = d.declare_function( "f", Linkage::Import, sig( { Type::i32, Type::ptr }, Type::i8 ) );
    REQUIRE( a.has_value( ) );
    REQUIRE( b.has_value( ) );
    CHECK( *a == *b );
    CHECK( d.size( ) == 1 );
}

TEST_CASE( "linkage merge table", "[module]" )
{
    struct Row
    {
        Linkage a, b;
        bool    ok;
        Linkage want;
    };

    const Row rows[] = {
        {Linkage::Import, Linkage::Import,  true, Linkage::Import},
        {Linkage::Import,  Linkage::Local,  true,  Linkage::Local},
        {Linkage::Import, Linkage::Export,  true, Linkage::Export},
        { Linkage::Local, Linkage::Import,  true,  Linkage::Local},
        { Linkage::Local,  Linkage::Local,  true,  Linkage::Local},
        { Linkage::Local, Linkage::Export, false,  Linkage::Local},
        {Linkage::Export, Linkage::Import,  true, Linkage::Export},
        {Linkage::Export,  Linkage::Local, false,  Linkage::Local},
        {Linkage::Export, Linkage::Export,  true, Linkage::Export},
    };
    for ( const auto& r : rows )
    {
        const auto m = module::ModuleDeclarations::merge( r.a, r.b );
        CHECK( m.has_value( ) == r.ok );
        if ( r.ok ) CHECK( *m == r.want );
        else
            CHECK( m.error( ) == ModuleError::IncompatibleDeclaration );
    }
}

TEST_CASE( "redeclaring a function merges linkage", "[module]" )
{
    module::ModuleDeclarations d;
    const auto                 s  = sig( { }, Type::i64 );
    const auto                 id = *d.declare_function( "f", Linkage::Import, s );
    CHECK( d.declare_function( "f", Linkage::Local, s ) == id );
    CHECK( d.get( id ).linkage == Linkage::Local );
    CHECK( d.declare_function( "f", Linkage::Import, s ) == id );
    CHECK( d.get( id ).linkage == Linkage::Local );
    const auto bad = d.declare_function( "f", Linkage::Export, s );
    REQUIRE_FALSE( bad.has_value( ) );
    CHECK( bad.error( ) == ModuleError::IncompatibleDeclaration );
    CHECK( d.get( id ).linkage == Linkage::Local );
}

TEST_CASE( "redeclaring a function with a different signature fails", "[module]" )
{
    module::ModuleDeclarations d;
    ( void ) d.declare_function( "f", Linkage::Import, sig( { Type::i64 }, Type::i64 ) );
    const module::Signature others[] = {
        sig( { Type::i32 },
         Type::i64 ),
        sig( { Type::i64 },
         Type::i32 ),
        sig( { },
         Type::i64 ),
        sig( { Type::i64, Type::i64 },
         Type::i64 ),
        sig( { Type::i64 },
         Type::unit ),
        module::Signature { .params = { Type::i64 }, .ret = Type::i64, .cc = codegen::CallConv::SystemV },
        module::Signature { .params = { Type::i64 }, .ret = Type::i64, .cc = codegen::CallConv::WindowsFastcall },
    };
    for ( const auto& o : others )
    {
        if ( o == sig( { Type::i64 }, Type::i64 ) ) continue;
        const auto r = d.declare_function( "f", Linkage::Import, o );
        REQUIRE_FALSE( r.has_value( ) );
        CHECK( r.error( ) == ModuleError::IncompatibleDeclaration );
    }
    CHECK( d.size( ) == 1 );
}

TEST_CASE( "function and data names clash in both orders", "[module]" )
{
    module::ModuleDeclarations d;
    ( void ) d.declare_function( "a", Linkage::Import, sig( { }, Type::i64 ) );
    const auto r1 = d.declare_data( "a", Linkage::Import, false );
    REQUIRE_FALSE( r1.has_value( ) );
    CHECK( r1.error( ) == ModuleError::IncompatibleDeclaration );

    ( void ) d.declare_data( "b", Linkage::Import, false );
    const auto r2 = d.declare_function( "b", Linkage::Import, sig( { }, Type::i64 ) );
    REQUIRE_FALSE( r2.has_value( ) );
    CHECK( r2.error( ) == ModuleError::IncompatibleDeclaration );
    CHECK( d.size( ) == 1 );
    CHECK( d.data_count( ) == 1 );
}

TEST_CASE( "redeclaring data merges linkage and ors writability", "[module]" )
{
    module::ModuleDeclarations d;
    const auto                 id = *d.declare_data( "x", Linkage::Import, false );
    CHECK_FALSE( d.get( id ).writable );
    CHECK( d.declare_data( "x", Linkage::Export, true ) == id );
    CHECK( d.get( id ).writable );
    CHECK( d.get( id ).linkage == Linkage::Export );
    CHECK( d.declare_data( "x", Linkage::Import, false ) == id );
    CHECK( d.get( id ).writable );
    CHECK( d.get( id ).linkage == Linkage::Export );
    const auto bad = d.declare_data( "x", Linkage::Local, false );
    REQUIRE_FALSE( bad.has_value( ) );
    CHECK( bad.error( ) == ModuleError::IncompatibleDeclaration );
    CHECK( d.data_count( ) == 1 );
}

TEST_CASE( "signature_of copies an ir function signature", "[module]" )
{
    ir::IRModule irm;
    const auto   r1 = irm.declare( "a", Type::i32, { Type::i64, Type::ptr, Type::i8 }, codegen::CallConv::SystemV );
    const auto   r2 = irm.declare( "b", Type::unit, { }, codegen::CallConv::WindowsFastcall );
    const auto   r3 = irm.declare( "c", Type::i64, { Type::i16 } );
    CHECK( module::signature_of( *irm.get( r1 ) )
           == module::Signature {
               .params = { Type::i64, Type::ptr, Type::i8 },
               .ret    = Type::i32,
               .cc     = codegen::CallConv::SystemV
    } );
    CHECK( module::signature_of( *irm.get( r2 ) )
           == module::Signature { .params = { }, .ret = Type::unit, .cc = codegen::CallConv::WindowsFastcall } );
    CHECK( module::signature_of( *irm.get( r3 ) ) == sig( { Type::i16 }, Type::i64 ) );
}

TEST_CASE( "default signature is a unit function in the host convention", "[module]" )
{
    const module::Signature s;
    CHECK( s.params.empty( ) );
    CHECK( s.ret == Type::unit );
    CHECK( s.cc == codegen::host_call_conv( ) );
}

TEST_CASE( "declare_func_in_func imports with the declaration signature and colocation", "[module]" )
{
    Jit        j;
    const auto imp = j.import( "imp", Type::i32, { Type::i64, Type::ptr } );
    const auto loc = j.declare( "loc", Type::i64, { }, Linkage::Local );
    const auto exp = j.declare( "exp", Type::unit, { Type::i8 }, Linkage::Export );

    const auto    ref = j.irm.declare( "host", Type::unit, { } );
    auto&         fn  = *j.irm.get( ref );
    ir::IRBuilder b { fn };
    const auto    r0 = j.mod.declare_func_in_func( imp, b );
    const auto    r1 = j.mod.declare_func_in_func( loc, b );
    const auto    r2 = j.mod.declare_func_in_func( exp, b );
    b.finalise( );

    CHECK( r0.id == 0 );
    CHECK( r1.id == 1 );
    CHECK( r2.id == 2 );
    REQUIRE( fn.ext_funcs.size( ) == 3 );
    CHECK( fn.ext_funcs[ 0 ].name == ir::ExternalName { ir::extns::function, imp.id } );
    CHECK_FALSE( fn.ext_funcs[ 0 ].colocated );
    CHECK( fn.ext_funcs[ 0 ].ret == Type::i32 );
    CHECK( fn.ext_funcs[ 0 ].params_count == 2 );
    CHECK( fn.ext_param_types[ fn.ext_funcs[ 0 ].params_offset ] == Type::i64 );
    CHECK( fn.ext_param_types[ fn.ext_funcs[ 0 ].params_offset + 1 ] == Type::ptr );
    CHECK( fn.ext_funcs[ 0 ].cc == codegen::host_call_conv( ) );
    CHECK( fn.ext_funcs[ 1 ].name == ir::ExternalName { ir::extns::function, loc.id } );
    CHECK( fn.ext_funcs[ 1 ].colocated );
    CHECK( fn.ext_funcs[ 1 ].params_count == 0 );
    CHECK( fn.ext_funcs[ 2 ].colocated );
    CHECK( fn.ext_funcs[ 2 ].ret == Type::unit );
    CHECK( fn.ext_param_types[ fn.ext_funcs[ 2 ].params_offset ] == Type::i8 );
}

TEST_CASE( "declare_data_in_func creates a global value with colocation", "[module]" )
{
    Jit        j;
    const auto imp = *j.mod.declare_data( "imp", Linkage::Import, false );
    const auto loc = *j.mod.declare_data( "loc", Linkage::Local, true );
    const auto exp = *j.mod.declare_data( "exp", Linkage::Export, false );

    const auto    ref = j.irm.declare( "host", Type::unit, { } );
    auto&         fn  = *j.irm.get( ref );
    ir::IRBuilder b { fn };
    const auto    g0 = j.mod.declare_data_in_func( imp, b );
    const auto    g1 = j.mod.declare_data_in_func( loc, b );
    const auto    g2 = j.mod.declare_data_in_func( exp, b );
    b.finalise( );

    CHECK( g0.id == 0 );
    CHECK( g1.id == 1 );
    CHECK( g2.id == 2 );
    REQUIRE( fn.global_values.size( ) == 3 );
    CHECK( fn.global_values[ 0 ].name == ir::ExternalName { ir::extns::data, imp.id } );
    CHECK_FALSE( fn.global_values[ 0 ].colocated );
    CHECK( fn.global_values[ 0 ].offset == 0 );
    CHECK( fn.global_values[ 1 ].name == ir::ExternalName { ir::extns::data, loc.id } );
    CHECK( fn.global_values[ 1 ].colocated );
    CHECK( fn.global_values[ 2 ].colocated );
}

TEST_CASE( "declare_func_in_data and declare_data_in_data use the declaration ids", "[module]" )
{
    Jit                     j;
    const auto              f = j.declare( "f", Type::i64, { } );
    const auto              x = *j.mod.declare_data( "x", Linkage::Local, false );
    module::DataDescription d;
    const auto              fr = j.mod.declare_func_in_data( f, d );
    const auto              gr = j.mod.declare_data_in_data( x, d );
    CHECK( fr.id == 0 );
    CHECK( gr.id == 0 );
    CHECK( d.function_decls( )[ 0 ] == ir::ExternalName { ir::extns::function, f.id } );
    CHECK( d.data_decls( )[ 0 ] == ir::ExternalName { ir::extns::data, x.id } );
}

TEST_CASE( "jit module exposes its isa and declarations", "[module][jit]" )
{
    Jit j;
    CHECK( j.mod.isa( ).name( ) == "x86_64" );
    CHECK( j.mod.isa( ).default_cc( ) == codegen::host_call_conv( ) );
    CHECK( j.mod.declarations( ).size( ) == 0 );
    ( void ) j.declare( "a", Type::i64, { } );
    ( void ) j.mod.declare_data( "b", Linkage::Local, false );
    CHECK( j.mod.declarations( ).size( ) == 1 );
    CHECK( j.mod.declarations( ).data_count( ) == 1 );
    CHECK( j.mod.declarations( ).get_name( "a" ).has_value( ) );
}

TEST_CASE( "jit module rejects declaration conflicts", "[module][jit]" )
{
    Jit j;
    ( void ) j.declare( "a", Type::i64, { } );
    const auto r = j.mod.declare_function( "a", Linkage::Local, sig( { Type::i64 }, Type::i64 ) );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::IncompatibleDeclaration );
    const auto r2 = j.mod.declare_data( "a", Linkage::Local, false );
    REQUIRE_FALSE( r2.has_value( ) );
    CHECK( r2.error( ) == ModuleError::IncompatibleDeclaration );
}

TEST_CASE( "defining an imported function is rejected", "[module][jit]" )
{
    Jit              j;
    const auto       id = j.import( "imp", Type::i64, { } );
    codegen::Context ctx { simple_fn( j.irm, "imp", 1 ) };
    const auto       r = j.mod.define_function( id, ctx );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::InvalidImportDefinition );
    CHECK( ctx.compiled( ) == nullptr );
}

TEST_CASE( "defining a function twice is rejected", "[module][jit]" )
{
    Jit              j;
    const auto       id = j.define( "f", Type::i64, { },
                                    []( ir::IRBuilder& b, const Args& )
                                    {
                                  b.ret( b.iconst( Type::i64, 1 ) );
                                    } );
    codegen::Context ctx { simple_fn( j.irm, "f2", 2 ) };
    const auto       r = j.mod.define_function( id, ctx );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::DuplicateDefinition );
    j.finalise( );
    CHECK( j.get< int64_t( ) >( id )( ) == 1 );
}

TEST_CASE( "defining imported data is rejected", "[module][jit]" )
{
    Jit                     j;
    const auto              id = *j.mod.declare_data( "imp", Linkage::Import, true );
    module::DataDescription d;
    d.define_zeroinit( 8 );
    const auto r = j.mod.define_data( id, d );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::InvalidImportDefinition );
}

TEST_CASE( "defining data twice is rejected", "[module][jit]" )
{
    Jit                     j;
    const auto              id = *j.mod.declare_data( "x", Linkage::Local, false );
    module::DataDescription d;
    d.define_zeroinit( 8 );
    REQUIRE( j.mod.define_data( id, d ).has_value( ) );
    const auto r = j.mod.define_data( id, d );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::DuplicateDefinition );
}

TEST_CASE( "defining data without contents is rejected", "[module][jit]" )
{
    Jit                           j;
    const auto                    id = *j.mod.declare_data( "x", Linkage::Export, true );
    const module::DataDescription d;
    const auto                    r = j.mod.define_data( id, d );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::UninitialisedData );
    module::DataDescription ok;
    ok.define_zeroinit( 1 );
    CHECK( j.mod.define_data( id, ok ).has_value( ) );
}

TEST_CASE( "finalising a call to a missing host symbol fails", "[module][jit]" )
{
    Jit        j;
    const auto missing = j.import( "xiao_definitely_not_a_symbol_9f3a", Type::i64, { Type::i64 } );
    ( void ) caller_of( j, "f", missing );
    const auto r = j.mod.finalise_definitions( );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::SymbolNotFound );
}

TEST_CASE( "finalising a reference to missing host data fails", "[module][jit]" )
{
    Jit        j;
    const auto missing = *j.mod.declare_data( "xiao_definitely_not_data_77b1", Linkage::Import, false );
    j.define( "f", Type::ptr, { },
              [ & ]( ir::IRBuilder& b, const Args& )
              {
                  b.ret( b.symbol_value( Type::ptr, j.mod.declare_data_in_func( missing, b ) ) );
              } );
    const auto r = j.mod.finalise_definitions( );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::SymbolNotFound );
}

TEST_CASE( "finalising a call to an undefined local function fails", "[module][jit]" )
{
    Jit        j;
    const auto g = j.declare( "g", Type::i64, { Type::i64 }, Linkage::Local );
    ( void ) caller_of( j, "f", g );
    const auto r = j.mod.finalise_definitions( );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::UndefinedFunction );
}

TEST_CASE( "finalising a reference to undefined local data fails", "[module][jit]" )
{
    Jit        j;
    const auto x = *j.mod.declare_data( "x", Linkage::Export, false );
    j.define( "f", Type::ptr, { },
              [ & ]( ir::IRBuilder& b, const Args& )
              {
                  b.ret( b.symbol_value( Type::ptr, j.mod.declare_data_in_func( x, b ) ) );
              } );
    const auto r = j.mod.finalise_definitions( );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::UndefinedData );
}

TEST_CASE( "finalising data that points at an undefined function fails", "[module][jit]" )
{
    Jit                     j;
    const auto              g   = j.declare( "g", Type::i64, { }, Linkage::Local );
    const auto              tbl = *j.mod.declare_data( "tbl", Linkage::Local, false );
    module::DataDescription d;
    d.define_zeroinit( 8 );
    d.write_function_addr( 0, j.mod.declare_func_in_data( g, d ) );
    REQUIRE( j.mod.define_data( tbl, d ).has_value( ) );
    const auto r = j.mod.finalise_definitions( );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::UndefinedFunction );
}

TEST_CASE( "finalising data that points at undefined data fails", "[module][jit]" )
{
    Jit                     j;
    const auto              x   = *j.mod.declare_data( "x", Linkage::Local, false );
    const auto              tbl = *j.mod.declare_data( "tbl", Linkage::Local, true );
    module::DataDescription d;
    d.define_zeroinit( 8 );
    d.write_data_addr( 0, j.mod.declare_data_in_data( x, d ), 0 );
    REQUIRE( j.mod.define_data( tbl, d ).has_value( ) );
    const auto r = j.mod.finalise_definitions( );
    REQUIRE_FALSE( r.has_value( ) );
    CHECK( r.error( ) == ModuleError::UndefinedData );
}

TEST_CASE( "finalising with nothing defined succeeds", "[module][jit]" )
{
    Jit j;
    CHECK( j.mod.finalise_definitions( ).has_value( ) );
    CHECK( j.mod.finalise_definitions( ).has_value( ) );
}

TEST_CASE( "functions can be defined across several finalise rounds", "[module][jit]" )
{
    Jit        j;
    const auto a = j.define( "a", Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& x )
                             {
                                 b.ret( b.imul( x[ 0 ], b.iconst( Type::i64, 7 ) ) );
                             } );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( a )( 3 ) == 21 );

    const auto b = caller_of( j, "b", a );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( b )( 4 ) == 28 );
    CHECK( j.get< int64_t( int64_t ) >( a )( 5 ) == 35 );

    const auto              x = *j.mod.declare_data( "x", Linkage::Local, true );
    module::DataDescription d;
    d.define( std::vector< uint8_t > { 9, 0, 0, 0, 0, 0, 0, 0 } );
    REQUIRE( j.mod.define_data( x, d ).has_value( ) );
    const auto c
        = j.define( "c", Type::i64, { Type::i64 },
                    [ & ]( ir::IRBuilder& bb, const Args& v )
                    {
                        const auto p = bb.symbol_value( Type::ptr, j.mod.declare_data_in_func( x, bb ) );
                        bb.ret( bb.iadd( bb.call( j.mod.declare_func_in_func( b, bb ), Args { v[ 0 ] } ), bb.load( Type::i64, p ) ) );
                    } );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( c )( 1 ) == 16 );
    CHECK( j.get< int64_t( int64_t ) >( b )( 1 ) == 7 );
    CHECK( j.get< int64_t( int64_t ) >( a )( 1 ) == 7 );
}

TEST_CASE( "many finalise rounds each add one function", "[module][jit]" )
{
    Jit                           j;
    std::vector< module::FuncId > ids;
    ids.push_back( j.define( "f0", Type::i64, { Type::i64 },
                             []( ir::IRBuilder& b, const Args& x )
                             {
                                 b.ret( b.iadd( x[ 0 ], b.iconst( Type::i64, 1 ) ) );
                             } ) );
    j.finalise( );
    for ( int k = 1; k < 20; ++k )
    {
        const auto prev = ids.back( );
        ids.push_back(
            j.define( std::format( "f{}", k ), Type::i64, { Type::i64 },
                      [ & ]( ir::IRBuilder& b, const Args& x )
                      {
                          b.ret( b.iadd( b.call( j.mod.declare_func_in_func( prev, b ), Args { x[ 0 ] } ), b.iconst( Type::i64, 1 ) ) );
                      } ) );
        j.finalise( );
    }
    for ( int k = 0; k < 20; ++k ) CHECK( j.get< int64_t( int64_t ) >( ids[ k ] )( 100 ) == 101 + k );
}

TEST_CASE( "explicit symbols win over lookup functions", "[module][jit]" )
{
    jit::JITBuilder builder { isa::lookup_host( ) };
    builder.symbol( "foo", reinterpret_cast< const void* >( &h_first ) );
    builder.symbol_lookup_fn(
        []( const std::string_view name ) -> const void*
        {
            if ( name == "foo" || name == "bar" ) return reinterpret_cast< const void* >( &h_second );
            return nullptr;
        } );
    Jit        j { std::move( builder ) };
    const auto foo = caller_of( j, "call_foo", j.import( "foo", Type::i64, { Type::i64 } ) );
    const auto bar = caller_of( j, "call_bar", j.import( "bar", Type::i64, { Type::i64 } ) );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( foo )( 10 ) == 11 );
    CHECK( j.get< int64_t( int64_t ) >( bar )( 10 ) == 12 );
}

TEST_CASE( "later lookup functions take priority and null falls through", "[module][jit]" )
{
    std::vector< std::string > asked;
    jit::JITBuilder            builder { isa::lookup_host( ) };
    builder.symbol_lookup_fn(
        [ & ]( const std::string_view name ) -> const void*
        {
            asked.push_back( "early:" + std::string { name } );
            if ( name == "x" || name == "y" ) return reinterpret_cast< const void* >( &h_first );
            return nullptr;
        } );
    builder.symbol_lookup_fn(
        [ & ]( const std::string_view name ) -> const void*
        {
            asked.push_back( "late:" + std::string { name } );
            if ( name == "x" ) return reinterpret_cast< const void* >( &h_third );
            return nullptr;
        } );
    Jit        j { std::move( builder ) };
    const auto x = caller_of( j, "call_x", j.import( "x", Type::i64, { Type::i64 } ) );
    const auto y = caller_of( j, "call_y", j.import( "y", Type::i64, { Type::i64 } ) );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( x )( 0 ) == 3 );
    CHECK( j.get< int64_t( int64_t ) >( y )( 0 ) == 1 );
    CHECK( asked == std::vector< std::string > { "late:x", "late:y", "early:y" } );
}

TEST_CASE( "builder symbol replaces an earlier registration", "[module][jit]" )
{
    jit::JITBuilder builder { isa::lookup_host( ) };
    builder.symbol( "s", reinterpret_cast< const void* >( &h_first ) ).symbol( "s", reinterpret_cast< const void* >( &h_third ) );
    Jit        j { std::move( builder ) };
    const auto s = caller_of( j, "call_s", j.import( "s", Type::i64, { Type::i64 } ) );
    j.finalise( );
    CHECK( j.get< int64_t( int64_t ) >( s )( 0 ) == 3 );
}

TEST_CASE( "default symbol lookup finds C runtime functions", "[module][jit]" )
{
    Jit        j;
    const auto strlen_id = j.import( "strlen", Type::i64, { Type::ptr } );
    const auto llabs_id  = j.import( "llabs", Type::i64, { Type::i64 } );
    const auto len       = j.define( "len", Type::i64, { Type::ptr },
                                     [ & ]( ir::IRBuilder& b, const Args& a )
                                     {
                                   b.ret( b.call( j.mod.declare_func_in_func( strlen_id, b ), a ) );
                                     } );
    const auto abs       = caller_of( j, "absval", llabs_id );
    j.finalise( );
    CHECK( j.get< int64_t( const char* ) >( len )( "" ) == 0 );
    CHECK( j.get< int64_t( const char* ) >( len )( "hello" ) == 5 );
    CHECK( j.get< int64_t( int64_t ) >( abs )( -42 ) == 42 );
    CHECK( j.get< int64_t( int64_t ) >( abs )( 42 ) == 42 );
    CHECK( jit::detail::default_symbol_lookup( "strlen" ) != nullptr );
    CHECK( jit::detail::default_symbol_lookup( "xiao_definitely_not_a_symbol_9f3a" ) == nullptr );
}

TEST_CASE( "context keeps the compiled code until cleared", "[module][jit]" )
{
    ir::IRModule     irm;
    codegen::Context ctx { simple_fn( irm, "k", 42 ) };
    CHECK( ctx.compiled( ) == nullptr );
    CHECK( &ctx.func( ) == irm.get( ir::FunctionRef { 0 } ) );
    const auto isa = isa::lookup_host( );
    const auto r   = ctx.compile( *isa );
    REQUIRE( r.has_value( ) );
    CHECK( *r == ctx.compiled( ) );
    CHECK_FALSE( ctx.compiled( )->code.empty( ) );
    CHECK( ctx.compiled( )->alignment == 16 );
    CHECK( ctx.compiled( )->relocs.empty( ) );
    ctx.clear( );
    CHECK( ctx.compiled( ) == nullptr );
}
