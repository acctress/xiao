#pragma once

#include "xiao/codegen/target.hh"

#include <xiao/codegen/machbuffer.hh>
#include <xiao-jit/builder.hh>
#include <xiao-jit/memory.hh>
#include <unordered_map>
#include <memory>
#include <string>

namespace xiao::jit
{
    class JITModule
    {
    public:
        explicit JITModule( const JITBuilder& builder = { } ) : m_builder( builder ) { }

        template < typename T >
        T* define( const std::string& name, codegen::Target& target, ir::IRFunction& fn, codegen::CallConv cc )
        {
            target.compile( fn, cc );

            auto mem = std::make_unique< JITMemoryProvider >( target.bytes( ).size( ) );
            mem->write( target.bytes( ) );

            if ( !mem->finalise_permissions( ) ) throw std::runtime_error( "[xiao-jit::JITModule] failed to switch perms to RX" );

            void* ptr = mem->data( );
            m_allocs.push_back( std::move( mem ) );
            m_functions[ name ] = ptr;
            return reinterpret_cast< T* >( ptr );
        }

        template < typename T >
        [[nodiscard]] T* get_function( const std::string& name ) const
        {
            if ( const auto iter = m_functions.find( name ); iter != m_functions.end( ) ) return reinterpret_cast< T* >( iter->second );
            return nullptr;
        }

    private:
        JITBuilder                                          m_builder;
        std::vector< std::unique_ptr< JITMemoryProvider > > m_allocs;
        std::unordered_map< std::string, void* >            m_functions;
    };
} // namespace xiao::jit