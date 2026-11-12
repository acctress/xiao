#pragma once

#include <xiao-object/object.hh>

#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

namespace xiao::object
{
    [[nodiscard]] inline std::string_view to_string( const SectionKind kind ) noexcept
    {
        switch ( kind )
        {
            case SectionKind::Text: return "code";
            case SectionKind::ReadOnlyData: return "read-only data";
            case SectionKind::Data: return "writable data";
            case SectionKind::UninitialisedData: return "zero fill";
        }
        return "?";
    }

    [[nodiscard]] inline std::string_view to_string( const SymbolScope scope ) noexcept
    {
        switch ( scope )
        {
            case SymbolScope::Local: return "local";
            case SymbolScope::Global: return "global";
            case SymbolScope::Undefined: return "undefined";
        }
        return "?";
    }

    [[nodiscard]] inline std::string_view to_string( const RelocationKind kind ) noexcept
    {
        switch ( kind )
        {
            case RelocationKind::Rel32: return "rel32";
            case RelocationKind::Abs64: return "abs64";
        }
        return "?";
    }

    [[nodiscard]] inline std::string dump( const Object& obj )
    {
        const auto& sections = obj.sections( );
        const auto& symbols  = obj.symbols( );

        std::string out;
        const auto  o = std::back_inserter( out );

        std::size_t name_width { 4 };
        for ( const auto& s : symbols ) name_width = std::max( name_width, s.name.size( ) );
        std::size_t section_width { 4 };
        for ( const auto& s : sections ) section_width = std::max( section_width, s.name.size( ) );

        std::format_to( o, "  sections\n" );
        for ( std::size_t i { 0 }; i < sections.size( ); ++i )
        {
            const auto& s = sections[ i ];
            std::format_to( o, "    [{}] {:<{}}  {:<14}  align {:<4}  size {:<6}  relocs {}\n", i, s.name, section_width,
                            to_string( s.kind ), s.align, s.size( ), s.relocs.size( ) );
        }

        std::format_to( o, "\n  symbols\n" );
        for ( std::size_t i { 0 }; i < symbols.size( ); ++i )
        {
            const auto& [ name, scope, section, value, is_function ] = symbols[ i ];
            const auto where
                = scope == SymbolScope::Undefined ? std::string { "-" } : std::format( "{}+{:#x}", sections[ section ].name, value );
            std::format_to( o, "    [{}] {:<{}}  {:<9}  {:<{}}  {}\n", i,  name, name_width, to_string( scope ), where, section_width + 8,
                            is_function ? "function" : "data" );
        }

        std::format_to( o, "\n  relocations\n" );
        bool any { false };
        for ( const auto& s : sections )
            for ( const auto& [ offset, symbol, kind, addend ] : s.relocs )
            {
                any = true;
                std::format_to( o, "    {}+{:#06x}  {}  {}{}\n", s.name, offset, to_string( kind ), symbols[ symbol ].name,
                                addend ? std::format( "  addend {}", addend ) : std::string { } );
            }
        if ( !any ) std::format_to( o, "    (none)\n" );

        for ( const auto& s : sections )
        {
            std::format_to( o, "\n  contents of {}\n", s.name );
            if ( s.kind == SectionKind::UninitialisedData )
            {
                std::format_to( o, "    ({} zero bytes, not stored in the file)\n", s.bss_size );
                continue;
            }

            for ( std::size_t row { 0 }; row < s.bytes.size( ); row += 16 )
            {
                std::string hex, text;
                for ( std::size_t k { 0 }; k < 16; ++k )
                {
                    if ( row + k < s.bytes.size( ) )
                    {
                        const auto b  = s.bytes[ row + k ];
                        hex          += std::format( "{:02x} ", b );
                        text         += b >= 0x20 && b < 0x7f ? static_cast< char >( b ) : '.';
                    }
                    else
                        hex += "   ";
                    if ( k == 7 ) hex += ' ';
                }
                std::format_to( o, "    {:04x}  {} |{}|\n", row, hex, text );
            }
        }

        return out;
    }
} // namespace xiao::object
