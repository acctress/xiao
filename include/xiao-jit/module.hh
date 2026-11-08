#pragma once

#include <xiao/codegen/context.hh>
#include <xiao/codegen/reloc.hh>
#include <xiao/module/module.hh>
#include <xiao-jit/builder.hh>
#include <xiao-jit/memory.hh>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace xiao::jit
{
    class JITModule final : public module::Module
    {
    public:
        static constexpr uint64_t readonly_data_alignment = 1;
        static constexpr uint64_t writable_data_alignment = 8;

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

        [[nodiscard]] std::expected< module::DataId, module::ModuleError >
            declare_data( const std::string_view name, const module::Linkage linkage, const bool writable ) override
        {
            auto id = m_decls.declare_data( name, linkage, writable );
            if ( id && m_data.size( ) <= id->id ) m_data.resize( id->id + 1 );
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
            m_compiled[ id.id ] = Blob { .ptr = *ptr, .size = code.code.size( ), .relocs = code.relocs, .finalised = false };
            m_pending_funcs.push_back( id );
            return { };
        }

        [[nodiscard]] std::expected< void, module::ModuleError > define_data( const module::DataId           id,
                                                                              const module::DataDescription& data ) override
        {
            assert( id.id < m_data.size( ) && "[xiao::jit::JITModule::define_data] undeclared DataId" );

            const auto& decl = m_decls.get( id );
            if ( decl.linkage == module::Linkage::Import ) return std::unexpected( module::ModuleError::InvalidImportDefinition );
            if ( m_data[ id.id ] ) return std::unexpected( module::ModuleError::DuplicateDefinition );
            if ( std::holds_alternative< std::monostate >( data.init( ) ) )
                return std::unexpected( module::ModuleError::UninitialisedData );

            auto&      memory = decl.writable ? m_writable : m_readonly;
            const auto align  = data.align( ).value_or( decl.writable ? writable_data_alignment : readonly_data_alignment );
            const auto size   = data.size( );

            const auto ptr = memory.allocate( size, align );
            if ( !ptr ) return std::unexpected( ptr.error( ) );

            if ( const auto* bytes = std::get_if< std::vector< uint8_t > >( &data.init( ) ) )
                std::memcpy( *ptr, bytes->data( ), bytes->size( ) );
            else
                std::memset( *ptr, 0, size );

            std::vector< codegen::Reloc > relocs;
            for ( const auto& r : data.function_relocs( ) )
                relocs.push_back(
                    { .offset = r.offset, .kind = codegen::RelocKind::Abs8, .target = data.function_decls( )[ r.func.id ], .addend = 0 } );
            for ( const auto& r : data.data_relocs( ) )
                relocs.push_back( { .offset = r.offset,
                                    .kind   = codegen::RelocKind::Abs8,
                                    .target = data.data_decls( )[ r.data.id ],
                                    .addend = r.addend } );

            m_data[ id.id ] = Blob { .ptr = *ptr, .size = size, .relocs = std::move( relocs ), .finalised = false };
            m_pending_data.push_back( id );
            return { };
        }

        [[nodiscard]] std::expected< void, module::ModuleError > finalise_definitions( )
        {
            for ( const auto [ id ] : m_pending_funcs )
                if ( const auto r = perform_relocations( *m_compiled[ id ] ); !r ) return r;
            for ( const auto [ id ] : m_pending_data )
                if ( const auto r = perform_relocations( *m_data[ id ] ); !r ) return r;

            for ( const auto [ id ] : m_pending_funcs ) m_compiled[ id ]->finalised = true;
            for ( const auto [ id ] : m_pending_data ) m_data[  id ]->finalised = true;
            m_pending_funcs.clear( );
            m_pending_data.clear( );

            if ( const auto r = m_code.finalise( ); !r ) return r;
            if ( const auto r = m_readonly.finalise( ); !r ) return r;
            return m_writable.finalise( );
        }

        template < typename F >
        [[nodiscard]] F* get_finalised_function( const module::FuncId id ) const
        {
            assert( id.id < m_compiled.size( ) && m_compiled[ id.id ] && m_compiled[ id.id ]->finalised
                    && "[xiao::jit::JITModule::get_finalised_function] function not defined and finalised" );
            return reinterpret_cast< F* >( m_compiled[ id.id ]->ptr );
        }

        [[nodiscard]] std::span< std::byte > get_finalised_data( const module::DataId id ) const
        {
            assert( id.id < m_data.size( ) && m_data[ id.id ] && m_data[ id.id ]->finalised
                    && "[xiao::jit::JITModule::get_finalised_data] data not defined and finalised" );
            return { m_data[ id.id ]->ptr, m_data[ id.id ]->size };
        }

    private:
        struct Blob
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
            const auto resolve = [ & ]( const auto& decl, const auto& blobs,
                                        const module::ModuleError undefined ) -> std::expected< const void*, module::ModuleError >
            {
                if ( decl.linkage == module::Linkage::Import )
                {
                    if ( const auto p = lookup_symbol( decl.name ) ) return p;
                    return std::unexpected( module::ModuleError::SymbolNotFound );
                }
                if ( name.index >= blobs.size( ) || !blobs[ name.index ] ) return std::unexpected( undefined );
                return blobs[ name.index ]->ptr;
            };

            if ( name.ns == ir::extns::function )
                return resolve( m_decls.get( module::FuncId { name.index } ), m_compiled, module::ModuleError::UndefinedFunction );

            assert( name.ns == ir::extns::data && "[xiao::jit::JITModule::address_of] unknown name namespace" );
            return resolve( m_decls.get( module::DataId { name.index } ), m_data, module::ModuleError::UndefinedData );
        }

        [[nodiscard]] std::expected< void, module::ModuleError > perform_relocations( const Blob& blob ) const
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
                    case codegen::RelocKind::X86PCRel4:
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

        JITBuilder                           m_builder;
        module::ModuleDeclarations           m_decls { };
        Region                               m_region { };
        Memory                               m_code { Protection::Executable, &m_region };
        Memory                               m_readonly { Protection::ReadOnly, &m_region };
        Memory                               m_writable { Protection::Writable, &m_region };
        std::vector< std::optional< Blob > > m_compiled { };
        std::vector< std::optional< Blob > > m_data { };
        std::vector< module::FuncId >        m_pending_funcs { };
        std::vector< module::DataId >        m_pending_data { };
    };
} // namespace xiao::jit
