#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace xiao::jit
{
    struct VirtualDeleter
    {
        std::size_t size { };

        void operator( ) ( std::byte* ptr ) const noexcept
        {
            if ( !ptr ) return;
#ifdef _WIN32
            VirtualFree( ptr, 0, MEM_RELEASE );
#else
            munmap( ptr, size );
#endif
        }
    };

    using ExecutableMemory = std::unique_ptr< std::byte[], VirtualDeleter >;

    class JITMemoryProvider
    {
    public:
        explicit JITMemoryProvider( std::size_t size ) :
            m_size( size ),
            m_memory(
                [ & ]( ) -> std::byte*
                {
#ifdef _WIN32
                    return static_cast< std::byte* >( VirtualAlloc( nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE ) );
#else
                    void* p = mmap( nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
                    return p == MAP_FAILED ? nullptr : static_cast< std::byte* >( p );
#endif
                }( ),
                VirtualDeleter { size } )
        {
            if ( !m_memory ) throw std::runtime_error( "xiao::JITMemoryProvider: failed to allocate executable memory page" );
        }

        void write( std::span< const uint8_t > bytes, std::size_t offset = 0 ) const
        {
            if ( offset + bytes.size( ) > m_size ) throw std::out_of_range( "xiao::JITMemoryProvider: JIT memory write overflow" );
            std::memcpy( m_memory.get( ) + offset, bytes.data( ), bytes.size( ) );
        }

        bool finalise_permissions( )
        {
#ifdef _WIN32
            DWORD old { };
            return VirtualProtect( m_memory.get( ), m_size, PAGE_EXECUTE_READ, &old );
#else
            return mprotect( m_memory.get( ), m_size, PROT_READ | PROT_EXEC ) == 0;
#endif
        }

        [[nodiscard]] void* data( ) const noexcept
        { return m_memory.get( ); }

        [[nodiscard]] std::size_t size( ) const noexcept
        { return m_size; }

    private:
        std::size_t      m_size { };
        ExecutableMemory m_memory { };
    };
} // namespace xiao::jit