#pragma once

#include <xiao/codegen/compiled.hh>
#include <xiao/codegen/flags.hh>
#include <xiao/ir/function.hh>
#include <expected>
#include <string_view>

namespace xiao::codegen
{
    struct CodegenError
    {
        enum class Kind : uint8_t
        {
            Verifier,
            Unsupported,
            ImplLimitExceeded,
            RegallocFailed,
        };

        Kind                       kind { };
        std::vector< std::string > messages { };
    };

    //! @brief A target. Heh.
    struct TargetIsa
    {
        virtual ~TargetIsa( ) = default;

        [[nodiscard]] virtual std::string_view                            name( ) const noexcept                       = 0;
        [[nodiscard]] virtual CallConv                                    default_cc( ) const noexcept                 = 0;
        [[nodiscard]] virtual const Flags&                                flags( ) const noexcept                      = 0;
        [[nodiscard]] virtual std::expected< CompiledCode, CodegenError > compile_function( ir::IRFunction& fn ) const = 0;
    };
} // namespace xiao::codegen