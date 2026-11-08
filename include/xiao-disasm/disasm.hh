#pragma once

#include <xiao/codegen/compiled.hh>
#include <xiao/codegen/reloc.hh>

#include <Zydis.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace xiao::disasm
{
    [[nodiscard]] inline std::string_view reloc_name( const codegen::RelocKind kind ) noexcept
    {
        switch ( kind )
        {
            case codegen::RelocKind::X86CallPCRel4: return "x86_call_pcrel4";
            case codegen::RelocKind::Abs8: return "abs8";
        }
        return "<?>";
    }

    [[nodiscard]] inline std::string dump( const codegen::CompiledCode& compiled, const uint64_t base_address = 0 )
    {
        constexpr std::size_t bytes_column = 11;

        const auto& code = compiled.code;
        std::string out;

        std::size_t offset { 0 };
        while ( offset < code.size( ) )
        {
            ZydisDisassembledInstruction inst { };
            const bool ok = ZYAN_SUCCESS( ZydisDisassembleIntel( ZYDIS_MACHINE_MODE_LONG_64, base_address + offset, code.data( ) + offset,
                                                                 code.size( ) - offset, &inst ) );

            const std::size_t length = ok ? inst.info.length : 1;

            std::string bytes;
            for ( std::size_t i { 0 }; i < length; ++i ) bytes += std::format( "{:02x} ", code[ offset + i ] );
            bytes.resize( std::max( bytes.size( ), bytes_column * 3 ), ' ' );

            std::format_to( std::back_inserter( out ), "  {:04x}  {}{}", offset, bytes, ok ? std::string_view { inst.text } : "(bad)" );

            for ( const auto& r : compiled.relocs )
                if ( r.offset >= offset && r.offset < offset + length )
                    std::format_to( std::back_inserter( out ), "    ; {} u{}:{}{}", reloc_name( r.kind ), r.target.ns, r.target.index,
                                    r.addend ? std::format( "{:+}", r.addend ) : std::string { } );

            out += '\n';
            offset += length;
        }

        return out;
    }
} // namespace xiao::disasm
