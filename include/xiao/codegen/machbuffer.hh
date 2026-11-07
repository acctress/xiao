#pragma once

#include <xiao/codegen/machinst.hh>
#include <xiao/codegen/compiled.hh>
#include <xiao/codegen/reloc.hh>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <vector>
#include <span>

namespace xiao::codegen
{
    //! @brief A machine code buffer for JIT and AOT modules and code emission.
    //! @details `MachBuffer` contains a contiguous vector of raw machine code bytes.
    template < LabelUseC LabelUse >
    class MachBuffer
    {
    public:
        struct LabelFixup
        {
            uint32_t offset { };
            uint32_t label { };
            LabelUse kind;
        };

        //! @param initial_capacity The pre allocated byte capacity
        explicit MachBuffer( const std::size_t initial_capacity = 1024 )
        { m_bytes.reserve( initial_capacity ); }

        //! @brief Emit a single unsigned 8-bit byte
        constexpr void emit_u8( const uint8_t value ) noexcept
        { m_bytes.push_back( value ); }

        //! @brief Emit an unsigned 16-bit byte in LE
        constexpr void emit_u16( const uint16_t value ) noexcept
        {
            m_bytes.push_back( static_cast< uint8_t >( value ) );
            m_bytes.push_back( static_cast< uint8_t >( value >> 8 ) );
        }

        //! @brief Emit an unsigned 32-bit byte in LE
        constexpr void emit_u32( const uint32_t value ) noexcept
        {
            m_bytes.push_back( static_cast< uint8_t >( value ) );
            m_bytes.push_back( static_cast< uint8_t >( value >> 8 ) );
            m_bytes.push_back( static_cast< uint8_t >( value >> 16 ) );
            m_bytes.push_back( static_cast< uint8_t >( value >> 24 ) );
        }

        //! @brief Emit a singed 32-bit integer
        constexpr void emit_i32( const int32_t value ) noexcept
        { emit_u32( static_cast< uint32_t >( value ) ); }

        //! @brief Emit an unsigned 64-bit integer in LE
        constexpr void emit_u64( const uint64_t value ) noexcept
        {
            emit_u32( static_cast< uint32_t >( value ) );
            emit_u32( static_cast< uint32_t >( value >> 32 ) );
        }

        //! @brief Emit a contiguous span of raw bytes
        constexpr void emit_bytes( std::span< const uint8_t > bytes )
        { m_bytes.insert( m_bytes.end( ), bytes.begin( ), bytes.end( ) ); }

        //! @brief Returns the current byte offset inside the buffer
        [[nodiscard]] constexpr std::size_t current_offset( ) const noexcept
        { return m_bytes.size( ); }

        //! @brief Patches a previousl emitted 32-bit slot with a new value
        constexpr void patch_i32( const std::size_t offset, const int32_t value ) noexcept
        { std::memcpy( m_bytes.data( ) + offset, &value, sizeof value ); }

        //! @brief Get a view of the emitted bytes
        [[nodiscard]] constexpr std::span< const uint8_t > data( ) const noexcept
        { return m_bytes; }

        //! @brief Get the total size of the buffer in bytes
        [[nodiscard]] constexpr std::size_t size( ) const noexcept
        { return m_bytes.size( ); }

        //! @brief Clears all emitted bytes in the buffer
        constexpr void clear( ) noexcept
        { m_bytes.clear( ); }

        //! @brief Pushes a sentinel value onto `m_label_offsets`, returning the index
        [[nodiscard]] constexpr uint32_t new_label( )
        {
            m_label_offsets.push_back( ~0u );
            return static_cast< uint32_t >( m_label_offsets.size( ) - 1 );
        }

        //! @brief Bind the current offset to `label`
        void bind_label( const uint32_t label ) noexcept
        { m_label_offsets[ label ] = current_offset( ); }

        //! @brief Emit four zero bytes and push the fixup
        void emit_label_ref( const uint32_t label, const LabelUse kind ) noexcept
        {
            const auto off = static_cast< uint32_t >( current_offset( ) );
            emit_u32( 0 );
            m_fixups.push_back( { off, label, kind } );
        }

        //! @brief Record a relocation for the field that starts at the current offset
        void add_reloc( const RelocKind kind, const ir::ExternalName target, const int64_t addend )
        { m_relocs.push_back( { static_cast< uint32_t >( current_offset( ) ), kind, target, addend } ); }

        [[nodiscard]] CompiledCode finish( ) &&
        {
            resolve_labels( );
            return CompiledCode { .code = std::move( m_bytes ), .relocs = std::move( m_relocs ) };
        }

        void resolve_labels( )
        {
            for ( const auto& [ offset, label, kind ] : m_fixups )
            {
                assert( m_label_offsets[ label ] != ~0u && "xiao::codegen::MachBuffer: unbound label at resolve time" );

                const auto target = m_label_offsets[ label ];
                const auto enc    = LabelUse::encode( kind, offset, target );
                patch_i32( offset, enc );
            }

            m_fixups.clear( );
        }

    private:
        std::vector< uint8_t >    m_bytes { };
        std::vector< LabelFixup > m_fixups { };
        std::vector< uint32_t >   m_label_offsets { };
        std::vector< Reloc >      m_relocs { };
    };

} // namespace xiao::codegen