#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xiao::object
{
    enum class RelocationKind : uint8_t
    {
        Rel32,
        Abs64
    };

    enum class SymbolScope : uint8_t
    {
        Local,
        Global,
        Undefined
    };

    enum class SectionKind : uint8_t
    {
        Text,
        ReadOnlyData,
        Data,
        UninitialisedData
    };

    struct Relocation
    {
        uint32_t       offset { };
        uint32_t       symbol { };
        RelocationKind kind { };
        int64_t        addend { };
    };

    struct Section
    {
        std::string               name { };
        SectionKind               kind { };
        uint32_t                  align { 1 };
        std::vector< uint8_t >    bytes { };
        uint32_t                  bss_size { };
        std::vector< Relocation > relocs { };

        [[nodiscard]] uint32_t size( ) const noexcept
        { return kind == SectionKind::UninitialisedData ? bss_size : static_cast< uint32_t >( bytes.size( ) ); }
    };

    struct Symbol
    {
        std::string name { };
        SymbolScope scope { };
        uint32_t    section { };
        uint32_t    value { };
        bool        is_function { };
    };

    class Object
    {
    public:
        uint32_t add_section( std::string name, const SectionKind kind, const uint32_t align )
        {
            m_sections.push_back( Section { .name = std::move( name ), .kind = kind, .align = align } );
            return static_cast< uint32_t >( m_sections.size( ) - 1 );
        }

        void add_reloc( const uint32_t section, const Relocation& rel )
        {
            assert( m_sections[ section ].kind != SectionKind::UninitialisedData
                    && "[xiao::object::Object::add_reloc] bss has no bytes to relocate" );
            m_sections[ section ].relocs.push_back( rel );
        }

        uint32_t add_symbol( Symbol sym )
        {
            m_symbols.push_back( std::move( sym ) );
            return static_cast< uint32_t >( m_symbols.size( ) - 1 );
        }

        uint32_t append( const uint32_t section, const std::span< const uint8_t > bytes, const uint32_t align )
        {
            auto& s = m_sections[ section ];
            assert( s.kind != SectionKind::UninitialisedData && "[xiao::object::Object::append] use reserve for bss" );

            s.align = std::max( s.align, align );
            s.bytes.resize( ( s.bytes.size( ) + align - 1 ) / align * align,
                            s.kind == SectionKind::Text ? uint8_t { 0xCC } : uint8_t { 0 } );
            const auto offset = static_cast< uint32_t >( s.bytes.size( ) );
            s.bytes.insert( s.bytes.end( ), bytes.begin( ), bytes.end( ) );
            return offset;
        }

        uint32_t reserve( const uint32_t section, const uint32_t size, const uint32_t align )
        {
            auto& s = m_sections[ section ];
            assert( s.kind == SectionKind::UninitialisedData && "[xiao::object::Object::reserve] only bss reserves space" );

            s.align           = std::max( s.align, align );
            const auto offset = ( s.bss_size + align - 1 ) / align * align;
            s.bss_size        = offset + size;
            return offset;
        }

        [[nodiscard]] const std::vector< Section >& sections( ) const noexcept
        { return m_sections; }

        [[nodiscard]] const std::vector< Symbol >& symbols( ) const noexcept
        { return m_symbols; }

    private:
        std::vector< Section > m_sections { };
        std::vector< Symbol >  m_symbols { };
    };
} // namespace xiao::object
