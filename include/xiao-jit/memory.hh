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
    enum class Protection : uint8_t
    {
        Executable,
        ReadOnly,
        Writable,
    };

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

        [[nodiscard]] inline std::byte* os_reserve( const std::size_t size ) noexcept
        {
#if defined( _WIN32 )
            return static_cast< std::byte* >( VirtualAlloc( nullptr, size, MEM_RESERVE, PAGE_NOACCESS ) );
#else
            void* p = mmap( nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0 );
            return p == MAP_FAILED ? nullptr : static_cast< std::byte* >( p );
#endif
        }

        [[nodiscard]] inline bool os_commit_rw( std::byte* const base, const std::size_t size ) noexcept
        {
#if defined( _WIN32 )
            return VirtualAlloc( base, size, MEM_COMMIT, PAGE_READWRITE ) != nullptr;
#else
            return mprotect( base, size, PROT_READ | PROT_WRITE ) == 0;
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

        [[nodiscard]] inline bool os_protect( std::byte* const base, const std::size_t size, const Protection prot ) noexcept
        {
#if defined( _WIN32 )
            DWORD old { };
            switch ( prot )
            {
                case Protection::Executable:
                    if ( !VirtualProtect( base, size, PAGE_EXECUTE_READ, &old ) ) return false;
                    return FlushInstructionCache( GetCurrentProcess( ), base, size ) != 0;
                case Protection::ReadOnly: return VirtualProtect( base, size, PAGE_READONLY, &old ) != 0;
                case Protection::Writable: return true;
            }
            return false;
#else
            switch ( prot )
            {
                case Protection::Executable:
                    if ( mprotect( base, size, PROT_READ | PROT_EXEC ) != 0 ) return false;
                    __builtin___clear_cache( reinterpret_cast< char* >( base ), reinterpret_cast< char* >( base + size ) );
                    return true;
                case Protection::ReadOnly: return mprotect( base, size, PROT_READ ) == 0;
                case Protection::Writable: return true;
            }
            return false;
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

    class Region
    {
    public:
        static constexpr std::size_t default_size = std::size_t { 1 } << 30;

        explicit Region( const std::size_t size = default_size ) :
            m_size { detail::align_up( size, detail::os_granularity( ) ) },
            m_base { detail::os_reserve( m_size ) }
        {
            if ( !m_base ) m_size = 0;
        }

        ~Region( )
        {
            if ( m_base ) detail::os_free( m_base, m_size );
        }

        Region( const Region& )             = delete;
        Region& operator= ( const Region& ) = delete;

        [[nodiscard]] std::byte* take( const std::size_t size ) noexcept
        {
            if ( !m_base || m_used + size > m_size ) return nullptr;
            auto* const p = m_base + m_used;
            if ( !detail::os_commit_rw( p, size ) ) return nullptr;
            m_used += size;
            return p;
        }

        [[nodiscard]] bool contains( const void* p ) const noexcept
        {
            const auto* b = static_cast< const std::byte* >( p );
            return m_base && b >= m_base && b < m_base + m_size;
        }

    private:
        std::size_t m_size { };
        std::byte*  m_base { };
        std::size_t m_used { };
    };

    class Memory
    {
    public:
        static constexpr std::size_t min_block_size = 64 * 1024;

        explicit Memory( const Protection prot = Protection::Executable, Region* const region = nullptr ) noexcept :
            m_prot { prot },
            m_region { region }
        {
        }

        ~Memory( )
        { release( ); }

        Memory( const Memory& )             = delete;
        Memory& operator= ( const Memory& ) = delete;

        Memory( Memory&& other ) noexcept :
            m_prot { other.m_prot },
            m_region { std::exchange( other.m_region, nullptr ) },
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
                m_prot   = other.m_prot;
                m_region = std::exchange( other.m_region, nullptr );
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

                std::byte* base  = m_region ? m_region->take( block_size ) : nullptr;
                const bool owned = base == nullptr;
                if ( owned ) base = detail::os_alloc_rw( block_size );
                if ( !base ) return std::unexpected( module::ModuleError::AllocationFailed );

                m_blocks.push_back( Block { .base = base, .size = block_size, .finalised = false, .owned = owned } );
                m_cursor = base;
                m_end    = base + block_size;
            }

            auto* const result = m_cursor
                               + ( detail::align_up( reinterpret_cast< std::uintptr_t >( m_cursor ), align )
                                   - reinterpret_cast< std::uintptr_t >( m_cursor ) );
            m_cursor = result + size;
            return result;
        }

        [[nodiscard]] std::expected< void, module::ModuleError > finalise( )
        {
            if ( m_prot == Protection::Writable ) return { };

            for ( auto& block : m_blocks )
            {
                if ( block.finalised ) continue;
                if ( !detail::os_protect( block.base, block.size, m_prot ) )
                    return std::unexpected( module::ModuleError::AllocationFailed );
                block.finalised = true;
            }

            m_cursor = nullptr;
            m_end    = nullptr;
            return { };
        }

        [[nodiscard]] std::size_t block_count( ) const noexcept
        { return m_blocks.size( ); }

        [[nodiscard]] Protection protection( ) const noexcept
        { return m_prot; }

    private:
        struct Block
        {
            std::byte*  base { };
            std::size_t size { };
            bool        finalised { };
            bool        owned { };
        };

        void release( ) noexcept
        {
            for ( const auto& block : m_blocks )
                if ( block.owned ) detail::os_free( block.base, block.size );
            m_blocks.clear( );
            m_cursor = nullptr;
            m_end    = nullptr;
        }

        Protection           m_prot { Protection::Executable };
        Region*              m_region { };
        std::vector< Block > m_blocks { };
        std::byte*           m_cursor { };
        std::byte*           m_end { };
    };
} // namespace xiao::jit
