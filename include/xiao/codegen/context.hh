#pragma once

#include <xiao/ir/verifier.hh>
#include <xiao/codegen/isa.hh>
#include <optional>

namespace xiao::codegen
{
    //! @brief Holds one function being compiled and aftwards... its compiled code
    class Context
    {
    public:
        explicit Context( ir::IRFunction& fn ) : m_fn { &fn } { }

        [[nodiscard]] std::expected< const CompiledCode*, CodegenError > compile( const TargetIsa& isa );

        [[nodiscard]] ir::IRFunction& func( ) const noexcept
        { return *m_fn; }

        [[nodiscard]] const CompiledCode* compiled( ) const noexcept
        { return m_compiled ? &*m_compiled : nullptr; }

        [[nodiscard]] const CodegenError* error( ) const noexcept
        { return m_error ? &*m_error : nullptr; }

        void clear( ) noexcept
        {
            m_compiled.reset( );
            m_error.reset( );
        }

    private:
        ir::IRFunction*               m_fn { nullptr };
        std::optional< CompiledCode > m_compiled { };
        std::optional< CodegenError > m_error { };
    };

    inline std::expected< const CompiledCode*, CodegenError > Context::compile( const TargetIsa& isa )
    {
        clear( );

        if ( isa.flags( ).enable_verifier )
        {
            ir::IRVerifier verifier;
            verifier.visit_function( *m_fn );

            if ( !verifier.ok( ) )
            {
                CodegenError err { .kind = CodegenError::Kind::Verifier };
                for ( auto& [ msg ] : verifier.errs ) err.messages.push_back( std::move( msg ) );
                m_error = err;

                return std::unexpected( std::move( err ) );
            }
        }

        auto result = isa.compile_function( *m_fn );
        if ( !result )
        {
            m_error = result.error( );
            return std::unexpected( std::move( result.error( ) ) );
        }

        m_compiled = std::move( *result );
        return &*m_compiled;
    }
} // namespace xiao::codegen