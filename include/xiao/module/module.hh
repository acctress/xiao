#pragma once

#include <xiao/codegen/context.hh>
#include <xiao/codegen/isa.hh>
#include <xiao/ir/builder.hh>
#include <cassert>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xiao::module
{
    struct FuncId
    {
        uint32_t    id { };
        friend bool operator== ( const FuncId&, const FuncId& ) = default;
    };

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

    enum class ModuleError : uint8_t
    {
        IncompatibleDeclaration,
        DuplicateDefinition,
        InvalidImportDefinition,
        UndefinedFunction,
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

    class ModuleDeclarations
    {
    public:
        [[nodiscard]] std::expected< FuncId, ModuleError > declare_function( std::string_view name, Linkage linkage, const Signature& sig );

        [[nodiscard]] const FunctionDecl& get( FuncId id ) const
        { return m_funcs[ id.id ]; }

        [[nodiscard]] std::optional< FuncId > lookup( std::string_view name ) const;

        [[nodiscard]] std::size_t size( ) const noexcept
        { return m_funcs.size( ); }

    private:
        std::vector< FunctionDecl >                                              m_funcs { };
        std::unordered_map< std::string, uint32_t, StringHash, std::equal_to<> > m_names { };
    };

    class Module
    {
    public:
        virtual ~Module( ) = default;

        [[nodiscard]] virtual const codegen::TargetIsa& isa( ) const noexcept          = 0;
        [[nodiscard]] virtual const ModuleDeclarations& declarations( ) const noexcept = 0;

        [[nodiscard]] virtual std::expected< FuncId, ModuleError > declare_function( std::string_view name, Linkage linkage,
                                                                                     const Signature& sig ) = 0;

        [[nodiscard]] virtual std::expected< void, ModuleError > define_function( FuncId id, codegen::Context& ctx ) = 0;

        //! @brief Import `id` into the function being built so it can be called.
        [[nodiscard]] ir::FuncRef declare_func_in_func( FuncId id, ir::IRBuilder& builder ) const;
    };

    inline Signature signature_of( const ir::IRFunction& fn )
    {
        return Signature {
            .params = { fn.sig.params.begin( ), fn.sig.params.end( ) },
            .ret    = fn.sig.ret,
            .cc     = fn.sig.cc
        };
    }

    inline std::expected< FuncId, ModuleError > ModuleDeclarations::declare_function( const std::string_view name, const Linkage linkage,
                                                                                      const Signature& sig )
    {
        if ( const auto it = m_names.find( name ); it != m_names.end( ) )
        {
            auto& decl = m_funcs[ it->second ];
            if ( decl.sig != sig ) return std::unexpected( ModuleError::IncompatibleDeclaration );

            if ( decl.linkage != linkage )
            {
                if ( decl.linkage == Linkage::Import ) decl.linkage = linkage;
                else if ( linkage != Linkage::Import )
                    return std::unexpected( ModuleError::IncompatibleDeclaration );
            }

            return FuncId { it->second };
        }

        const auto id = static_cast< uint32_t >( m_funcs.size( ) );
        m_funcs.push_back( FunctionDecl { .name = std::string( name ), .linkage = linkage, .sig = sig } );
        m_names.emplace( std::string( name ), id );
        return FuncId { id };
    }

    inline std::optional< FuncId > ModuleDeclarations::lookup( const std::string_view name ) const
    {
        if ( const auto it = m_names.find( name ); it != m_names.end( ) ) return FuncId { it->second };
        return std::nullopt;
    }

    inline ir::FuncRef Module::declare_func_in_func( const FuncId id, ir::IRBuilder& builder ) const
    {
        const auto& decl = declarations( ).get( id );
        return builder.import_function( ir::ExternalName { ir::extns::function, id.id }, decl.sig.params, decl.sig.ret, decl.sig.cc,
                                        decl.linkage != Linkage::Import );
    }
} // namespace xiao::module