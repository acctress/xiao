#pragma once

#include <xiao/codegen/context.hh>
#include <xiao/codegen/reloc.hh>
#include <xiao/module/module.hh>
#include <xiao-jit/builder.hh>
#include <xiao-jit/memory.hh>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace xiao::jit
{
    class JITModule final : public module::Module
    {
    public:
        explicit JITModule( JITBuilder builder ) : m_builder { std::move( builder ) } { }

        JITModule( const JITModule& )             = delete;
        JITModule& operator= ( const JITModule& ) = delete;

        [[nodiscard]] const codegen::TargetIsa& isa( ) const noexcept override
        { return *m_builder.m_isa; }

        [[nodiscard]] const module::ModuleDeclarations& declarations( ) const noexcept override
        { return m_decls; }

        [[nodiscard]] std::expected< module::FuncId, module::ModuleError >
            declare_function( const std::string_view name, const module::Linkage linkage, const module::Signature& sig ) override
        {
            auto id = m_decls.declare_function( name, linkage, sig );
            if ( id && m_compiled.size( ) <= id->id ) m_compiled.resize( id->id + 1 );
            return id;
        }

        [[nodiscard]] std::expected< void, module::ModuleError > define_function( const module::FuncId id, codegen::Context& ctx ) override
        {
            assert( id.id < m_compiled.size( ) && "[xiao::jit::JITModule::define_function] undeclared FuncId" );

            if ( m_decls.get( id ).linkage == module::Linkage::Import )
                return std::unexpected( module::ModuleError::InvalidImportDefinition );
            if ( m_compiled[ id.id ] ) return std::unexpected( module::ModuleError::DuplicateDefinition );

            const auto compiled = ctx.compile( isa( ) );
            if ( !compiled ) return std::unexpected( module::ModuleError::CompilationFailed );

            const auto& code = **compiled;
            const auto  ptr  = m_code.allocate( code.code.size( ), code.alignment );
            if ( !ptr ) return std::unexpected( ptr.error( ) );

            std::memcpy( *ptr, code.code.data( ), code.code.size( ) );
            m_compiled[ id.id ] = CompiledBlob { .ptr = *ptr, .size = code.code.size( ), .relocs = code.relocs, .finalised = false };
            m_pending.push_back( id );
            return { };
        }

        [[nodiscard]] std::expected< void, module::ModuleError > finalise_definitions( )
        {
            for ( const auto id : m_pending )
                if ( const auto r = perform_relocations( *m_compiled[ id.id ] ); !r ) return r;

            for ( const auto id : m_pending ) m_compiled[ id.id ]->finalised = true;
            m_pending.clear( );

            return m_code.set_readable_and_executable( );
        }

        template < typename F >
        [[nodiscard]] F* get_finalised_function( const module::FuncId id ) const
        {
            assert( id.id < m_compiled.size( ) && m_compiled[ id.id ] && m_compiled[ id.id ]->finalised
                    && "[xiao::jit::JITModule::get_finalised_function] function not defined and finalised" );
            return reinterpret_cast< F* >( m_compiled[ id.id ]->ptr );
        }

    private:
        struct CompiledBlob
        {
            std::byte*                    ptr { };
            std::size_t                   size { };
            std::vector< codegen::Reloc > relocs { };
            bool                          finalised { };
        };

        [[nodiscard]] const void* lookup_symbol( const std::string_view name ) const
        {
            if ( const auto it = m_builder.m_symbols.find( name ); it != m_builder.m_symbols.end( ) ) return it->second;

            for ( auto it = m_builder.m_lookup_fns.rbegin( ); it != m_builder.m_lookup_fns.rend( ); ++it )
                if ( const auto p = ( *it )( name ) ) return p;

            return nullptr;
        }

        [[nodiscard]] std::expected< const void*, module::ModuleError > address_of( const ir::ExternalName name ) const
        {
            assert( name.ns == ir::extns::function && "[xiao::jit::JITModule::address_of] only function names supported YET" );

            const module::FuncId id { name.index };
            const auto&          decl = m_decls.get( id );

            if ( decl.linkage == module::Linkage::Import )
            {
                if ( const auto p = lookup_symbol( decl.name ) ) return p;
                return std::unexpected( module::ModuleError::SymbolNotFound );
            }

            if ( id.id >= m_compiled.size( ) || !m_compiled[ id.id ] ) return std::unexpected( module::ModuleError::UndefinedFunction );
            return m_compiled[ id.id ]->ptr;
        }

        [[nodiscard]] std::expected< void, module::ModuleError > perform_relocations( const CompiledBlob& blob ) const
        {
            for ( const auto& r : blob.relocs )
            {
                const auto target = address_of( r.target );
                if ( !target ) return std::unexpected( target.error( ) );

                auto* const site = blob.ptr + r.offset;
                const auto  addr = reinterpret_cast< std::intptr_t >( *target );

                switch ( r.kind )
                {
                    case codegen::RelocKind::Abs8:
                    {
                        const auto v = static_cast< uint64_t >( addr + r.addend );
                        std::memcpy( site, &v, sizeof v );
                        break;
                    }
                    case codegen::RelocKind::X86CallPCRel4:
                    {
                        const auto diff = addr + r.addend - reinterpret_cast< std::intptr_t >( site );
                        if ( diff < std::numeric_limits< int32_t >::min( ) || diff > std::numeric_limits< int32_t >::max( ) )
                            return std::unexpected( module::ModuleError::RelocOutOfRange );

                        const auto v = static_cast< int32_t >( diff );
                        std::memcpy( site, &v, sizeof v );
                        break;
                    }
                }
            }

            return { };
        }

        JITBuilder                                   m_builder;
        module::ModuleDeclarations                   m_decls { };
        Memory                                       m_code { };
        std::vector< std::optional< CompiledBlob > > m_compiled { };
        std::vector< module::FuncId >                m_pending { };
    };
} // namespace xiao::jit
