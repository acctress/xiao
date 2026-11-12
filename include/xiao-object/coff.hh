#pragma once

#include <xiao-object/object.hh>

#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xiao::object::coff
{
    //! Machine types: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#machine-types
    inline constexpr uint16_t machine_amd64 = 0x8664;

    //! Record sizes:
    //!   file header     https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-file-header-object-and-image
    //!   section header  https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-table-section-headers
    //!   relocation      https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-relocations-object-only
    //!   symbol          https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-symbol-table
    inline constexpr uint32_t file_header_size    = 20;
    inline constexpr uint32_t section_header_size = 40;
    inline constexpr uint32_t relocation_size     = 10;
    inline constexpr uint32_t symbol_size         = 18;

    //! Section flags: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-flags
    inline constexpr uint32_t scn_cnt_code               = 0x00000020;
    inline constexpr uint32_t scn_cnt_initialized_data   = 0x00000040;
    inline constexpr uint32_t scn_cnt_uninitialized_data = 0x00000080;
    inline constexpr uint32_t scn_mem_execute            = 0x20000000;
    inline constexpr uint32_t scn_mem_read               = 0x40000000;
    inline constexpr uint32_t scn_mem_write              = 0x80000000;

    //! x64 relocation types: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#type-indicators
    inline constexpr uint16_t rel_amd64_addr64 = 0x0001;
    inline constexpr uint16_t rel_amd64_rel32  = 0x0004;

    //! Storage classes: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#storage-class
    //! Symbol types: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#type-representation
    inline constexpr uint8_t  sym_class_external = 2;
    inline constexpr uint8_t  sym_class_static   = 3;
    inline constexpr uint16_t sym_type_function  = 0x20;

    //! IMAGE_SCN_ALIGN_*BYTES: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-flags
    [[nodiscard]] inline uint32_t section_flags( const Section& s )
    {
        assert( std::has_single_bit( s.align ) && s.align <= 8192
                && "[xiao::object::coff] section alignment must be a power of two up to 8192" );
        const uint32_t align = static_cast< uint32_t >( std::countr_zero( s.align ) + 1 ) << 20;

        switch ( s.kind )
        {
            case SectionKind::Text: return scn_cnt_code | scn_mem_execute | scn_mem_read | align;
            case SectionKind::ReadOnlyData: return scn_cnt_initialized_data | scn_mem_read | align;
            case SectionKind::Data: return scn_cnt_initialized_data | scn_mem_read | scn_mem_write | align;
            case SectionKind::UninitialisedData: return scn_cnt_uninitialized_data | scn_mem_read | scn_mem_write | align;
        }
        return 0;
    }

    class ByteWriter
    {
    public:
        void u8( const uint8_t v )
        { m_out.push_back( v ); }

        void u16( const uint16_t v )
        { put( v ); }

        void u32( const uint32_t v )
        { put( v ); }

        void bytes( const std::span< const uint8_t > b )
        { m_out.insert( m_out.end( ), b.begin( ), b.end( ) ); }

        void zeros( const std::size_t n )
        { m_out.resize( m_out.size( ) + n, 0 ); }

        void pad_to( const std::size_t offset )
        {
            assert( m_out.size( ) <= offset && "[xiao::object::coff::ByteWriter::pad_to] layout and output disagree" );
            m_out.resize( offset, 0 );
        }

        void name8( const std::string_view s )
        {
            assert( s.size( ) <= 8 );
            bytes( { reinterpret_cast< const uint8_t* >( s.data( ) ), s.size( ) } );
            zeros( 8 - s.size( ) );
        }

        [[nodiscard]] std::size_t size( ) const noexcept
        { return m_out.size( ); }

        [[nodiscard]] std::vector< uint8_t > take( ) &&
        { return std::move( m_out ); }

    private:
        template < typename T >
        void put( const T v )
        {
            static_assert( std::endian::native == std::endian::little );
            uint8_t raw[ sizeof( T ) ];
            std::memcpy( raw, &v, sizeof( T ) );
            bytes( raw );
        }

        std::vector< uint8_t > m_out { };
    };

    //! String table: a u32 size that includes itself, then NUL-terminated names:
    //! https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-string-table
    class StringTable
    {
    public:
        [[nodiscard]] uint32_t add( const std::string_view s )
        {
            if ( const auto iter = m_offsets.find( std::string { s } ); iter != m_offsets.end( ) ) return iter->second;
            const auto offset = static_cast< uint32_t >( 4 + m_bytes.size( ) );
            m_bytes.insert( m_bytes.end( ), s.begin( ), s.end( ) );
            m_bytes.push_back( 0 );
            m_offsets.emplace( std::string { s }, offset );
            return offset;
        }

        void write( ByteWriter& out ) const
        {
            out.u32( static_cast< uint32_t >( 4 + m_bytes.size( ) ) );
            out.bytes( m_bytes );
        }

    private:
        std::vector< uint8_t >                      m_bytes { };
        std::unordered_map< std::string, uint32_t > m_offsets { };
    };

    //! COFF relocations have no addend field, so the addend is stored in the bytes being relocated
    //! rel32 is relative to the byte after the 4 byte field:
    //! https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#type-indicators relocation records:
    //! https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-relocations-object-only
    [[nodiscard]] inline std::vector< uint8_t > relocated_bytes( const Section& s )
    {
        auto bytes = s.bytes;
        for ( const auto& r : s.relocs )
        {
            if ( r.kind == RelocationKind::Rel32 )
            {
                assert( r.offset + 4 <= bytes.size( ) );
                const int64_t implicit = r.addend + 4;
                assert( implicit >= std::numeric_limits< int32_t >::min( ) && implicit <= std::numeric_limits< int32_t >::max( )
                        && "[xiao::object::coff] rel32 addend out of range" );
                const auto v = static_cast< int32_t >( implicit );
                std::memcpy( bytes.data( ) + r.offset, &v, 4 );
            }
            else
            {
                assert( r.offset + 8 <= bytes.size( ) );
                std::memcpy( bytes.data( ) + r.offset, &r.addend, 8 );
            }
        }
        return bytes;
    }

    //! Object file layout: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#file-headers
    //! section data and relocation placement: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-data
    [[nodiscard]] inline std::vector< uint8_t > write( const Object& obj )
    {
        const auto& sections = obj.sections( );
        const auto& symbols  = obj.symbols( );

        assert( sections.size( ) < 0xFFFF && "[xiao::object::coff::write] too many sections" );

        struct Layout
        {
            uint32_t data { };
            uint32_t relocs { };
        };

        std::vector< Layout > layout( sections.size( ) );
        uint32_t              cursor = file_header_size + section_header_size * static_cast< uint32_t >( sections.size( ) );

        for ( std::size_t i { 0 }; i < sections.size( ); ++i )
        {
            const auto& s = sections[ i ];
            assert( s.relocs.size( ) < 0xFFFF && "[xiao::object::coff::write] too many relocations in one section" );

            if ( s.kind != SectionKind::UninitialisedData && s.size( ) > 0 )
            {
                cursor            = ( cursor + 3 ) & ~3u;
                layout[ i ].data  = cursor;
                cursor           += s.size( );
            }
            if ( !s.relocs.empty( ) )
            {
                layout[ i ].relocs  = cursor;
                cursor             += relocation_size * static_cast< uint32_t >( s.relocs.size( ) );
            }
        }

        cursor                       = ( cursor + 3 ) & ~3u;
        const uint32_t symtab_offset = cursor;
        const uint32_t first_symbol  = 2 * static_cast< uint32_t >( sections.size( ) );
        const uint32_t symbol_count  = first_symbol + static_cast< uint32_t >( symbols.size( ) );

        StringTable strings;

        ByteWriter out;

        //! COFF file header: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-file-header-object-and-image
        out.u16( machine_amd64 );
        out.u16( static_cast< uint16_t >( sections.size( ) ) );
        out.u32( 0 );
        out.u32( symtab_offset );
        out.u32( symbol_count );
        out.u16( 0 );
        out.u16( 0 );

        for ( std::size_t i { 0 }; i < sections.size( ); ++i )
        {
            const auto& s = sections[ i ];

            //! Section header: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-table-section-headers
            if ( s.name.size( ) <= 8 ) out.name8( s.name );
            else
            {
                const auto ref = "/" + std::to_string( strings.add( s.name ) );
                assert( ref.size( ) <= 8 && "[xiao::object::coff::write] section name offset too large" );
                out.name8( ref );
            }

            out.u32( 0 );
            out.u32( 0 );
            out.u32( s.size( ) );
            out.u32( layout[ i ].data );
            out.u32( layout[ i ].relocs );
            out.u32( 0 );
            out.u16( static_cast< uint16_t >( s.relocs.size( ) ) );
            out.u16( 0 );
            out.u32( section_flags( s ) );
        }

        for ( std::size_t i { 0 }; i < sections.size( ); ++i )
        {
            const auto& s = sections[ i ];

            if ( layout[ i ].data )
            {
                out.pad_to( layout[ i ].data );
                out.bytes( relocated_bytes( s ) );
            }

            if ( layout[ i ].relocs )
            {
                //! Relocation records: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-relocations-object-only
                out.pad_to( layout[ i ].relocs );
                for ( const auto& r : s.relocs )
                {
                    assert( r.symbol < symbols.size( ) && "[xiao::object::coff::write] relocation against an unknown symbol" );
                    out.u32( r.offset );
                    out.u32( first_symbol + r.symbol );
                    out.u16( r.kind == RelocationKind::Rel32 ? rel_amd64_rel32 : rel_amd64_addr64 );
                }
            }
        }

        out.pad_to( symtab_offset );

        //! Symbol names: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#symbol-name-representation
        const auto symbol_name = [ & ]( const std::string_view name )
        {
            if ( name.size( ) <= 8 ) return out.name8( name );
            out.u32( 0 );
            out.u32( strings.add( name ) );
        };

        for ( std::size_t i { 0 }; i < sections.size( ); ++i )
        {
            const auto& s = sections[ i ];

            //! Section symbol followed by an aux format 5 record:
            //! https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#auxiliary-format-5-section-definitions
            symbol_name( s.name );
            out.u32( 0 );
            out.u16( static_cast< uint16_t >( i + 1 ) );
            out.u16( 0 );
            out.u8( sym_class_static );
            out.u8( 1 );

            out.u32( s.size( ) );
            out.u16( static_cast< uint16_t >( s.relocs.size( ) ) );
            out.u16( 0 );
            out.u32( 0 );
            out.u16( 0 );
            out.u8( 0 );
            out.zeros( 3 );
        }

        //! Symbol records, section numbers are 1 based and 0 means undefined:
        //! https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-symbol-table
        //! https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-number-values
        for ( const auto& [ name, scope, section, value, is_function ] : symbols )
        {
            const bool defined = scope != SymbolScope::Undefined;
            assert( ( !defined || section < sections.size( ) ) && "[xiao::object::coff::write] symbol in an unknown section" );

            symbol_name( name );
            out.u32( defined ? value : 0 );
            out.u16( defined ? static_cast< uint16_t >( section + 1 ) : uint16_t { 0 } );
            out.u16( is_function ? sym_type_function : uint16_t { 0 } );
            out.u8( scope == SymbolScope::Local ? sym_class_static : sym_class_external );
            out.u8( 0 );
        }

        //! String table: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-string-table
        strings.write( out );
        return std::move( out ).take( );
    }
} // namespace xiao::object::coff
