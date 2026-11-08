#pragma once

#include <xiao/codegen/context.hh>
#include <xiao/codegen/isa.hh>
#include <xiao/ir/builder.hh>
#include <cassert>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace xiao::module
{
    struct FuncId
    {
        uint32_t    id { };
        friend bool operator== ( const FuncId&, const FuncId& ) = default;
    };

    struct DataId
    {
        uint32_t    id { };
        friend bool operator== ( const DataId&, const DataId& ) = default;
    };

    using FuncOrDataId = std::variant< FuncId, DataId >;

    enum class Linkage : uint8_t
    {
        Import,
        Local,
        Export,
    };

    struct Signature
    {
        std::vector< ir::Type > params { };
        ir::Type                ret { ir::Type::unit };
        codegen::CallConv       cc { codegen::host_call_conv( ) };

        friend bool operator== ( const Signature&, const Signature& ) = default;
    };

    //! @brief Read the signature of an IR function
    [[nodiscard]] inline Signature signature_of( const ir::IRFunction& fn );

    struct FunctionDecl
    {
        std::string name { };
        Linkage     linkage { };
        Signature   sig { };
    };

    struct DataDecl
    {
        std::string name { };
        Linkage     linkage { };
        bool        writable { };
    };

    enum class ModuleError : uint8_t
    {
        IncompatibleDeclaration,
        DuplicateDefinition,
        InvalidImportDefinition,
        UndefinedFunction,
        UndefinedData,
        UninitialisedData,
        SymbolNotFound,
        RelocOutOfRange,
        AllocationFailed,
        CompilationFailed,
    };

    struct StringHash
    {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator( ) ( const std::string_view s ) const noexcept
        { return std::hash< std::string_view > { }( s ); }
    };

    class DataDescription
    {
    public:
        struct Zeros
        {
            std::size_t size { };
        };

        struct FuncReloc
        {
            uint32_t    offset { };
            ir::FuncRef func { };
        };

        struct DataReloc
        {
            uint32_t           offset { };
            ir::GlobalValueRef data { };
            int64_t            addend { };
        };

        using Init = std::variant< std::monostate, Zeros, std::vector< uint8_t > >;

        void define_zeroinit( const std::size_t size )
        {
            assert( std::holds_alternative< std::monostate >( m_init ) && "[xiao::module::DataDescription] data defined twice" );
            m_init = Zeros { size };
        }

        void define( const std::span< const uint8_t > contents )
        {
            assert( std::holds_alternative< std::monostate >( m_init ) && "[xiao::module::DataDescription] data defined twice" );
            m_init = std::vector< uint8_t > { contents.begin( ), contents.end( ) };
        }

        void define( const std::string_view contents )
        { define( std::span { reinterpret_cast< const uint8_t* >( contents.data( ) ), contents.size( ) } ); }

        void set_align( const uint64_t align )
        {
            assert( align != 0 && ( align & ( align - 1 ) ) == 0 && "[xiao::module::DataDescription] alignment must be a power of two" );
            m_align = align;
        }

        [[nodiscard]] ir::FuncRef import_function( const ir::ExternalName name )
        {
            m_function_decls.push_back( name );
            return ir::FuncRef { static_cast< uint32_t >( m_function_decls.size( ) - 1 ) };
        }

        [[nodiscard]] ir::GlobalValueRef import_global_value( const ir::ExternalName name )
        {
            m_data_decls.push_back( name );
            return ir::GlobalValueRef { static_cast< uint32_t >( m_data_decls.size( ) - 1 ) };
        }

        void write_function_addr( const uint32_t offset, const ir::FuncRef func )
        {
            assert( offset + 8 <= size( ) && "[xiao::module::DataDescription::write_function_addr] pointer does not fit in the data" );
            m_function_relocs.push_back( { offset, func } );
        }

        void write_data_addr( const uint32_t offset, const ir::GlobalValueRef data, const int64_t addend )
        {
            assert( offset + 8 <= size( ) && "[xiao::module::DataDescription::write_data_addr] pointer does not fit in the data" );
            m_data_relocs.push_back( { offset, data, addend } );
        }

        void clear( )
        { *this = DataDescription { }; }

        [[nodiscard]] std::size_t size( ) const noexcept
        {
            if ( const auto* z = std::get_if< Zeros >( &m_init ) ) return z->size;
            if ( const auto* b = std::get_if< std::vector< uint8_t > >( &m_init ) ) return b->size( );
            return 0;
        }

        [[nodiscard]] const Init& init( ) const noexcept
        { return m_init; }

        [[nodiscard]] std::optional< uint64_t > align( ) const noexcept
        { return m_align; }

        [[nodiscard]] std::span< const ir::ExternalName > function_decls( ) const noexcept
        { return m_function_decls; }

        [[nodiscard]] std::span< const ir::ExternalName > data_decls( ) const noexcept
        { return m_data_decls; }

        [[nodiscard]] std::span< const FuncReloc > function_relocs( ) const noexcept
        { return m_function_relocs; }

        [[nodiscard]] std::span< const DataReloc > data_relocs( ) const noexcept
        { return m_data_relocs; }

    private:
        Init                            m_init { };
        std::optional< uint64_t >       m_align { };
        std::vector< ir::ExternalName > m_function_decls { };
        std::vector< ir::ExternalName > m_data_decls { };
        std::vector< FuncReloc >        m_function_relocs { };
        std::vector< DataReloc >        m_data_relocs { };
    };

    class ModuleDeclarations
    {
    public:
        [[nodiscard]] std::expected< FuncId, ModuleError > declare_function( std::string_view name, Linkage linkage, const Signature& sig );

        [[nodiscard]] std::expected< DataId, ModuleError > declare_data( std::string_view name, Linkage linkage, bool writable );

        [[nodiscard]] const FunctionDecl& get( FuncId id ) const
        { return m_funcs[ id.id ]; }

        [[nodiscard]] const DataDecl& get( DataId id ) const
        { return m_data[ id.id ]; }

        [[nodiscard]] std::optional< FuncOrDataId > get_name( std::string_view name ) const;

        [[nodiscard]] std::size_t size( ) const noexcept
        { return m_funcs.size( ); }

        [[nodiscard]] std::size_t data_count( ) const noexcept
        { return m_data.size( ); }

        [[nodiscard]] static std::expected< Linkage, ModuleError > merge( Linkage a, Linkage b ) noexcept;

    private:
        std::vector< FunctionDecl >                                                  m_funcs { };
        std::vector< DataDecl >                                                      m_data { };
        std::unordered_map< std::string, FuncOrDataId, StringHash, std::equal_to<> > m_names { };
    };

    class Module
    {
    public:
        virtual ~Module( ) = default;

        [[nodiscard]] virtual const codegen::TargetIsa& isa( ) const noexcept          = 0;
        [[nodiscard]] virtual const ModuleDeclarations& declarations( ) const noexcept = 0;

        [[nodiscard]] virtual std::expected< FuncId, ModuleError > declare_function( std::string_view name, Linkage linkage,
                                                                                     const Signature& sig ) = 0;

        [[nodiscard]] virtual std::expected< DataId, ModuleError > declare_data( std::string_view name, Linkage linkage, bool writable ) = 0;

        [[nodiscard]] virtual std::expected< void, ModuleError > define_function( FuncId id, codegen::Context& ctx ) = 0;

        [[nodiscard]] virtual std::expected< void, ModuleError > define_data( DataId id, const DataDescription& data ) = 0;

        //! @brief Import `id` into the function being built so it can be called.
        [[nodiscard]] ir::FuncRef declare_func_in_func( FuncId id, ir::IRBuilder& builder ) const;

        [[nodiscard]] ir::GlobalValueRef declare_data_in_func( DataId id, ir::IRBuilder& builder ) const;

        [[nodiscard]] ir::FuncRef declare_func_in_data( FuncId id, DataDescription& data ) const;

        [[nodiscard]] ir::GlobalValueRef declare_data_in_data( DataId id, DataDescription& data ) const;
    };

    inline Signature signature_of( const ir::IRFunction& fn )
    {
        return Signature {
            .params = { fn.sig.params.begin( ), fn.sig.params.end( ) },
            .ret    = fn.sig.ret,
            .cc     = fn.sig.cc
        };
    }

    inline std::expected< Linkage, ModuleError > ModuleDeclarations::merge( const Linkage a, const Linkage b ) noexcept
    {
        if ( a == b ) return a;
        if ( a == Linkage::Import ) return b;
        if ( b == Linkage::Import ) return a;
        return std::unexpected( ModuleError::IncompatibleDeclaration );
    }

    inline std::expected< FuncId, ModuleError > ModuleDeclarations::declare_function( const std::string_view name, const Linkage linkage,
                                                                                      const Signature& sig )
    {
        if ( const auto it = m_names.find( name ); it != m_names.end( ) )
        {
            const auto* id = std::get_if< FuncId >( &it->second );
            if ( !id ) return std::unexpected( ModuleError::IncompatibleDeclaration );

            auto& decl = m_funcs[ id->id ];
            if ( decl.sig != sig ) return std::unexpected( ModuleError::IncompatibleDeclaration );

            const auto merged = merge( decl.linkage, linkage );
            if ( !merged ) return std::unexpected( merged.error( ) );
            decl.linkage = *merged;
            return *id;
        }

        const auto id = FuncId { static_cast< uint32_t >( m_funcs.size( ) ) };
        m_funcs.push_back( FunctionDecl { .name = std::string( name ), .linkage = linkage, .sig = sig } );
        m_names.emplace( std::string( name ), id );
        return id;
    }

    inline std::expected< DataId, ModuleError > ModuleDeclarations::declare_data( const std::string_view name, const Linkage linkage,
                                                                                  const bool writable )
    {
        if ( const auto it = m_names.find( name ); it != m_names.end( ) )
        {
            const auto* id = std::get_if< DataId >( &it->second );
            if ( !id ) return std::unexpected( ModuleError::IncompatibleDeclaration );

            auto&      decl   = m_data[ id->id ];
            const auto merged = merge( decl.linkage, linkage );
            if ( !merged ) return std::unexpected( merged.error( ) );
            decl.linkage  = *merged;
            decl.writable = decl.writable || writable;
            return *id;
        }

        const auto id = DataId { static_cast< uint32_t >( m_data.size( ) ) };
        m_data.push_back( DataDecl { .name = std::string( name ), .linkage = linkage, .writable = writable } );
        m_names.emplace( std::string( name ), id );
        return id;
    }

    inline std::optional< FuncOrDataId > ModuleDeclarations::get_name( const std::string_view name ) const
    {
        if ( const auto it = m_names.find( name ); it != m_names.end( ) ) return it->second;
        return std::nullopt;
    }

    inline ir::FuncRef Module::declare_func_in_func( const FuncId id, ir::IRBuilder& builder ) const
    {
        const auto& decl = declarations( ).get( id );
        return builder.import_function( ir::ExternalName { ir::extns::function, id.id }, decl.sig.params, decl.sig.ret, decl.sig.cc,
                                        decl.linkage != Linkage::Import );
    }

    inline ir::GlobalValueRef Module::declare_data_in_func( const DataId id, ir::IRBuilder& builder ) const
    {
        const auto& decl = declarations( ).get( id );
        return builder.create_global_value(
            ir::GlobalValueData { .name = ir::ExternalName { ir::extns::data, id.id }, .offset = 0, .colocated = decl.linkage != Linkage::Import } );
    }

    inline ir::FuncRef Module::declare_func_in_data( const FuncId id, DataDescription& data ) const
    { return data.import_function( ir::ExternalName { ir::extns::function, id.id } ); }

    inline ir::GlobalValueRef Module::declare_data_in_data( const DataId id, DataDescription& data ) const
    { return data.import_global_value( ir::ExternalName { ir::extns::data, id.id } ); }
} // namespace xiao::module
