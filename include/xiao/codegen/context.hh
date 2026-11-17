#pragma once

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

        void clear( ) noexcept
        { m_compiled.reset( ); }

    private:
        ir::IRFunction*               m_fn { nullptr };
        std::optional< CompiledCode > m_compiled { };
    };

    inline std::expected< const CompiledCode*, CodegenError > Context::compile( const TargetIsa& isa )
    {
        auto result = isa.compile_function( *m_fn );
        if ( !result ) return std::unexpected( result.error( ) );

        m_compiled = std::move( *result );
        return &*m_compiled;
    }
} // namespace xiao::codegen