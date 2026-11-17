#pragma once

#include <xiao/codegen/context.hh>
#include <xiao/codegen/isa.hh>
#include <xiao/codegen/reloc.hh>
#include <xiao/module/module.hh>
#include <xiao-object/coff.hh>
#include <xiao-object/object.hh>

#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace xiao::object
{
    enum class ObjectFormat : uint8_t
    {
        Coff,
    };

    class ObjectBuilder
    {
    public:
        ObjectBuilder( std::unique_ptr< codegen::TargetIsa > isa, std::string name, const ObjectFormat format = ObjectFormat::Coff ) :
            m_isa { std::move( isa ) },
            m_name { std::move( name ) },
            m_format { format }
        {
        }

    private:
        friend class ObjectModule;

        std::unique_ptr< codegen::TargetIsa > m_isa;
        std::string                           m_name;
        ObjectFormat                          m_format;
    };

    class ObjectProduct
    {
    public:
        ObjectProduct( Object object, std::string name, const ObjectFormat format ) :
            m_object { std::move( object ) },
            m_name { std::move( name ) },
            m_format { format }
        {
        }

        [[nodiscard]] std::vector< uint8_t > emit( ) const
        {
            switch ( m_format )
            {
                case ObjectFormat::Coff: return coff::write( m_object );
            }
            return { };
        }

        [[nodiscard]] bool write( const std::filesystem::path& path ) const
        {
            const auto    bytes = emit( );
            std::ofstream file { path, std::ios::binary };
            file.write( reinterpret_cast< const char* >( bytes.data( ) ), static_cast< std::streamsize >( bytes.size( ) ) );
            return static_cast< bool >( file );
        }

        [[nodiscard]] const Object& object( ) const noexcept
        { return m_object; }

        [[nodiscard]] std::string_view name( ) const noexcept
        { return m_name; }

    private:
        Object       m_object;
        std::string  m_name;
        ObjectFormat m_format;
    };

    class ObjectModule final : public module::Module
    {
    public:
        static constexpr uint64_t readonly_data_alignment = 1;
        static constexpr uint64_t writable_data_alignment = 8;

        explicit ObjectModule( ObjectBuilder builder ) : m_builder { std::move( builder ) } { }

        ObjectModule( const ObjectModule& )             = delete;
        ObjectModule& operator= ( const ObjectModule& ) = delete;

        [[nodiscard]] const codegen::TargetIsa& isa( ) const noexcept override
        { return *m_builder.m_isa; }

        [[nodiscard]] const module::ModuleDeclarations& declarations( ) const noexcept override
        { return m_decls; }

        [[nodiscard]] std::expected< module::FuncId, module::ModuleError >
            declare_function( const std::string_view name, const module::Linkage linkage, const module::Signature& sig ) override
        {
            auto id = m_decls.declare_function( name, linkage, sig );
            if ( id && m_functions.size( ) <= id->id ) m_functions.resize( id->id + 1 );
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
            assert( id.id < m_functions.size( ) && "[xiao::object::ObjectModule::define_function] undeclared FuncId" );

            if ( m_decls.get( id ).linkage == module::Linkage::Import )
                return std::unexpected( module::ModuleError::InvalidImportDefinition );
            if ( m_functions[ id.id ] ) return std::unexpected( module::ModuleError::DuplicateDefinition );

            const auto compiled = ctx.compile( isa( ) );
            if ( !compiled ) return std::unexpected( module::ModuleError::CompilationFailed );

            const auto& code    = **compiled;
            const auto  section = section_for( SectionKind::Text );
            const auto  offset  = m_object.append( section, code.code, code.alignment );

            m_functions[ id.id ] = Placement { section, offset };
            for ( const auto& r : code.relocs ) m_relocs.push_back( { section, offset + r.offset, r.target, r.kind, r.addend } );
            return { };
        }

        [[nodiscard]] std::expected< void, module::ModuleError > define_data( const module::DataId           id,
                                                                              const module::DataDescription& data ) override
        {
            assert( id.id < m_data.size( ) && "[xiao::object::ObjectModule::define_data] undeclared DataId" );

            const auto& decl = m_decls.get( id );
            if ( decl.linkage == module::Linkage::Import ) return std::unexpected( module::ModuleError::InvalidImportDefinition );
            if ( m_data[ id.id ] ) return std::unexpected( module::ModuleError::DuplicateDefinition );
            if ( std::holds_alternative< std::monostate >( data.init( ) ) )
                return std::unexpected( module::ModuleError::UninitialisedData );

            const auto align
                = static_cast< uint32_t >( data.align( ).value_or( decl.writable ? writable_data_alignment : readonly_data_alignment ) );
            const auto* bytes = std::get_if< std::vector< uint8_t > >( &data.init( ) );

            uint32_t section { };
            uint32_t offset { };
            if ( !bytes && decl.writable )
            {
                section = section_for( SectionKind::UninitialisedData );
                offset  = m_object.reserve( section, static_cast< uint32_t >( data.size( ) ), align );
            }
            else
            {
                section = section_for( decl.writable ? SectionKind::Data : SectionKind::ReadOnlyData );
                offset  = bytes ? m_object.append( section, *bytes, align )
                                : m_object.append( section, std::vector< uint8_t >( data.size( ), 0 ), align );
            }

            m_data[ id.id ] = Placement { section, offset };

            for ( const auto& r : data.function_relocs( ) )
                m_relocs.push_back( { section, offset + r.offset, data.function_decls( )[ r.func.id ], codegen::RelocKind::Abs8, 0 } );
            for ( const auto& r : data.data_relocs( ) )
                m_relocs.push_back( { section, offset + r.offset, data.data_decls( )[ r.data.id ], codegen::RelocKind::Abs8, r.addend } );
            return { };
        }

        [[nodiscard]] std::expected< ObjectProduct, module::ModuleError > finish( ) &&
        {
            std::vector< std::optional< uint32_t > > func_symbols( m_decls.size( ) );
            std::vector< std::optional< uint32_t > > data_symbols( m_decls.data_count( ) );

            for ( uint32_t i { 0 }; i < m_decls.size( ); ++i )
            {
                const auto& decl = m_decls.get( module::FuncId { i } );
                if ( decl.linkage == module::Linkage::Import ) continue;
                if ( i >= m_functions.size( ) || !m_functions[ i ] ) return std::unexpected( module::ModuleError::UndefinedFunction );
                func_symbols[ i ] = m_object.add_symbol( defined_symbol( decl.name, decl.linkage, *m_functions[ i ], true ) );
            }

            for ( uint32_t i { 0 }; i < m_decls.data_count( ); ++i )
            {
                const auto& decl = m_decls.get( module::DataId { i } );
                if ( decl.linkage == module::Linkage::Import ) continue;
                if ( i >= m_data.size( ) || !m_data[ i ] ) return std::unexpected( module::ModuleError::UndefinedData );
                data_symbols[ i ] = m_object.add_symbol( defined_symbol( decl.name, decl.linkage, *m_data[ i ], false ) );
            }

            const auto symbol_for = [ & ]( const ir::ExternalName name ) -> uint32_t
            {
                const bool is_func = name.ns == ir::extns::function;
                assert( ( is_func || name.ns == ir::extns::data ) && "[xiao::object::ObjectModule::finish] unknown name namespace" );

                auto& slot = is_func ? func_symbols[ name.index ] : data_symbols[ name.index ];
                if ( !slot )
                {
                    const auto& decl_name
                        = is_func ? m_decls.get( module::FuncId { name.index } ).name : m_decls.get( module::DataId { name.index } ).name;
                    slot = m_object.add_symbol( Symbol { .name = decl_name, .scope = SymbolScope::Undefined, .is_function = is_func } );
                }
                return *slot;
            };

            for ( const auto& r : m_relocs )
            {
                const auto kind = r.kind == codegen::RelocKind::Abs8 ? RelocationKind::Abs64 : RelocationKind::Rel32;
                m_object.add_reloc( r.section,
                                    Relocation { .offset = r.offset, .symbol = symbol_for( r.target ), .kind = kind, .addend = r.addend } );
            }

            return ObjectProduct { std::move( m_object ), std::move( m_builder.m_name ), m_builder.m_format };
        }

    private:
        struct Placement
        {
            uint32_t section { };
            uint32_t offset { };
        };

        struct PendingReloc
        {
            uint32_t           section { };
            uint32_t           offset { };
            ir::ExternalName   target { };
            codegen::RelocKind kind { };
            int64_t            addend { };
        };

        [[nodiscard]] static Symbol defined_symbol( const std::string& name, const module::Linkage linkage, const Placement where,
                                                    const bool is_function )
        {
            return Symbol {
                .name        = name,
                .scope       = linkage == module::Linkage::Export ? SymbolScope::Global : SymbolScope::Local,
                .section     = where.section,
                .value       = where.offset,
                .is_function = is_function,
            };
        }

        [[nodiscard]] uint32_t section_for( const SectionKind kind )
        {
            auto& slot = m_sections[ static_cast< std::size_t >( kind ) ];
            if ( slot ) return *slot;

            switch ( kind )
            {
                case SectionKind::Text: slot = m_object.add_section( ".text", kind, 16 ); break;
                case SectionKind::ReadOnlyData: slot = m_object.add_section( ".rdata", kind, 1 ); break;
                case SectionKind::Data: slot = m_object.add_section( ".data", kind, 8 ); break;
                case SectionKind::UninitialisedData: slot = m_object.add_section( ".bss", kind, 8 ); break;
            }
            return *slot;
        }

        ObjectBuilder                              m_builder;
        module::ModuleDeclarations                 m_decls { };
        Object                                     m_object { };
        std::array< std::optional< uint32_t >, 4 > m_sections { };
        std::vector< std::optional< Placement > >  m_functions { };
        std::vector< std::optional< Placement > >  m_data { };
        std::vector< PendingReloc >                m_relocs { };
    };
} // namespace xiao::object
