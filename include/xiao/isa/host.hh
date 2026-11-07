#pragma once

#include <xiao/isa/x86/isa.hh>
#include <memory>

namespace xiao::isa
{
    //! @brief The ISA for the current machine
    [[nodiscard]] inline std::unique_ptr< codegen::TargetIsa > lookup_host( )
    {
#if defined( _M_X64 ) || defined( __x86_64__ )
        return std::make_unique< x86_64::X86Isa >( );
#else
#error "xiao: no ISA for this architecture"
#endif
    }
} // namespace xiao::isa