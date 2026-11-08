#pragma once

#include <xiao/module/module.hh>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>
#include <vector>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace xiao::jit
{
    namespace detail
    {
        [[nodiscard]] inline std::size_t os_granularity( ) noexcept
        {
#if defined( _WIN32 )
            SYSTEM_INFO info { };
            GetSystemInfo( &info );
            return info.dwAllocationGranularity;
#else
            return static_cast< std::size_t >( sysconf( _SC_PAGESIZE ) );
#endif
        }

        [[nodiscard]] inline std::byte* os_alloc_rw( const std::size_t size ) noexcept
        {
#if defined( _WIN32 )
            return static_cast< std::byte* >( VirtualAlloc( nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );
#else
            void* p = mmap( nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
            return p == MAP_FAILED ? nullptr : static_cast< std::byte* >( p );
#endif
        }

        [[nodiscard]] inline bool os_protect_rx( std::byte* const base, const std::size_t size ) noexcept
        {
#if defined( _WIN32 )
            DWORD old { };
            if ( !VirtualProtect( base, size, PAGE_EXECUTE_READ, &old ) ) return false;
            return FlushInstructionCache( GetCurrentProcess( ), base, size ) != 0;
#else
            if ( mprotect( base, size, PROT_READ | PROT_EXEC ) != 0 ) return false;
            __builtin___clear_cache( reinterpret_cast< char* >( base ), reinterpret_cast< char* >( base + size ) );
            return true;
#endif
        }

        inline void os_free( std::byte* const base, [[maybe_unused]] const std::size_t size ) noexcept
        {
#if defined( _WIN32 )
            VirtualFree( base, 0, MEM_RELEASE );
#else
            munmap( base, size );
#endif
        }

        [[nodiscard]] constexpr std::uintptr_t align_up( const std::uintptr_t v, const std::size_t align ) noexcept
        { return ( v + align - 1 ) & ~( static_cast< std::uintptr_t >( align ) - 1 ); }
    } // namespace detail

    class Memory
    {
    public:
        static constexpr std::size_t min_block_size = 64 * 1024;

        Memory( ) = default;

        ~Memory( )
        { release( ); }

        Memory( const Memory& )             = delete;
        Memory& operator= ( const Memory& ) = delete;

        Memory( Memory&& other ) noexcept :
            m_blocks { std::exchange( other.m_blocks, { } ) },
            m_cursor { std::exchange( other.m_cursor, nullptr ) },
            m_end { std::exchange( other.m_end, nullptr ) }
        {
        }

        Memory& operator= ( Memory&& other ) noexcept
        {
            if ( this != &other )
            {
                release( );
                m_blocks = std::exchange( other.m_blocks, { } );
                m_cursor = std::exchange( other.m_cursor, nullptr );
                m_end    = std::exchange( other.m_end, nullptr );
            }
            return *this;
        }

        [[nodiscard]] std::expected< std::byte*, module::ModuleError > allocate( std::size_t size, const std::size_t align )
        {
            assert( std::has_single_bit( align ) && "[xiao::jit::Memory::allocate] align must be a power of two" );
            if ( size == 0 ) size = 1;

            const auto fits = [ & ]
            {
                if ( !m_cursor ) return false;
                const auto aligned = detail::align_up( reinterpret_cast< std::uintptr_t >( m_cursor ), align );
                return aligned + size <= reinterpret_cast< std::uintptr_t >( m_end );
            };

            if ( !fits( ) )
            {
                const auto block_size = detail::align_up( std::max( size + align, min_block_size ), detail::os_granularity( ) );

                auto* const base = detail::os_alloc_rw( block_size );
                if ( !base ) return std::unexpected( module::ModuleError::AllocationFailed );

                m_blocks.push_back( Block { .base = base, .size = block_size, .finalized = false } );
                m_cursor = base;
                m_end    = base + block_size;
            }

            auto* const result = m_cursor
                               + ( detail::align_up( reinterpret_cast< std::uintptr_t >( m_cursor ), align )
                                   - reinterpret_cast< std::uintptr_t >( m_cursor ) );
            m_cursor = result + size;
            return result;
        }

        [[nodiscard]] std::expected< void, module::ModuleError > set_readable_and_executable( )
        {
            for ( auto& block : m_blocks )
            {
                if ( block.finalized ) continue;
                if ( !detail::os_protect_rx( block.base, block.size ) ) return std::unexpected( module::ModuleError::AllocationFailed );
                block.finalized = true;
            }

            m_cursor = nullptr;
            m_end    = nullptr;
            return { };
        }

        [[nodiscard]] std::size_t block_count( ) const noexcept
        { return m_blocks.size( ); }

    private:
        struct Block
        {
            std::byte*  base { };
            std::size_t size { };
            bool        finalized { };
        };

        void release( ) noexcept
        {
            for ( const auto& block : m_blocks ) detail::os_free( block.base, block.size );
            m_blocks.clear( );
            m_cursor = nullptr;
            m_end    = nullptr;
        }

        std::vector< Block > m_blocks { };
        std::byte*           m_cursor { };
        std::byte*           m_end { };
    };
} // namespace xiao::jit
